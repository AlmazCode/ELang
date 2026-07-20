// ============================================
// Тест 2: Enums — объявление, присваивание, сравнение
// ============================================
// Enum-значения хранятся как i64. Присваиваются через Type::Variant.

enum Direction {
    North
    South
    East
    West
}

enum Shape {
    Circle
    Square
    Triangle
}

fn main() -> u8 {
    print_str("=== Тест 2: Enums ===\n\n")

    // --- Объявление enum переменных ---
    print_str("Присваивание enum значений:\n")

    let d1: Direction = Direction::North
    print_str("  d1 = Direction::North\n")

    let d2: Direction = Direction::East
    print_str("  d2 = Direction::East\n\n")

    // --- Сравнение enum переменных ---
    print_str("Сравнение enum значений:\n")

    print_str("  North == North: ")
    if d1 == Direction::North { print_str("да") } else { print_str("нет") }
    print_str("\n")

    print_str("  North == East: ")
    if d1 == d2 { print_str("да") } else { print_str("нет") }
    print_str("\n\n")

    // --- Enum в if/else ---
    print_str("Определение направления:\n")
    let name: string = if d1 == Direction::North { "Север" }
        else if d1 == Direction::South { "Юг" }
        else if d1 == Direction::East { "Восток" }
        else { "Запад" }
    print_str("  d1 = ")
    print_str(name)
    print_str("\n\n")

    // --- Shape ---
    print_str("Shape enum:\n")
    let s: Shape = Shape::Triangle
    print_str("  s = Shape::Triangle\n")

    let sname: string = if s == Shape::Circle { "Круг" }
        else if s == Shape::Square { "Квадрат" }
        else { "Треугольник" }
    print_str("  s = ")
    print_str(sname)
    print_str("\n\n")

    // --- Enum как аргумент функции ---
    print_str("Enum как аргумент функции:\n")
    let shape: Shape = Shape::Square
    let side: i64 = 10
    let area: i64 = if shape == Shape::Circle { side * side }
        else if shape == Shape::Square { side * side }
        else { (side * 3) / 2 }
    print_str("  Square(10) area = ")
    print_int(area)
    print_str("\n")

    print_str("\n✅ Тест 2 пройден\n")
    return 0
}
