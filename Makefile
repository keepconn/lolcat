CC=clang
CFLAGS=-O2 -g -Wall -Wextra -lm

.PHONY:
all: bin/lolcat

bin/lolcat: src/lolcat.c
	mkdir -p bin/
	$(CC) $(CFLAGS) -o $@ $^

.PHONY:
clean:
	rm -rf bin/
