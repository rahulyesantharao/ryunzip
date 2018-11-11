/*
  A basic implementation of an unzip utility that conforms to the DEFLATE specifications (RFC 1951, 1952 for format)
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#include "ryunzip.h"

void print_huffman_tree(struct huffman_node *root, unsigned int cur, int len) {
    int i;
    if(root->val != -1) {
        for(i = len-1; i>=0; --i) {
            printf("%d", (cur&(1<<i))?1:0);
        }
        printf(": %d\n", root->val);
    } else {
        if(root->children[0] != NULL) print_huffman_tree(root->children[0], 2*cur, len+1);
        if(root->children[1] != NULL) print_huffman_tree(root->children[1], 2*cur+1, len+1);
    }
}

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
    // bit order is LSB->MSB
    for(i=0; i<n; ++i) {
        ret |= (read_bit(stream) << i);
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
    bl_count[0] = 0;

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

void decode_block(struct huffman_node *literal_root, struct huffman_node *dist_root, struct deflate_stream *stream, FILE *out) {
    static char buf[MAX_BACK_DIST]; // buffer for backwards distances; remains across blocks
    
    int extra, length, dist, bit, pos, backpos;
    struct huffman_node *node;
    
    printf("decode_block started\n");
    pos = 0;

    while(1) {
        node = literal_root;
        while(node->val == -1) { // not a leaf node
            bit = read_bit(stream);
            printf("%d", bit);
            if(node->children[bit] == NULL) {
                fprintf(stderr, "Unknown Huffman code for literal encountered.\n");
                exit(1);
            }
            node = node->children[bit];
        }
        printf("; val: %d\n", node->val);
        if(node->val == END_OF_BLOCK) {
            break;
        } else if(node->val < LITERAL_EXT_BASE) {
            buf[pos] = (char)node->val;
            fwrite(buf + pos, 1, 1, out);
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
           node = dist_root;
            while(node->val == -1) { // not a leaf node
                bit = read_bit(stream);
                printf("%d", bit);
                if(node->children[bit] == NULL) {
                    fprintf(stderr, "Unknown Huffman code for dist encountered.\n");
                    exit(1);
                }
                node = node->children[bit];
            }
            printf("; val: %d\n", node->val);
            extra = read_bits(stream, DIST_EXTRA_BITS(node->val));
            dist = extra_dist_start[node->val] + extra;
        }

        // copy dist bits from backpos to pos
        backpos = pos - dist;
        if(backpos < 0) backpos += MAX_BACK_DIST;
        while(length-->0) {
            buf[pos] = buf[backpos];
            fwrite(buf + pos, 1, 1, out);
            pos = (pos + 1)%MAX_BACK_DIST;
            backpos = (backpos + 1)%MAX_BACK_DIST;
        }
    }

    if(fclose(out) != 0) {
        perror("Error occurred when closing output file.\n");
        exit(1);
    }
}

void decode_code_lengths(struct deflate_stream *stream, struct huffman_node *code_length_root, int *all_lens, int num) {
    int bit, len, i, rep_val;
    struct huffman_node *node;

    printf("decode_code_lengths:\n");

    for(i = 0; i < num;) {
        node = code_length_root;
        while(node->val == -1) { // not a leaf node
            bit = read_bit(stream);
            printf("%d", bit);
            if(node->children[bit] == NULL) {
                fprintf(stderr, "Unknown Huffman code encountered while decoding literal huffman tree.\n");
                exit(1);
            }
            node = node->children[bit];
        }
        printf(": %d", node->val);
        if(node->val < CODE_LENGTH_EXT_BASE) {
            printf("\n");
            all_lens[i++] = node->val;
        } else {
            len = read_bits(stream, code_length_extra_bits[node->val - CODE_LENGTH_EXT_BASE]) + code_length_extra_offsets[node->val - CODE_LENGTH_EXT_BASE];
            printf("; rep=%d\n", len);
            rep_val = (node->val > CODE_LENGTH_EXT_BASE)?0:all_lens[i-1];
            while(len-->0) {
                all_lens[i++] = rep_val;
            }
        }
    }
    printf("decoded %d lengths\n", i);
}

void read_huffman_codes(struct deflate_stream *stream, struct huffman_node *literal_root, struct huffman_node *dist_root) {
    int hlit, hdist, hclen, i, j;
    int all[LITERAL_MAX + DIST_MAX + 1];
    struct huffman_length code_lengths[19], temp_lengths[LITERAL_MAX+DIST_MAX+1];
    struct huffman_node code_length_root, *node;

    memset(&code_lengths, 0, sizeof(code_lengths));
    memset(&temp_lengths, 0, sizeof(code_lengths));
    memset(&code_length_root, 0, sizeof(code_length_root));

    for(i=0; i<19; i++) code_lengths[i].end = i;
    hlit = read_bits(stream, HLIT_LEN);
    hdist = read_bits(stream, HDIST_LEN);
    hclen = read_bits(stream, HCLEN_LEN);

    // Build code lengths huffman tree
    for(i = 0; i < hclen + HCLEN_OFFSET; ++i) { // read code length huffman tree
        code_lengths[code_length_order[i]].len = read_bits(stream, 3);
    }
    build_tree(&code_length_root, code_lengths, 19);
    
    // Read in all codes
    decode_code_lengths(stream, &code_length_root, all, (hlit + hdist + HLIT_OFFSET + HDIST_OFFSET));

    // Build literal huffman tree
    printf("Lit Size: %d\n", hlit + HLIT_OFFSET);
    j = -1;
    for(i = 0; i<(hlit+HLIT_OFFSET); ++i) {
        if(i>0 && all[i] == all[i-1]) {
            ++temp_lengths[j].end;
        } else {
            ++j;
            temp_lengths[j].len = all[i];
            temp_lengths[j].end = (j>0)?(temp_lengths[j-1].end + 1):0;
        }
    }
    build_tree(literal_root, temp_lengths, j+1);

    // Build dynamic huffman tree
    printf("Dist Size: %d\n", hdist + HDIST_OFFSET);
    j = -1;
    for(; i<(hdist+hlit+HLIT_OFFSET+HDIST_OFFSET); ++i) {
        if(i>(hlit+HLIT_OFFSET) && all[i] == all[i-1]) {
            ++temp_lengths[j].end;
        } else {
            ++j;
            temp_lengths[j].len = all[i];
            temp_lengths[j].end = (j>0)?(temp_lengths[j-1].end + 1):0;
        }
    }
    build_tree(dist_root, temp_lengths, j+1);
}

void inflate(struct deflate_stream *stream, char *orig_filename) {
    int bfinal, btype;
    int len, nlen; // case 0
    char buf[NONCOMPRESSIBLE_BLOCK_SIZE];
    struct huffman_node literal_root, dist_root; // cases 1, 2
    char filename[MAX_FILE_NAME];
    FILE *out;

    snprintf(filename, MAX_FILE_NAME, "%s", orig_filename);
    if((out = fopen(filename, "wb")) == NULL) {
        perror("Error occurred while opening output file.\n");
        exit(1);
    }
    memset(&literal_root, 0, sizeof(literal_root));
    memset(&dist_root, 0, sizeof(dist_root));
    
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
            fwrite(buf, 1, len, out);        
        } else if(btype == 1) { // compressed with fixed Huffman
            build_tree(&literal_root, fixed_huffman, 4);
            // struct huffman_node *temp = traverse_tree(&root, 0b10011000, 8, 0);
            // printf("10011000: %d\n", temp->val);
            decode_block(&literal_root, NULL, stream, out);
        } else if(btype == 2) { // compressed with dynamic Huffman
            read_huffman_codes(stream, &literal_root, &dist_root);
            print_huffman_tree(&dist_root, 0, 0);
            decode_block(&literal_root, &dist_root, stream, out);
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