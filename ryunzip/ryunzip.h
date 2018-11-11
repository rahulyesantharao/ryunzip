/*
 Standard definitions for gzip (from RFCs 1951/1952).
 */

#define MAX_HUFFMAN_LENGTH 11 // TODO: FIGURE THIS OUT
#define MAX_FILE_NAME 100 // arbitrarily set
#define NONCOMPRESSIBLE_BLOCK_SIZE (1<<16) // max size for uncompressed block

// Flags (FLG)
#define FTEXT (0x01) // only optionally set
#define FHCRC (0x02)
#define FEXTRA (0x04)
#define FNAME (0x08) // the only one we will be handling
#define FCOMMENT (0x10)

// Structs to handle formatting
struct deflate_stream {
    FILE *fp;
    unsigned char buf;
    unsigned char pos;
};

struct Header {
    unsigned char id1, id2;
    unsigned char cm, flg;
    time_t mtime;
    unsigned char xfl, os;
    char filename[MAX_FILE_NAME];
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
    unsigned int val;
    struct huffman_node *children[2];
};

// Fixed structures
struct huffman_length fixed_huffman[4] = { // defined in RFC 1952 (Section 3.2.6)
    {143, 8},
    {255, 9},
    {279, 7},
    {287, 8}
};

int alphabet[30];

// Functions
int read_bit(struct deflate_stream *stream);
int read_bits(struct deflate_stream *stream, int n);

void read_header(struct deflate_stream *file, struct Header *header);
void print_header(struct Header *header);

struct huffman_node* traverse_tree(struct huffman_node *root, unsigned int code, int create);
void build_tree(struct huffman_node *root, struct huffman_length lengths[], int lengths_size);

void deflate(struct deflate_stream *file);