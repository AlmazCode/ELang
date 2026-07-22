# ELang Documentation

ELang v0.46.0 — статически типизированный, компилируемый системный язык программирования с expression-oriented синтаксисом. Компилируется напрямую в x86_64 ассемблер (NASM), создаёт нативные Linux бинарники без runtime overhead.

Автор: **AlmazCode**

## Оглавление

| Раздел | Описание |
|--------|----------|
| [Быстрый старт](getting-started.md) | Установка, сборка, первый проект |
| [Система типов](types.md) | Все типы: integers, floats, bool, string, array, pointer, Result |
| [Переменные](variables.md) | let, let mut, деструктуризация, область видимости |
| [Функции](functions.md) | Объявление, параметры, default values, named args, closures |
| [Управление потоком](control-flow.md) | if/else, while, for, match |
| [ООП](oop.md) | Structs, enums, impl блоки, методы |
| [Обработка ошибок](error-handling.md) | Result<T, E>, ?, catch, panic, assert |
| [Модули](modules.md) | import, export, namespace |
| [Pipeline](pipeline.md) | Оператор \|> |
| [Стандартная библиотека](standard-library.md) | Core и std функции |
| [Компилятор](compiler.md) | elc, флаги, сборка, архитектура |
| [Примеры](examples.md) | Обзор всех примеров |

## Ключевые особенности

- **Явная типизация** — все переменные и функции обязаны иметь типы
- **Struct = Class** — heap-allocated объекты с методами через `impl`
- **Enums** — простые enum + auto-методы (`tag()`, `name()`, `count()`)
- **Closures** — анонимные функции с захватом переменных из внешней области
- **Pipeline `\|>`** — цепочки вызовов: `x |> f() |> g()`
- **Result\<T, E\>** — типобезопасная обработка ошибок с `?` оператором
- **Expression-oriented** — `if`, `match` возвращают значения
- **Массивы** — heap-allocated, bounds checking, map/filter/reduce
- **Defer** — автоматическая очистка ресурсов при выходе из scope
- **Нулевой runtime** — bump allocator, без GC, без VM

## Быстрый пример

```elang
fn factorial(n: i64) -> i64 {
    if n <= 1 { 1 } else { n * factorial(n - 1) }
}

fn main() -> u8 {
    print_int(factorial(10))
    print_str("\n")
    return 0
}
```
