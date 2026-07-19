# ELang v0.43.0 Roadmap

Автор: **AlmazCode**

## Текущий статус

Компилятор компилирует programs в x86_64 asm, включая closures с captures, pipeline operations, массивы с bounds checking, map/filter/reduce, if expressions, `fn main() -> u8`. Runtime на чистом asm (bump allocator через brk, без libc).

---

## Приоритет 1: Explicit Typing (неделя 1)

**Философия:** Явная типизация везде. Программист всегда знает какой тип у переменной.

### 1.1 Обязательный тип в `let`
**Проблема:** `let x = 42` — тип неизвестен без inference. Не низкоуровнево.

**Решение:** Требовать `let x: i64 = 42` всегда.

```elang
// ПРАВИЛЬНО:
let x: i64 = 42
let name: string = "hello"
let flag: bool = true

// ОШИБКА компиляции:
let x = 42  // Parse error: expected ':'
```

**Изменения:**
- `parser.c` (parse_let): после имени переменной требовать `TOKEN_COLON`, иначе ошибка
- `parser.c` (parse_for): переменные цикла тоже требуют тип: `for i: i64 in 0..5`
- Убрать inference из `semantics.c` — тип всегда известен из AST

**Сложность:** ~50 строк
**Статус:** 🔲 Не начато

### 1.2 Обязательный тип возврата функций
**Проблема:** Некоторые функции не имеют return type.

**Решение:** Все функции обязаны иметь `-> Type` аннотацию. `void` для функций без возврата.

```elang
// ПРАВИЛЬНО:
fn add(a: i64, b: i64) -> i64 { a + b }
fn print_msg(msg: string) -> void { print_str(msg) }

// ОШИБКА:
fn add(a: i64, b: i64) { a + b }  // Parse error: expected '->'
```

**Сложность:** ~30 строк (уже частично сделано для main)
**Статус:** 🔲 Не начато

### 1.3 Обязательный тип в struct fields
**Проблема:** Уже сделано (`struct Point { x: i64, y: i64 }`), но нужно убедиться что нет исключений.

**Сложность:** ~0 строк (уже есть)
**Статус:** ✅ Готово

---

## Приоритет 2: Struct as Class (неделя 2-3)

**Философия:** Struct = Class. Один тип для всего. Heap-allocated с refcount.

### 2.1 Heap allocation для structs
**Проблема:** `gen_struct_decl` выделяет на стеке — use-after-return баг.

**Решение:** Struct конструктор выделяет на heap через `_bump_alloc`.

**Memory layout** (как массивы):
```
[refcount:8][field_count:8][field0:8][field1:8]...
```

**Изменения:**
- `codegen.c` (gen_struct_decl): заменить `sub rsp` на `_bump_alloc`
- Добавить `struct_create` в `lib/core.asm`: выделение heap + запись полей

**Сложность:** ~80 строк

### 2.2 Field registry в type system
**Проблема:** Нет информации о полях struct для codegen.

**Решение:** Добавить в `type_info_t` реестр полей.

```c
// semantics.h
struct type_info {
    // ... существующие поля ...
    struct {
        char **names;
        int *offsets;      // смещение от начала данных
        type_info_t **types;
        int count;
    } fields;
};
```

**Изменения:**
- `semantics.h`: добавить fields в type_info_t
- `semantics.c`: при парсинге struct заполнять registry
- `semantics.c`: type_equal для struct сравнивает имена

**Сложность:** ~100 строк

### 2.3 Парсер: struct literal
**Проблема:** `Point { x: 1, y: 2 }` не парсится.

**Решение:** Добавить `AST_STRUCT_LITERAL` в `parse_primary`.

**Синтаксис:**
```elang
let p: Point = Point { x: 1, y: 2 }
let origin: Point = Point { x: 0, y: 0 }
```

**Изменения:**
- `parser.c`: после IDENT, если `{` → struct literal
- `codegen.c`: генерация heap allocation + запись полей

**Сложность:** ~80 строк

### 2.4 Парсер: dot access (общий)
**Проблема:** Только `.len` работает. Нет `p.x`.

**Решение:** Заменить хардкод `.len` на общий dot access.

**Синтаксис:**
```elang
let p: Point = Point { x: 1, y: 2 }
let val: i64 = p.x    // field access
let len: i64 = arr.len // array length (оставить)
```

**Изменения:**
- `parser.c`: обобщить dot access → `AST_MEMBER`
- `ast.h`: добавить `as.member` в union
- `codegen.c`: field access через offset из registry

**Сложность:** ~60 строк

### 2.5 Codegen: struct literal + field access
**Изменения:**
- `codegen.c`: struct literal → `_bump_alloc` + store fields
- `codegen.c`: `p.x` → `mov rax, [obj]; mov rax, [rax+offset]`

**Сложность:** ~60 строк

### 2.6 Парсер: method call через dot
**Проблема:** `p.distance(other)` не работает.

**Решение:** Dot вызов → функция с self параметром.

**Синтаксис:**
```elang
fn distance(self: Point, other: Point) -> f64 {
    let dx: i64 = self.x - other.x
    let dy: i64 = self.y - other.y
    dx * dx + dy * dy
}

let p1: Point = Point { x: 1, y: 2 }
let p2: Point = Point { x: 4, y: 6 }
let d: f64 = p1.distance(p2)  // → distance(p1, p2)
```

**Изменения:**
- `parser.c`: `expr.method(args)` → AST_CALL с self как первый arg
- `codegen.c`: передавать self как первый аргумент

**Сложность:** ~60 строк

---

## Приоритет 3: Enums (неделя 4-5)

**Философия:** Enums = algebraic types + auto-introspection.

### 3.1 Парсер: enum declaration
**Проблема:** `parse_enum` не существует.

**Решение:** Добавить `parse_enum` в парсер.

**Синтаксис:**
```elang
enum Color { Red, Green, Blue }
enum Option { Some(i64), None }
enum Month { Jan = 1, Feb = 2, Mar = 3 }
```

**Три формы variants:**
- `Name` — simple (auto ordinal 0, 1, 2...)
- `Name(Type)` — with data
- `Name = value` — manual ordinal

**Изменения:**
- `parser.c`: добавить `parse_enum`
- `parser.c`: добавить `case TOKEN_ENUM` в `parse_stmt`

**Сложность:** ~80 строк

### 3.2 Codegen: simple enum
**Решение:** Tag encoding — variant index в младших битах.

**Memory layout:**
```
[refcount:8][tag:8]
```

**Codegen:**
```asm
; Color::Red
call with_capacity       ; rax = ptr
mov qword [rax-16], 2   ; field_count = 2
mov qword [rax+0], 0    ; tag = 0 (Red)
```

**Сложность:** ~60 строк

### 3.3 Автоматические методы
**Решение:** Каждый enum получает бесплатно:

| Метод | Возвращает | Описание |
|-------|-----------|----------|
| `tag()` | `u64` | Индекс варианта |
| `name()` | `*const char` | Имя варианта |
| `count()` | `u64` | Количество вариантов |

**Пример:**
```elang
enum Color { Red, Green, Blue }

let c: Color = Red
c.tag()       // → 0
c.name()      // → "Red"
Color.count() // → 3
```

**Codegen:** Генерируемые функции + name table в .data

**Сложность:** ~100 строк

### 3.4 Enum with data (tagged union)
**Синтаксис:**
```elang
enum Option { Some(i64), None }

let val: Option = Some(42)
match val {
    Some(x) => print_int(x),
    None => print_str("nothing\n"),
}
```

**Memory layout:**
```
[refcount:8][tag:8][data:8]
```

**Изменения:**
- `codegen.c`: match по enum variants (проверка tag + извлечение data)
- `parser.c`: enum literal `Option::Some(42)`

**Сложность:** ~80 строк

### 3.5 Парсер: enum literal
**Синтаксис:**
```elang
let c: Color = Color::Red
let opt: Option = Option::Some(42)
let none: Option = Option::None
```

**Изменения:**
- `parser.c`: `Type::Variant` → `AST_ENUM_LITERAL`
- `codegen.c`: генерация enum object

**Сложность:** ~60 строк

---

## Приоритет 4: impl blocks (неделя 6)

**Философия:** Методы через impl, как в Rust.

### 4.1 Парсер: impl declaration
**Синтаксис:**
```elang
impl Point {
    fn new(x: i64, y: i64) -> Point {
        Point { x: x, y: y }
    }

    fn distance(self: Point, other: Point) -> f64 {
        let dx: i64 = self.x - other.x
        let dy: i64 = self.y - other.y
        dx * dx + dy * dy
    }
}

impl Color {
    fn is_primary(self: Color) -> bool {
        self == Red || self == Blue
    }
}
```

**Изменения:**
- `parser.c`: добавить `parse_impl`
- `ast.h`: добавить `AST_IMPL_DECL`

**Сложность:** ~60 строк

### 4.2 Codegen: impl methods
**Решение:** Методы → функции с префиксом типа.

```asm
; Point::new → _Point_new
; Point::distance → _Point_distance
```

**Вызов:**
```elang
let p: Point = Point::new(1, 2)    // → call _Point_new(1, 2)
let d: f64 = p.distance(other)     // → call _Point_distance(p, other)
```

**Сложность:** ~60 строк

### 4.3 Связь методов с типами
**Изменения:**
- `semantics.c`: регистрировать methods в struct type info
- `semantics.c`: проверка self типа при вызове метода

**Сложность:** ~40 строк

---

## Приоритет 5: Quality (месяц 2+)

### 5.1 Slices
```elang
let slice: [i64] = arr[1..3]
```
**Сложность:** ~80 строк

### 5.2 Pattern matching с guard'ами
```elang
match x {
    n if n > 0 => "positive",
    _ => "negative",
}
```
**Сложность:** ~40 строк

### 5.3 Named arguments
```elang
connect(host: "x", port: 8080)
```
**Сложность:** ~60 строк

---

## Приоритет 6: Advanced (месяц 3+)

### 6.1 Generics
```elang
fn first<T>(arr: [T; N]) -> T { arr[0] }
```
**Сложность:** ~400 строк

### 6.2 Traits
```elang
trait Printable {
    fn print(self)
}
```
**Сложность:** ~500 строк

---

## Порядок реализации

```
Этап 1: Explicit Typing (1.1, 1.2)
    ↓
Этап 2: Struct Foundation (2.1, 2.2, 2.3, 2.4, 2.5)
    ↓
Этап 3: Methods (2.6)
    ↓
Этап 4: Enums (3.1, 3.2, 3.3, 3.4, 3.5)
    ↓
Этап 5: impl blocks (4.1, 4.2, 4.3)
    ↓
Этап 6: Quality features (5.1, 5.2, 5.3)
```

---

## Оценка сложности

| Этап | Строк | Зависимости |
|------|-------|-------------|
| 1. Explicit Typing | ~80 | — |
| 2. Struct Foundation | ~300 | heap allocator (есть) |
| 3. Methods | ~60 | Struct Foundation |
| 4. Enums | ~380 | Struct Foundation |
| 5. impl blocks | ~160 | Methods |
| 6. Quality | ~180 | — |
| **Итого** | **~1160** | |

---

## Пример использования (финальный вид)

```elang
// === Explicit typing ===
let x: i64 = 42
let name: string = "hello"

// === Struct as Class ===
struct Point {
    x: i64
    y: i64
}

impl Point {
    fn new(x: i64, y: i64) -> Point {
        Point { x: x, y: y }
    }

    fn distance(self: Point, other: Point) -> f64 {
        let dx: i64 = self.x - other.x
        let dy: i64 = self.y - other.y
        dx * dx + dy * dy
    }
}

let p1: Point = Point::new(1, 2)
let p2: Point = Point::new(4, 6)
let d: f64 = p1.distance(p2)

// === Enums ===
enum Color { Red, Green, Blue }
enum Option { Some(i64), None }

impl Color {
    fn is_primary(self: Color) -> bool {
        self == Red || self == Blue
    }
}

let c: Color = Red
c.tag()       // 0
c.name()      // "Red"
Color.count() // 3

// === Enum with data ===
let opt: Option = Option::Some(42)
match opt {
    Some(x) => print_int(x),
    None => print_str("nothing\n"),
}
```

---

## Технический долг

### Must-fix
- [x] Closure captures (env struct) ✅ 2026-07-19
- [x] Error recovery (has_error flag) ✅ 2026-07-19
- [x] `fn main() -> u8` + `return 0` ✅ 2026-07-19
- [x] reduce/filter rcx clobbering bug ✅ 2026-07-19
- [x] If expressions ✅ 2026-07-19
- [x] `?` operator type unwrap bug ✅ 2026-07-19
- [x] Struct heap allocation ✅ 2026-07-19
- [x] Enum declaration + auto methods ✅ 2026-07-19
- [x] Method dispatch (obj.method()) ✅ 2026-07-19
- [x] impl blocks ✅ 2026-07-19
- [x] Match expression type inference ✅ 2026-07-19

### Should-fix
- [ ] RBX usage в function bodies (callee-saved violation)
- [ ] Label patching в codegen (sub rsp placeholder)
- [x] examples/11_structs_enums.el parse error ✅ 2026-07-19

---

## Архитектурные решения (утверждённые)

### 1. Struct = Class
**Решение:** Struct — единственный тип объектов. Heap-allocated с refcount. Методы через `impl`. Без наследования.

### 2. Explicit typing
**Решение:** `let x: Type = value` обязательно. Функции обязаны иметь return type. Inference запрещён.

### 3. Memory model
**Решение:** Bump allocator + refcount. Struct/enum на heap. Watermark для автоматической очистки.

### 4. Enum encoding
**Решение:** Simple enum — tag в [ptr+0]. Enum with data — tag + data. Auto-generated tag/name/count methods.

### 5. Method dispatch
**Решение:** Статический. `obj.method(args)` → `method(obj, args)`. Без vtable, без dynamic dispatch.
