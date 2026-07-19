// ============================================
// Выражения — всё возвращает значение
// ============================================

fn main() -> u8 {
    print_str("=== Выражения ===\n\n")

    // --- Функции возвращают значение ---
    print_str("--- Функции как выражения ---\n")
    print_str("add(3, 4) = ")
    print_int(add(3, 4))
    print_str("\n")

    print_str("double(7) = ")
    print_int(double(7))
    print_str("\n")

    print_str("square(5) = ")
    print_int(square(5))
    print_str("\n")

    // --- Цепочки вызовов ---
    print_str("\n--- Цепочки вызовов ---\n")
    print_str("((5 + 5) * 2) + 1 = ")
    print_int(add(5, 5) |> double() |> add(1))
    print_str("\n")

    // --- Рекурсия ---
    print_str("\n--- Рекурсия ---\n")
    print_str("factorial(10) = ")
    print_int(factorial(10))
    print_str("\n")

    print_str("fibonacci(10) = ")
    print_int(fibonacci(10))
    print_str("\n")

    // --- Условные выражения ---
    print_str("\n--- Условные выражения ---\n")
    let x: i64 = 42
    let category: string = classify(x)
    print_str("classify(42) = ")
    print_str(category)
    print_str("\n")
    return 0
}

fn add(a: i64, b: i64) -> i64 { a + b }
fn double(x: i64) -> i64 { x * 2 }
fn square(x: i64) -> i64 => x * x

fn factorial(n: i64) -> i64 {
    if n <= 1 { 1 } else { n * factorial(n - 1) }
}

fn fibonacci(n: i64) -> i64 {
    if n <= 1 { n } else { fibonacci(n - 1) + fibonacci(n - 2) }
}

fn classify(x: i64) -> string {
    if x > 100 { "big" } else if x > 10 { "medium" } else { "small" }
}
