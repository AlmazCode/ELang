// ============================================
// Модули — import, export, namespace
// ============================================
// Компиляция: ./bin/elc examples/10_modules.el
// Требуется файл math.el в той же директории

// --- Импорт проектного модуля ---
import "math"

fn main() -> u8 {
    print_str("=== Модули ===\n\n")

    // --- Вызов функций из модуля ---
    print_str("--- math::power ---\n")
    print_str("2^10 = ")
    print_int(math::power(2, 10))
    print_str("\n")

    print_str("3^5 = ")
    print_int(math::power(3, 5))
    print_str("\n")

    // --- Другие функции из math ---
    print_str("\n--- math::abs_val ---\n")
    print_str("abs(-42) = ")
    print_int(math::abs_val(-42))
    print_str("\n")

    print_str("abs(42) = ")
    print_int(math::abs_val(42))
    print_str("\n")

    // --- math::max ---
    print_str("\n--- math::max ---\n")
    print_str("max(10, 20) = ")
    print_int(math::max(10, 20))
    print_str("\n")

    // --- Pipeline с модулями ---
    print_str("\n--- Pipeline с модулями ---\n")
    print_str("10 |> math::power(2) = ")
    10 |> math::power(2) |> print_int()
    print_str("\n")

    print_str("5 |> math::abs_val() = ")
    (-5) |> math::abs_val() |> print_int()
    print_str("\n")
    return 0
}
