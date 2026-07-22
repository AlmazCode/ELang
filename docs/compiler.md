# Компилятор elc

## Архитектура

```
Исходный код (.el)
       ↓
   [Lexer]          → токены
       ↓
   [Parser]         → AST (Abstract Syntax Tree)
       ↓
   [Import resolver] → merged AST (проектные модули)
       ↓
   [Semantics]      → type checking + constant folding
       ↓
   [Codegen]        → x86_64 NASM ассемблер (.asm)
       ↓
   [nasm]           → объектный файл (.o)
       ↓
   [ld]             → нативный бинарник
```

## Флаги

```
elc [опции] <файл.el>
```

| Флаг | Описание |
|------|----------|
| `-o <файл>` | Выходной файл (по умолчанию: output) |
| `-t` | Вывод токенов |
| `-a` | Вывод AST |
| `-l` | Режим библиотеки (без `_start`) |
| `-check` | Только проверка типов (без codegen) |
| `-fold` | Только constant folding |
| `-h` | Справка |
| `-v`, `--version` | Версия компилятора |

### Примеры

```bash
# Показать токены
./bin/elc -t examples/01_hello_world.el

# Показать AST
./bin/elc -a examples/01_hello_world.el

# Только проверка типов
./bin/elc -check examples/01_hello_world.el

# Только constant folding
./bin/elc -fold examples/01_hello_world.el

# Компиляция в指定 выходной файл
./bin/elc examples/01_hello_world.el -o hello.asm

# Режим библиотеки (без _start)
./bin/elc -l lib_module.el -o lib_module.asm
```

## Этапы компиляции

### 1. Lexer (lexer.c)

Токенизация исходного кода. Поддерживает:
- Все ключевые слова (fn, let, if, while, for, struct, enum, impl, ...)
- Операторы (+, -, *, /, ==, !=, <, >, <=, >=, &&, ||, !, |>, ::, .., =>)
- Literals (int, float, string, char, bool)
- Hex/octal/binary literals (0xFF, 0o77, 0b1010)
- Комментарии (// и /* */ с вложенностью)

### 2. Parser (parser.c)

Рекурсивный descent парсер с Pratt parsing для выражений. Обработка ошибок с восстановлением (sync до `;`/`}`/newline).

### 3. Import Resolver (main.c)

- Резолвит пути к модулям
- Рекурсивно парсит проектные модули
- Merge'ит AST проектных модулей в основной
- Проверяет циклические импорты
- Дедуплицирует модули

### 4. Semantics (semantics.c)

- Type checking: проверка совместимости типов
- Type inference: выведение типов (пока не реализовано)
- Constant folding: вычисление `2 + 3` → `5` на этапе компиляции
- Scope analysis: проверка области видимости переменных
- Function table: кросс-модульный поиск функций

### 5. Codegen (codegen.c + codegen_helpers.c + codegen_call.c)

Генерация x86_64 NASM ассемблера. Разделён на 3 модуля:

| Модуль | Ответственность |
|--------|-----------------|
| `codegen.c` | gen_expr, gen_stmt, gen_node, codegen_program |
| `codegen_helpers.c` | emit, symbols, scopes, strings, floats, defer, collect |
| `codegen_call.c` | emit_call, emit_closure_call, resolve_call_args |

Включает:
- Все арифметические операции (int и float через SSE2)
- Функции с closure convention
- Closures с environment struct
- Struct heap allocation
- Enum auto-методы
- Match expressions
- loop / break / continue
- Defer statements
- Defer + ? interaction
- Universal print dispatch
- Named args / default values resolution
- Float constants в .data секции

## Модель памяти

- **Bump allocator** — выделение через `brk` syscall, никогда не освобождает
- **Refcount** для arrays
- **Watermark** для bulk-очистки
- Нет GC, нет VM

## Константы (config.h)

```c
#define MAX_IDENT_LEN    128
#define MAX_PATH_LEN     1024
#define MAX_MODULES      64
#define MAX_PARSE_DEPTH  2000
#define ELANG_VERSION    "0.46.0"
```

## Безопасные макросы

```c
SAFE_MALLOC(size)      // malloc + проверка
SAFE_CALLOC(n, size)   // calloc + проверка
SAFE_REALLOC(p, size)  // realloc + проверка
SAFE_STRDUP(s)         // strdup + проверка
SAFE_STRNDUP(s, n)     // strndup + проверка
```

Все аллокации защищены от OOM. При нехватке памяти — `abort()`.

## Сборка и установка

```bash
# Сборка компилятора
make

# Установка (собирает + ставит в /usr/local)
./install.sh

# Пользовательская установка (без sudo)
./install.sh --prefix=$HOME/.local

# Удаление
./install.sh --uninstall
```

### Makefile

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -D_GNU_SOURCE
SRCS = src/token.c src/lexer.c src/ast.c src/parser.c src/semantics.c src/codegen.c src/codegen_helpers.c src/codegen_call.c src/main.c
```

## Требования

- GCC или Clang (C11)
- NASM (Netwide Assembler)
- ld (GNU linker)
- Linux x86_64

## Поиск библиотек

Компилятор ищет runtime библиотеки в следующем порядке:

1. `$ELANG_LIB_PATH/` — переменная окружения
2. `./lib/` относительно исходного файла
3. `./lib/` относительно текущей директории
4. `/usr/local/share/elang/lib/` — стандартный путь установки

Приоритет: `.o` файлы (собранные) выше `.asm` (исходники).

## Статистика

| Метрика | Значение |
|---------|----------|
| Версия | v0.46.0 |
| Строк кода (C) | ~7000 |
| Строк asm (core) | ~1550 |
| Файлов исходников | 17 (.c/.h) + 3 (.asm) |
