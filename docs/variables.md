# Переменные

## Объявление

```elang
let name: Type = value       // immutable
let mut name: Type = value   // mutable
```

### Immutable (по умолчанию)

```elang
let x: i64 = 42
let name: string = "ELang"
let flag: bool = true

// x = 100   // ОШИБКА: cannot reassign immutable variable
```

### Mutable

```elang
let mut counter: i64 = 0
let mut sum: i64 = 0

while counter < 5 {
    sum = sum + counter
    counter = counter + 1
}
```

## Область видимости

Переменные видны только внутри блока `{}`:

```elang
fn main() -> u8 {
    let x: i64 = 10
    {
        let y: i64 = 20
        print_int(x + y)  // OK: x и y видны
    }
    // print_int(y)  // ОШИБКА: y не видна здесь
    print_int(x)
    return 0
}
```

## Деструктуризация кортежей

```elang
fn divmod(a: i64, b: i64) -> (i64, i64) {
    (a / b, a % b)
}

fn main() -> u8 {
    let (quotient, remainder) = divmod(17, 5)
    // quotient = 3, remainder = 2

    // Обмен без временной переменной
    let (a, b) = (1, 2)
    let (x, y) = (b, a)
    // x = 2, y = 1
    return 0
}
```

## Типы переменных

Переменные могут быть любого типа:

```elang
let i: i64 = 42
let f: f64 = 3.14
let s: string = "hello"
let b: bool = true
let arr: [i64] = [1, 2, 3]
let p: *i64 = ptr
```

## Неявный вывод типа

Тип выводится из аннотации, не из значения:

```elang
let x: i64 = 100      // тип i64 из аннотации
let y: f64 = 3.14     // тип f64 из аннотации
```

## Переменные в циклах

Переменные цикла `for` автоматически mutable в области цикла:

```elang
// Переменная цикла автоматически immutable
for i: i64 in 0..10 {
    print_int(i)
    // i = 100  // ОШИБКА: i immutable
}

// Для изменения используйте отдельную mutable переменную
let mut sum: i64 = 0
for x: i64 in [1, 2, 3, 4, 5] {
    sum = sum + x
}
```

## Параметры функций

Параметры функций immutable по умолчанию:

```elang
fn add(a: i64, b: i64) -> i64 {
    // a = 0  // ОШИБКА: a immutable
    a + b
}
```
