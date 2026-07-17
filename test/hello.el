// Hello World
fn main() -> void {
    print_str("Hello, ELang!\n")

    let x = 42
    let y = 100
    let sum = x + y
    print_str("Sum: ")
    print_int(sum)
    print_str("\n")

    if x > 0 {
        print_str("x is positive\n")
    }

    let i = 0
    while i < 5 {
        print_str("Loop: ")
        print_int(i)
        print_str("\n")
        i = i + 1
    }

    for j in 0..3 {
        print_str("For: ")
        print_int(j)
        print_str("\n")
    }

    // Test Ok/Err with match
    let result = match divide(10, 2) {
        Ok(val) => val
        Err(_) => 0
    }
    print_str("Divide: ")
    print_int(result)
    print_str("\n")

    // Test Err case
    let err_result = match divide(10, 0) {
        Ok(val) => val
        Err(_) => 0
    }
    print_str("Divide by zero: ")
    print_int(err_result)
    print_str("\n")
}

fn add(a: i64, b: i64) -> i64 {
    a + b
}

fn divide(a: i64, b: i64) -> i64 {
    if b == 0 {
        return Err(0)
    }
    return Ok(a / b)
}
