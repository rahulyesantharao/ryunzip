/*
 Standard definitions for gzip (from RFCs 1951/1952).
 */

#define MAX_BITS 10 // not sure about this
#define MAX_FILE_NAME 100 // arbitrarily set

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
    int len, code;
}; 

// Fixed structures
int fixed_huffman[(1<<9)];
int alphabet[30];

// Functions
void init();

int read_bit(struct deflate_stream *stream);
int read_bits(struct deflate_stream *stream, int n);
int write_data();

void read_header();
void print_header();

void deflate();