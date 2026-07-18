; ELang Math Library
; Available with: using "math"
section .text

; ============================================================
; Basic Arithmetic
; ============================================================

; add(a: i64, b: i64) -> i64
global add
add:
    mov rax, rdi
    add rax, rsi
    ret

; subtract(a: i64, b: i64) -> i64
global subtract
subtract:
    mov rax, rdi
    sub rax, rsi
    ret

; multiply(a: i64, b: i64) -> i64
global multiply
multiply:
    mov rax, rdi
    imul rax, rsi
    ret

; divide(a: i64, b: i64) -> i64
; Returns 0 if b == 0
global divide
divide:
    test rsi, rsi
    jz .zero
    mov rax, rdi
    cqo                 ; sign-extend rax into rdx:rax
    idiv rsi
    ret
.zero:
    xor rax, rax
    ret

; modulo(a: i64, b: i64) -> i64
; Returns 0 if b == 0
global modulo
modulo:
    test rsi, rsi
    jz .zero
    mov rax, rdi
    cqo
    idiv rsi
    mov rax, rdx        ; remainder is in rdx
    ret
.zero:
    xor rax, rax
    ret

; negate(x: i64) -> i64
global negate
negate:
    mov rax, rdi
    neg rax
    ret

; i64_abs(x: i64) -> i64
global i64_abs
i64_abs:
    mov rax, rdi
    test rax, rax
    jns .done
    neg rax
.done:
    ret

; ============================================================
; Comparison
; ============================================================

; min(a: i64, b: i64) -> i64
global min
min:
    mov rax, rdi
    cmp rax, rsi
    jle .done
    mov rax, rsi
.done:
    ret

; max(a: i64, b: i64) -> i64
global max
max:
    mov rax, rdi
    cmp rax, rsi
    jge .done
    mov rax, rsi
.done:
    ret

; clamp(value: i64, lo: i64, hi: i64) -> i64
global clamp
clamp:
    mov rax, rdi
    cmp rax, rsi
    jl .use_lo
    cmp rax, rdx
    jg .use_hi
    ret
.use_lo:
    mov rax, rsi
    ret
.use_hi:
    mov rax, rdx
    ret

; ============================================================
; Power & Roots
; ============================================================

; power(base: i64, exp: i64) -> i64
; Returns base^exp. Handles negative exponents (returns 0).
global power
power:
    mov r8, rdi         ; r8 = base
    mov r9, rsi         ; r9 = exp
    mov rax, 1          ; result = 1

    ; handle negative exponent
    test r9, r9
    jns .loop
    xor rax, rax        ; return 0 for negative exp
    ret

.loop:
    test r9, r9
    jz .done
    imul rax, r8        ; result *= base
    dec r9
    jmp .loop

.done:
    ret

; isqrt(n: i64) -> i64
; Integer square root (floor)
global isqrt
isqrt:
    mov rax, rdi        ; rax = n
    test rax, rax
    jz .zero

    ; Newton's method
    mov rcx, rax        ; rcx = n
    shr rcx, 1          ; rcx = n / 2 (initial guess)
    jz .one             ; if n < 2, return 1

.loop:
    mov rdx, rax        ; rdx = x (current)
    mov rax, rcx        ; rax = guess
    xor rdx, rdx
    div rcx             ; rax = n / guess
    add rax, rcx        ; rax = (n / guess) + guess
    shr rax, 1          ; rax = ((n / guess) + guess) / 2

    cmp rax, rcx
    jge .done
    mov rcx, rax        ; update guess
    jmp .loop

.done:
    mov rax, rcx
    ret

.zero:
    xor rax, rax
    ret

.one:
    mov rax, 1
    ret

; ============================================================
; Bit Operations
; ============================================================

; bitwise_and(a: i64, b: i64) -> i64
global bitwise_and
bitwise_and:
    mov rax, rdi
    and rax, rsi
    ret

; bitwise_or(a: i64, b: i64) -> i64
global bitwise_or
bitwise_or:
    mov rax, rdi
    or rax, rsi
    ret

; bitwise_xor(a: i64, b: i64) -> i64
global bitwise_xor
bitwise_xor:
    mov rax, rdi
    xor rax, rsi
    ret

; bitwise_not(x: i64) -> i64
global bitwise_not
bitwise_not:
    mov rax, rdi
    not rax
    ret

; shift_left(x: i64, n: i64) -> i64
global shift_left
shift_left:
    mov rax, rdi
    mov rcx, rsi
    shl rax, cl
    ret

; shift_right(x: i64, n: i64) -> i64
global shift_right
shift_right:
    mov rax, rdi
    mov rcx, rsi
    sar rax, cl         ; arithmetic shift right (preserves sign)
    ret

; ============================================================
; Random (simple LCG)
; ============================================================

section .data
rand_state: dq 12345    ; seed

section .text

; rand() -> i64
; Returns pseudo-random number
global rand
rand:
    mov rax, [rand_state]
    ; LCG: state = state * 6364136223846793005 + 1442695040888963407
    mov rcx, 6364136223846793005
    imul rax, rcx
    mov rcx, 1442695040888963407
    add rax, rcx
    mov [rand_state], rax
    ; return upper 32 bits for better distribution
    shr rax, 33
    ret

; srand(seed: i64) -> void
; Set random seed
global srand
srand:
    mov [rand_state], rdi
    ret

; rand_range(min: i64, max: i64) -> i64
; Returns random number in range [min, max]
global rand_range
rand_range:
    push rbx
    push r12
    mov rbx, rdi        ; rbx = min
    mov r12, rsi        ; r12 = max
    call rand           ; rax = random
    ; rax % (max - min + 1) + min
    mov rcx, r12
    sub rcx, rbx
    inc rcx             ; rcx = max - min + 1
    xor rdx, rdx
    div rcx             ; rax = rax / (max-min+1), rdx = remainder
    mov rax, rdx
    add rax, rbx        ; rax = remainder + min
    pop r12
    pop rbx
    ret

; ============================================================
; Trigonometry (integer approximations)
; ============================================================

; sin_approx(x: i64) -> i64
; Approximate sine using Taylor series (x in radians * 1000)
; Returns value * 1000 (fixed-point)
global sin_approx
sin_approx:
    ; Taylor: sin(x) ≈ x - x^3/6 + x^5/120 - x^7/5040
    ; Input: x * 1000 (fixed-point)
    mov rax, rdi        ; rax = x
    mov r8, rax         ; r8 = x
    imul rax, rax       ; rax = x^2
    mov r9, rax         ; r9 = x^2

    ; term1 = x
    mov r10, r8         ; r10 = result = x

    ; term2 = -x^3/6
    mov rax, r8
    imul rax, r9        ; rax = x^3
    mov rcx, 6
    cqo
    idiv rcx            ; rax = x^3/6
    neg rax
    add r10, rax        ; result += -x^3/6

    ; term3 = x^5/120
    mov rax, r8
    imul rax, r9        ; rax = x^3
    imul rax, r9        ; rax = x^5
    mov rcx, 120
    cqo
    idiv rcx
    add r10, rax        ; result += x^5/120

    mov rax, r10
    ret

; cos_approx(x: i64) -> i64
; Approximate cosine (x in radians * 1000)
; Returns value * 1000 (fixed-point)
global cos_approx
cos_approx:
    ; cos(x) = sin(x + pi/2)
    ; pi/2 ≈ 1571 (in fixed-point)
    add rdi, 1571
    call sin_approx
    ret

; ============================================================
; Clamping & Conversions
; ============================================================

; i64_to_f64(x: i64) -> f64
; Convert integer to float (returns bits in rax)
global i64_to_f64
i64_to_f64:
    cvtsi2sd xmm0, rdi
    ; Return bits in rax (for passing as i64)
    movq rax, xmm0
    ret

; f64_to_i64(bits: i64) -> i64
; Convert float bits back to integer
global f64_to_i64
f64_to_i64:
    movq xmm0, rdi
    cvttsd2si rax, xmm0
    ret
