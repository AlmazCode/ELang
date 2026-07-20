# Функции

## Объявление

```elang
fn name(param1: Type1, param2: Type2) -> ReturnType {
    body
}
```

### Неявный возврат

Последнее выражение в теле функции автоматически возвращается:

```elang
fn add(a: i64, b: i64) -> i64 {
    a + b  // неявный возврат
}

fn abs(x: i64) -> i64 {
    if x < 0 { 0 - x } else { x }
}
```

Explicit `return` работает для раннего выхода:

```elang
fn divide(a: i64, b: i64) -> Result<i64, i64> {
    if b == 0 { return Err(0) }  // ранний возврат
    Ok(a / b)
}
```

### Однострочные функции

Синтаксис `=>` для однострочных тел:

```elang
fn double(x: i64) -> i64 => x * 2
fn square(x: i64) -> i64 => x * x
fn negate(x: i64) -> i64 => 0 - x
```

### Функции без возврата

```elang
fn greet(name: i64) -> void {
    print_str("Hello, ")
    print_int(name)
    print_str("!\n")
}
```

## Требование к main

Функция `main` обязательна и должна возвращать `u8`:

```elang
fn main() -> u8 {
    print_str("Hello!\n")
    return 0
}

// ОШИБКА: fn main must have a return type: fn main() -> u8
// fn main() { ... }

// ОШИБКА: fn main must return u8, not i64
// fn main() -> i64 { return 0 }
```

## Рекурсия

```elang
fn factorial(n: i64) -> i64 {
    if n <= 1 { 1 } else { n * factorial(n - 1) }
}

fn fibonacci(n: i64) -> i64 {
    if n <= 1 { n } else { fibonacci(n - 1) + fibonacci(n - 2) }
}
```

## Default values

Параметры могут иметь значения по умолчанию:

```elang
fn greet(name: string, greeting: string = "Hello") -> void {
    print_str(greeting)
    print_str(", ")
    print_str(name)
    print_str("!\n")
}

greet("World")                    // Hello, World!
greet("Alice", greeting: "Hi")   // Hi, Alice!
```

## Named arguments

Аргументы можно передавать по имени (порядок не важен):

```elang
fn add(a: i64, b: i64 = 10) -> i64 { a + b }

add(5, b: 20)    // 25
add(b: 3, a: 7)  // 10
add(5)           // 15 (b берёт default = 10)
```

### Правила

1. Positional аргументы идут первыми
2. Named аргументы могут идти в любом порядке
3. Default values подставляются для пропущенных параметров
4. Named args работают только для пользовательских функций (объявленных в текущем файле)

## Closures

Анонимные функции с захватом переменных:

```elang
fn main() -> u8 {
    let y: i64 = 42
    let f = fn x => x + y       // захватывает y
    let result: i64 = f(8)
    print_int(result)            // 50
    return 0
}
```

### Синтаксис

```elang
// Однострочный closure
let double = fn x => x * 2

// Многострочный не поддерживается — только выражение после =>
let add = fn(x: i64, y: i64) => x + y
```

### Использование в map/filter/reduce

```elang
let arr: [i64] = [1, 2, 3, 4, 5]
let offset: i64 = 10

let mapped: [i64] = arr |> map(fn(x: i64) => x + offset)
// [11, 12, 13, 14, 15]

let total: i64 = arr |> reduce(0, fn(acc: i64, x: i64) => acc + x)
// 15
```

### Захват переменных (captures)

Closures захватывают переменные из внешней области через environment struct:

```
[fn_ptr:8][env_ptr:8]
            ↓
    [count:8][env[0]:8][env[1]:8]...
```

Переменные захватываются по значению (копия).

## Вызов функции как значения

Функции можно передавать как аргументы:

```elang
fn double(x: i64) -> i64 { x * 2 }
fn apply(f: fn, x: i64) -> i64 { f(x) }

// Функция оборачивается в closure object [fn_ptr, NULL]
let result: i64 = apply(double, 5)  // 10
```

## Calling convention

Все пользовательские функции используют closure convention:

```
rdi = env_ptr (NULL для не-closure функций)
rsi = arg0
rdx = arg1
rcx = arg2
r8  = arg3
r9  = arg4
```

Для struct конструкторов используется стандартный System V ABI:

```
rdi = arg0
rsi = arg1
rdx = arg2
```
