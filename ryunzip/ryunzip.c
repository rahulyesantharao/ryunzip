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
        if(fread(&stream->buf, 1, 1, stream->fp) < 1) {
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

void read_header(struct deflate_stream *stream, struct FullFile *file) {
    int i;

    // read in header
    fread(&file->header, 1, sizeof(file->header), stream->fp);
    
    // Check header validity
    if(!(file->header.id1 == 0x1f && file->header.id2 == 0x8b)) { // magic bits not set
        fprintf(stderr, "missing magic bits: 0x%02x 0x%02x\n", file->header.id1, file->header.id2); 
        exit(1);
    }
    if(file->header.cm != 0x08) {
        fprintf(stderr, "compression method isn't DEFLATE: 0x%02x\n", file->header.cm);
        exit(1);
    }
    if(!(file->header.xfl == 0 || file->header.xfl == 2 || file->header.xfl == 4)) {
        fprintf(stderr, "unhandled XFL: 0x%02x\n", file->header.xfl);
        exit(1);
    }

    // deal with flags (only fname right now)
    if(file->header.flg & FNAME) { // read name
        i = 0;
        while(i < MAX_FILE_NAME - 1) {
            fread(file->filename + i, 1, 1, stream->fp);
            if(file->filename[i++] == '\0') break;
        }
        if(file->filename[i-1] != '\0') {
            fprintf(stderr, "Filename too long!\n");
            exit(1);
        }
    }
}

void print_header(struct FullFile *file) {
    struct tm mtime;
    time_t mtime_s;
    char buf[80];
    
    // OS type
    if(file->header.os == 3) printf("Compression OS: Unix\n");
    else printf("Compression OS: Not Unix\n");

    // file name
    if(file->header.flg & FNAME) {
        printf("Original File Name: %s\n", file->filename);
    }

    // time
    mtime_s = *(time_t*)(file->header.mtime);
    mtime_s &= (0xffffffff); // clear out top 4 bits
    mtime = *localtime(&mtime_s);
    strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &mtime);
    printf("Original File mtime: %s\n", buf);

    // extra flags
    if(file->header.xfl == 2) printf("XFL=2: Max compression, slowest algorithm\n");
    else if(file->header.xfl == 4) printf("XFL=4: Fastest compression algorithm\n");
    else printf("XFL=0: No extra information\n");
}

struct huffman_node* traverse_tree(struct huffman_node *root, unsigned int code, int len, int create) {
    unsigned int bit, mask = (1<<(len-1));
    while(mask != 0) {
        bit = (code & mask)?1:0;
        mask >>= 1;
        if(root->children[bit] == NULL) {
            if(create) {
                if((root->children[bit] = calloc(1, sizeof(struct huffman_node))) == NULL) {
                    perror("calloc failed in traverse_tree");
                    exit(1);
                }
                root->children[bit]->val = -1;
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
    root->val = -1;
    for(i = 0; i < lengths[lengths_size-1].end+1; ++i) {
        curnode = traverse_tree(root, tree[i].code, tree[i].len, 1);
        curnode->val = i;
    }
}

void decode_block(struct huffman_node *literal_root, struct huffman_node *dist_root, struct deflate_stream *stream, char *orig_filename) {
    static char buf[MAX_BACK_DIST]; // buffer for backwards distances; remains across blocks
    
    int extra, length, dist, bit, pos, backpos;
    struct huffman_node *node;
    char filename[MAX_FILE_NAME];
    FILE *out;
    
    printf("decode_block started\n");
    snprintf(filename, MAX_FILE_NAME, "%s", orig_filename);
    if((out = fopen(filename, "w")) == NULL) {
        perror("Error occurred while opening output file.\n");
        exit(1);
    }
    printf("file opened\n");
    // MODIFY MTIME
    pos = 0;

    while(1) {
        node = literal_root;
        while(node->val == -1) { // not a leaf node
            bit = read_bit(stream);
            printf("%d", bit);
            if(node->children[bit] == NULL) {
                fprintf(stderr, "Unknown Huffman code encountered.\n");
                exit(1);
            }
            node = node->children[bit];
        }
        printf("; val: %d\n", node->val);
        if(node->val == END_OF_BLOCK) {
            break;
        } else if(node->val < LITERAL_EXT_BASE) {
            buf[pos] = (char)node->val;
            fprintf(out, "%c", buf[pos]);
            pos = (pos + 1)%MAX_BACK_DIST;
            continue;
        } else {
            extra = read_bits(stream, LITERAL_EXTRA_BITS(node->val));
            length = extra_alpha_start[node->val - LITERAL_EXT_BASE] + extra;
        }

        if(dist_root == NULL) {
            dist = read_bits(stream, FIXED_DIST_BITS);
        }
        else {

        }

        // copy dist bits from backpos to pos
        backpos = pos - dist;
        if(backpos < 0) backpos += MAX_BACK_DIST;
        while(length-->0) {
            buf[pos] = buf[backpos];
            fprintf(out, "%c", buf[pos]);
            pos = (pos + 1)%MAX_BACK_DIST;
            backpos = (backpos + 1)%MAX_BACK_DIST;
        }
    }

    if(fclose(out) != 0) {
        perror("Error occurred when closing output file.\n");
        exit(1);
    }
}

void inflate(struct deflate_stream *stream, char *orig_filename) {
    int bfinal, btype;
    int len, nlen; // case 0
    char buf[NONCOMPRESSIBLE_BLOCK_SIZE];
    struct huffman_node root; // cases 1, 2
    
    do {
        bfinal = read_bits(stream, 1);
        btype = read_bits(stream, 2);
        printf("\nbfinal: %d, btype: %d\n", bfinal, btype);
        if(btype == 0) { // uncompressed
            while(stream->pos != 0) read_bits(stream, 1); // ignore remainder of block
            len = read_bits(stream, 2);
            nlen = read_bits(stream, 2);
            if((len & nlen) != 0) { // sanity check
                fprintf(stderr, "len, nlen are not complements\n");
                exit(1);
            }
            fread(buf, 1, len, stream->fp);
            // TODO: Copy to output
        } else if(btype == 1) { // compressed with fixed Huffman
            build_tree(&root, fixed_huffman, 4);
            // struct huffman_node *temp = traverse_tree(&root, 0b10011000, 8, 0);
            // printf("10011000: %d\n", temp->val);
            decode_block(&root, NULL, stream, orig_filename);


        } else if(btype == 2) { // compressed with dynamic Huffman
            // decode(stream);
        } else {
            fprintf(stderr, "Invalid block type: %d", btype);
            exit(1);
        }
    } while(bfinal != 1);
}

int main(int argc, char *argv[]) {
    struct deflate_stream stream;
    struct FullFile file;

    memset(&stream, 0, sizeof(stream));
    memset(&file, 0, sizeof(file));
    
    // Open file
    if(argc != 2) { // check number of arguments
        fprintf(stderr, "Usage: ryunzip <file>\n");
        return 1;
    }

    if((stream.fp=fopen(argv[1], "rb")) == NULL) {
        perror("Invalid file; can't open.\n");
        return 1;
    }

    read_header(&stream, &file);
    print_header(&file);
    inflate(&stream, file.filename);

    if(fclose(stream.fp) != 0) {
        perror("Error occurred while closing file.\n");
        return 1;
    }
    return 0;
}