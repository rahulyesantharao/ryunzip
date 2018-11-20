CC=gcc
CFLAGS=-I.
DEPS = ryunzip.h
OBJ = ryunzip.o
SHELL = /bin/sh

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

ryunzip: $(OBJ) 
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean test reset-test

clean:
	rm -f *.o ryunzip

test:
	./runtests.sh

reset-test:
	mv tests/passed/* tests/
