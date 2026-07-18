// ============================================
// Pipeline оператор |> — цепочки преобразований
// ============================================
// |> передаёт левое значение как ПЕРВЫЙ аргумент правой функции

fn add(a: i64, b: i64) -> i64 { a + b }
fn multiply(a: i64, b: i64) -> i64 { a * b }
fn double(x: i64) -> i64 { x * 2 }
fn negate(x: i64) -> i64 { 0 - x }
fn abs(x: i64) -> i64 { if x < 0 { 0 - x } else { x } }

fn main() -> void {
    print_str("=== Pipeline |> ===\n\n")

    // --- Простой pipeline ---
    print_str("--- Простой ---\n")
    5 |> double() |> print_int()
    print_str("\n")

    // --- Цепочка преобразований ---
    print_str("\n--- Цепочка ---\n")
    10 |> double() |> add(5) |> print_int()
    print_str("\n")

    // --- Pipeline с math операциями ---
    print_str("\n--- Pipeline math ---\n")
    print_str("5 |> double() |> add(10) = ")
    5 |> double() |> add(10) |> print_int()
    print_str("\n")

    print_str("3 |> multiply(7) |> add(1) = ")
    3 |> multiply(7) |> add(1) |> print_int()
    print_str("\n")

    // --- Pipeline с функциями ---
    print_str("\n--- Pipeline functions ---\n")
    print_str("abs(-42) = ")
    (-42) |> abs() |> print_int()
    print_str("\n")

    print_str("negate(10) = ")
    10 |> negate() |> print_int()
    print_str("\n")

    // --- Цепочки для обработки данных ---
    print_str("\n--- Обработка данных ---\n")
    print_str("((5 + 5) * 2) + 1 = ")
    5 |> add(5) |> double() |> add(1) |> print_int()
    print_str("\n")
}
