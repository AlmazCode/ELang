# ELang — Язык программирования нового поколения

**Быстрый, статически типизированный, компилируемый системный язык программирования с expression-oriented синтаксисом и pipeline-first подходом.**

ELang компилируется напрямую в x86_64 ассемблер, создавая нативные Linux бинарники с нулевым runtime overhead. Он сочетает производительность C с современными возможностями языков: pattern matching, pipeline операторы, система модулей и обработка ошибок.

---

## Быстрый старт

```bash
# Сборка компилятора
make

# Компиляция и запуск за один шаг
./elc examples/01_hello_world.el

# Или пошагово
./bin/elc -o output.asm examples/01_hello_world.el
nasm -f elf64 output.asm -o output.o
ld output.o lib/build/core.o -o output
./output
```

---

## Ключевые особенности

| Особенность | Описание |
|-------------|----------|
| **Статическая типизация** | Типы проверяются во время компиляции, вывод типов |
| **Expression-oriented** | `if`, `match` возвращают значения |
| **Pipeline оператор `\|>`** | Цепочки вызовов функций: `x \|> f() \|> g()` |
| **Result\<T, E\>** | Типобезопасная обработка ошибок |
| **Система модулей** | `using "math"`, `math::add()`, `export fn` |
| **Pattern matching** | `match` с `Ok`/`Err` для обработки ошибок |
| **Обработка ошибок** | `?` operator, `catch` блоки, `panic`, `assert` |
| **`defer`** | Автоматическая очистка ресурсов при выходе из scope |
| **Массивы** | Литералы `[1, 2, 3]`, доступ `arr[i]`, длина `arr.len` |
| **Constant folding** | Вычисление константных выражений во время компиляции |
| **Нулевой runtime** | Нет сборщика мусора, нет VM, нет скрытых аллокаций |
| **Стандартная библиотека** | Core (auto) + std (using) — print, str, sys, assert |
| **Линковщик резолвит символы** | Компилятор не знает о библиотеках, линковщик находит функции |

---

## Примеры программ

Каждый пример демонстрирует конкретную фичу языка:

| Пример | Описание |
|--------|----------|
| [`01_hello_world.el`](examples/01_hello_world.el) | Hello World — базовый запуск |
| [`02_variables.el`](examples/02_variables.el) | Переменные: `let`, `let mut`, типы |
| [`03_functions.el`](examples/03_functions.el) | Функции: неявный возврат, `=>`, рекурсия |
| [`04_control_flow.el`](examples/04_control_flow.el) | Управление потоком: `if/else if`, `while`, `for` |
| [`05_error_handling.el`](examples/05_error_handling.el) | Ошибки: `Result<T,E>`, `?`, `catch`, `match` |
| [`06_arrays.el`](examples/06_arrays.el) | Массивы: литералы, индексы, `.len` |
| [`07_pipeline.el`](examples/07_pipeline.el) | Pipeline оператор `|>` |
| [`08_defer.el`](examples/08_defer.el) | Defer — автоматическая очистка |
| [`09_tuples.el`](examples/09_tuples.el) | Кортежи и деструктуризация |
| [`10_modules.el`](examples/10_modules.el) | Модули: `using`, `export` |
| [`11_structs_enums.el`](examples/11_structs_enums.el) | Структуры и перечисления |
| [`12_expressions.el`](examples/12_expressions.el) | Выражения: всё возвращает значение |
| [`13_fizzbuzz.el`](examples/13_fizzbuzz.el) | FizzBuzz — все фичи вместе |

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
| `Result<T, E>` | Результат: `Ok(T)` или `Err(E)` |

---

## Синтаксис языка

### Переменные

`let` создаёт immutable переменную, `let mut` — mutable:

```elang
let x = 42          // нельзя изменить
let mut i = 0       // можно изменить
i = i + 1           // OK

// x = 100          // ОШИБКА: нельзя изменить immutable переменную
```

**Правило:** immutable по умолчанию защищает от случайной перезаписи. Используйте `let mut` только когда переменная действительно должна изменяться.

### Функции

#### Неявный возврат

Последнее выражение в теле функции автоматически возвращается:

```elang
fn add(a: i64, b: i64) -> i64 {
    a + b  // автоматически возвращается
}

fn abs(x: i64) -> i64 {
    if x < 0 {
        0 - x  // неявный возврат из ветки
    } else {
        x      // неявный возврат из ветки
    }
}
```

#### Явный возврат

Используйте `return` для раннего выхода:

```elang
fn divide(a: i64, b: i64) -> Result<i64, i64> {
    if b == 0 {
        return Err(0)  // ранний выход
    }
    Ok(a / b)  // неявный возврат
}
```

#### Однострочные функции

Синтаксис `=>` для однострочных тел:

```elang
fn double(x: i64) -> i64 => x * 2

fn square(x: i64) -> i64 => x * x

fn abs(x: i64) -> i64 =>
    if x < 0 { 0 - x } else { x }
```

#### Рекурсия

```elang
fn factorial(n: i64) -> i64 {
    if n <= 1 {
        1
    } else {
        n * factorial(n - 1)
    }
}

fn fibonacci(n: i64) -> i64 {
    if n <= 1 { n } else { fibonacci(n - 1) + fibonacci(n - 2) }
}
```

### Управление потоком

#### if/else if/else

`if` — expression, возвращает значение:

```elang
let temp = 25

let weather = if temp > 30 {
    "hot"
} else if temp > 20 {
    "warm"
} else if temp > 10 {
    "cool"
} else {
    "cold"
}

// Использование как statement
if x > 0 {
    print_str("positive\n")
}
```

#### while

```elang
let mut i = 0
while i < 10 {
    print_int(i)
    i = i + 1
}
```

#### for (диапазон)

```elang
// Цикл от 0 до 9
for i in 0..10 {
    print_int(i)
}

// Однострочное тело
for i in 0..5 => print_int(i)

// Таблица умножения
for i in 1..4 {
    for j in 1..4 {
        print_int(i * j)
        print_str("  ")
    }
    print_str("\n")
}
```

### Pattern Matching

#### match по значениям

```elang
let code = 2
let message = match code {
    1 => "one"
    2 => "two"
    3 => "three"
    _ => "other"  // wildcard
}
```

#### match по Result

```elang
fn divide(a: i64, b: i64) -> Result<i64, i64> {
    if b == 0 { return Err(0) }
    Ok(a / b)
}

let result = match divide(100, 7) {
    Ok(val) => val
    Err(_) => 0
}
```

### Обработка ошибок

#### Result\<T, E\>

Типобезопасная обработка ошибок через `Result`:

```elang
fn divide(a: i64, b: i64) -> Result<i64, i64> {
    if b == 0 {
        return Err(0)  // ошибка: деление на ноль
    }
    Ok(a / b)  // успех
}
```

#### Оператор `?` — пропагация ошибок

Автоматически возвращает `Err` наверх по стеку вызовов:

```elang
fn double_divide(a: i64, b: i64) -> Result<i64, i64> {
    let result = divide(a, b)?  // если Err — вернуть сразу
    Ok(result * 2)
}

fn process(x: i64) -> Result<i64, i64> {
    let a = divide(x, 2)?      // если Err — вернуть Err
    let b = double_divide(a, 3)?  // если Err — вернуть Err
    Ok(a + b)
}
```

#### Блок `catch` — инлайн обработка

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

// Проверка условия (panic при false)
assert(x > 0, "x должен быть положительным")
```

### Defer

Автоматическая очистка ресурсов при выходе из scope (в обратном порядке):

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

**Порядок выполнения:**

```elang
print_str("1. Start\n")
defer print_str("4. First defer (выполняется последним)\n")
defer print_str("3. Second defer\n")
defer print_str("2. Third defer (выполняется первым)\n")
print_str("5. End\n")

// Вывод:
// 1. Start
// 5. End
// 2. Third defer
// 3. Second defer
// 4. First defer
```

### Массивы

#### Создание и доступ

```elang
// Создание массива
let arr = [10, 20, 30, 40, 50]

// Доступ по индексу (с 0)
let first = arr[0]   // 10
let third = arr[2]   // 30
let last = arr[4]    // 50

// Длина массива
let length = arr.len  // 5
```

#### Массив с выражениями

```elang
let x = 5
let arr2 = [x, x + 1, x * 2]  // [5, 6, 10]
```

#### Обход массива

```elang
// Через while
let mut sum = 0
let mut i = 0
while i < arr.len {
    sum = sum + arr[i]
    i = i + 1
}

// Через for
for i in 0..arr.len {
    print_int(arr[i])
}
```

**Ограничения v1:**
- Максимум 32 элемента (стековый фрейм)
- Все элементы одного типа (выводится из первого элемента)
- Элементы хранятся как `i64` (8 байт каждый)

### Pipeline оператор `|>`

Передаёт левое значение как **первый** аргумент правой функции:

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

**Примеры:**

```elang
// Простой pipeline
5 |> double() |> print_int()        // double(5) = 10

// Цепочка арифметических операций
5 |> add(5) |> double() |> add(1)   // ((5+5)*2)+1 = 21

// Pipeline с модулями
2 |> math::power(10) |> print_int()  // 2^10 = 1024
```

### Tuple unpacking

```elang
// Возврат нескольких значений
fn divmod(a: i64, b: i64) -> (i64, i64) {
    (a / b, a % b)
}

// Деструктуризация
let (quotient, remainder) = divmod(17, 5)
// quotient = 3, remainder = 2

// Обмен без временной переменной
let (a, b) = (b, a)
```

### Модули

#### Экспорт функций

```elang
// math.el
export fn add(a: i64, b: i64) -> i64 { a + b }
export fn multiply(a: i64, b: i64) -> i64 { a * b }
export fn power(base: i64, exp: i64) -> i64 {
    let mut result = 1
    let mut i = 0
    while i < exp {
        result = result * base
        i = i + 1
    }
    result
}
```

#### Импорт и использование

```elang
// main.el
using "math"

fn main() -> void {
    let x = math::add(1, 2)        // 3
    let y = math::multiply(3, 4)   // 12
    let z = math::power(2, 10)     // 1024

    // Pipeline с модулями
    10 |> math::power(2) |> print_int()  // 1024
}
```

### Структуры и перечисления

```elang
// Объявление структуры
struct Point {
    x: f64
    y: f64
}

struct Rectangle {
    width: f64
    height: f64
}

// Объявление перечисления
enum Color {
    Red
    Green
    Blue
}

enum Direction {
    North
    South
    East
    West
}
```

> **Примечание:** Создание экземпляров и методы структур запланированы в дорожной карте.

---

## Стандартная библиотека

### Архитектура

```
lib/
├── core.asm     → core.o     (всегда подключается)
├── std.asm      → std.o      (по запросу: using "std")
└── math.asm     → math.o     (по запросу: using "math")
```

### Core функции (автоматически, без импорта)

| Функция | Описание |
|---------|----------|
| `print_str(s)` | Вывод строки |
| `print_int(n)` | Вывод числа (отрицательные тоже) |
| `print_hex(n)` | Вывод hex |
| `str_len(s)` | Длина строки |
| `sys_open/close/read/write` | Файловые операции |
| `sys_getpid/exit` | Процесс |
| `panic(msg)` | Аварийное завершение |
| `assert(cond, msg)` | Проверка условия |

### Расширенные функции (требуют импорт)

```elang
using "std"    // str_cmp, str_dup, str_cat, read_input, sys_brk
using "math"   // add, multiply, power, abs, min, max, rand...
```

| Модуль | Функции |
|--------|---------|
| **std** | `str_cmp`, `str_dup`, `str_cat`, `read_input`, `sys_brk` |
| **math** | `add`, `subtract`, `multiply`, `divide`, `modulo`, `i64_abs`, `negate`, `min`, `max`, `clamp`, `power`, `isqrt`, `rand`, `rand_range`, `srand`, `bitwise_and/or/xor/not`, `shift_left/right`, `sin_approx`, `cos_approx` |

### Поведение линковщика

| Сценарий | Результат |
|----------|-----------|
| `print_str()` без import | ✓ Работает (core.o) |
| `str_cmp()` без import | ✗ undefined reference |
| `str_cmp()` с `using "std"` | ✓ Работает (std.o подключён) |

---

## Опции компилятора

```
elc [опции] <файл.el>
  -o <файл>    Файл ассемблера (по умолчанию: output.asm)
  -check        Только проверка типов (без генерации кода)
  -fold         Только constant folding (без генерации кода)
  -t            Вывод токенов
  -a            Вывод AST
  -l            Режим библиотеки (без _start)
  -h            Справка
```

**Примеры:**

```bash
# Проверка типов
./bin/elc -check examples/13_fizzbuzz.el

# Вывод токенов
./bin/elc -t examples/01_hello_world.el

# Вывод AST
./bin/elc -a examples/03_functions.el

# Компиляция в файл
./bin/elc -o output.asm examples/13_fizzbuzz.el
```

---

## Структура проекта

```
ELang/
├── src/                      # Исходники компилятора (C)
│   ├── lexer.c              # Токенизатор
│   ├── parser.c             # Синтаксический анализатор
│   ├── semantics.h/.c       # Проверка типов и вывод типов
│   ├── codegen.c            # Генерация x86_64 ассемблера
│   ├── ast.h/.c             # Управление AST узлами
│   ├── token.h/.c           # Определения токенов
│   └── main.c               # CLI и pipeline компиляции
├── include/                  # Заголовочные файлы
├── lib/                      # Стандартная библиотека (x86_64 ассемблер)
│   ├── core.asm             # Core функции (всегда подключаются)
│   ├── std.asm              # Extended: str_cmp, str_dup, str_cat, read_input
│   ├── math.asm             # Extended: add, multiply, power, abs, min, max, rand...
│   └── build/               # Скомпилированные .o файлы
├── examples/                 # Примеры программ (с подробными описаниями)
│   ├── 01_hello_world.el    # Hello World — базовый запуск
│   ├── 02_variables.el      # Переменные: let, let mut, типы
│   ├── 03_functions.el      # Функции: неявный возврат, =>, рекурсия
│   ├── 04_control_flow.el   # Управление потоком: if, while, for
│   ├── 05_error_handling.el # Ошибки: Result, ?, catch, match
│   ├── 06_arrays.el         # Массивы: литералы, индексы, .len
│   ├── 07_pipeline.el       # Pipeline оператор |>
│   ├── 08_defer.el          # Defer — автоматическая очистка
│   ├── 09_tuples.el         # Кортежи и деструктуризация
│   ├── 10_modules.el        # Модули: using, export
│   ├── 11_structs_enums.el  # Структуры и перечисления
│   ├── 12_expressions.el    # Выражения: всё возвращает значение
│   └── 13_fizzbuzz.el       # FizzBuzz — все фичи вместе
├── test/                     # Тестовые программы
│   ├── hello.el             # Hello World + базовые фичи
│   ├── arrays.el            # Тесты массивов
│   ├── result.el            # Тест Result<T, E>
│   └── new_syntax.el        # Тесты pipeline, defer, match
├── DESIGN.md                 # Документация дизайна языка
├── elc                       # Скрипт компилятора (обёртка)
└── Makefile
```

---

## Быстрый пример: FizzBuzz

```elang
using "math"

fn is_divisible(n: i64, d: i64) -> i64 {
    if n % d == 0 { 1 } else { 0 }
}

fn fizzbuzz(n: i64) -> void {
    let mut i = 1
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

fn main() -> void {
    fizzbuzz(30)

    // Pipeline
    2 |> math::power(10) |> print_int()

    // Match
    let result = match divide(100, 7) {
        Ok(val) => val
        Err(_) => 0
    }
}

fn divide(a: i64, b: i64) -> Result<i64, i64> {
    if b == 0 { return Err(0) }
    Ok(a / b)
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
make test     # Запуск тестов
make run      # Сборка и запуск теста
```

---

## Дорожная карта

- [x] **Arrays** — литералы `[1, 2, 3]`, доступ `arr[i]`, длина `arr.len`
- [x] **Result\<T, E\>** — типобезопасная обработка ошибок
- [x] **Pipeline `|>`** — цепочки вызовов функций
- [x] **Expression-oriented `if`** — `if` возвращает значение
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
