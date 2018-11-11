/*
  A basic implementation of an unzip utility that conforms to the DEFLATE specifications (RFC 1951, 1952 for format)
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#include "ryunzip.h"

int read_bit(struct deflate_stream *stream) {
    if(!stream->pos) { // read in new byte
        stream->pos = 0x01;
        if(fread(&stream->buf, sizeof(unsigned char), 1, stream->fp) < 1) {
            perror("Error reading in read_bit\n");
            exit(1);
        }
    }
    int bit = (stream->buf & stream->pos)?1:0;
    stream->pos <<= 1; // advance the bit position
    return bit;
}

int read_bits(struct deflate_stream *stream, int n) {
    int i, ret = 0;
    for(i=0; i<n; ++i) {
        ret |= (read_bit(stream) << i); // bit order is LSB->MSB
    }
    return ret;
}

void read_header(struct deflate_stream *file, struct Header *header) {
    int i;

    // magic bits
    fread(&header->id1, 1, 1, file->fp);
    fread(&header->id2, 1, 1, file->fp);
    if(!(header->id1 == 0x1f && header->id2 == 0x8b)) { // magic bits not set
        fprintf(stderr, "missing magic bits: 0x%02x 0x%02x\n", header->id1, header->id2); 
        exit(1);
    }

    // compression method
    fread(&header->cm, 1, 1, file->fp);
    if(header->cm != 0x08) {
        fprintf(stderr, "compression method isn't DEFLATE: 0x%02x\n", header->cm);
        exit(1);
    }

    fread(&header->flg, 1, 1, file->fp); // flags
    header->mtime = 0; // clear the top 4 bytes
    fread(&header->mtime, 1, 4, file->fp); // file mtime

    fread(&header->xfl, 1, 1, file->fp); // extra flags
    if(!(header->xfl == 0 || header->xfl == 2 || header->xfl == 4)) {
        fprintf(stderr, "unhandled XFL: 0x%02x\n", header->xfl);
        exit(1);
    }

    fread(&header->os, 1, 1, file->fp); // OS

    // deal with flags (only fname)
    if(header->flg & FNAME) { // read name
        i = 0;
        while(i < MAX_FILE_NAME - 1) {
            fread(header->filename + i, 1, 1, file->fp);
            if(header->filename[i++] == '\0') break;
        }
        if(header->filename[i-1] != '\0') {
            fprintf(stderr, "Filename too long!\n");
            exit(1);
        }
    }
}

void print_header(struct Header *header) {
    struct tm mtime;
    char buf[80];
    
    // OS type
    if(header->os == 3) printf("Compression OS: Unix\n");
    else printf("Compression OS: Not Unix\n");

    // file name
    if(header->flg & FNAME) {
        printf("Original File Name: %s\n", header->filename);
    }

    // time  
    mtime = *localtime(&header->mtime);
    strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &mtime);
    printf("Original File mtime: %s\n", buf);

    // extra flags
    if(header->xfl == 2) printf("XFL=2: Max compression, slowest algorithm\n");
    else if(header->xfl == 4) printf("XFL=4: Fastest compression algorithm\n");
    else printf("XFL=0: No extra information\n");
}

struct huffman_node* traverse_tree(struct huffman_node *root, unsigned int code, int create) {
    unsigned int bit, mask = 0x01;
    while(mask != 0) {
        bit = (code & mask)?1:0;
        mask <<= 1;
        if(root->children[bit] == NULL) {
            if(create) {
                if((root->children[bit] = calloc(1, sizeof(struct huffman_node))) == NULL) {
                    perror("calloc failed in traverse_tree");
                    exit(1);
                }
            } else return NULL;
        }
        root = root->children[bit];
    }
}

void build_tree(struct huffman_node* root, struct huffman_length lengths[], int lengths_size) {
    struct huffman_node *curnode;
    int tmp, i, bl_count[MAX_HUFFMAN_LENGTH+1];
    unsigned int next_code[MAX_HUFFMAN_LENGTH+1], code;
    struct Tree *tree;

    // Allocate space for tree
    if((tree = calloc(lengths[lengths_size-1].end+1, sizeof(struct Tree))) == NULL) {
        perror("calloc for tree failed in huffman_tree");
        exit(1);
    }

    // Build bl_count 
    memset(bl_count, 0, sizeof(bl_count));
    for(i=0; i < lengths_size; ++i) {
        bl_count[lengths[i].len] += lengths[i].end - ((i>0)?lengths[i-1].end:-1);
    }

    // Compute next_code (from RFC 1951, Section 3.2.2)
    code = 0;
    for(i=1; i <= MAX_HUFFMAN_LENGTH; ++i) {
        code = (code + bl_count[i-1]) << 1;
        next_code[i] = code;
    }

    // Build tree (modified from RFC 1951, Section 3.2.2)
    tmp = 0;
    for(i = 0; i < lengths[lengths_size-1].end+1; ++i) {
        if(i > lengths[tmp].end) tmp++;
        tree[i].len = lengths[tmp].len;
        tree[i].code = next_code[tree[i].len]++;
    }
    
    // Build the Huffman lookup tree
    for(i = 0; i < lengths[lengths_size-1].end+1; ++i) {
        curnode = traverse_tree(root, tree[i].code, 1);
        curnode->val = i;
    }
}

void deflate(struct deflate_stream *file) {
    int bfinal, btype;
    int len, nlen; // case 0
    char buf[NONCOMPRESSIBLE_BLOCK_SIZE];
    struct huffman_node root; // cases 1, 2
    
    do {
        bfinal = read_bits(file, 1);
        btype = read_bits(file, 2);
        printf("\nbfinal: %d, btype: %d\n", bfinal, btype);
        if(btype == 0) { // uncompressed
            while(file->pos != 0) read_bits(file, 1); // ignore remainder of block
            len = read_bits(file, 2);
            nlen = read_bits(file, 2);
            if((len & nlen) != 0) { // sanity check
                fprintf(stderr, "len, nlen are not complements\n");
                exit(1);
            }
            fread(buf, 1, len, file->fp);
            // TODO: Copy to output
        } else if(btype == 1) { // compressed with fixed Huffman
            build_tree(&root, fixed_huffman, 4);
        } else if(btype == 2) { // compressed with dynamic Huffman
            // decode(file);
        } else {
            fprintf(stderr, "Invalid block type: %d", btype);
            exit(1);
        }
    } while(bfinal != 1);
}

int main(int argc, char *argv[]) {
    struct deflate_stream file;
    struct Header header;
    
    // Open file
    if(argc != 2) { // check number of arguments
        fprintf(stderr, "Usage: ryunzip <file>\n");
        return 1;
    }

    if((file.fp=fopen(argv[1], "rb")) == NULL) {
        perror("Invalid file; can't open.\n");
        return 1;
    }

    read_header(&file, &header);
    print_header(&header);
    deflate(&file);

    if(fclose(file.fp) != 0) {
        perror("Error occurred while closing file.\n");
        return 1;
    }
    return 0;
}