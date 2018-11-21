# ryunzip
An implementation of a DEFLATE-compliant decompressor, based on RFCs 1951 (for DEFLATE) and 1952 (for the gzip file format).

## Building
Simply clone the repo and run `make` in the directory to build the unzip utility. 

## Using
Command Format: `./ryunzip [-v] <file>`.
The `-v` flag indicates verbosity; the command will print out the details of the operation as it unzips the file.

## Testing
The testing framework tests the files in the `tests/` folder and moves them to `tests/passed/` if they pass. To add tests, add the text files you wish to test to `tests/` as `<name>.txt`.

To test all the currently unpassed tests, use `make test`.

To test a single text file (`<name>.txt`), use `make test-<name>` or `make vtest-<name>` (to see the verbose output of the `ryunzip` program).

Use `make reset-test` to reset all of the tests (move them out from `tests/passed` back to `tests/`).

## Limitations
It has only been tested on ASCII text files at the moment, and is untested as of yet on blocks with `BTYPE=00`. Nevertheless, it demonstrates all the key concepts of the DEFLATE algorithm (which is not used in `BTYPE 00`).

## Writeup
See a full description here: https://rahulyesantharao.com/blog/posts/compression-a-deep-dive-into-gzip.

## References
 - https://www.ietf.org/rfc/rfc1951.txt
 - https://www.ietf.org/rfc/rfc1952.txt
 - https://www.forensicswiki.org/wiki/Gzip
 - http://www.onicos.com/staff/iz/formats/gzip.html
