// fizzbuzz.el — классическая задачаFizzBuzz с демонстрацией всех фич ELang

using "math"

fn is_divisible(n: i64, d: i64) -> i64 {
    if n % d == 0 { 1 } else { 0 }
}

fn divide(a: i64, b: i64) -> i64 {
    if b == 0 { return Err(0) }
    return Ok(a / b)
}

fn fizzbuzz(n: i64) -> void {
    let i = 1
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
    // FizzBuzz
    print_str("=== FizzBuzz 1-30 ===\n")
    fizzbuzz(30)

    // Math module
    print_str("\n=== Math Module ===\n")
    print_str("2^10 = ")
    print_int(math::power(2, 10))
    print_str("\n")

    print_str("abs(-42) = ")
    print_int(math::abs_val(-42))
    print_str("\n")

    print_str("max(10, 20) = ")
    print_int(math::max(10, 20))
    print_str("\n")

    // Pipeline
    print_str("\n=== Pipeline ===\n")
    10 |> math::power(2) |> print_int()
    print_str("\n")

    3 |> math::power(3) |> print_int()
    print_str("\n")

    // When expression
    print_str("\n=== When Expression ===\n")
    let temp = 35
    let weather = when temp > 30 {
        "hot"
    } else when temp > 20 {
        "warm"
    } else when temp > 10 {
        "cool"
    } else {
        "cold"
    }
    print_str("Temperature ")
    print_int(temp)
    print_str(" is ")
    print_str(weather)
    print_str("\n")

    // Match Ok/Err
    print_str("\n=== Match Ok/Err ===\n")
    let safe_div = match divide(100, 7) {
        Ok(val) => val
        Err(_) => 0
    }
    print_str("100 / 7 = ")
    print_int(safe_div)
    print_str("\n")

    let err_result = match divide(100, 0) {
        Ok(val) => val
        Err(_) => 0
    }
    print_str("100 / 0 = ")
    print_int(err_result)
    print_str("\n")

    // For loop
    print_str("\n=== For Loop (squares) ===\n")
    for i in 0..10 {
        print_int(i)
        print_str("^2 = ")
        print_int(i * i)
        print_str("  ")
    }
    print_str("\n")

    // Defer
    print_str("\n=== Defer ===\n")
    print_str("Before defer\n")
    defer print_str("Deferred cleanup\n")
    print_str("After defer\n")

    // Expression functions
    print_str("\n=== Expression Functions ===\n")
    print_str("is_even(7) = ")
    print_int(math::is_even(7))
    print_str("\n")
    print_str("is_even(8) = ")
    print_int(math::is_even(8))
    print_str("\n")

    // Clamp
    print_str("clamp(15, 0, 10) = ")
    print_int(math::clamp(15, 0, 10))
    print_str("\n")

    // Implicit return
    print_str("\n=== Implicit Return ===\n")
    print_str("power(2, 5) = ")
    print_int(math::power(2, 5))
    print_str("\n")

    // Type checking demo
    print_str("\n=== Type Checking ===\n")
    print_str("Types are verified at compile time!\n")
    print_str("elc -check examples/fizzbuzz.el\n")
}
