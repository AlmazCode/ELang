# Модули

## Два типа модулей

| Тип | Синтаксис | Описание |
|-----|-----------|----------|
| Проектный | `import "path"` | `.el` файл, merged в основной AST |
| Стандартная библиотека | `import name` | `.asm` файл, отдельный `.o`, linked через ld |

## Проектные модули

### Экспорт функций

```elang
// math.el
export fn power(base: i64, exp: i64) -> i64 {
    let result: i64 = 1
    let i: i64 = 0
    while i < exp {
        result = result * base
        i = i + 1
    }
    return result
}

export fn abs_val(x: i64) -> i64 {
    if x < 0 { return 0 - x }
    return x
}
```

### Импорт и использование

```elang
// main.el
import "math"

fn main() -> u8 {
    let x: i64 = math::power(2, 10)
    print_int(x)  // 1024

    let y: i64 = math::abs_val(-42)
    print_int(y)  // 42
    return 0
}
```

### Namespace

Имя модуля берётся из последнего компонента пути:

```elang
import "math"           // namespace: math
import "utils/string"   // namespace: string
import "lib/collections" // namespace: collections
```

### Алиасы

```elang
import "math" as m

let x: i64 = m::power(2, 10)
```

### Разрешение путей

Проектные модули ищутся по относительному пути от исходного файла:

```
./<имя_файла>.el
```

Например, `import "math"` для файла `examples/main.el` найдёт `examples/math.el`.

### Переименование при merge

Экспортируемые функции переименовываются: `func` → `ns_func`:

```elang
// math.el
export fn power(...) → в codegen становится _math_power
```

### Циклические импорты

Компилятор отслеживает стек импортов и запрещает циклы:

```
Error: circular import of 'module_b'
```

### Дедупликация

Один и тот же модуль не импортируется дважды, даже если указан несколько раз.

## Стандартная библиотека

### Синтаксис

```elang
import std           // стандартная библиотека
import std as s      // с алиасом
import math          // математика
import math as m     // с алиасом
```

### Разрешение путей

Стандартные модули ищутся в порядке:

1. `$ELANG_LIB_PATH/<name>.asm`
2. `./lib/<name>.asm` (от исходного файла)
3. `./lib/<name>.asm` (от cwd)
4. `/usr/local/share/elang/lib/<name>.asm`

### Доступ к функциям

```elang
import std

fn main() -> u8 {
    let s: string = std::read_input()
    let cmp: i64 = std::str_cmp("abc", "abd")
    return 0
}
```

### Запрет: import "core"

```elang
import "core"  // ОШИБКА: 'core' is auto-linked, do not import it
```

`core.asm` всегда линкуется автоматически.

## Автоматическая сборка

При компиляции `elc` автоматически:

1. Ассемблирует сгенерированный `.asm`
2. Собирает `lib/core.asm` → `lib/build/core.o`
3. Собирает все импортированные stdlib `.asm` → `lib/build/<name>.o`
4. Линкует все `.o` файлы через `ld`

```bash
# Одна команда — полная сборка
./bin/elc examples/10_modules.el -o program
./program
```
