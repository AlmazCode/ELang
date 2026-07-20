# ООП: Structs, Enums, Impl

## Structs

Struct в ELang — heap-allocated объект с полями. Аналог class в других языках.

### Объявление

```elang
struct Point {
    x: i64
    y: i64
}

struct Person {
    name: string
    age: i64
}

struct Transaction {
    amount: i64
    status: i64
}
```

### Создание экземпляров

```elang
let p: Point = Point(1, 2)
let person: Person = Person("Alice", 30)
let tx: Transaction = Transaction(15000, 1)
```

Поля передаются позиционно в порядке объявления.

### Доступ к полям

```elang
let p: Point = Point(3, 4)
print_int(p.x)  // 3
print_int(p.y)  // 4

// p.x = 10  // ОШИБКА: поля immutable
```

### Memory layout

```
[refcount:8][field_count:8][field0:8][field1:8]...
             ^                            ^
             header                       data start (returned pointer)
```

- `refcount` — счётчик ссылок
- `field_count` — количество полей
- Поля доступны через смещения от указателя на данные

## Impl блоки

Методы определяются через `impl` блоки:

```elang
struct Point {
    x: i64
    y: i64
}

impl Point {
    fn new(px: i64, py: i64) -> Point {
        Point(px, py)
    }

    fn distance_sq(p: Point, other: Point) -> i64 {
        let dx: i64 = p.x - other.x
        let dy: i64 = p.y - other.y
        dx * dx + dy * dy
    }

    fn describe(p: Point) -> void {
        print_str("(")
        print_int(p.x)
        print_str(", ")
        print_int(p.y)
        print_str(")")
    }
}
```

### Вызов методов

```elang
let a: Point = Point(3, 4)
let b: Point = Point(0, 0)

a.describe()                     // (3, 4)
let d: i64 = a.distance_sq(b)   // 25
```

### Параметр self

Первый параметр метода — объект. Синтаксис `obj.method(args)` является сахаром для `method(obj, args)`:

```elang
// Эти вызовы эквивалентны:
a.describe()      // sugar
describe(a)       // реальный вызов
```

### Статический диспатч

Все методы компилируются как обычные функции с префиксом типа:

```asm
; Point::new → _Point_new
; Point::distance_sq → _Point_distance_sq
```

Нет vtable, нет dynamic dispatch.

## Enums

### Простые enums

```elang
enum Color { Red, Green, Blue }
enum Direction { North, South, East, West }
enum AccountType { Savings, Checking, Credit }
```

### Enum с ручными значениями

```elang
enum Month { Jan = 1, Feb = 2, Mar = 3, Apr = 4 }
```

### Enum с данными (парсится, но data-часть не полностью работает)

```elang
enum Option { Some(i64), None }
```

### Автоматические методы

Каждый enum автоматически получает три метода:

```elang
enum Color { Red, Green, Blue }

let c: i64 = Color::Green

c.tag()       // → 1 (индекс варианта)
c.name()      // → "Green" (имя варианта)
Color.count() // → 3 (количество вариантов)
```

### Использование

```elang
enum Direction { North, South, East, West }

let d: i64 = Direction::North

// Сравнение
if d == Direction::North { print_str("Север\n") }

// В if/else
let name: string = if d == Direction::North { "Север" }
    else if d == Direction::South { "Юг" }
    else if d == Direction::East { "Восток" }
    else { "Запад" }
```

### Codegen enum

Enum значения хранятся как `i64`. Кодирование: tag = индекс варианта (0, 1, 2...).

Auto-методы генерируются как функции с именем `EnumName_method`:

```asm
; Color_tag() → возвращает индекс
; Color_name() → возвращает указатель на строку
; Color_count() → возвращает количество вариантов
```

## Composition

Структуры можно использовать как поля других структур:

```elang
struct GameObject {
    pos_x: i64
    pos_y: i64
    vel_x: i64
    vel_y: i64
    health: i64
}

impl GameObject {
    fn move(obj: GameObject) -> GameObject {
        let new_x: i64 = obj.pos_x + obj.vel_x
        let new_y: i64 = obj.pos_y + obj.vel_y
        GameObject(new_x, new_y, obj.vel_x, obj.vel_y, obj.health)
    }

    fn damage(obj: GameObject, amount: i64) -> GameObject {
        let new_hp: i64 = obj.health - amount
        if new_hp < 0 {
            GameObject(obj.pos_x, obj.pos_y, obj.vel_x, obj.vel_y, 0)
        } else {
            GameObject(obj.pos_x, obj.pos_y, obj.vel_x, obj.vel_y, new_hp)
        }
    }

    fn is_alive(obj: GameObject) -> i64 {
        if obj.health > 0 { 1 } else { 0 }
    }
}
```

## Ограничения

- Поля immutable (нет setter-методов через `obj.field = value`)
- Нет наследования
- Нет интерфейсов / traits
- Методы возвращают новые объекты вместо мутации
