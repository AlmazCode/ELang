// ============================================
// Массивы — литералы, индексы, .len
// ============================================

fn main() -> void {
    print_str("=== Массивы ===\n\n")

    // --- Создание массива ---
    print_str("--- Создание ---\n")
    let arr = [10, 20, 30, 40, 50]

    print_str("arr = [10, 20, 30, 40, 50]\n")

    // --- Доступ по индексу ---
    print_str("\n--- Индексы ---\n")
    print_str("arr[0] = ")
    print_int(arr[0])
    print_str("\n")

    print_str("arr[2] = ")
    print_int(arr[2])
    print_str("\n")

    print_str("arr[4] = ")
    print_int(arr[4])
    print_str("\n")

    // --- Длина массива ---
    print_str("\n--- Длина ---\n")
    print_str("arr.len = ")
    print_int(arr.len)
    print_str("\n")

    // --- Сумма элементов через while ---
    print_str("\n--- Сумма (while) ---\n")
    let mut sum = 0
    let mut i = 0
    while i < arr.len {
        sum = sum + arr[i]
        i = i + 1
    }
    print_str("sum = ")
    print_int(sum)
    print_str("\n")

    // --- Массив с выражениями ---
    print_str("\n--- Массив с выражениями ---\n")
    let x = 5
    let arr2 = [x, x + 1, x * 2]
    print_str("arr2 = [5, 6, 10]\n")
    print_str("arr2[1] = ")
    print_int(arr2[1])
    print_str("\n")

    // --- Поиск максимума ---
    print_str("\n--- Максимум ---\n")
    let mut max = arr[0]
    let mut j = 1
    while j < arr.len {
        if arr[j] > max {
            max = arr[j]
        }
        j = j + 1
    }
    print_str("max = ")
    print_int(max)
    print_str("\n")

    // --- Массив в цикле for ---
    print_str("\n--- for по массиву ---\n")
    for k in 0..arr.len {
        print_str("arr[")
        print_int(k)
        print_str("] = ")
        print_int(arr[k])
        print_str("\n")
    }
}
