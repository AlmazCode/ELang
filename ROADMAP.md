# ELang v0.42.0 Roadmap

Автор: **AlmazCode**

## Текущий статус

Компилятор компилирует programs в x86_64 asm, включая closures, pipeline operations, массивы с bounds checking, map/filter/reduce. Runtime на чистом asm (bump allocator через brk, без libc).

---

## Приоритет 1: Stabilize (неделя 1-2)

### 1.1 Closures: captures
**Проблема:** `fn x => x + y` не работает если `y` из outer scope. `env_ptr` всегда NULL.

**Решение:** Environment struct на heap:
```
Header: [count:8][env[0]:8][env[1]:8]...
```
- При компиляции closure: определить какие переменные захватываются
- Сгенерировать env struct: `lea rax, [y_value]; mov [env + 0], rax`
- Closure object: `[fn_ptr, env_ptr]`
- Closure function: `rdi = env_ptr`, читает captures через `[rdi + offset]`

**Сложность:** ~150 строк (codegen + parser)
**Зависимости:** heap allocator (уже есть)

### 1.2 Struct methods
**Проблема:** `struct Point { x, y }` объявлен, но нет способа создать экземпляр или вызвать метод.

**Решение:**
```elang
struct Point { x: i64, y: i64 }

fn distance(self: Point, other: Point) -> f64 {
    let dx = self.x - other.x
    let dy = self.y - other.y
    // ...
}

let p = Point { x: 1, y: 2 }
p.distance(Point { x: 4, y: 6 })
```

- Синтаксис: `Type { field: value, ... }`
- Методы: `fn method(self: Type, ...)` — self передаётся как первый аргумент
- Авто-доступ: `self.x` через смещение в структуре

**Сложность:** ~100 строк (parser + codegen)

### 1.3 Error recovery
**Проблема:** `exit(1)` в codegen при ошибке. Компилятор падает вместо graceful degradation.

**Решение:** Добавить `cg->has_error` флаг, проверять перед codegen. При ошибке — вывести все ошибки и выйти с кодом 1.

**Сложность:** ~30 строк

---

## Приоритет 2: Practical (неделя 3-4)

### 2.1 Enum variants с данными
**Проблема:** `enum Option { Some(i64), None }` — варианты не могут хранить данные.

**Решение:**
```elang
enum Option {
    Some(i64)
    None
}

match value {
    Some(x) => print_int(x),
    None => print_str("nothing\n"),
}
```

- Tag encoding: variant index в младших битах, данные в старших
- Match: проверка tag, извлечение данных

**Сложность:** ~120 строк (codegen + semantics)

### 2.2 Slices
**Проблема:** `arr[1..3]` должен быть view, не copy.

**Решение:**
```elang
let slice = arr[1..3]  // view на [arr+8, arr+16)
slice[0]  // == arr[1]
slice.len // == 2
```

- Slice struct: `[ptr:8][len:8]`
- `arr[1..3]` создаёт slice struct
- Bounds checking на read/write

**Сложность:** ~80 строк

### 2.3 Watermark allocator improvements
**Проблема:** `array_free` — no-op. Память не освобождается.

**Решение:**
- При `return` из функции: откат brk если нетescaping arrays
- `free(arr)` — decrement refcount, если 0 → откат brk
- Tracking escaping arrays через refcount > 1

**Сложность:** ~100 строк

---

## Приоритет 3: Quality (месяц 2)

### 3.1 Register allocator
**Проблема:** Все register assignments захардкожены. `rbx` используется как scratch.

**Решение:** Linear scan register allocator:
- Определить live ranges для каждого value
- Назначить регистры (rdi-r15, rbx если preserved)
- Spill на стек если регистров не хватает

**Сложность:** ~300 строк

### 3.2 Type-aware codegen
**Проблема:** Массивы — просто `i64*`. Нет различия между `[i64; 3]` и `[i64; 5]`.

**Решение:** Пробросить type info через AST → codegen. Генерировать bounds check с правильным сравнением.

**Сложность:** ~80 строк

### 3.3 Named arguments
**Проблема:** `connect(host: "x", port: 8080)` — парсер не поддерживает.

**Решение:**
- Парсер: `[name: expr, ...]` syntax
- Codegen: rearrange args по имени

**Сложность:** ~60 строк

---

## Приоритет 4: Advanced (месяц 3+)

### 4.1 Generics
```elang
fn first<T>(arr: [T; N]) -> T { arr[0] }
```

**Подход:** Monomorphization (как C++ template). Компилятор генерирует отдельную копию для каждого набора типов.

**Сложность:** ~400 строк

### 4.2 Traits
```elang
trait Printable {
    fn print(self)
}

impl Printable for i64 {
    fn print(self) { print_int(self) }
}
```

**Подход:** Vtable-based dynamic dispatch или monomorphization.

**Сложность:** ~500 строк

### 4.3 Pattern matching с guard'ами
```elang
match x {
    n if n > 0 => "positive",
    0 => "zero",
    _ => "negative",
}
```

**Сложность:** ~40 строк

---

## Технический долг

### Must-fix
- [ ] Closure captures (env struct)
- [ ] Error recovery (has_error flag)
- [ ] Struct methods + instances
- [ ] `exit(1)` → graceful error в codegen

### Should-fix
- [ ] RBX usage в function bodies (callee-saved violation)
- [ ] Magic numbers → constants (частично сделано)
- [ ] gen_stmt decomposition (сделано)
- [ ] Buffer truncation detection

### Nice-to-have
- [ ] Separate files: codegen_expr.c, codegen_stmt.c
- [ ] SSA form
- [ ] Virtual registers
- [ ] Instruction scheduling
- [ ] Dead code elimination

---

## Архитектурные решения (открытые)

### 1. Calling convention
**Текущий:** Closure convention для ВСЕХ функций (rdi=env, rsi=arg0).
**Альтернатива:** Разные conventions для user/extern функций.
**Рекомендация:** Оставить closure convention — проще, нет dispatch overhead.

### 2. Memory model
**Текущий:** Bump allocator + refcount. `free` = no-op.
**Альтернатива:** Mark-and-sweep GC.
**Рекомендация:** Bump + watermark (restore on function exit) + refcount для escaping arrays.

### 3. Type system
**Текущий:** Inference only. Нет explicit type annotations в кодогене.
**Альтернатива:** Full type info через AST.
**Рекомендация:** Добавить `type_info_t *` в каждый AST node (уже есть `typed` поле). Использовать в codegen для bounds check, array ops.

### 4. Error handling model
**Текущий:** Result<T,E> через tagged union (bit 0). Panic через syscall.
**Альтернатива:** setjmp/longjmp для unwind.
**Рекомендация:** Оставить текущую модель + add proper error recovery в компиляторе.
