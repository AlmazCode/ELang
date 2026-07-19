// ============================================
// Defer — автоматическая очистка ресурсов
// ============================================
// defer выполняется при выходе из scope в обратном порядке

fn main() -> u8 {
    print_str("=== Defer ===\n\n")

    // --- Базовый defer ---
    print_str("--- Базовый ---\n")
    print_str("1. Before defer\n")
    defer print_str("3. Deferred (последний)\n")
    print_str("2. After defer\n")
    print_str("\n")

    // --- Несколько defer (обратный порядок) ---
    print_str("--- Несколько defer ---\n")
    print_str("1. Start\n")
    defer print_str("4. First defer (выполняется последним)\n")
    defer print_str("3. Second defer\n")
    defer print_str("2. Third defer (выполняется первым)\n")
    print_str("5. End\n")
    print_str("\n")

    // --- Defer в цикле ---
    print_str("--- Defer в цикле ---\n")
    for i: i64 in 0..3 {
        print_str("Iteration ")
        print_int(i)
        print_str("\n")
    }
    return 0
}
