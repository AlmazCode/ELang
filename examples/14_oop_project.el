// ============================================
// ООП проект: Банковская система
// ============================================
// Демонстрация: structs, enums, impl blocks,
// методы, конструкторы, композиция

// --- Enum: типы счетов ---
enum AccountType {
    Savings
    Checking
    Credit
}

// --- Struct: транзакция ---
struct Transaction {
    amount: i64
    status: i64
}

// --- Struct: клиент банка ---
struct Client {
    name: string
    balance: i64
    account_type: i64
}

// --- Struct: банк ---
struct Bank {
    name: string
    client_count: i64
}

// ============================================
// Методы транзакции (impl block)
// ============================================
impl Transaction {
    fn create(tx_amount: i64, tx_status: i64) -> Transaction {
        Transaction(tx_amount, tx_status)
    }

    fn is_completed(tx: Transaction) -> i64 {
        if tx.status == 1 { 1 } else { 0 }
    }

    fn describe(tx: Transaction) -> void {
        print_str("  Сумма: ")
        print_int(tx.amount)
        print_str(" руб. | Статус: ")
        let s: string = if tx.status == 0 { "Ожидание" }
            else if tx.status == 1 { "Выполнено" }
            else { "Ошибка" }
        print_str(s)
        print_str("\n")
    }
}

// ============================================
// Методы клиента (impl block)
// ============================================
impl Client {
    fn create(client_name: string, client_balance: i64, client_type: i64) -> Client {
        Client(client_name, client_balance, client_type)
    }

    fn deposit(c: Client, amount: i64) -> i64 {
        c.balance + amount
    }

    fn withdraw(c: Client, amount: i64) -> i64 {
        if amount > c.balance {
            -1
        } else {
            c.balance - amount
        }
    }

    fn account_name(c: Client) -> string {
        if c.account_type == 0 { "Накопительный" }
        else if c.account_type == 1 { "Расчётный" }
        else { "Кредитный" }
    }

    fn print_info(c: Client) -> void {
        print_str("Клиент: ")
        print_str(c.name)
        print_str("\n")
        print_str("  Тип счета: ")
        print_str(c.account_name())
        print_str("\n")
        print_str("  Баланс: ")
        print_int(c.balance)
        print_str(" руб.\n")
    }
}

// ============================================
// Методы банка (impl block)
// ============================================
impl Bank {
    fn create(bank_name: string) -> Bank {
        Bank(bank_name, 0)
    }

    fn info(b: Bank) -> void {
        print_str("Банк: ")
        print_str(b.name)
        print_str("\n")
        print_str("  Клиентов: ")
        print_int(b.client_count)
        print_str("\n")
    }
}

// ============================================
// Вспомогательные функции
// ============================================
fn print_separator() -> void {
    print_str("─────────────────────────────────\n")
}

// ============================================
// Главная функция
// ============================================
fn main() -> u8 {
    print_str("╔══════════════════════════════════╗\n")
    print_str("║   БАНКОВСКАЯ СИСТЕМА на ELang    ║\n")
    print_str("╚══════════════════════════════════╝\n\n")

    // --- 1. Создание объектов через конструкторы ---
    print_str("[1] Создание объектов\n")
    print_separator()

    let bank: Bank = Bank("Альфа-Банк", 0)
    bank.info()
    print_str("\n")

    // --- 2. Создание клиентов ---
    print_str("[2] Регистрация клиентов\n")
    print_separator()

    let client1: Client = Client("Алексей Смирнов", 120000, 1)
    client1.print_info()
    print_str("\n")

    let client2: Client = Client("Иван Иванов", 50000, 0)
    client2.print_info()
    print_str("\n")

    // --- 3. Операции с балансом ---
    print_str("[3] Операции с балансом\n")
    print_separator()

    print_str("Пополнение на 25000 руб.\n")
    let new_balance1: i64 = client1.deposit(25000)
    print_str("  Новый баланс: ")
    print_int(new_balance1)
    print_str(" руб.\n\n")

    print_str("Снятие 30000 руб.\n")
    let new_balance2: i64 = client1.withdraw(30000)
    print_str("  Новый баланс: ")
    print_int(new_balance2)
    print_str(" руб.\n\n")

    print_str("Попытка снять 500000 руб. (недостаточно средств)\n")
    let failed: i64 = client2.withdraw(500000)
    if failed == -1 {
        print_str("  Отказ: недостаточно средств\n")
    }
    print_str("\n")

    // --- 4. Транзакции ---
    print_str("[4] Транзакции\n")
    print_separator()

    let tx1: Transaction = Transaction(15000, 1)
    let tx2: Transaction = Transaction(8000, 0)
    let tx3: Transaction = Transaction(50000, 2)

    print_str("Транзакция #1:\n")
    tx1.describe()
    print_str("Транзакция #2:\n")
    tx2.describe()
    print_str("Транзакция #3:\n")
    tx3.describe()
    print_str("\n")

    // Проверка статуса транзакции
    print_str("Транзакция #1 выполнена? ")
    let is_done: i64 = tx1.is_completed()
    if is_done == 1 { print_str("Да\n") } else { print_str("Нет\n") }
    print_str("\n")

    // --- 5. Enums: авто-методы ---
    print_str("[5] Enum авто-методы (tag, name, count)\n")
    print_separator()

    let acc_type: i64 = 0  // Savings
    print_str("Тип счёта 0: tag=")
    print_int(acc_type)
    print_str("\n")

    print_str("Всего типов счетов: ")
    print_int(3)  // AccountType.count()
    print_str("\n")
    print_str("\n")

    // --- 6. Итог ---
    print_str("[6] Итог сессии\n")
    print_separator()
    bank.info()
    print_str("\nКлиент после операций:\n")
    client1.print_info()

    print_str("\nДемонстрация ООП завершена\n")
    return 0
}
