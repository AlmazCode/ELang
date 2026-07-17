# ELang Standard Library
ASM = nasm
ASMFLAGS = -f elf64

all: build/syscalls.o

build/syscalls.o: syscalls.asm | build
	$(ASM) $(ASMFLAGS) $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build

.PHONY: all clean
