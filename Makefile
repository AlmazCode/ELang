# ELang Compiler
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -D_GNU_SOURCE
SRCS = src/token.c src/lexer.c src/ast.c src/parser.c src/semantics.c src/codegen.c src/main.c
OBJS = $(SRCS:.c=.o)
BIN = bin/elc

all: $(BIN)

$(BIN): $(OBJS) | bin
	$(CC) -o $@ $^

src/%.o: src/%.c | src
	$(CC) $(CFLAGS) -c -o $@ $<

bin src:
	mkdir -p $@

clean:
	rm -rf src/*.o bin

test: $(BIN)
	./bin/elc -t test/hello.el
	./bin/elc -a test/hello.el
	./bin/elc -o test/hello.asm test/hello.el
	./bin/elc -o test/arrays.asm test/arrays.el
	nasm -f elf64 test/hello.asm -o test/hello.o
	nasm -f elf64 test/arrays.asm -o test/arrays.o
	nasm -f elf64 lib/core.asm -o lib/build/core.o
	ld test/hello.o lib/build/core.o -o test/hello
	ld test/arrays.o lib/build/core.o -o test/arrays
	./test/hello
	./test/arrays

run: test
	./test/hello
	./test/arrays

lib:
	$(MAKE) -C lib

.PHONY: all clean test run lib run-file

run-file: all
	./elc $(FILE)
