// math.el — математическая библиотека для калькулятора

export fn power(base: i64, exp: i64) -> i64 {
    let result = 1
    let i = 0
    while i < exp {
        result = result * base
        i = i + 1
    }
    result
}

export fn abs_val(x: i64) -> i64 =>
    if x < 0 { 0 - x } else { x }

export fn is_even(x: i64) -> i64 =>
    if x % 2 == 0 { 1 } else { 0 }

export fn max(a: i64, b: i64) -> i64 =>
    if a > b { a } else { b }

export fn min(a: i64, b: i64) -> i64 =>
    if a < b { a } else { b }

export fn clamp(val: i64, lo: i64, hi: i64) -> i64 {
    let r = val
    if r < lo { r = lo }
    if r > hi { r = hi }
    r
}
