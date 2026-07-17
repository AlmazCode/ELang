// Test: Arrays
// Basic array creation, index access, and .len

fn main() -> void {
    let arr = [10, 20, 30, 40, 50]
    let first = arr[0]
    let third = arr[2]
    let last = arr[4]
    let length = arr.len

    // Print results
    print_str("Array test\n")
    print_str("first: ")
    print_int(first)
    print_str("\n")

    print_str("third: ")
    print_int(third)
    print_str("\n")

    print_str("last: ")
    print_int(last)
    print_str("\n")

    print_str("length: ")
    print_int(length)
    print_str("\n")

    // Array in a loop
    let sum = 0
    let i = 0
    while i < arr.len {
        sum = sum + arr[i]
        i = i + 1
    }
    print_str("sum: ")
    print_int(sum)
    print_str("\n")

    // Array literal with expression
    let x = 5
    let arr2 = [x, x + 1, x * 2]
    print_str("arr2[1]: ")
    print_int(arr2[1])
    print_str("\n")
}
