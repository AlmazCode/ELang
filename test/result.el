// Test: Result<T, E> type
fn divide(a: i64, b: i64) -> Result<i64, i64> {
    if b == 0 { return Err(0) }
    return Ok(a / b)
}

fn main() -> void {
    let result = match divide(100, 7) {
        Ok(val) => val
        Err(_) => 0
    }
    print_str("100 / 7 = ")
    print_int(result)
    print_str("\n")

    let err_result = match divide(100, 0) {
        Ok(val) => val
        Err(_) => 0
    }
    print_str("100 / 0 = ")
    print_int(err_result)
    print_str("\n")
}
