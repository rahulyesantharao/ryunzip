/*
  A basic implementation of an unzip utility that conforms to the DEFLATE specifications (RFC 1951, 1952 for format)
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "ryunzip.h"

void init() {
    int i, n = 0;
    
    // Initialize the static Huffman table (RFC 1952)
    for(i = 0b00110000; i<0b10111111; ++i) fixed_huffman[i] = n++;
    for(i = 0b110010000; i<0b111111111; ++i) fixed_huffman[i] = n++;
    for(i = 0b0000000; i<0b0010111; ++i) fixed_huffman[i] = n++;
    for(i = 0b11000000; i<0b11000111; ++i) fixed_huffman[i] = n++;
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

int read_bit(struct deflate_stream *stream) {
    if(!stream->pos) { // read in new byte
        stream->pos = 0x01;
        if(fread(&stream->buf, sizeof(unsigned char), 1, stream->fp) < 1) {
            fprintf(stderr, "Error reading data\n");
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

// void convert(struct Tree tree[]) {
//     int n, bits, max_code, code;
//     int next_code[MAX_BITS+1], bl_count[MAX_BITS + 1];

//     // Build bl_count
//     memset(bl_count, 0, sizeof(bl_count));
//     for(n=0; n<=???; ++n) {
//         bl_count[tree[n].len]++;
//     }

//     // Compute next_code
//     code = 0;
//     for(bits=1; bits <= MAX_BITS; ++bits) {
//         code = (code + bl_count[bits-1]) << 1;
//         next_code[bits] = code;
//     }

//     // Set codes    
//     // what is max_code ???
//     for(n=0; n<=max_code; n++) {
//         if(tree[n].len != 0) {
//             tree[n].code = next_code[tree[n].len];
//             next_code[tree[n].len]++;
//         }
//     }
// }

void deflate(struct deflate_stream *file) {
    int bfinal, btype;

    do {
        bfinal = read_bits(file, 1);
        btype = read_bits(file, 2);
        printf("bfinal: %d, btype: %d\n", bfinal, btype);
        if(btype == 0) {

        } else if(btype == 1) {

        } else if(btype == 2) {

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
        fprintf(stderr, "Invalid file; can't open.\n");
        return 1;
    }

    init();
    read_header(&file, &header);
    print_header(&header);
    deflate(&file);

    if(fclose(file.fp) != 0) {
        fprintf(stderr, "Error occurred while closing file.\n");
        return 1;
    }
    return 0;
}