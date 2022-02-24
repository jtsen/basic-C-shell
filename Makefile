CC=gcc
CFLAGS=-g -pedantic -std=gnu17 -Wall -Wextra

.PHONY: all
all: shell

shell: shell.o

shell.o: shell.c shell.h

.PHONY: clean
clean:
	rm -f *.o 