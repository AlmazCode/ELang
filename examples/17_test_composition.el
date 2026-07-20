// ============================================
// Тест 3: Composition — GameObject
// ============================================
// Примечание: Type::method() синтаксис пока не работает
// для конструкторов с литеральными аргументами.
// Используем прямой вызов конструктора.

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

    fn print_pos(obj: GameObject) -> void {
        print_str("(")
        print_int(obj.pos_x)
        print_str(", ")
        print_int(obj.pos_y)
        print_str(") hp=")
        print_int(obj.health)
    }
}

fn main() -> u8 {
    print_str("=== Тест 3: Composition + методы ===\n\n")

    // Создаём игрока напрямую через конструктор
    let player: GameObject = GameObject(0, 0, 3, 2, 100)
    print_str("Игрок: ")
    player.print_pos()
    print_str("\n\n")

    // Двигаем 3 раза
    print_str("Движение:\n")
    let p1: GameObject = player.move()
    print_str("  Шаг 1: ")
    p1.print_pos()
    print_str("\n")

    let p2: GameObject = p1.move()
    print_str("  Шаг 2: ")
    p2.print_pos()
    print_str("\n")

    let p3: GameObject = p2.move()
    print_str("  Шаг 3: ")
    p3.print_pos()
    print_str("\n\n")

    // Тест урона
    print_str("Урон: 30\n")
    let damaged: GameObject = player.damage(30)
    print_str("После урона: ")
    damaged.print_pos()
    print_str("\n")

    print_str("Жив? ")
    if damaged.is_alive() == 1 { print_str("Да\n") } else { print_str("Нет\n") }

    // Смертельный урон
    print_str("\nСмертельный урон: 500\n")
    let killed: GameObject = damaged.damage(500)
    print_str("После урона: ")
    killed.print_pos()
    print_str("\n")

    print_str("Жив? ")
    if killed.is_alive() == 1 { print_str("Да\n") } else { print_str("Нет\n") }

    // Урон 0
    print_str("\nУрон: 0\n")
    let no_dmg: GameObject = player.damage(0)
    print_str("После урона: ")
    no_dmg.print_pos()
    print_str("\n")

    print_str("Жив? ")
    if no_dmg.is_alive() == 1 { print_str("Да\n") } else { print_str("Нет\n") }

    print_str("\n✅ Тест 3 пройден\n")
    return 0
}
