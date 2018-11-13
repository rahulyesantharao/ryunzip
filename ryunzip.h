/*
 Standard definitions for gzip (from RFCs 1951/1952).
 */

#define MAX_HUFFMAN_LENGTH 15 // because the code length spec only gives literals from 0-15
#define MAX_FILE_NAME 100 // arbitrarily set
#define MAX_BACK_DIST (1<<15)
#define NONCOMPRESSIBLE_BLOCK_SIZE (1<<16) // max size for uncompressed block

#define HLIT_LEN 5
#define HLIT_OFFSET 257
#define HDIST_LEN 5
#define HDIST_OFFSET 1
#define HCLEN_LEN 4
#define HCLEN_OFFSET 4

// Structs to handle formatting
struct deflate_stream {
    FILE *fp;
    unsigned char buf;
    unsigned char pos;
};

// Gzip File Format
// https://www.forensicswiki.org/wiki/Gzip

// Flags (FLG)
#define FTEXT (0x01) // only optionally set
#define FHCRC (0x02)
#define FEXTRA (0x04)
#define FNAME (0x08) // the only one we will be handling
#define FCOMMENT (0x10)

struct Header {
    unsigned char id1, id2;
    unsigned char cm, flg;
    unsigned char mtime[4]; // time_t
    unsigned char xfl, os;
};

struct Footer {
    unsigned char checksum[4];
    int filesize;
};

struct FullFile {
    struct Header header;
    char *fextra;
    char filename[MAX_FILE_NAME];
    char *fcomment;
    unsigned char crc16[2]; 
    struct Footer footer;
};

struct Tree {
    unsigned int len;
    unsigned int code;
}; 

struct huffman_length {
    unsigned int end;
    unsigned int len;
};

struct huffman_node {
    int val;
    struct huffman_node *children[2];
};

// Fixed structures
struct huffman_length fixed_huffman[4] = { // defined in RFC 1952 (Section 3.2.6)
    {143, 8},
    {255, 9},
    {279, 7},
    {287, 8}
};

#define END_OF_BLOCK 256
#define LITERAL_EXT_BASE 257
#define LITERAL_EXTRA_BITS(x) ((x<265)?(0):((x-261)/4))
#define LITERAL_MAX 285
int extra_alpha_start[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 
    13, 15, 17, 19, 
    23, 27, 31, 35, 
    43, 51, 59, 67,
    83, 99, 115, 131,
    163, 195, 227, 258
};

#define FIXED_DIST_BITS 5
#define DIST_EXTRA_BITS(x) ((x<4)?(0):((x-2)/2))
#define DIST_MAX 29 
int extra_dist_start[30] = {
    1, 2, 3, 4, 5, 
    7, 9, 13, 17,
    25, 33, 49, 65,
    97, 129, 193, 257,
    385, 513, 769, 1025,
    1537, 2049, 3073, 4097,
    6145, 8193, 12289, 16385,
    24577
};

#define CODE_LENGTH_EXT_BASE 16
int code_length_order[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};
int code_length_extra_bits[3] = {2, 3, 7};
int code_length_extra_offsets[3] = {3, 3, 11};


// Functions
int read_bit(struct deflate_stream *stream);
int read_bits(struct deflate_stream *stream, int n, int huffman);

void read_header(struct deflate_stream *stream, struct FullFile *file);
void print_header(struct FullFile *file);

void print_huffman_tree(struct huffman_node *root, unsigned int cur, int len);
struct huffman_node* traverse_tree(struct huffman_node *root, unsigned int code, int len, int create);
void build_tree(struct huffman_node *root, struct huffman_length lengths[], int lengths_size);

void decode_block(struct huffman_node *literal_root, struct huffman_node *dist_root, struct deflate_stream *stream, FILE *out);
void decode_code_lengths(struct deflate_stream *stream, struct huffman_node *code_length_root, int *all_lens, int num);
void read_huffman_codes(struct deflate_stream *stream, struct huffman_node *literal_root, struct huffman_node *dist_root);

void inflate(struct deflate_stream *stream, char *orig_filename);