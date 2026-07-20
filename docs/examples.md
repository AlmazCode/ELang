# Примеры

Все примеры находятся в `examples/`.

## Базовые

| Файл | Описание |
|------|----------|
| `01_hello_world.el` | Hello World — базовый пример |
| `02_variables.el` | Переменные: let, let mut, типы |
| `03_functions.el` | Функции: неявный возврат, =>, рекурсия |
| `04_control_flow.el` | if/else if, while, for (диапазон, enumerate) |

## Средние

| Файл | Описание |
|------|----------|
| `05_error_handling.el` | Result, ?, catch, assert |
| `06_arrays.el` | Массивы: литералы, индексы, .len |
| `07_pipeline.el` | Pipeline оператор `\|>` |
| `08_defer.el` | Defer — автоматическая очистка |
| `09_tuples.el` | Кортежи и деструктуризация |
| `10_modules.el` | Модули: import, export, namespace |
| `11_structs_enums.el` | Structs, enums (объявления) |
| `12_expressions.el` | Expression-oriented стиль |

## Продвинутые

| Файл | Описание |
|------|----------|
| `13_fizzbuzz.el` | FizzBuzz — все фичи вместе |
| `14_oop_project.el` | ООП: банковская система (structs + impl + enums) |
| `15_test_struct_methods.el` | Тест: struct конструктор + методы |
| `16_test_enums.el` | Тест: enums — объявление, сравнение |
| `17_test_composition.el` | Тест: composition — GameObject |

## Модули

| Файл | Описание |
|------|----------|
| `math.el` | Пример проектного модуля (export fn) |

## Запуск примеров

```bash
# Компиляция и запуск
./bin/elc examples/01_hello_world.el -o hello
./hello

# Автоматически (nasm + ld вызываются из elc)
./bin/elc examples/13_fizzbuzz.el -o fizzbuzz
./fizzbuzz

# Пример с модулем
./bin/elc examples/10_modules.el -o modules
./modules
```

## Что демонстрирует каждый пример

### 01_hello_world.el
Минимальная программа. Вывод строки через `print_str`.

### 02_variables.el
Immutable и mutable переменные, типы, while цикл для суммирования.

### 03_functions.el
Неявный возврат, однострочные функции (`=>`), рекурсия (factorial), clamp.

### 04_control_flow.el
Вложенные if/else if, while, for с диапазоном, for с `=>`, вложенные циклы.

### 05_error_handling.el
Result<T, E>, match по Ok/Err, catch, пропагация через `?`, assert.

### 06_arrays.el
Создание массивов, доступ по индексу, .len, while для суммирования, for по массиву.

### 07_pipeline.el
Цепочки `|>`: `5 |> double() |> add(10) |> print_int()`.

### 08_defer.el
Defer в обратном порядке, несколько defer.

### 09_tuples.el
Деструктуризация кортежей, обмен значений.

### 10_modules.el
Import проектного модуля `math.el`, вызов `math::power`, pipeline с модулями.

### 11_structs_enums.el
Объявление struct и enum (без использования).

### 12_expressions.el
Функции как выражения, цепочки вызовов, if expression.

### 13_fizzbuzz.el
Классическая задача: функции, while, if/else if, модули, pipeline, match, defer.

### 14_oop_project.el
Полноценная ООП: structs (Transaction, Client, Bank), enums (AccountType), impl блоки с методами, композиция.

### 15_test_struct_methods.el
Тест struct методов: Point (distance, describe), Person (is_adult, greet).

### 16_test_enums.el
Тест enum: присваивание, сравнение, использование в if/else.

### 17_test_composition.el
Тест composition: GameObject с методами move, damage, is_alive.
