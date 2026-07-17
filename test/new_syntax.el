// ELang v0.2 — new syntax features test

fn add(a: i64, b: i64) -> i64 { a + b }

fn mul(a: i64, b: i64) -> i64 { a * b }

fn negate(x: i64) -> i64 { 0 - x }

fn iabs(x: i64) -> i64 =>
    if x < 0 { negate(x) } else { x }

fn divide(a: i64, b: i64) -> i64 {
    if b == 0 {
        return Err(0)
    }
    return Ok(a / b)
}

fn main() -> void {
    // Implicit return: last expression is the return value
    print_str("=== Implicit Return ===\n")
    print_str("add(3, 4) = ")
    print_int(add(3, 4))
    print_str("\n")

    // Pipeline operator |>
    print_str("\n=== Pipeline |>\n")
    5 |> add(10) |> print_int()
    print_str("\n")

    3 |> mul(7) |> add(1) |> print_int()
    print_str("\n")

    // when expression
    print_str("\n=== When Expression ===\n")
    let x = 42
    when x > 0 {
        print_str("positive\n")
    } else {
        print_str("non-positive\n")
    }

    // Optional braces with =>
    print_str("\n=== Optional Braces ===\n")
    for i in 0..5 => print_int(i)
    print_str("\n")

    // Defer
    print_str("\n=== Defer ===\n")
    print_str("before defer\n")
    defer print_str("deferred cleanup\n")
    print_str("after defer\n")

    // Match with Ok/Err
    print_str("\n=== Match Ok/Err ===\n")
    let result = match divide(10, 2) {
        Ok(val) => val
        Err(_) => 0
    }
    print_str("10 / 2 = ")
    print_int(result)
    print_str("\n")

    let err_result = match divide(10, 0) {
        Ok(val) => val
        Err(_) => 0
    }
    print_str("10 / 0 = ")
    print_int(err_result)
    print_str("\n")

    return 0
}
