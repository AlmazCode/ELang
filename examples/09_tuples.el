// ============================================
// Tuples — кортежи и деструктуризация
// ============================================

// --- Функция, возвращающая кортеж ---
fn divmod(a: i64, b: i64) -> (i64, i64) {
    (a / b, a % b)
}

fn swap_pair(a: i64, b: i64) -> (i64, i64) {
    (b, a)
}

fn main() -> u8 {
    print_str("=== Tuples ===\n\n")

    // --- Создание кортежа ---
    print_str("--- Создание ---\n")
    let pair: (i64, i64) = (10, 20)
    print_str("(10, 20)\n")

    // --- Деструктуризация ---
    print_str("\n--- Деструктуризация ---\n")
    let (a, b) = divmod(17, 5)
    print_str("17 / 5 = ")
    print_int(a)
    print_str(" remainder ")
    print_int(b)
    print_str("\n")

    // --- Обмен без временной переменной ---
    print_str("\n--- Обмен ---\n")
    let (x, y) = swap_pair(42, 99)
    print_str("swap(42, 99) = (")
    print_int(x)
    print_str(", ")
    print_int(y)
    print_str(")\n")

    // --- Кортеж в переменной ---
    print_str("\n--- Кортеж в переменной ---\n")
    let point: (i64, i64) = (100, 200)
    let (px, py) = point
    print_str("point = (")
    print_int(px)
    print_str(", ")
    print_int(py)
    print_str(")\n")
    return 0
}
