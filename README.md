# ELang — Язык программирования нового поколения

**Быстрый, статически типизированный, компилируемый системный язык программирования с expression-oriented синтаксисом и pipeline-first подходом.**

ELang компилируется напрямую в x86_64 ассемблер, создавая нативные Linux бинарники с нулевым runtime overhead. Он сочетает производительность C с современными возможностями языков: pattern matching, pipeline операторы, система модулей и обработка ошибок.

---

## Быстрый старт

```bash
# Сборка компилятора
make

# Компиляция и запуск за один шаг
./elc examples/fizzbuzz.el

# Или пошагово
./bin/elc -o output.asm examples/fizzbuzz.el
nasm -f elf64 output.asm -o output.o
ld output.o lib/build/syscalls.o -o output
./output
```

---

## Ключевые особенности

| Особенность | Описание |
|-------------|----------|
| **Статическая типизация** | Типы проверяются во время компиляции, вывод типов |
| **Expression-oriented** | `if`, `match`, `when` возвращают значения |
| **Pipeline оператор `\|>`** | Цепочки вызовов функций: `x \|> f() \|> g()` |
| **Система модулей** | `using "math"`, `math::add()`, `export fn` |
| **Pattern matching** | `match` с `Ok`/`Err` для обработки ошибок |
| **Обработка ошибок** | `?` operator, `catch` блоки, `panic`, `assert` |
| **`defer`** | Автоматическая очистка ресурсов при выходе из scope |
| **Массивы** | Литералы `[1, 2, 3]`, доступ `arr[i]`, длина `arr.len` |
| **Constant folding** | Вычисление константных выражений во время компиляции |
| **Нулевой runtime** | Нет сборщика мусора, нет VM, нет скрытых аллокаций |

---

## Система типов

### Целочисленные типы

| Тип | Размер | Диапазон |
|-----|--------|----------|
| `i8` | 1 байт | -128 .. 127 |
| `i16` | 2 байта | -32768 .. 32767 |
| `i32` | 4 байта | -2^31 .. 2^31-1 |
| `i64` | 8 байт | -2^63 .. 2^63-1 |
| `u8` | 1 байт | 0 .. 255 |
| `u16` | 2 байта | 0 .. 65535 |
| `u32` | 4 байта | 0 .. 2^32-1 |
| `u64` | 8 байт | 0 .. 2^64-1 |

### Типы с плавающей точкой

| Тип | Размер |
|-----|--------|
| `f32` | 4 байта |
| `f64` | 8 байт |

### Другие типы

| Тип | Описание |
|-----|----------|
| `bool` | Логический (`true` / `false`) |
| `char` | Символ (1 байт) |
| `string` | Строка (указатель + длина) |
| `void` | Отсутствие значения |
| `*T` | Указатель на T |
| `[T; N]` | Массив из N элементов типа T |

---

## Синтаксис языка

### Функции с неявным возвратом

Последнее выражение в теле функции автоматически возвращается:

```elang
// Неявный возврат
fn add(a: i64, b: i64) -> i64 {
    a + b  // автоматически возвращается
}

// Явный возврат (для раннего выхода)
fn divide(a: i64, b: i64) -> i64 {
    if b == 0 { return Err(0) }
    return Ok(a / b)
}

// Однострочные функции
fn double(x: i64) -> i64 => x * 2

fn abs(x: i64) -> i64 =>
    if x < 0 { 0 - x } else { x }
```

### Pipeline оператор `|>`

Передаёт левое значение как последний аргумент правой функции:

```elang
// Цепочки преобразований
let result = input
    |> parse()
    |> validate()
    |> transform()
    |> save()

// Комбинация с обычными вызовами
5 |> add(10) |> print_int()

// С функциями из модулей
10 |> math::power(2) |> print_int()
```

### Pattern Matching

```elang
fn divide(a: i64, b: i64) -> i64 {
    if b == 0 { return Err(0) }
    return Ok(a / b)
}

// Match по Ok/Err
let result = match divide(100, 7) {
    Ok(val) => val
    Err(e) => 0
}

// When expression (expression-oriented if)
let status = when temp > 30 {
    "hot"
} else when temp > 20 {
    "warm"
} else {
    "cold"
}
```

### Обработка ошибок

#### Оператор `?` — пропагация ошибок

Автоматически возвращает `Err` наверх по стеку вызовов:

```elang
fn read_config(path: string) -> i64 {
    let fd = sys_open(path, 0, 0)?  // если Err — вернуть сразу
    defer sys_close(fd)
    // здесь fd содержит файловый дескриптор
    Ok(fd)
}

// Цепочки с обработкой ошибок
fn process(x: i64) -> i64 {
    let a = divide(x, 2)?      // если Err — вернуть Err
    let b = add(a, 10)?        // если Err — вернуть Err
    Ok(b * 3)
}
```

#### Блок `catch` — инлайн обработка ошибок

```elang
// Обработка ошибки прямо на месте
let safe = divide(10, 0) catch { 0 }

// С дефолтным значением
let value = risky_operation() catch { default_value }
```

#### `panic` и `assert`

```elang
// Аварийное завершение с сообщением
panic("недостижимое состояние")

// Проверка условия
assert(x > 0, "x должен быть положительным")
```

### Defer — автоматическая очистка

Выполняется при выходе из scope (в обратном порядке):

```elang
fn read_file(path: string) -> string {
    let fd = sys_open(path, 0, 0)
    defer sys_close(fd)           // закроется последним

    let buf = alloc(4096)
    defer free(buf)               // освободится первым

    sys_read(fd, buf, 4096)
    str_dup(buf)
}
```

### Массивы

Литералы, доступ по индексу, свойство `.len`:

```elang
fn main() -> void {
    // Создание массива
    let arr = [10, 20, 30, 40, 50]

    // Доступ по индексу (с 0)
    let first = arr[0]   // 10
    let third = arr[2]   // 30

    // Длина массива
    let length = arr.len  // 5

    // Массив в цикле
    let sum = 0
    let i = 0
    while i < arr.len {
        sum = sum + arr[i]
        i = i + 1
    }

    // Массив с выражениями
    let x = 5
    let arr2 = [x, x + 1, x * 2]  // [5, 6, 10]
}
```

**Ограничения v1:**
- Максимум 32 элемента (стековый фрейм)
- Все элементы одного типа (выводится из первого элемента)
- Элементы хранятся как `i64` (8 байт каждый)

### Tuple unpacking

```elang
// Возврат нескольких значений
fn divmod(a: i64, b: i64) -> (i64, i64) {
    (a / b, a % b)
}

// Деструктуризация
let (quotient, remainder) = divmod(17, 5)

// Обмен без временной переменной
let (a, b) = (b, a)
```

### Структуры и перечисления

Структуры объявляются, но создание экземпляров и методы ещё не реализованы:

```elang
struct Point {
    x: f64
    y: f64
}

enum Color {
    Red
    Green
    Blue
}
```

> **Примечание:** Методы структур и enum variants с данными запланированы в дорожной карте.

### Модули

```elang
// math.el
export fn add(a: i64, b: i64) -> i64 { a + b }
export fn multiply(a: i64, b: i64) -> i64 { a * b }

// main.el
using "math"

fn main() -> void {
    let x = math::add(1, 2)
    let y = math::multiply(3, 4)
}
```

### Named Arguments

> **Примечание:** Именованные аргументы запланированы, но пока не реализованы.

```elang
fn connect(host: string, port: u16, timeout: u32) -> socket { ... }

// Будущий синтаксис:
let s = connect(port: 8080, host: "localhost", timeout: 5000)

// Сейчас работают только позиционные аргументы
let s = connect("localhost", 8080, 5000)
```

---

## Сравнение: До и После

### До (базовый синтаксис)

```elang
fn main() -> void {
    let x = 42
    let y = 100
    let sum = x + y
    print_str("Sum: ")
    print_int(sum)
    print_str("\n")

    let result = match divide(10, 2) {
        Ok(val) => val
        Err(_) => 0
    }
    print_str("Result: ")
    print_int(result)
    print_str("\n")
    return 0
}
```

### После (новый синтаксис)

```elang
fn main() -> void {
    let sum = 42 |> add(100)
    print_str("Sum: ")
    print_int(sum)
    print_str("\n")

    let result = match divide(10, 2) {
        Ok(val) => val
        Err(e) => 0
    }
    print_str("Result: ")
    print_int(result)
    print_str("\n")
}
```

### Массивы

```elang
fn main() -> void {
    let arr = [10, 20, 30, 40, 50]
    print_str("First: ")
    print_int(arr[0])
    print_str("\n")

    print_str("Length: ")
    print_int(arr.len)
    print_str("\n")

    // Сумма элементов
    let sum = 0
    let i = 0
    while i < arr.len {
        sum = sum + arr[i]
        i = i + 1
    }
    print_str("Sum: ")
    print_int(sum)
    print_str("\n")
}
```

---

## Стандартная библиотека

### Вывод

| Функция | Описание |
|---------|----------|
| `print_str(s)` | Вывод строки |
| `print_int(n)` | Вывод целого числа |
| `print_hex(n)` | Вывод числа в hex формате |

### Строки

| Функция | Описание |
|---------|----------|
| `str_len(s)` | Длина строки |

### Системные вызовы

| Функция | Описание |
|---------|----------|
| `sys_open(path, flags, mode)` | Открытие файла |
| `sys_close(fd)` | Закрытие файла |
| `sys_read(fd, buf, count)` | Чтение из файла |
| `sys_write(fd, buf, count)` | Запись в файл |
| `sys_getpid()` | Получение PID |
| `sys_exit(code)` | Завершение процесса |

### Обработка ошибок

| Функция | Описание |
|---------|----------|
| `panic(msg)` | Аварийное завершение |
| `assert(cond, msg)` | Проверка условия |

---

## Опции компилятора

```
elc [опции] <файл.el>
  -o <файл>    Файл ассемблера (по умолчанию: output.asm)
  -check        Только проверка типов (без генерации кода)
  -t            Вывод токенов
  -a            Вывод AST
  -l            Режим библиотеки (без _start)
  -h            Справка
```

---

## Структура проекта

```
ELang/
├── src/                  # Исходники компилятора (C)
│   ├── lexer.c          # Токенизатор
│   ├── parser.c         # Синтаксический анализатор
│   ├── semantics.c      # Проверка типов и вывод типов
│   ├── codegen.c        # Генерация x86_64 ассемблера
│   ├── ast.c            # Управление AST узлами
│   ├── token.c          # Определения токенов
│   └── main.c           # CLI и pipeline компиляции
├── include/              # Заголовочные файлы
├── lib/                  # Стандартная библиотека (x86_64 ассемблер)
│   └── syscalls.asm     # Обёртки над Linux syscall + panic/assert
├── examples/             # Примеры программ
│   ├── fizzbuzz.el      # FizzBuzz со всеми возможностями
│   ├── errors.el        # Демонстрация обработки ошибок
│   └── math.el          # Математическая библиотека
├── test/                 # Тестовые программы
│   ├── hello.el          # Hello World + базовые фичи
│   ├── arrays.el         # Тесты массивов
│   └── new_syntax.el     # Тесты pipeline, when, defer, match
├── DESIGN.md             # Документация дизайна языка
├── elc                   # Скрипт компилятора
└── Makefile
```

---

## Примеры

### FizzBuzz

```elang
fn fizzbuzz(n: i64) -> void {
    let i = 1
    while i <= n {
        if is_divisible(i, 15) == 1 {
            print_str("FizzBuzz")
        } else if is_divisible(i, 3) == 1 {
            print_str("Fizz")
        } else if is_divisible(i, 5) == 1 {
            print_str("Buzz")
        } else {
            print_int(i)
        }
        print_str(" ")
        i = i + 1
    }
    print_str("\n")
}
```

### Обработка ошибок

```elang
fn divide(a: i64, b: i64) -> i64 {
    if b == 0 { return Err(0) }
    return Ok(a / b)
}

fn double_divide(a: i64, b: i64) -> i64 {
    let result = divide(a, b)?  // пропагация ошибки
    return Ok(result * 2)
}

fn main() -> void {
    // Match
    let result = match divide(100, 7) {
        Ok(val) => val
        Err(e) => 0
    }
    print_str("100 / 7 = ")
    print_int(result)
    print_str("\n")

    // Catch
    let safe = divide(10, 0) catch { 0 }
    print_str("10 / 0 (catch) = ")
    print_int(safe)
    print_str("\n")
}
```

---

## Сборка из исходников

### Требования

- GCC (или Clang)
- NASM (Netwide Assembler)
- Linux x86_64

### Сборка

```bash
make          # Сборка компилятора
make lib      # Сборка стандартной библиотеки
make run      # Сборка и запуск теста
```

---

## Дорожная карта

- [x] **Arrays и срезы** — литералы `[1, 2, 3]`, доступ `arr[i]`, длина `arr.len`
- [ ] Методы структур
- [ ] Enum variants с данными
- [ ] Slices (`arr[1..3]`)
- [ ] Generics
- [ ] Closures / lambdas
- [ ] Traits / интерфейсы
- [ ] Кросс-компиляция
- [ ] Pass оптимизатора
- [ ] Async/await
- [ ] Pattern matching с guard'ами

---

## Лицензия

MIT License
