# Обработка ошибок

ELang использует тип `Result<T, E>` для типобезопасной обработки ошибок.

## Result\<T, E\>

### Конструкторы

```elang
Ok(value)    // успех
Err(value)   // ошибка
```

### Кодировка в памяти

```
Ok(val)  = val << 1 | 0   (бит 0 = 0)
Err(val) = val << 1 | 1   (бит 0 = 1)
```

### Пример

```elang
fn divide(a: i64, b: i64) -> Result<i64, i64> {
    if b == 0 { return Err(0) }
    return Ok(a / b)
}
```

## Match по Result

```elang
let result: i64 = match divide(100, 7) {
    Ok(val) => val
    Err(_) => 0
}
// result = 14

let err_result: i64 = match divide(100, 0) {
    Ok(val) => val
    Err(_) => 0
}
// err_result = 0
```

## Оператор `?` — пропагация ошибок

`?` проверяет Result. Если `Err` — выполняет defer и возвращает ошибку. Если `Ok` — извлекает значение.

```elang
fn double_divide(a: i64, b: i64) -> Result<i64, i64> {
    let result: i64 = divide(a, b)?   // если Err — вернуть Err
    Ok(result * 2)
}

fn process(x: i64) -> Result<i64, i64> {
    let a: i64 = divide(x, 2)?
    let b: i64 = double_divide(a, 3)?
    Ok(a + b)
}
```

### Использование с match

```elang
let proc_result: i64 = match process(100) {
    Ok(val) => val
    Err(_) => 0
}
```

### Defer + ?

При `?` на Err сначала выполняются все `defer` в текущем scope:

```elang
fn read_config(path: string) -> Result<i64, i64> {
    let fd: i64 = sys_open(path, 0, 0)?
    defer sys_close(fd)
    // если open failed → Err, но sys_close(fd) выполнится
    Ok(fd)
}
```

## Catch — инлайн обработка

`catch` обрабатывает ошибку без выхода из scope:

```elang
// Если Ok — возвращает Ok значение
// Если Err — выполняет catch блок и возвращает его значение

let safe: i64 = divide(10, 0) catch { 0 }
// safe = 0

let good: i64 = divide(10, 2) catch { 0 }
// good = 5
```

### Catch блок

```elang
let value: i64 = risky_operation() catch {
    print_str("operation failed\n")
    0    // значение по умолчанию
}
```

## Panic

Аварийное завершение программы с сообщением:

```elang
panic("unreachable code")

// Без сообщения
panic(nil)
```

Panic вызывает `panic_handler` в runtime, который выводит сообщение в stderr и завершает программу с кодом 1.

## Assert

Проверка условия с аварийным завершением при ошибке:

```elang
assert(x > 0, "x must be positive")
assert(1 > 0, "1 should be > 0")
```

Если условие ложно, выводит `assertion failed: <сообщение>` в stderr и завершает с кодом 2.

## Panic в runtime

```
panic at <file>:<line>: <message>
```

Пример вывода при division by zero:
```
panic at <input>:7: division by zero
```

## Encode/decode Result в match

```elang
// Result кодируется через бит 0
// match проверяет бит 0: 0 = Ok, 1 = Err
// Затем сдвигает вправо на 1 чтобы получить значение

let result: i64 = match divide(100, 7) {
    Ok(val) => val    // val = 100/7 = 14
    Err(_) => 0
}
```
