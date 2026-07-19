// ============================================
// Функции — объявление, неявный возврат, =>
// ============================================

// --- Простая функция с неявным возвратом ---
// Последнее выражение автоматически возвращается
fn add(a: i64, b: i64) -> i64 {
    a + b  // неявный возврат
}

// --- Однострочная функция через => ---
fn double(x: i64) -> i64 => x * 2

fn square(x: i64) -> i64 => x * x

// --- Функция с явным возвратом (return) ---
fn abs(x: i64) -> i64 {
    if x < 0 {
        return 0 - x  // явный возврат для раннего выхода
    }
    x  // неявный возврат
}

// --- Рекурсивная функция ---
fn factorial(n: i64) -> i64 {
    if n <= 1 {
        1
    } else {
        n * factorial(n - 1)
    }
}

// --- Функция без возврата (void) ---
fn greet(name: i64) -> void {
    print_str("Hello, ")
    print_int(name)
    print_str("!\n")
}

// --- Функция с несколькими параметрами ---
fn clamp(value: i64, min_val: i64, max_val: i64) -> i64 {
    if value < min_val {
        min_val
    } else if value > max_val {
        max_val
    } else {
        value
    }
}

fn main() -> u8 {
    print_str("=== Функции ===\n\n")

    // Вызов функций
    print_str("add(3, 4) = ")
    print_int(add(3, 4))
    print_str("\n")

    print_str("double(7) = ")
    print_int(double(7))
    print_str("\n")

    print_str("square(5) = ")
    print_int(square(5))
    print_str("\n")

    print_str("abs(-42) = ")
    print_int(abs(-42))
    print_str("\n")

    print_str("factorial(10) = ")
    print_int(factorial(10))
    print_str("\n")

    print_str("clamp(15, 0, 10) = ")
    print_int(clamp(15, 0, 10))
    print_str("\n")

    // Функция без возврата
    print_str("\n")
    greet(42)
    return 0
}
