# ryunzip
An implementation of a DEFLATE-compliant decompressor, based on RFCs 1951 (for DEFLATE) and 1952 (for the gzip file format).

## Building
Simply clone the repo and run `make` in the directory to build the unzip utility. It can be used with the same basic invocation as `gunzip`; `ryunzip <file>`.

## Testing
To test, add the test files you wish to run to `tests/` as `testname.txt`. Then, run `./testfile.sh <testname>` to test just that file, or run `./runtests.sh` to test all of the files in `tests/`.

## Limitations
It has only been tested on ASCII text files at the moment, and is untested as of yet on blocks with `BTYPE=00`. Nevertheless, it demonstrates all the key concepts of the DEFLATE algorithm (which is not used in `BTYPE 00`).

## Writeup
See a full description here: https://rahulyesantharao.com/blog/posts/compression-a-deep-dive-into-gzip.
