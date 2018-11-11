# ryunzip
An implementation of a DEFLATE-compliant decompressor, based on RFCs 1951 (for DEFLATE) and 1952 (for the gzip file format).

## Building
Simply clone the repo and run `make` in the directory to build the unzip utility. It can be used with the same basic invocation as `gunzip`; `ryunzip <file>`.

## Limitations
It has only been tested on ASCII text files at the moment, and is untested as of yet on blocks with `BTYPE=00`. Nevertheless, it demonstrates all the key concepts of the DEFLATE algorithm.

## Writeup
See a full description here: (coming soon).
