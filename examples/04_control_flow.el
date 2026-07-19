// ============================================
// Управление потоком — if/else if, while, for
// ============================================

fn main() -> u8 {
    print_str("=== Управление потоком ===\n\n")

    // --- if/else if/else ---
    print_str("--- if/else if ---\n")
    let temp: i64 = 25

    if temp > 30 {
        print_str("hot\n")
    } else if temp > 20 {
        print_str("warm\n")
    } else if temp > 10 {
        print_str("cool\n")
    } else {
        print_str("cold\n")
    }

    // --- while цикл ---
    print_str("\n--- while ---\n")
    let mut i: i64 = 0
    while i < 5 {
        print_str("i = ")
        print_int(i)
        print_str("\n")
        i = i + 1
    }

    // --- for цикл (диапазон) ---
    print_str("\n--- for (диапазон) ---\n")
    for j: i64 in 0..5 {
        print_str("j = ")
        print_int(j)
        print_str("\n")
    }

    // --- for с => (однострочное тело) ---
    print_str("\n--- for с => ---\n")
    print_str("Квадраты: ")
    for k: i64 in 0..5 => print_int(k * k)
    print_str("\n")

    // --- Вложенные циклы ---
    print_str("\n--- Таблица умножения (3x3) ---\n")
    for a: i64 in 1..4 {
        for b: i64 in 1..4 {
            print_int(a * b)
            print_str("  ")
        }
        print_str("\n")
    }
    return 0
}
