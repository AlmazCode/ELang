# ELang

A fast, statically-typed, compiled systems programming language with expression-oriented syntax and pipeline-first design.

ELang compiles directly to x86_64 assembly, producing native Linux binaries with zero runtime overhead. It combines the performance of C with modern language features like pattern matching, pipeline operators, and a module system.

## Key Features

- **Statically typed** with type inference — catch errors at compile time
- **Expression-oriented** — `if`, `match`, `when` all return values
- **Pipeline operator `|>`** — chain function calls naturally
- **Module system** — `using "math"`, `math::add()`
- **Pattern matching** — `match` with `Ok`/`Err` for error handling
- **`defer`** — automatic resource cleanup on scope exit
- **Constant folding** — expressions evaluated at compile time
- **Zero runtime** — no garbage collector, no VM, no hidden allocations

## Quick Start

```bash
# Build the compiler
make

# Compile and run in one step
./elc examples/fizzbuzz.el

# Or step by step
./bin/elc -o output.asm examples/fizzbuzz.el
nasm -f elf64 output.asm -o output.o
ld output.o lib/build/syscalls.o -o output
./output
```

## Language Overview

### Functions with Implicit Return

```elang
fn add(a: i64, b: i64) -> i64 {
    a + b  // last expression is automatically returned
}

fn abs(x: i64) -> i64 =>
    if x < 0 { 0 - x } else { x }
```

### Pipeline Operator

```elang
// Data flows through function chains
5 |> add(10) |> print_int()

// Chained transformations
input |> parse() |> validate() |> save()
```

### Pattern Matching

```elang
fn divide(a: i64, b: i64) -> i64 {
    if b == 0 { return Err(0) }
    return Ok(a / b)
}

let result = match divide(10, 2) {
    Ok(val) => val
    Err(_) => 0
}
```

### When Expressions

```elang
let status = when temp > 30 {
    "hot"
} else when temp > 20 {
    "warm"
} else {
    "cold"
}
```

### Module System

```elang
// math.el
export fn add(a: i64, b: i64) -> i64 { a + b }

// main.el
using "math"

fn main() -> void {
    let x = math::add(1, 2)
}
```

### Defer for Cleanup

```elang
fn read_file(path: string) -> string {
    let fd = sys_open(path, 0, 0)
    defer sys_close(fd)

    let buf = alloc(4096)
    defer free(buf)

    sys_read(fd, buf, 4096)
    str_dup(buf)
}
```

## Type System

ELang supports:
- Integers: `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`
- Floats: `f32`, `f64`
- Booleans: `bool`
- Characters: `char`
- Strings: `string`
- Pointers: `*i64`
- Arrays: `[i64; 10]`

Type checking is performed at compile time:
```bash
./bin/elc -check my_program.el
```

## Compiler Options

```
elc [options] <file.el>
  -o <file>    Output assembly file (default: output.asm)
  -check       Type check only (no codegen)
  -t           Show tokens
  -a           Show AST
  -l           Library mode (no _start)
  -h           Show help
```

## Project Structure

```
ELang/
├── src/                  # Compiler source (C)
│   ├── lexer.c          # Tokenizer
│   ├── parser.c         # Syntax parser
│   ├── semantics.c      # Type checker & inference
│   ├── codegen.c        # x86_64 assembly generation
│   ├── ast.c            # AST node management
│   ├── token.c          # Token definitions
│   └── main.c           # CLI & compilation pipeline
├── include/              # Header files
├── lib/                  # Standard library (x86_64 assembly)
│   └── syscalls.asm     # Linux syscall wrappers
├── examples/             # Example programs
│   ├── fizzbuzz.el      # FizzBuzz with all features
│   └── math.el          # Math module
├── test/                 # Test programs
├── DESIGN.md             # Language design document
├── elc                   # Compiler wrapper script
└── Makefile
```

## Examples

See the [`examples/`](examples/) directory for complete programs demonstrating all language features.

### FizzBuzz

```elang
fn fizzbuzz(n: i64) -> void {
    let i = 1
    while i <= n {
        if i % 15 == 0 { print_str("FizzBuzz") }
        else if i % 3 == 0 { print_str("Fizz") }
        else if i % 5 == 0 { print_str("Buzz") }
        else { print_int(i) }
        print_str(" ")
        i = i + 1
    }
    print_str("\n")
}
```

## Building from Source

### Requirements
- GCC (or Clang)
- NASM (Netwide Assembler)
- Linux x86_64

### Build
```bash
make          # Build compiler
make lib      # Build standard library
make run      # Build and run test
```

## Roadmap

- [ ] Generics
- [ ] Closures / lambdas
- [ ] Arrays and slices
- [ ] Struct methods
- [ ] Enum variants with data
- [ ] Traits / interfaces
- [ ] Cross-compilation
- [ ] Optimizer passes

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
