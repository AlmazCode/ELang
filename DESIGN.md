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

Pass the left value as the LAST argument of the right function.

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

## Feature 4: `when` — Expression-oriented If

`when` is an expression that returns a value. `if` remains a statement.

```elang
// when returns a value
let status = when x > 0 {
    "positive"
} else when x == 0 {
    "zero"
} else {
    "negative"
}

// if stays as a statement (no confusion)
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

## Feature 8: Named Arguments

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

    when str_len(result) > 0 {
        io::print(result)
    } else {
        io::print("(empty)")
    }
}
```

### Error Handling with Pipeline
```elang
fn parse_config(raw: string) -> Result<Config, string> {
    let lines = raw |> str_split("\n") |> filter(str_not_empty)

    when lines {
        [] => Err("empty config")
        _ => {
            let kv = lines |> map(parse_line) |> collect()
            Ok(Config { lines: kv })
        }
    }
}
```

---

## Implementation Priority

| Feature | Complexity | Impact |
|---------|-----------|--------|
| Implicit return | Low | High — less boilerplate everywhere |
| `\|>` pipeline | Medium | High — defines the language's identity |
| `when` expression | Low | Medium — cleaner than if-as-expression |
| `defer` | Medium | High — essential for systems programming |
| Optional braces (`=>`) | Low | Medium — cleaner single-line functions |
| Tuple unpacking | Medium | Medium — multiple returns become natural |
| `using` imports | Low | Low — nice to have |
| Named arguments | Medium | Low — quality of life |

Start with implicit return + pipeline + when. Those three define ELang's character.
