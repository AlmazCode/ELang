# ELang v0.44.0

**Статически типизированный, компилируемый системный язык программирования с expression-oriented синтаксисом.**

Автор: **AlmazCode**

Компилируется напрямую в x86_64 ассемблер (NASM), создаёт нативные Linux бинарники без runtime overhead.

## Документация

| Раздел | Описание |
|--------|----------|
| [Быстрый старт](docs/getting-started.md) | Установка, сборка, первый проект |
| [Система типов](docs/types.md) | Все типы языка |
| [Переменные](docs/variables.md) | let, let mut, деструктуризация |
| [Функции](docs/functions.md) | Параметры, default values, named args, closures |
| [Управление потоком](docs/control-flow.md) | if/else, while, for, match |
| [ООП](docs/oop.md) | Structs, enums, impl блоки |
| [Обработка ошибок](docs/error-handling.md) | Result, ?, catch, panic, assert |
| [Модули](docs/modules.md) | import, export, namespace |
| [Pipeline](docs/pipeline.md) | Оператор \|> |
| [Стандартная библиотека](docs/standard-library.md) | Core и std функции |
| [Компилятор](docs/compiler.md) | elc, флаги, архитектура |
| [Примеры](docs/examples.md) | Обзор всех примеров |

## Быстрый старт

```bash
# Клонирование и установка
git clone https://github.com/AlmazCode/ELang
cd ELang
./install.sh

# Компиляция и запуск
elang examples/01_hello_world.el
```

## Ключевые особенности

- **Явная типизация** — все переменные и функции обязаны иметь типы
- **Struct = Class** — heap-allocated объекты с методами через `impl`
- **Enums** — простые enum + auto-методы (`tag()`, `name()`, `count()`)
- **Closures** — анонимные функции с захватом переменных
- **Pipeline `\|>`** — цепочки вызовов
- **Result\<T, E\>** — типобезопасная обработка ошибок с `?` оператором
- **Expression-oriented** — `if`, `match` возвращают значения
- **Массивы** — heap-allocated, bounds checking, map/filter/reduce
- **Defer** — автоматическая очистка ресурсов
- **Нулевой runtime** — bump allocator, без GC, без VM

## Требования

- Linux x86_64
- GCC или Clang
- NASM (Netwide Assembler)
- ld (GNU linker)

## Установка

```bash
# В /usr/local (нужен sudo)
./install.sh

# В пользовательскую директорию (без sudo)
./install.sh --prefix=$HOME/.local

# Удаление
./install.sh --uninstall
```

Установщик создаёт:
- `/usr/local/bin/elc` — бинарник компилятора
- `/usr/local/bin/elang` — обёртка с CLI-флагами
- `/usr/local/share/elang/lib/` — runtime библиотеки (.o + .asm)

## Лицензия

MIT License
