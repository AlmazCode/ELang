// ============================================
// FizzBuzz — классическая задача на ELang
// ============================================
// Демонстрация: функции, while, if/else if, модули, pipeline

using "math"

fn is_divisible(n: i64, d: i64) -> i64 {
    if n % d == 0 { 1 } else { 0 }
}

fn fizzbuzz(n: i64) -> void {
    let mut i = 1
    while i <= n {
        if is_divisible(i, 15) == 1 {
            print_str("FizzBuzz")
        } else if is_divisible(i, 3) == 1 {
            print_str("Fizz")
        } else if is_divisible(i, 5) == 1 {
            print_str("Buzz")
        } else {
            print_int(i)
        }
        print_str(" ")
        i = i + 1
    }
    print_str("\n")
}

fn main() -> void {
    print_str("=== FizzBuzz 1-30 ===\n")
    fizzbuzz(30)

    // --- Демонстрация возможностей ---
    print_str("\n=== Возможности языка ===\n\n")

    // Pipeline
    print_str("Pipeline: 2^10 = ")
    2 |> math::power(10) |> print_int()
    print_str("\n")

    // Match
    print_str("\nMatch: divide(100, 7) = ")
    let result = match divide(100, 7) {
        Ok(val) => val
        Err(_) => 0
    }
    print_int(result)
    print_str("\n")

    // Defer
    print_str("\nDefer:\n")
    print_str("  before\n")
    defer print_str("  deferred\n")
    print_str("  after\n")

    // For loop
    print_str("\nFor loop: ")
    for i in 0..5 => print_int(i * i)
    print_str("\n")

    // If expression
    print_str("\nIf expression: ")
    let temp = 25
    let weather = if temp > 30 { "hot" } else if temp > 20 { "warm" } else { "cold" }
    print_str(weather)
    print_str("\n")
}

fn divide(a: i64, b: i64) -> Result<i64, i64> {
    if b == 0 { return Err(0) }
    return Ok(a / b)
}
