# Управление потоком

## if/else

`if` — expression, возвращает значение.

```elang
let temp: i64 = 25
let weather: string = if temp > 30 {
    "hot"
} else if temp > 20 {
    "warm"
} else {
    "cold"
}
// weather = "warm"
```

### if как statement

```elang
if x > 0 {
    print_str("positive\n")
}
```

### Вложенные if/else

```elang
let category: string = if score >= 90 {
    "A"
} else if score >= 80 {
    "B"
} else if score >= 70 {
    "C"
} else {
    "F"
}
```

## while

```elang
let mut i: i64 = 0
while i < 10 {
    print_int(i)
    print_str(" ")
    i = i + 1
}
print_str("\n")
```

## for

### Диапазон

```elang
// 0, 1, 2, 3, 4
for i: i64 in 0..5 {
    print_int(i)
    print_str(" ")
}
```

### По элементам массива

```elang
let arr: [i64] = [10, 20, 30]
for x: i64 in arr {
    print_int(x)
    print_str(" ")
}
```

### С индексом (enumerate)

```elang
let arr: [i64] = [10, 20, 30]
for i: i64, x: i64 in arr {
    print_int(i)
    print_str(": ")
    print_int(x)
    print_str("\n")
}
```

Вывод:
```
0: 10
1: 20
2: 30
```

### Однострочное тело с `=>`

```elang
for i: i64 in 0..5 => print_int(i * i)
print_str("\n")
```

### Вложенные циклы

```elang
for a: i64 in 1..4 {
    for b: i64 in 1..4 {
        print_int(a * b)
        print_str("  ")
    }
    print_str("\n")
}
```

## match

Pattern matching по значениям:

```elang
fn divide(a: i64, b: i64) -> Result<i64, i64> {
    if b == 0 { return Err(0) }
    Ok(a / b)
}

fn main() -> u8 {
    // Match по Ok/Err
    let result: i64 = match divide(100, 7) {
        Ok(val) => val
        Err(_) => 0
    }
    print_int(result)  // 14

    // Match по enum значению
    let c: i64 = Color::Green
    let name: string = match c {
        Color::Red => "Red"
        Color::Green => "Green"
        Color::Blue => "Blue"
    }

    // Match по числовому значению
    let x: i64 = 42
    let desc: string = match x {
        0 => "zero"
        1 => "one"
        _ => "other"
    }
    return 0
}
```

### Паттерны

| Паттерн | Описание |
|---------|----------|
| `Ok(val)` | Result — Ok, извлекает значение |
| `Err(val)` | Result — Err, извлекает значение |
| `Color::Red` | Enum variant |
| `42` | Константа |
| `_` | Wildcard (любое значение) |

match expression возвращает значение — آخرнее выражение в ветке становится результатом.

## break и continue

В текущей версии `break` и `continue` **не реализованы** в codegen. Лексер распознаёт токены, но парсер их не обрабатывает.

Для выхода из цикла используйте `while` с флагом:

```elang
let mut found: i64 = 0
let mut i: i64 = 0
while i < arr.len && found == 0 {
    if arr[i] == target { found = 1 }
    i = i + 1
}
```
