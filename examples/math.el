// ============================================
// Math module — example project module
// ============================================
// Demonstrates: import "math", export fn, namespace::func()
// Compile: ./bin/elc examples/10_modules.el

// --- Exported functions (available as math::func()) ---
export fn power(base: i64, exp: i64) -> i64 {
    let result: i64 = 1
    let i: i64 = 0
    while i < exp {
        result = result * base
        i = i + 1
    }
    return result
}

export fn abs_val(x: i64) -> i64 {
    if x < 0 {
        return 0 - x
    }
    return x
}

export fn max(a: i64, b: i64) -> i64 {
    if a > b {
        return a
    }
    return b
}

export fn min(a: i64, b: i64) -> i64 {
    if a < b {
        return a
    }
    return b
}

export fn clamp(value: i64, lo: i64, hi: i64) -> i64 {
    if value < lo {
        return lo
    }
    if value > hi {
        return hi
    }
    return value
}
