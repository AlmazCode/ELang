// ============================================
// Переменные — let и let mut
// ============================================
// ELang: immutable по умолчанию, mut для изменяемых переменных

fn main() -> u8 {
    // --- Immutable переменные ---
    let x: i64 = 42
    let name: string = "ELang"
    let flag: bool = true

    print_str("=== Immutable ===\n")
    print_str("x = ")
    print_int(x)
    print_str("\n")

    print_str("name = ")
    print_str(name)
    print_str("\n")

    // x = 100  // ОШИБКА: нельзя изменить immutable переменную

    // --- Mutable переменные ---
    let mut counter: i64 = 0
    let mut sum: i64 = 0

    print_str("\n=== Mutable ===\n")
    while counter < 5 {
        sum = sum + counter
        counter = counter + 1
    }
    print_str("sum(0..4) = ")
    print_int(sum)
    print_str("\n")

    // --- Типы переменных ---
    print_str("\n=== Типы ===\n")
    let big: i64 = 9223372036854775807

    print_str("i64 max = ")
    print_int(big)
    print_str("\n")

    // --- Неявный вывод типа ---
    let auto: i64 = 100  // тип выводится как i64
    print_str("\nauto = ")
    print_int(auto)
    print_str("\n")
    return 0
}
