# Быстрый старт

## Требования

- Linux x86_64
- GCC или Clang
- NASM (Netwide Assembler)
- ld (GNU linker)

## Установка NASM

```bash
# Ubuntu/Debian
sudo apt install nasm

# Arch
sudo pacman -S nasm

# Fedora
sudo dnf install nasm
```

## Установка

```bash
git clone https://github.com/AlmazCode/ELang
cd ELang
./install.sh
```

Установщик автоматически соберёт компилятор и установит:
- `elc` — бинарник компилятора
- `elang` — обёртка с CLI-флагами (compile + run)
- Runtime библиотеки в `/usr/local/share/elang/lib/`

### Пользовательская установка (без sudo)

```bash
./install.sh --prefix=$HOME/.local
```

### Удаление

```bash
./install.sh --uninstall
```

## Компиляция и запуск программы

```bash
# Компиляция и запуск
elang examples/01_hello_world.el

# Только компиляция
elang -c examples/01_hello_world.el

# Пользовательский выходной файл
elang -o hello examples/01_hello_world.el

# Проверка типов
elang -check examples/01_hello_world.el
```

## Структура проекта

```
ELang/
├── src/                      # Исходники компилятора (C)
│   ├── lexer.c              # Токенизатор
│   ├── token.c              # Токены
│   ├── parser.c             # Парсер (Pratt parsing)
│   ├── ast.c                # AST узлы
│   ├── semantics.c          # Type checker, constant folding
│   ├── codegen.c            # Генерация x86_64 asm
│   └── main.c               # CLI, импорты, сборка
├── include/                  # Заголовочные файлы
│   ├── config.h             # Константы, безопасные макросы
│   ├── token.h              # Типы токенов
│   ├── lexer.h              # Интерфейс лексера
│   ├── ast.h                # AST узлы
│   ├── parser.h             # Интерфейс парсера
│   ├── semantics.h          # Типы, scope, семантика
│   └── codegen.h            # Интерфейс codegen
├── lib/                      # Стандартная библиотека (x86_64 asm)
│   ├── core.asm             # Runtime (всегда линкуется)
│   ├── std.asm              # Расширенная библиотека (import "std")
│   ├── math.asm             # Математика (import math)
│   └── build/               # Собранные .o файлы
├── examples/                 # Примеры программ
├── docs/                     # Документация
├── install.sh                # Установщик
├── Makefile
└── README.md
```
