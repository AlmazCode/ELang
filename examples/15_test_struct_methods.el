// ============================================
// Тест 1: Struct конструктор + методы
// ============================================
struct Point {
    x: i64
    y: i64
}

struct Person {
    name: string
    age: i64
}

impl Point {
    fn create(px: i64, py: i64) -> Point {
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

impl Person {
    fn create(person_name: string, person_age: i64) -> Person {
        Person(person_name, person_age)
    }

    fn is_adult(p: Person) -> i64 {
        if p.age >= 18 { 1 } else { 0 }
    }

    fn greet(p: Person) -> void {
        print_str("Привет, я ")
        print_str(p.name)
        print_str(", мне ")
        print_int(p.age)
        print_str(" лет\n")
    }
}

fn main() -> u8 {
    print_str("=== Тест 1: Struct + методы ===\n\n")

    // Точка A и B
    let a: Point = Point(3, 4)
    let b: Point = Point(0, 0)

    print_str("Точка A = ")
    a.describe()
    print_str("\n")
    print_str("Точка B = ")
    b.describe()
    print_str("\n")

    let dist_sq: i64 = a.distance_sq(b)
    print_str("dist^2(A, B) = ")
    print_int(dist_sq)
    print_str("\n")

    // Person
    let alice: Person = Person("Алиса", 25)
    let bob: Person = Person("Боб", 16)

    alice.greet()
    bob.greet()

    print_str("Алиса совершеннолетняя? ")
    if alice.is_adult() == 1 { print_str("Да\n") } else { print_str("Нет\n") }

    print_str("Боб совершеннолетний? ")
    if bob.is_adult() == 1 { print_str("Да\n") } else { print_str("Нет\n") }

    print_str("\n✅ Тест 1 пройден\n")
    return 0
}
