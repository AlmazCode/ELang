# Стандартная библиотека

## Core (автоматически линкуется)

`core.asm` всегда подключается. Не требует `import`.

### Вывод

| Функция | Аргумент | Описание |
|---------|----------|----------|
| `print_str(s)` | `*char` | Вывести строку |
| `print_int(n)` | `i64` | Вывести целое число |
| `print_i64(n)` | `i64` | Вывести signed integer |
| `print_u64(n)` | `u64` | Вывести unsigned integer |
| `print_bool(b)` | `i64` | Вывести "true"/"false" |
| `print_f64(bits)` | `u64` | Вывести float (IEEE 754 bits) |
| `print_hex(n)` | `i64` | Вывести hex |
| `print_ptr(p)` | `u64` | Вывести указатель как `0x...` |
| `print_newline()` | — | Вывести `\n` |
| `print_arr_i64(arr)` | `*i64` | Вывести массив как `[1, 2, 3]` |
| `print_arr_str(arr)` | `*char*` | Вывести массив строк |
| `print_arr_bool(arr)` | `*i64` | Вывести массив bool |

### Форматированный вывод

```asm
; print_i64_fmt(rdi=value, rsi=width, rdx=flags)
; flags:
;   FMT_SIGN (1)    — показывать + для положительных
;   FMT_ZERO (2)    — заполнять нулями
;   FMT_LEFT (4)    — выравнивание влево
;   FMT_BASE2 (8)   — двоичный
;   FMT_BASE8 (16)  — восьмеричный
;   FMT_BASE16 (32) — шестнадцатеричный
;   FMT_UPPER (64)  — uppercase hex
```

### Universal print

`print()` — универсальная функция, определяет тип аргумента при компиляции:

```elang
print(42)           // print_i64
print("hello")      // print_str
print(3.14)         // print_f64
print(true)         // print_bool
print([1, 2, 3])    // print_arr_i64
```

### Keyword arguments для print

```elang
print(1, 2, 3, sep: ", ")       // 1, 2, 3
print("A", end: "")              // A (без переноса)
print("x", "y", sep: " - ")     // x - y
```

### Строки и числа

| Функция | Описание |
|---------|----------|
| `str_len(s)` | Длина строки |
| `sys_write(fd, buf, count)` | Системный вызов write |
| `sys_read(fd, buf, count)` | Системный вызов read |
| `sys_exit(code)` | Системный вызов exit |
| `sys_getpid()` | PID процесса |
| `exit(code)` | Выход из программы |

### Error handling

| Функция | Описание |
|---------|----------|
| `panic_handler(msg, line, file)` | Выводит `panic at file:line: msg` и завершает |
| `assert_handler(cond, msg)` | Если cond == 0, выводит `assertion failed: msg` |

### Arrays

| Функция | Описание |
|---------|----------|
| `create(elem_size, count)` | Создать массив |
| `with_capacity(elem_size, cap)` | Создать с capacity, length = 0 |
| `get(arr, index)` | Получить элемент (bounds-checked) |
| `len(arr)` | Длина массива |
| `array_push(arr, elem)` | Добавить элемент (realloc) |
| `pop(arr)` | Убрать последний элемент |
| `slice(arr, start, end)` | Срез (копия) |
| `concat(a, b)` | Конкатенация (новый массив) |
| `map(arr, closure)` | Map |
| `filter(arr, closure)` | Filter |
| `reduce(arr, init, closure)` | Reduce |
| `retain(arr)` | Увеличить refcount |
| `free(arr)` | Уменьшить refcount |

### Memory

| Функция | Описание |
|---------|----------|
| `_bump_alloc(size)` | Выделение через brk |
| `save_watermark()` | Сохранить watermark |
| `restore_watermark(addr)` | Восстановить watermark |
| `_memmove(dst, src, n)` | Копирование памяти |

### Утилиты

| Функция | Описание |
|---------|----------|
| `alloc(size)` | Выделить память (alias _bump_alloc) |

## Standard Library (import std)

`std.asm` — расширенные функции, требуют `import std`.

| Функция | Описание |
|---------|----------|
| `str_cmp(a, b)` | Лексикографическое сравнение строк |
| `str_dup(s)` | Дублировать строку (пока возвращает оригинал) |
| `str_cat(a, b)` | Конкатенация in-place (небезопасно, перезаписывает a) |
| `read_input()` | Прочитать строку из stdin |
| `sys_brk(addr)` | Изменить program break |

### Пример использования std

```elang
import std

fn main() -> u8 {
    let s: string = std::read_input()
    let cmp: i64 = std::str_cmp("abc", "abd")
    // cmp < 0

    return 0
}
```

## Math Library (import math)

`math.asm` — математические функции.

> Внимание: math.asm содержит реализации, но в текущей версии может быть неполным. Проверяйте наличие нужных функций.

## Calling convention для runtime

Все runtime функции используют System V AMD64 ABI:

```
rdi = arg0
rsi = arg1
rdx = arg2
rcx = arg3
r8  = arg4
r9  = arg5
```

Возвращаемое значение в `rax` (или `xmm0` для float).
