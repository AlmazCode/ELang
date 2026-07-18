# ELang Standard Library
ASM = nasm
ASMFLAGS = -f elf64

all: build/core.o build/std.o build/math.o

build/core.o: core.asm | build
	$(ASM) $(ASMFLAGS) $< -o $@

build/std.o: std.asm | build
	$(ASM) $(ASMFLAGS) $< -o $@

build/math.o: math.asm | build
	$(ASM) $(ASMFLAGS) $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build

.PHONY: all clean
