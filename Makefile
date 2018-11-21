CC=gcc
CFLAGS=-I.
DEPS = ryunzip.h
OBJ = ryunzip.o
SHELL = /bin/sh

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

ryunzip: $(OBJ) 
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean test test-% vtest-% reset-test

clean:
	rm -f *.o ryunzip

test:
	scripts/runtests.sh

test-%:
	scripts/testfile.sh $* || true

vtest-%:
	scripts/testfile.sh -v $* || true

reset-test:
	mv tests/passed/* tests/
