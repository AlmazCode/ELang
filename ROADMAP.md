# ELang v0.45.0 Roadmap

Автор: **AlmazCode**

## Текущий статус

Компилятор компилирует programs в x86_64 asm. Runtime на чистом asm (bump allocator через brk, без libc).

**Реализовано:**
- Explicit typing (`let x: i64 = 42`)
- closures с captures (env struct)
- pipeline operations (`|>`)
- массивы с bounds checking, map/filter/reduce
- if/while/for expressions
- match expressions
- structs с heap allocation + methods (impl)
- enums с auto-introspection (tag/name/count)
- error handling (Result, ?, catch, panic)
- defer statements
- tuples, arrays literals
- **float type (f32/f64)** — SSE2 арифметика, print_f64
- **universal print()** — compile-time dispatch по типу
- **import system** — project modules (`import "..."`) + stdlib (`import name`)
- **keyword arguments** — `print(x, sep: ", ", end: "")`
- **default values** — `fn foo(x: i64 = 10)`
- **hex/octal/binary literals** — `0xFF`, `0o77`, `0b1010`
- **block comments** — `/* ... */` с вложенностью
- **error recovery** — парсер продолжает после ошибок
- **loop/break/continue** — бесконечные циклы с выходом
- **forward declarations** — вызов функций до объявления

---

## Приоритет 1: Безопасность + архитектура ✅

### 1.1 config.h + CHECK_ALLOC макры ✅
- Общие константы в `include/config.h`
- SAFE_MALLOC, SAFE_CALLOC, SAFE_REALLOC, SAFE_STRDUP макры
- **Убрано:** MAX_PARSE_DEPTH, MAX_EXTNAME_LEN (мёртвый код)

### 1.2 NULL-checks (~82 места) ✅
- Все malloc/calloc/realloc/strdup/strndup защищены SAFE_* макрами
- Файлы: ast.c, lexer.c, parser.c, semantics.c, codegen.c, main.c

### 1.3 Command injection fix ✅
- `system()` → `fork()+execvp()` для безопасных команд
- `fork()+execlp("sh","sh","-c",...)` для shell команд
- Buffer overflow: `strcat` → `snprintf` с bounds checking

### 1.4 TRY/CATCH dedup ✅
- ~40 строк дублирующегося кода удалено
- gen_stmt вызывает gen_expr для TRY_EXPR и CATCH_EXPR

### 1.5 Версия в codegen ✅
- Hardcoded `0.43.0` → `ELANG_VERSION` макро

### 1.6 compiler_ctx_t ✅
- Global state в main.c заменён на `compiler_ctx_t`
- g_libs, g_import_stack, g_resolved → g_ctx.*

### 1.7 Rotating buffers ✅
- `type_name()` удалена (используется `type_name_buf`)
- `find_string_label` возвращает int label (не строку)

---

## Приоритет 2: Float ✅

### 2.1 SSE2 codegen ✅
- Float literals: `movsd xmm0, [fconst_N]`
- Арифметика: `addsd`, `subsd`, `mulsd`, `divsd`
- Сравнения: `comisd`, `sete/setne/setl/setg`
- Symbol table с `is_float` флагом

### 2.2 print_f64 ✅
- IEEE 754 → десятичная строка (6 знаков)
- Поддержка: целая часть, дробная, отрицательные, 0

### 2.3 Print dispatch ✅
- `print(x)` автоматически вызывает `print_f64` для f64 переменных
- `print_f64(x)` — прямой вызов через emit_call

### 2.4 Division by zero ✅
- Перед `idiv`/`div` добавлен `test rcx, rcx; jz panic`
- Constant folding больше не скрывает division by zero

---

## Приоритет 3: Import system ✅

### 3.1 Синтаксис ✅
```elang
import "utils"           // проектный модуль
import "utils" as u      // с алиасом
import math              // stdlib
import math as m         // stdlib с алиасом
import "core"            // ОШИБКА (auto-linked)
```

### 3.2 Разрешение путей ✅
- Проектные: `{base_dir}/{path}.el`
- Стандартные: `$ELANG_LIB_PATH/{name}.asm`, `./lib/{name}.asm`

### 3.3 Codegen ✅
- Таблица модулей в codegen_t
- Проектные модули: `call _module_func` (merged, closure convention)
- Стандартные: `call func` (separate .o, standard ABI)

### 3.4 Pipeline ✅
- nasm + ld автоматически вызываются из компилятора
- core.asm всегда пересобирается

---

## Приоритет 4: Default values + keyword args ✅

### 4.1 Default values ✅
```elang
fn greet(name: string, greeting: string = "Hello") -> void { ... }
greet("World")           // Hello, World
greet("Alice", greeting: "Hi")  // Hi, Alice
```

### 4.2 Keyword arguments ✅
```elang
fn add(a: i64, b: i64 = 10) -> i64 { return a + b }
add(5, b: 20)    // 25
add(b: 3, a: 7)  // 10
```

### 4.3 Print с sep/end ✅
```elang
print(1, 2, 3, sep: ", ")     // 1, 2, 3
print("A", end: "")            // A (без переноса)
print("x", "y", sep: " - ")   // x - y
```

---

## Приоритет 5: Фичи ✅

### 5.1 Print tuple ✅
- `print((1, 2, 3))` → `(1, 2, 3)`
- Автоматический dispatch по типам элементов

### 5.2 Print array по типу ✅
- `print_arr_i64`, `print_arr_str`, `print_arr_bool`
- Dispatch по типу первого элемента

### 5.3 Hex/octal/binary literals ✅
- `0xFF` = 255, `0o77` = 63, `0b1010` = 10
- Lexer + parser с bounds checking

### 5.4 Block comments ✅
- `/* ... */` с поддержкой вложенности

### 5.5 Error recovery ✅
- Парсер синхронизируется до `;`/`}`/newline при ошибке
- Все ошибки сообщаются, не только первая

---

## Технический долг

### Must-fix
- [x] Closure captures (env struct) ✅
- [x] Error recovery ✅
- [x] `fn main() -> u8` + `return 0` ✅
- [x] If expressions ✅
- [x] Struct heap allocation ✅
- [x] Enum declaration + auto methods ✅
- [x] Method dispatch ✅
- [x] impl blocks ✅
- [x] Command injection fix ✅
- [x] Buffer overflow fix ✅
- [x] NULL-checks ✅
- [x] Float type ✅
- [x] Import system ✅
- [x] Default values ✅
- [x] Keyword arguments ✅
- [x] Hex/octal/binary literals ✅
- [x] Block comments ✅
- [x] Error recovery (parser) ✅

### Should-fix
- [x] read_file → SAFE_MALLOC ✅
- [x] Дедупликация extern в codegen ✅
- [x] print_arr_f64 в core.asm ✅
- [x] loop/break/continue ✅
- [x] Forward declarations ✅
- [x] Smoke test framework ✅
- [x] Удаление unused variables (left_is_float, saved, run_shell) ✅
- [x] Fix sign-compare warnings ✅
- [x] MAX_IDENT_LEN 64→128 ✅
- [x] Удаление dead code (when token) ✅
- [ ] Разделить codegen.c на модули (expr/stmt/print/array) — 2100+ строк, монолитный
- [ ] Рефакторинг emit_call/emit_closure_call (70% дублирования)
- [ ] Match exhaustive check (покрытие вариантов enum)
- [ ] Unsigned division (нет type tracking для u* типов)
- [ ] Label patching в codegen (sub rsp placeholder)

---

## Следующие шаги

### P3: Тесты + полировка
- [ ] Unit test framework (tests/run_tests.sh)
- [ ] -Werror в CFLAGS
- [ ] Dead code cleanup (ast_free, MAX_EXTNAME_LEN)

### P4: Продвинутые фичи
- [ ] Forward declarations (функции до использования)
- [ ] Multi-line strings `"""`
- [ ] Struct/enum literal syntax `Point{x: 1}`
- [ ] Enum variants with data (tagged union)
- [ ] Generics (максимально упрощённые)
- [ ] Traits (интерфейсы)
- [ ] Closures с rebinding captured variables
- [ ] String operations (concat, slice, compare)
- [ ] Incremental compilation
- [ ] Optimizer pass (dead code elimination, inlining)

---

## Архитектурные решения (утверждённые)

### 1. Struct = Class
Heap-allocated с refcount. Методы через `impl`. Без наследования.

### 2. Explicit typing
`let x: Type = value` обязательно. Inference только для простых случаев.

### 3. Memory model
Bump allocator + refcount. Struct/enum на heap. Watermark для очистки.

### 4. Enum encoding
Simple enum — tag в [ptr+0]. Auto-generated tag/name/count methods.

### 5. Method dispatch
Статический. `obj.method(args)` → `method(obj, args)`. Без vtable.

### 6. Import system
- `"..."` = проектный модуль (merged в основной AST)
- bare name = stdlib (отдельный .o, linked через ld)
- `core.asm` auto-linked (runtime)

### 7. Float type
- Все float операции через SSE2 (xmm0/xmm1)
- Значения хранятся как IEEE 754 bits в стеке
- Print через `print_f64` (6 знаков после точки)

### 8. Universal print
- Compile-time dispatch по типу аргумента
- `print(x)` → `print_i64` / `print_str` / `print_f64` / `print_bool`
- `sep:`, `end:` — keyword arguments для настройки вывода

---

## Статистика

| Метрика | Значение |
|---------|----------|
| Версия | v0.44.0 |
| Строк кода (C) | ~6500 |
| Строк asm (core) | ~1400 |
| Файлов исходников | 14 (.c/.h) + 3 (.asm) |
| Завершённых задач | 24 |
| Оставшихся задач | 11 |
