# ELang Syntax Design

## Identity: "Systems programming that reads like data flow"

ELang is not "C with features." It's a language where code reads as a sequence of
transformations. Data flows through pipes. Results propagate automatically.
The compiler does the boring work so you don't have to.

Three pillars:
1. **Expression-oriented** — everything returns a value
2. **Pipeline-first** — data flows through `|>` chains
3. **Zero ceremony** — the compiler infers what it can

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

Stack-allocated fixed-size arrays with literal syntax, indexing, and `.len`.

```elang
fn main() -> void {
    // Array literal — type inferred from elements
    let arr = [10, 20, 30, 40, 50]

    // Index access (0-based)
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

    // Array with expressions
    let x = 5
    let arr2 = [x, x + 1, x * 2]  // [5, 6, 10]
}
```

### Storage Model

Arrays are stack-allocated. The layout in memory:

```
[rsp]      → length (i64)
[rsp+8]    → element[0]
[rsp+16]   → element[1]
...
[rsp+8+N*8] → element[N-1]
```

The array variable holds a pointer to `element[0]` (i.e., `rsp+8`). `.len` reads from `[pointer-8]`.

### Limitations (v1)

- Maximum 32 elements (256 bytes on stack)
- All elements must be the same type (inferred from first element)
- Elements stored as `i64` (8 bytes each)
- No slices, no heap allocation (planned for future)

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
| Implicit return | Low | High — less boilerplate everywhere | Done |
| `\|>` pipeline | Medium | High — defines the language's identity | Done |
| Expression-oriented `if` | Low | Medium — cleaner than if-as-expression | Done |
| `defer` | Medium | High — essential for systems programming | Done |
| Optional braces (`=>`) | Low | Medium — cleaner single-line functions | Done |
| Tuple unpacking | Medium | Medium — multiple returns become natural | Done |
| `using` imports | Low | Low — nice to have | Done |
| **Arrays** | **Medium** | **High — enables practical programs** | **Done** |
| **Result\<T, E\>** | **Medium** | **High — type-safe error handling** | **Done** |
| Struct methods | Medium | High — OOP-style organization | Planned |
| Enum variants with data | Medium | High — algebraic data types | Planned |
| Error handling (`?`, `catch`) | Medium | High — ergonomic error propagation | Done |
| Named arguments | Medium | Low — quality of life | Planned |

Start with implicit return + pipeline + expression-oriented if. Those three define ELang's character.
