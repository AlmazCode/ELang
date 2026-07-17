// errors.el — демонстрация системы ошибок ELang v1.0

// ===== Базовые Result функции =====

fn divide(a: i64, b: i64) -> i64 {
    if b == 0 { return Err(0) }
    return Ok(a / b)
}

fn safe_parse_int(input: i64) -> i64 {
    // Простая "проверка" — если число отрицательное, ошибка
    if input < 0 { return Err(1) }
    return Ok(input)
}

// ===== Пример 1: Match (базовый, уже работает) =====

fn demo_match() -> void {
    print_str("=== Match Ok/Err ===\n")

    let result = match divide(100, 7) {
        Ok(val) => val
        Err(e) => 0
    }
    print_str("100 / 7 = ")
    print_int(result)
    print_str("\n")

    let err_result = match divide(100, 0) {
        Ok(val) => val
        Err(e) => 0
    }
    print_str("100 / 0 = ")
    print_int(err_result)
    print_str("\n")
}

// ===== Пример 2: ? operator (error propagation) =====

fn double_divide(a: i64, b: i64) -> i64 {
    let result = divide(a, b)?    // если Err — немедленно вернуть Err
    return Ok(result * 2)
}

fn demo_try() -> void {
    print_str("\n=== ? Operator (Propagation) ===\n")

    let ok = double_divide(20, 4)
    let val = match ok {
        Ok(v) => v
        Err(e) => 0
    }
    print_str("20 / 4 * 2 = ")
    print_int(val)
    print_str("\n")

    let err = double_divide(20, 0)
    let val2 = match err {
        Ok(v) => v
        Err(e) => 0
    }
    print_str("20 / 0 * 2 = ")
    print_int(val2)
    print_str("\n")
}

// ===== Пример 3: catch block =====

fn demo_catch() -> void {
    print_str("\n=== Catch Block ===\n")

    // Успешный случай
    let a = divide(10, 2) catch { 0 }
    print_str("10 / 2 (catch) = ")
    print_int(a)
    print_str("\n")

    // Ошибка — handler вернёт 0
    let b = divide(10, 0) catch { 0 }
    print_str("10 / 0 (catch) = ")
    print_int(b)
    print_str("\n")
}

// ===== Пример 4: Pipeline с ошибками =====

fn process_value(x: i64) -> i64 {
    let doubled = divide(x, 2)?
    let result = doubled + 10
    return Ok(result)
}

fn demo_pipeline_errors() -> void {
    print_str("\n=== Pipeline with Errors ===\n")

    let val = match process_value(8) {
        Ok(v) => v
        Err(e) => 0
    }
    print_str("process(8) = (8/2)+10 = ")
    print_int(val)
    print_str("\n")

    // process_value calls divide(x, 2)? — so x must be such that division triggers error
    // divide(0, 2) returns Ok(0), so process_value(0) = Ok(10)
    let err_val = match process_value(0) {
        Ok(v) => v
        Err(e) => 0
    }
    print_str("process(0) = (0/2)+10 = ")
    print_int(err_val)
    print_str("\n")
}

// ===== Пример 5: Defer + Errors =====

fn read_with_cleanup(fd: i64) -> i64 {
    defer print_str("  [cleanup: closed fd]\n")
    if fd < 0 { return Err(-1) }
    print_str("  [reading from fd]\n")
    return Ok(fd * 10)
}

fn demo_defer_errors() -> void {
    print_str("\n=== Defer + Errors ===\n")

    print_str("Good fd:\n")
    let ok = read_with_cleanup(5) catch { 0 }
    print_str("Result: ")
    print_int(ok)
    print_str("\n")

    print_str("Bad fd:\n")
    let err = read_with_cleanup(-1) catch { 0 }
    print_str("Result: ")
    print_int(err)
    print_str("\n")
}

// ===== Пример 6: Panic =====

fn demo_panic() -> void {
    print_str("\n=== Panic ===\n")
    print_str("About to panic...\n")
    panic("something went terribly wrong")
    print_str("This will never print\n")
}

// ===== Пример 7: Assert =====

fn demo_assert() -> void {
    print_str("\n=== Assert ===\n")
    let x = 42
    assert(x > 0, "x must be positive")
    print_str("assert passed! x = ")
    print_int(x)
    print_str("\n")

    // This would panic:
    // assert(x < 0, "x must be negative")
}

// ===== Main =====

fn main() -> void {
    demo_match()
    demo_try()
    demo_catch()
    demo_pipeline_errors()
    demo_defer_errors()
    demo_assert()
    // demo_panic()  // uncomment to see panic in action
    print_str("\nAll error handling demos completed!\n")
}
