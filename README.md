# ELang v0.43.0 — Язык программирования нового поколения

**Быстрый, статически типизированный, компилируемый системный язык программирования с expression-oriented синтаксисом и OOP через structs.**

Автор: **AlmazCode**

ELang компилируется напрямую в x86_64 ассемблер, создавая нативные Linux бинарники с нулевым runtime overhead. Он сочетает производительность C с современными возможностями: closures, pipeline, pattern matching, structs как классы, enums с auto methods.

---

## Быстрый старт

```bash
# Сборка компилятора
make

# Компиляция и запуск
./bin/elc examples/01_hello_world.el -o hello.asm
nasm -f elf64 hello.asm -o hello.o
ld hello.o lib/build/core.o -o hello
./hello
```

---

## Ключевые особенности

| Особенность | Описание |
|-------------|----------|
| **Явная типизация** | Все переменные и функции обязаны иметь типы |
| **Struct = Class** | Heap-allocated объекты с методами и полями |
| **Enums** | Простые enum + enum с данными + auto methods (tag/name/count) |
| **impl blocks** | Методы через `impl Type { fn method() {} }` |
| **Closures** | Анонимные функции с захватом переменных |
| **Pipeline `\|>`** | Цепочки вызовов: `x \|> f() \|> g()` |
| **Result\<T, E\>** | Типобезопасная обработка ошибок с `?` operator |
| **Expression-oriented** | `if`, `match` возвращают значения |
| **Массивы** | Heap-allocated, bounds checking, map/filter/reduce |
| **Defer** | Автоматическая очистка ресурсов |
| **Нулевой runtime** | Bump allocator, без GC, без VM |

---

## Система типов

### Обязательная явная типизация

Все переменные и функции обязаны иметь явные типы:

```elang
// ПРАВИЛЬНО:
let x: i64 = 42
let name: string = "hello"
let flag: bool = true

fn add(a: i64, b: i64) -> i64 { a + b }
fn main() -> u8 { return 0 }

// ОШИБКА компиляции:
let x = 42           // Parse error: expected ':'
fn add(a, b) { a+b } // Parse error: expected ':'
```

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

### Другие типы

| Тип | Описание |
|-----|----------|
| `f32`, `f64` | Числа с плавающей точкой |
| `bool` | Логический (`true` / `false`) |
| `char` | Символ (1 байт) |
| `string` | Строка |
| `void` | Отсутствие значения |
| `*T` | Указатель на T |
| `[T]` | Массив элементов типа T |
| `Result<T, E>` | Результат: `Ok(T)` или `Err(E)` |

---

## Struct = Class

Struct в ELang — это heap-allocated объект с полями и методами. Аналог class в других языках.

### Объявление struct

```elang
struct Point {
    x: i64
    y: i64
}

struct Person {
    name: string
    age: i64
}
```

### Создание экземпляров

```elang
let p: Point = Point(1, 2)
let person: Person = Person("Alice", 30)
```

### Доступ к полям

```elang
let p: Point = Point(1, 2)
print_int(p.x)  // → 1
print_int(p.y)  // → 2

p.x = 10        // ОШИБКА: поля immutable
```

### Методы через impl

```elang
struct Point {
    x: i64
    y: i64
}

impl Point {
    fn new(x: i64, y: i64) -> Point {
        Point(x, y)
    }

    fn distance(self: Point, other: Point) -> i64 {
        let dx: i64 = self.x - other.x
        let dy: i64 = self.y - other.y
        dx * dx + dy * dy
    }
}

// Использование
let p1: Point = Point::new(1, 2)
let p2: Point = Point::new(4, 6)
let d: i64 = p1.distance(p2)  // → 25
```

### Память

Struct автоматически выделяется на heap через bump allocator:

```
[refcount:8][field_count:8][field0:8][field1:8]...
             ^                            ^
             header                       data start (returned pointer)
```

- `refcount` — счётчик ссылок для автоматического управления памятью
- Поля доступны через смещения от указателя на данные

---

## Enums

### Простые enums

```elang
enum Color { Red, Green, Blue }
enum Direction { North, South, East, West }
```

### Enum с ручными значениями

```elang
enum Month { Jan = 1, Feb = 2, Mar = 3, Apr = 4 }
```

### Автоматические методы

Каждый enum автоматически получает три метода:

| Метод | Возвращает | Описание |
|-------|-----------|----------|
| `tag()` | `i64` | Индекс варианта (0, 1, 2...) |
| `name()` | `string` | Имя варианта ("Red", "Green"...) |
| `count()` | `i64` | Количество вариантов |

```elang
enum Color { Red, Green, Blue }

let c: i64 = Color::Green
c.tag()      // → 1
c.name()     // → "Green"
Color.count() // → 3
```

### Использование в match

```elang
enum Color { Red, Green, Blue }

let c: i64 = Color::Green
let name: string = c.name()
print_str(name)  // → "Green"
```

---

## Обработка ошибок

### Result\<T, E\>

```elang
fn divide(a: i64, b: i64) -> Result<i64, i64> {
    if b == 0 {
        return Err(0)
    }
    Ok(a / b)
}
```

### Оператор `?` — пропагация ошибок

```elang
fn double_divide(a: i64, b: i64) -> Result<i64, i64> {
    let result: i64 = divide(a, b)?  // если Err — вернуть сразу
    Ok(result * 2)
}

fn process(x: i64) -> Result<i64, i64> {
    let a: i64 = divide(x, 2)?
    let b: i64 = double_divide(a, 3)?
    Ok(a + b)
}
```

### Match по Result

```elang
let result: i64 = match divide(100, 7) {
    Ok(val) => val
    Err(_) => 0
}
// result = 14
```

### Catch — инлайн обработка

```elang
let safe: i64 = divide(10, 0) catch { 0 }
// safe = 0
```

---

## Closures

### Замыкания с захватом переменных

```elang
fn main() -> u8 {
    let y: i64 = 42
    let f = fn x => x + y  // захватывает y из outer scope
    let result: i64 = f(8)
    print_int(result)  // → 50
    return 0
}
```

### Closures в map/filter/reduce

```elang
let arr: [i64] = [1, 2, 3, 4, 5]
let offset: i64 = 10

let mapped: [i64] = map(arr, fn(x: i64) => x + offset)
// mapped = [11, 12, 13, 14, 15]

let total: i64 = reduce(mapped, 0, fn(acc: i64, x: i64) => acc + x)
// total = 65
```

---

## Pipeline оператор `|>`

Передаёт левое значение как **первый** аргумент правой функции:

```elang
// Цепочки преобразований
let result: i64 = 5 |> double() |> add(10) |> print_int()

// С модулями
10 |> math::power(2) |> print_int()

// С массивами
let arr: [i64] = [1, 2, 3, 4, 5]
let sum: i64 = arr |> filter(fn(x: i64) => x > 2) |> reduce(0, fn(acc: i64, x: i64) => acc + x)
```

---

## Управление потоком

### if/else if/else (expression)

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

### while

```elang
let mut i: i64 = 0
while i < 10 {
    print_int(i)
    i = i + 1
}
```

### for (диапазон и массивы)

```elang
// Цикл от 0 до 9
for i: i64 in 0..10 {
    print_int(i)
}

// По элементам массива
for x: i64 in arr {
    print_int(x)
}

// С индексом (enumerate)
for i: i64, x: i64 in arr {
    print_int(i)
    print_str(": ")
    print_int(x)
}
```

---

## Массивы

```elang
// Создание
let arr: [i64] = [10, 20, 30, 40, 50]

// Доступ по индексу (bounds-checked)
let first: i64 = arr[0]   // 10

// Длина
let length: i64 = arr.len  // 5

// Обход
for x: i64 in arr {
    print_int(x)
}

// Pipeline операции
let doubled: [i64] = arr |> map(fn(x: i64) => x * 2)
let big: [i64] = arr |> filter(fn(x: i64) => x > 20)
let sum: i64 = arr |> reduce(0, fn(acc: i64, x: i64) => acc + x)
```

---

## Defer

Автоматическая очистка ресурсов при выходе из scope (в обратном порядке):

```elang
fn read_file(path: string) -> string {
    let fd: i64 = sys_open(path, 0, 0)
    defer sys_close(fd)           // закроется последним

    let buf: i64 = alloc(4096)
    defer free(buf)               // освободится первым

    sys_read(fd, buf, 4096)
    str_dup(buf)
}
```

---

## Модули

### Экспорт функций

```elang
// math.el
export fn add(a: i64, b: i64) -> i64 { a + b }
export fn power(base: i64, exp: i64) -> i64 {
    let mut result: i64 = 1
    let mut i: i64 = 0
    while i < exp {
        result = result * base
        i = i + 1
    }
    result
}
```

### Импорт и использование

```elang
// main.el
using "math"

fn main() -> u8 {
    let x: i64 = math::add(1, 2)
    let y: i64 = math::power(2, 10)
    10 |> math::power(2) |> print_int()
    return 0
}
```

---

## Примеры программ

| Пример | Описание |
|--------|----------|
| [`01_hello_world.el`](examples/01_hello_world.el) | Hello World |
| [`02_variables.el`](examples/02_variables.el) | Переменные с типами |
| [`03_functions.el`](examples/03_functions.el) | Функции: неявный возврат, рекурсия |
| [`04_control_flow.el`](examples/04_control_flow.el) | if/else, while, for |
| [`05_error_handling.el`](examples/05_error_handling.el) | Result, ?, catch, match |
| [`06_arrays.el`](examples/06_arrays.el) | Массивы, индексы, .len |
| [`07_pipeline.el`](examples/07_pipeline.el) | Pipeline оператор \|> |
| [`08_defer.el`](examples/08_defer.el) | Defer — автоматическая очистка |
| [`09_tuples.el`](examples/09_tuples.el) | Кортежи и деструктуризация |
| [`10_modules.el`](examples/10_modules.el) | Модули: using, export |
| [`11_structs_enums.el`](examples/11_structs_enums.el) | Structs, enums, impl blocks |
| [`12_expressions.el`](examples/12_expressions.el) | Выражения: всё возвращает значение |
| [`13_fizzbuzz.el`](examples/13_fizzbuzz.el) | FizzBuzz — все фичи вместе |

---

## Полный пример: OOP

```elang
struct Point {
    x: i64
    y: i64
}

enum Color { Red, Green, Blue }

impl Point {
    fn new(x: i64, y: i64) -> Point {
        Point(x, y)
    }

    fn distance(self: Point, other: Point) -> i64 {
        let dx: i64 = self.x - other.x
        let dy: i64 = self.y - other.y
        dx * dx + dy * dy
    }
}

fn main() -> u8 {
    // Struct
    let p1: Point = Point::new(1, 2)
    let p2: Point = Point::new(4, 6)
    let d: i64 = p1.distance(p2)
    print_int(d)
    print_str("\n")

    // Enum
    let c: i64 = Color::Green
    print_str(c.name())
    print_str("\n")
    print_int(Color.count())
    print_str("\n")

    return 0
}
```

---

## Структура проекта

```
ELang/
├── src/                      # Исходники компилятора (C)
│   ├── lexer.c              # Токенизатор
│   ├── parser.c             # Парсер
│   ├── semantics.c          # Type checker
│   ├── codegen.c            # Генерация x86_64 asm
│   ├── ast.c                # AST узлы
│   ├── token.c              # Токены
│   └── main.c               # CLI
├── include/                  # Заголовочные файлы
├── lib/                      # Стандартная библиотека (asm)
│   ├── core.asm             # Core (всегда)
│   ├── std.asm              # Extended (using "std")
│   ├── math.asm             # Math (using "math")
│   └── build/               # .o файлы
├── examples/                 # 13 примеров программ
├── ROADMAP.md               # Дорожная карта
├── DESIGN.md                # Дизайн языка
└── Makefile
```

---

## Опции компилятора

```
elc [опции] <файл.el>
  -o <файл>    Файл ассемблера (по умолчанию: output.asm)
  -check        Только проверка типов
  -fold         Только constant folding
  -t            Вывод токенов
  -a            Вывод AST
  -l            Режим библиотеки (без _start)
  -h            Справка
```

---

## Сборка

### Требования

- GCC или Clang
- NASM (Netwide Assembler)
- Linux x86_64

### Команды

```bash
make          # Сборка компилятора
make lib      # Сборка стандартной библиотеки
```

---

## Лицензия

MIT License
