fn main() -> u8 {
    let i: i64 = 0
    loop {
        i = i + 1
        if i == 5 {
            break
        }
    }
    print(i)

    let sum: i64 = 0
    let j: i64 = 1
    while j <= 10 {
        if j == 3 {
            j = j + 1
            continue
        }
        sum = sum + j
        j = j + 1
    }
    print(sum)

    let x: i64 = 0
    for k: i64 in 1..10 {
        if k == 5 {
            break
        }
        x = x + k
    }
    print(x)

    0
}
