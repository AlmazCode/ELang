# ELang v0.43.0 Syntax Design

## Identity: "Systems programming that reads like data flow"

ELang is not "C with features." It's a language where code reads as a sequence of
transformations. Data flows through pipes. Results propagate automatically.
The compiler does the boring work so you don't have to.

Автор: **AlmazCode**

Four pillars:
1. **Expression-oriented** — everything returns a value
2. **Pipeline-first** — data flows through `|>` chains
3. **Explicit typing** — all types are declared explicitly
4. **Struct = Class** — OOP through structs with methods

---

## Feature 1: Implicit Return

The last expression in a function body is automatically returned.

```elang
// Before
fn add(a: i64, b: i64) -> i64 {
    return a + b
}

// After
fn add(a: i64, b: i64) -> i64 {
    a + b
}
```

```elang
fn factorial(n: i64) -> i64 {
    if n <= 1 { 1 } else { n * factorial(n - 1) }
}
```

Rule: if the last statement is an expression (not `return`, `let`, `if` without value),
its value becomes the function's return value. Explicit `return` still works for early exit.

---

## Feature 2: Pipeline Operator `|>`

Pass the left value as the FIRST argument of the right function.

```elang
// Before
print_int(add(x, y))

// After
x |> add(y) |> print_int()
```

```elang
// Before
let trimmed = str_trim(str_lower(read_line()))

// After
let trimmed = read_line() |> str_lower() |> str_trim()
```

Multiple pipes chain naturally:
```elang
input
    |> parse()
    |> validate()
    |> transform()
    |> save()
```

Pipeline binds tighter than assignment:
```elang
let result = data |> filter(valid) |> map(transform) |> collect()
```

---

## Feature 3: Tuple Unpacking

Multiple return values and destructuring:

```elang
// Functions can return tuples
fn divmod(a: i64, b: i64) -> (i64, i64) {
    (a / b, a % b)
}

// Unpack with let
let (quotient, remainder) = divmod(17, 5)

// Swap without temp variable
let (a, b) = (b, a)
```

---

## Feature 4: Expression-oriented `if`

`if` is an expression that returns a value. `else if` chains are supported.

```elang
// if returns a value
let status = if x > 0 {
    "positive"
} else if x == 0 {
    "zero"
} else {
    "negative"
}

// if as a statement
if x > 0 {
    print_str("positive\n")
}
```

---

## Feature 5: `defer`

Runs when leaving the current scope. Cleans up resources automatically.

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

`defer` runs in reverse order (like Go/Rust). Multiple defers stack.

---

## Feature 6: Optional Braces

Single-expression bodies don't need `{}`.

```elang
fn double(x: i64) -> i64 => x * 2

fn abs(x: i64) -> i64 =>
    if x < 0 { -x } else { x }

// But only for single expressions:
for i in 0..10 => print_int(i)
```

Syntax: `=>` after params/keywords means "single expression body."

---

## Feature 7: `using` — Clean Imports

```elang
using "math"
using "io" as io

// Then call directly
let r = sqrt(2.0)
io::print("hello")
```

---

## Feature 8: Arrays

Heap-allocated arrays with literal syntax, indexing, bounds checking, and pipeline operations.

```elang
fn main() -> void {
    // Array literal — heap-allocated
    let arr = [10, 20, 30, 40, 50]

    // Index access (0-based, bounds-checked)
    let first = arr[0]   // 10
    let third = arr[2]   // 30

    // Length property
    let len = arr.len    // 5

    // Arrays in loops
    let sum = 0
    let i = 0
    while i < arr.len {
        sum = sum + arr[i]
        i = i + 1
    }

    // for-in (iterate elements)
    for x in arr {
        print_int(x)
    }

    // for-in with index (enumerate)
    for i, x in arr {
        print_int(i)
        print_str(": ")
        print_int(x)
    }

    // Array with expressions
    let x = 5
    let arr2 = [x, x + 1, x * 2]  // [5, 6, 10]
}
```

### Storage Model

Arrays are **heap-allocated** via bump allocator (`brk` syscall). The layout in memory:

```
[ptr - 24] = refcount (i64)     // reference counting
[ptr - 16] = capacity (i64)     // allocated slots
[ptr - 8]  = length (i64)       // current element count
[ptr + 0]  = element[0]
[ptr + 8]  = element[1]
...
[ptr + N*8] = element[N-1]
```

The array variable holds a pointer to `element[0]` (i.e., `ptr`). `.len` reads from `[ptr-8]`.

### Pipeline Operations

```elang
arr |> map(fn x => x * 2)              // map with closure
arr |> map(double)                      // map with named function
arr |> filter(fn x => x > 10)          // filter
arr |> reduce(0, fn acc, x => acc + x) // reduce
arr |> map(fn x => x * 2) |> filter(fn x => x > 30)  // chains
```

### Calling Convention

All user-defined functions use **closure convention**: `rdi=env_ptr`, `rsi=arg0`, `rdx=arg1`, ... This allows transparent passing of functions to `map`/`filter`/`reduce` without wrapping.

### Limitations

- No captures in closures (env_ptr is always NULL)
- No `push`/`pop`/`append` on existing arrays (immutable data)
- No slices (`arr[1..3]` returns copy, not view)
- `array_free` is no-op (bump allocator)

---

## Feature 9: Named Arguments

```elang
fn connect(host: string, port: u16, timeout: u32) -> socket { ... }

// Call with names (order doesn't matter)
let s = connect(port: 8080, host: "localhost", timeout: 5000)

// Positional still works
let s = connect("localhost", 8080, 5000)
```

---

## Combined: Before vs After

### Before (current ELang)
```elang
fn main() -> void {
    let x = 42
    let y = 100
    let sum = x + y
    print_str("Sum: ")
    print_int(sum)
    print_str("\n")

    let result = match divide(10, 2) {
        Ok(val) => val
        Err(_) => 0
    }
    print_str("Result: ")
    print_int(result)
    print_str("\n")
    return 0
}
```

### After (new syntax)
```elang
fn main() -> void {
    let sum = 42 |> add(100)
    "Sum: " |> print() |> print(sum) |> print("\n")

    let result = match divide(10, 2) {
        Ok(val) => val
        Err(_) => 0
    }
    "Result: " |> print() |> print(result) |> print("\n")
}
```

### Complex Example
```elang
using "io"
using "math"

fn process(data: string) -> string {
    data
        |> str_trim()
        |> str_lower()
        |> str_split(" ")
        |> map(str_upper)
        |> str_join("-")
}

fn main() -> void {
    let input = io::read_line()
    let result = process(input)

    if str_len(result) > 0 {
        io::print(result)
    } else {
        io::print("(empty)")
    }
}
```

### Error Handling with Pipeline

ELang uses a tagged-union Result type for error handling. Ok/Err values are encoded
using the lowest bit: bit 0 = 0 for Ok, bit 0 = 1 for Err. The actual value is
stored in the upper 63 bits (shifted left by 1).

```elang
fn divide(a: i64, b: i64) -> i64 {
    if b == 0 { return Err(0) }
    return Ok(a / b)
}

// Match — pattern matching on Ok/Err
let result = match divide(100, 7) {
    Ok(val) => val
    Err(e) => 0
}

// ? operator — propagate errors up the call stack
fn double_divide(a: i64, b: i64) -> i64 {
    let result = divide(a, b)?    // if Err, return immediately
    return Ok(result * 2)
}

// catch — handle errors inline
let safe = divide(10, 0) catch { 0 }    // returns 0 on error

// panic — abort with message
panic("unreachable")

// assert — check condition, panic on failure
assert(x > 0, "x must be positive")
```

### Result Encoding

| Value | Encoding | Bit 0 |
|-------|----------|-------|
| `Ok(val)` | `val << 1` | 0 |
| `Err(val)` | `(val << 1) \| 1` | 1 |

### Error Propagation with `?`

The `?` operator checks if a Result is Err. If so, it runs any pending `defer`
statements and returns the error immediately. If Ok, it extracts the value:

```elang
fn read_config(path: string) -> i64 {
    let fd = sys_open(path, 0, 0)?
    defer sys_close(fd)
    // if open failed, returns Err immediately (defer runs first)
    // if ok, fd contains the file descriptor
    Ok(fd)
}
```

### Catch Blocks

`catch` provides inline error handling without leaving the current scope:

```elang
// On success: returns the Ok value
// On error: runs the catch block and returns its value
let value = risky_operation() catch {
    print_str("operation failed\n")
    0    // fallback value
}
```

---

## Implementation Priority

| Feature | Complexity | Impact | Status |
|---------|-----------|--------|--------|
| Implicit return | Low | High | Done |
| `\|>` pipeline | Medium | High | Done |
| Expression-oriented `if` | Low | Medium | Done |
| `defer` | Medium | High | Done |
| Optional braces (`=>`) | Low | Medium | Done |
| Tuple unpacking | Medium | Medium | Done |
| `using` imports | Low | Low | Done |
| **Arrays (heap)** | **High** | **High** | **Done** |
| **Result\<T, E\>** | **Medium** | **High** | **Done** |
| **Closures** | **High** | **High** | **Done** |
| **map/filter/reduce** | **Medium** | **High** | **Done** |
| **for-in + enumerate** | **Medium** | **Medium** | **Done** |
| Closures: captures | High | High | ✅ Done |
| Struct methods | Medium | High | ✅ Done |
| Enum variants with data | Medium | High | ✅ Done |
| Named arguments | Medium | Low | Planned |
| Generics | High | High | Planned |
| Traits | High | High | Planned |

---

## Feature: Explicit Typing

All variables and functions must have explicit type annotations.

```elang
// Correct
let x: i64 = 42
let name: string = "hello"
fn add(a: i64, b: i64) -> i64 { a + b }
fn main() -> u8 { return 0 }

// Error
let x = 42           // Parse error: expected ':'
fn add(a, b) { a+b } // Parse error: expected ':'
```

**Design decision:** Explicit types improve code readability and catch errors early. All variables and function parameters require explicit type annotations.

---

## Feature: Struct = Class

Structs are heap-allocated objects with fields and methods. They serve as the primary OOP mechanism.

### Memory Layout

```
[refcount:8][field_count:8][field0:8][field1:8]...
             ^                            ^
             header                       data start (returned pointer)
```

### Declaration

```elang
struct Point {
    x: i64
    y: i64
}
```

### Constructor

```elang
impl Point {
    fn new(x: i64, y: i64) -> Point {
        Point(x, y)
    }
}
```

### Methods

```elang
impl Point {
    fn distance(self: Point, other: Point) -> i64 {
        let dx: i64 = self.x - other.x
        let dy: i64 = self.y - other.y
        dx * dx + dy * dy
    }
}

let p1: Point = Point::new(1, 2)
let p2: Point = Point::new(4, 6)
let d: i64 = p1.distance(p2)
```

**Design decision:** Methods are syntactic sugar for functions with `self` as first parameter. No vtable, no dynamic dispatch. Static dispatch only.

---

## Feature: Enums with Auto Methods

Enums automatically get `tag()`, `name()`, and `count()` methods.

### Simple Enums

```elang
enum Color { Red, Green, Blue }
enum Direction { North, South, East, West }
```

### Auto Methods

```elang
let c: i64 = Color::Green
c.tag()       // → 1
c.name()      // → "Green"
Color.count() // → 3
```

**Implementation:** Auto-generated functions `Color_tag()`, `Color_name()`, `Color_count()` with name table in .data section.

---

## Feature: impl Blocks

Methods are defined in `impl` blocks associated with a type.

```elang
struct Point { x: i64, y: i64 }

impl Point {
    fn new(x: i64, y: i64) -> Point { Point(x, y) }
    fn distance(self: Point, other: Point) -> i64 { ... }
}
```

**Codegen:** Methods are compiled as functions with type prefix: `Point_new`, `Point_distance`.

---

## Feature: Closures with Captures

Closures can capture variables from outer scope via environment struct.

```elang
let y: i64 = 42
let f = fn x => x + y  // captures y
let result: i64 = f(8)  // → 50
```

**Implementation:** Environment struct on heap: `[count:8][env[0]:8][env[1]:8]...`. Closure object: `[fn_ptr, env_ptr]`.

---

## Calling Conventions

### User Functions

```asm
; rdi = env_ptr (NULL for non-closures)
; rsi = arg0
; rdx = arg1
; rcx = arg2
; r8 = arg3
; r9 = arg4
```

### Struct Constructors

```asm
; Standard convention:
; rdi = arg0
; rsi = arg1
; rdx = arg2
```

### Enum Auto Methods

```asm
; Standard convention:
; rdi = enum_value
```
