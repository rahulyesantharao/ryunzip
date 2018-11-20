# ryunzip
An implementation of a DEFLATE-compliant decompressor, based on RFCs 1951 (for DEFLATE) and 1952 (for the gzip file format).

## Building
Simply clone the repo and run `make` in the directory to build the unzip utility. 

## Using
Command Format: `./ryunzip [-v] <file>`.
The `-v` flag indicates verbosity; the command will print out the details of the operation as it unzips the file.

## Testing
The testing framework tests the files in the `tests/` folder and moves them to `tests/passed/` if they pass. To add tests, add the test files you wish to run to `tests/` as `testname.txt`.

To test, run `./testfile.sh <testname>` to test a single file, or run `./runtests.sh` to test all of the files in `tests/`.

You can also use `make test` to run all the tests, and `make reset-test` to reset all of the tests (move them out from `tests/passed` back to `tests/`).

## Limitations
It has only been tested on ASCII text files at the moment, and is untested as of yet on blocks with `BTYPE=00`. Nevertheless, it demonstrates all the key concepts of the DEFLATE algorithm (which is not used in `BTYPE 00`).

## Writeup
See a full description here: https://rahulyesantharao.com/blog/posts/compression-a-deep-dive-into-gzip.
