# Pipeline оператор `|>`

## Синтаксис

```elang
value |> function(args)
```

Передаёт левое значение как **первый** аргумент правой функции.

## Базовое использование

```elang
// Без pipeline
print_int(add(x, y))

// С pipeline
x |> add(y) |> print_int()
```

## Цепочки преобразований

```elang
10 |> double() |> add(5) |> print_int()
// Эквивалентно: print_int(add(double(10), 5))

5 |> double() |> add(10) |> print_int()
// (5 * 2) + 10 = 20
```

## С функциями

```elang
fn double(x: i64) -> i64 { x * 2 }
fn add(a: i64, b: i64) -> i64 { a + b }
fn negate(x: i64) -> i64 { 0 - x }
fn abs(x: i64) -> i64 { if x < 0 { 0 - x } else { x } }

5 |> double() |> print_int()     // 10
(-42) |> abs() |> print_int()    // 42
10 |> negate() |> print_int()    // -10
```

## С модулями

```elang
import "math"

10 |> math::power(2) |> print_int()   // 1024
(-5) |> math::abs_val() |> print_int() // 5
```

## С массивами

```elang
let arr: [i64] = [1, 2, 3, 4, 5]

let doubled: [i64] = arr |> map(fn(x: i64) => x * 2)
let big: [i64] = arr |> filter(fn(x: i64) => x > 2)
let total: i64 = arr |> reduce(0, fn(acc: i64, x: i64) => acc + x)

// Цепочки
let result: i64 = arr
    |> filter(fn(x: i64) => x > 2)
    |> reduce(0, fn(acc: i64, x: i64) => acc + x)
// result = 12 (3 + 4 + 5)
```

## Приоритет

Pipeline связывает tightest среди бинарных операторов:

```elang
// Это:
let result = data |> filter(valid) |> map(transform) |> collect()

// Эквивалентно:
let result = collect(map(filter(data, valid), transform))
```

## Codegen

Pipeline компилируется как обычный вызов функции:

```asm
; x |> f() |> g()
; Генерируется:
    ; вычислить x → rax
    ; push rax
    ; вызвать f(rax)
    ; результат → rax
    ; push rax
    ; вызвать g(rax)
```

## Ограничения

- Pipeline передаёт только как **первый** аргумент
- Нет способа передать как второй или третий аргумент
- Для этого используйте обычный вызов: `f(x, extra_arg)`
