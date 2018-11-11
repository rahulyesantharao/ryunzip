CC=gcc
CFLAGS=-I.
DEPS = ryunzip.h
OBJ = ryunzip.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

ryunzip: $(OBJ) 
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o ryunzip