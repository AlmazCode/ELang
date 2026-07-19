// ============================================
// Обработка ошибок — Result<T, E>, ?, catch
// ============================================

// --- Функция, возвращающая Result ---
fn divide(a: i64, b: i64) -> Result<i64, i64> {
    if b == 0 {
        return Err(0)  // ошибка: деление на ноль
    }
    return Ok(a / b)
}

// --- Пропагация ошибки через ? ---
fn double_divide(a: i64, b: i64) -> Result<i64, i64> {
    let result: i64 = divide(a, b)?  // если Err — вернуть сразу
    Ok(result * 2)
}

// --- Цепочка с обработкой ошибок ---
fn process(x: i64) -> Result<i64, i64> {
    let a: i64 = divide(x, 2)?   // если Err — вернуть Err
    let b: i64 = double_divide(a, 3)?  // если Err — вернуть Err
    Ok(a + b)
}

fn main() -> u8 {
    print_str("=== Обработка ошибок ===\n\n")

    // --- Match по Ok/Err ---
    print_str("--- Match Ok/Err ---\n")
    let result: i64 = match divide(100, 7) {
        Ok(val) => val
        Err(_) => 0
    }
    print_str("100 / 7 = ")
    print_int(result)
    print_str("\n")

    // Деление на ноль
    let err_result: i64 = match divide(100, 0) {
        Ok(val) => val
        Err(_) => 0
    }
    print_str("100 / 0 = ")
    print_int(err_result)
    print_str("\n")

    // --- Catch — инлайн обработка ---
    print_str("\n--- Catch ---\n")
    let safe: i64 = divide(10, 0) catch { 0 }
    print_str("10 / 0 (catch) = ")
    print_int(safe)
    print_str("\n")

    let good: i64 = divide(10, 2) catch { 0 }
    print_str("10 / 2 (catch) = ")
    print_int(good)
    print_str("\n")

    // --- Пропагация через ? ---
    print_str("\n--- Пропагация ? ---\n")
    let proc_result: i64 = match process(100) {
        Ok(val) => val
        Err(_) => 0
    }
    print_str("process(100) = ")
    print_int(proc_result)
    print_str("\n")

    // --- Panic и assert ---
    print_str("\n--- Panic и Assert ---\n")
    print_str("assert(1 > 0) — OK\n")
    assert(1 > 0, "1 should be > 0")

    // panic("авария")  // аварийное завершение с сообщением
    return 0
}
