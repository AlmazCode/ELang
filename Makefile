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

run: test
	nasm -f elf64 test/hello.asm -o test/hello.o
	ld test/hello.o lib/build/syscalls.o -o test/hello
	./test/hello

lib:
	$(MAKE) -C lib

.PHONY: all clean test run lib install run-file

run-file: all
	./elc $(FILE)

install: all
	cp elc /usr/local/bin/elc 2>/dev/null || sudo cp elc /usr/local/bin/elc
	@echo "Installed: /usr/local/bin/elc"
