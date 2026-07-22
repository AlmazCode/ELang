; ELang Core Library
; Always linked, no import required
section .text

; print_str(const char *str)
global print_str
print_str:
    push rbp
    mov rbp, rsp
    push rbx
    mov rsi, rdi
    xor rdx, rdx
.lp: cmp byte [rsi+rdx], 0
    je .done
    inc rdx
    jmp .lp
.done:
    mov rax, 1
    mov rdi, 1
    syscall
    pop rbx
    pop rbp
    ret

; print_int(long num)
global print_int
print_int:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov rax, rdi
    test rax, rax
    jns .positive
    push rax
    mov byte [rsp-1], '-'
    lea rsi, [rsp-1]
    mov rdx, 1
    mov rax, 1
    mov rdi, 1
    syscall
    pop rax
    neg rax
.positive:
    lea rsi, [rbp-1]
    mov byte [rsi], 0
    mov rcx, 10
    test rax, rax
    jnz .cv
    dec rsi
    mov byte [rsi], '0'
    jmp .pr
.cv: test rax, rax
    jz .pr
    xor rdx, rdx
    div rcx
    add dl, '0'
    dec rsi
    mov [rsi], dl
    jmp .cv
.pr: lea rdx, [rbp-1]
    sub rdx, rsi
    mov rax, 1
    mov rdi, 1
    syscall
    leave
    ret

; print_hex(long num)
global print_hex
print_hex:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov rax, rdi
    lea rsi, [rbp-1]
    mov byte [rsi], 0
    mov rcx, 16
    test rax, rax
    jnz .hp
    dec rsi
    mov byte [rsi], '0'
    jmp .hpr
.hp: test rax, rax
    jz .hpr
    xor rdx, rdx
    div rcx
    cmp dl, 10
    jl .hd
    add dl, 'a'-10
    jmp .hs
.hd: add dl, '0'
.hs: dec rsi
    mov [rsi], dl
    jmp .hp
.hpr: lea rdx, [rbp-1]
    sub rdx, rsi
    mov rax, 1
    mov rdi, 1
    syscall
    leave
    ret

; ============================================================
; Universal print functions
; ============================================================

; --- Format flags ---
FMT_SIGN    equ 1       ; show + for positive numbers
FMT_ZERO    equ 2       ; pad with zeros
FMT_LEFT    equ 4       ; left-align
FMT_BASE_SH equ 3       ; bits 3-5: base selector
FMT_BASE2   equ (1 << FMT_BASE_SH)   ; binary
FMT_BASE8   equ (2 << FMT_BASE_SH)   ; octal
FMT_BASE16  equ (3 << FMT_BASE_SH)   ; hex
FMT_UPPER   equ 64      ; uppercase hex (A-F)
FMT_BASE_MASK equ 0x38  ; mask for base bits

section .data
nz_sign: db "+", 0
neg_sign: db "-", 0
hex_chars: db "0123456789abcdef"
hex_upper: db "0123456789ABCDEF"

section .text

; print_i64(rdi=value)
; Print signed integer in decimal.
global print_i64
print_i64:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov rax, rdi

    ; --- Handle sign ---
    test rax, rax
    jns .pi_positive
    ; negative: print '-', negate
    push rax
    mov rax, 1
    mov rdi, 1
    lea rsi, [rel neg_sign]
    mov rdx, 1
    syscall
    pop rax
    neg rax
    jmp .pi_convert

.pi_positive:
    ; no sign prefix

.pi_convert:
    ; rax = absolute value
    lea rsi, [rbp-1]
    mov byte [rsi], 0       ; null terminator
    mov rcx, 10
    test rax, rax
    jnz .pi_loop
    dec rsi
    mov byte [rsi], '0'
    jmp .pi_print
.pi_loop:
    test rax, rax
    jz .pi_print
    xor rdx, rdx
    div rcx
    add dl, '0'
    dec rsi
    mov [rsi], dl
    jmp .pi_loop

.pi_print:
    lea rdx, [rbp-1]
    sub rdx, rsi            ; rdx = length
    mov rax, 1
    mov rdi, 1
    ; rsi already points to start of number
    syscall
    leave
    ret

; print_i64_fmt(rdi=value, rsi=width, rdx=flags)
; rdi = signed integer value
; rsi = minimum field width (0 = auto)
; rdx = format flags (FMT_SIGN | FMT_ZERO | FMT_LEFT | FMT_BASE* | FMT_UPPER)
global print_i64_fmt
print_i64_fmt:
    push rbp
    mov rbp, rsp
    sub rsp, 64             ; local buffer: [rbp-64] = start of number string
    mov [rbp-8], rdi        ; save value
    mov [rbp-16], rsi       ; save width
    mov [rbp-24], rdx       ; save flags

    ; --- Determine sign ---
    mov rax, rdi
    mov byte [rbp-25], 0    ; sign_char = 0 (no sign)
    test rax, rax
    jns .pfmt_check_sign
    ; negative: print '-', negate
    mov byte [rbp-25], '-'
    neg rax
    jmp .pfmt_convert
.pfmt_check_sign:
    test dl, FMT_SIGN
    jz .pfmt_convert
    mov byte [rbp-25], '+'

.pfmt_convert:
    ; --- Extract base from flags ---
    mov rcx, rdx
    and rcx, FMT_BASE_MASK
    shr rcx, FMT_BASE_SH
    ; rcx: 0=base10, 1=base2, 2=base8, 3=base16
    cmp rcx, 0
    je .pfmt_dec
    cmp rcx, 1
    je .pfmt_bin
    cmp rcx, 2
    je .pfmt_oct
    jmp .pfmt_hex

.pfmt_dec:
    mov rcx, 10
    jmp .pfmt_do_convert
.pfmt_bin:
    mov rcx, 2
    jmp .pfmt_do_convert
.pfmt_oct:
    mov rcx, 8
    jmp .pfmt_do_convert
.pfmt_hex:
    mov rcx, 16

.pfmt_do_convert:
    ; rax = |value|, rcx = base
    ; Convert to string (right to left), store at [rbp-2]
    lea rdi, [rbp-2]
    mov byte [rdi], 0       ; null terminator
    test rax, rax
    jnz .pfmt_loop
    ; value == 0 → special case
    dec rdi
    mov byte [rdi], '0'
    jmp .pfmt_reverse_done
.pfmt_loop:
    test rax, rax
    jz .pfmt_reverse_done
    xor rdx, rdx
    div rcx                 ; rax = quotient, rdx = remainder
    ; Look up hex digit
    cmp rdx, 10
    jl .pfmt_digit_dec
    ; hex digit
    test byte [rbp-24], FMT_UPPER
    jnz .pfmt_digit_upper
    lea r8, [rel hex_chars]
    jmp .pfmt_digit_store
.pfmt_digit_upper:
    lea r8, [rel hex_upper]
.pfmt_digit_store:
    add r8, rdx
    mov dl, [r8]
    jmp .pfmt_digit_write
.pfmt_digit_dec:
    add dl, '0'
.pfmt_digit_write:
    dec rdi
    mov [rdi], dl
    jmp .pfmt_loop

.pfmt_reverse_done:
    ; rdi = pointer to start of number string
    ; Calculate length: rbp-2 - rdi
    lea rax, [rbp-2]
    sub rax, rdi
    mov [rbp-32], rax       ; num_len
    mov [rbp-48], rdi       ; save num_start pointer

    ; --- Print sign character if present ---
    cmp byte [rbp-25], 0
    je .pfmt_calc_padding
    mov rax, 1
    mov rdi, 1
    lea rsi, [rbp-25]
    mov rdx, 1
    syscall
    ; Reduce width by 1 for sign
    dec qword [rbp-16]

.pfmt_calc_padding:
    ; --- Calculate padding ---
    mov rax, [rbp-16]       ; width
    sub rax, [rbp-32]       ; width - num_len
    jle .pfmt_print_num     ; no padding needed
    mov [rbp-40], rax       ; pad_count

    ; --- Print left padding ---
    test byte [rbp-24], FMT_LEFT
    jnz .pfmt_print_num     ; skip left padding if left-aligned

    ; Choose pad char: '0' if FMT_ZERO, else ' '
    mov dl, ' '
    test byte [rbp-24], FMT_ZERO
    jz .pfmt_pad_loop
    mov dl, '0'
.pfmt_pad_loop:
    cmp qword [rbp-40], 0
    jle .pfmt_print_num
    mov byte [rbp-41], dl
    mov rax, 1
    mov rdi, 1
    lea rsi, [rbp-41]
    mov rdx, 1
    syscall
    dec qword [rbp-40]
    jmp .pfmt_pad_loop

.pfmt_print_num:
    ; --- Print the number ---
    mov rax, 1
    mov rdi, 1
    mov rsi, [rbp-48]       ; num_start pointer (saved before sign print)
    mov rdx, [rbp-32]       ; num_len
    syscall

    ; --- Print right padding (if left-aligned) ---
    test byte [rbp-24], FMT_LEFT
    jz .pfmt_done
    mov rax, [rbp-16]
    sub rax, [rbp-32]
    jle .pfmt_done
    mov [rbp-40], rax
.pfmt_rpad_loop:
    cmp qword [rbp-40], 0
    jle .pfmt_done
    mov byte [rbp-41], ' '
    mov rax, 1
    mov rdi, 1
    lea rsi, [rbp-41]
    mov rdx, 1
    syscall
    dec qword [rbp-40]
    jmp .pfmt_rpad_loop

.pfmt_done:
    leave
    ret

; print_u64(rdi=value)
; Print unsigned integer in decimal.
global print_u64
print_u64:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov rax, rdi
    lea rsi, [rbp-1]
    mov byte [rsi], 0
    mov rcx, 10
    test rax, rax
    jnz .pu_loop
    dec rsi
    mov byte [rsi], '0'
    jmp .pu_pr
.pu_loop:
    test rax, rax
    jz .pu_pr
    xor rdx, rdx
    div rcx
    add dl, '0'
    dec rsi
    mov [rsi], dl
    jmp .pu_loop
.pu_pr: lea rdx, [rbp-1]
    sub rdx, rsi
    mov rax, 1
    mov rdi, 1
    syscall
    leave
    ret

; print_bool(rdi=value)
; Print boolean: 0 → "false", non-zero → "true"
section .data
bool_true: db "true", 0
bool_true_len equ 4
bool_false: db "false", 0
bool_false_len equ 5
section .text
global print_bool
print_bool:
    test rdi, rdi
    jnz .pb_true
    ; print "false"
    mov rax, 1
    mov rdi, 1
    lea rsi, [bool_false]
    mov rdx, bool_false_len
    syscall
    ret
.pb_true:
    mov rax, 1
    mov rdi, 1
    lea rsi, [bool_true]
    mov rdx, bool_true_len
    syscall
    ret

; print_f64(rdi=bits)
; Print IEEE 754 double as decimal string.
; rdi contains the double bits (as integer).
section .data
f64_dot: db ".", 0
f64_neg: db "-", 0
section .text
global print_f64
print_f64:
    push rbp
    mov rbp, rsp
    sub rsp, 64

    ; Save original bits
    mov [rbp-8], rdi

    ; Load double into xmm0
    movq xmm0, rdi

    ; Check sign
    mov rax, rdi
    shr rax, 63
    test rax, rax
    jz .pf64_abs
    ; Print '-'
    push rdi
    mov rax, 1
    mov rdi, 1
    lea rsi, [rel f64_neg]
    mov rdx, 1
    syscall
    pop rdi
    ; Negate: flip sign bit
    btc rdi, 63
    movq xmm0, rdi
    mov [rbp-8], rdi         ; save negated bits

.pf64_abs:
    ; Extract integer part via truncation
    cvttsd2si rax, xmm0
    mov [rbp-16], rax        ; save integer part

    ; Print integer part
    mov rdi, [rbp-16]
    call print_i64

    ; Extract fractional part: frac = original - floor(original)
    ; Reload the (possibly negated) double
    movq xmm0, [rbp-8]
    cvttsd2si rax, xmm0      ; rax = truncated integer
    cvtsi2sd xmm1, rax       ; xmm1 = integer as double
    subsd xmm0, xmm1         ; xmm0 = fractional part (0.0 to 0.999...)

    ; Check if fractional part is effectively zero
    movsd xmm1, [rel f64_zero]
    comisd xmm0, xmm1
    jp .pf64_done
    jbe .pf64_done

    ; Print '.'
    push rdi
    mov rax, 1
    mov rdi, 1
    lea rsi, [rel f64_dot]
    mov rdx, 1
    syscall
    pop rdi

    ; Print up to 6 decimal digits
    mov rcx, 6
.pf64_digit_loop:
    test rcx, rcx
    jz .pf64_done
    ; digit = frac * 10
    movsd xmm1, [rel f64_ten]
    mulsd xmm0, xmm1
    ; extract integer digit
    cvttsd2si rax, xmm0
    mov [rbp-24], rax
    ; print digit character
    add al, '0'
    mov [rbp-25], al
    push rcx
    push rdi
    mov rax, 1
    mov rdi, 1
    lea rsi, [rbp-25]
    mov rdx, 1
    syscall
    pop rdi
    pop rcx
    ; frac = frac - digit (via xmm0 still has fractional*10, subtract integer part)
    mov rax, [rbp-24]
    cvtsi2sd xmm1, rax
    subsd xmm0, xmm1
    ; Check if remaining frac is effectively zero
    movsd xmm1, [rel f64_zero]
    comisd xmm0, xmm1
    jp .pf64_done
    jbe .pf64_done
    dec rcx
    jmp .pf64_digit_loop

.pf64_done:
    leave
    ret

section .data
f64_zero: dq 0x3E45798EE2308C3A  ; ~1e-6 (threshold for "zero")
f64_ten: dq 0x4024000000000000    ; 10.0

; print_ptr(rdi=value)
; Print pointer as "0x" + hex digits (always 12 hex digits for 64-bit).
section .data
ptr_prefix: db "0x", 0
ptr_zeros: db "000000000000", 0  ; 12 zeros
section .text
global print_ptr
print_ptr:
    push rbp
    mov rbp, rsp
    push rbx
    mov rbx, rdi            ; save value

    ; print "0x"
    mov rax, 1
    mov rdi, 1
    lea rsi, [ptr_prefix]
    mov rdx, 2
    syscall

    ; Convert 64-bit value to 16 hex digits (right to left)
    mov rax, rbx
    mov rcx, 16             ; digit count
    lea rsi, [rbp-1]
    mov byte [rsi], 0       ; null terminator
.pp_loop:
    dec rsi
    mov rdx, rax
    and rdx, 0xF
    lea rdi, [rel hex_chars]
    add rdi, rdx
    mov dl, [rdi]
    mov [rsi], dl
    shr rax, 4
    dec rcx
    jnz .pp_loop

    ; Print 16 hex digits
    mov rax, 1
    mov rdi, 1
    mov rdx, 16
    ; rsi already points to start
    syscall

    pop rbx
    pop rbp
    ret

; print_newline()
; Print a newline character.
global print_newline
print_newline:
    push rbp
    mov rbp, rsp
    mov rax, 1
    mov rdi, 1
    lea rsi, [newline_char]
    mov rdx, 1
    syscall
    pop rbp
    ret

; print_arr_i64(rdi=arr_ptr)
; Print array of i64 as "[1, 2, 3]"
section .data
global arr_open
arr_open: db "[", 0
global arr_close
arr_close: db "]", 0
global arr_sep
arr_sep: db ", ", 0
global str_lparen
str_lparen: db "(", 0
global str_rparen
str_rparen: db ")", 0
global str_comma
str_comma: db ", ", 0
section .text
global print_arr_i64
print_arr_i64:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    mov rbx, rdi            ; rbx = arr_ptr

    ; print "["
    mov rax, 1
    mov rdi, 1
    lea rsi, [arr_open]
    mov rdx, 1
    syscall

    mov r13, [rbx - 8]      ; r13 = length
    test r13, r13
    jz .pai_done

    xor r12, r12            ; r12 = i = 0
.pai_loop:
    ; print element
    mov rdi, [rbx + r12*8]
    call print_i64
    inc r12
    cmp r12, r13
    jge .pai_done
    ; print ", "
    mov rax, 1
    mov rdi, 1
    lea rsi, [arr_sep]
    mov rdx, 2
    syscall
    jmp .pai_loop

.pai_done:
    ; print "]"
    mov rax, 1
    mov rdi, 1
    lea rsi, [arr_close]
    mov rdx, 1
    syscall

    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; print_arr_str(rdi=arr_ptr)
; Print array of strings as ["a", "b"]
section .data
str_quote: db '"', 0
section .text
global print_arr_str
print_arr_str:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    mov rbx, rdi

    mov rax, 1
    mov rdi, 1
    lea rsi, [arr_open]
    mov rdx, 1
    syscall

    mov r13, [rbx - 8]
    test r13, r13
    jz .pas_done

    xor r12, r12
.pas_loop:
    ; print '"'
    mov rax, 1
    mov rdi, 1
    lea rsi, [str_quote]
    mov rdx, 1
    syscall
    ; print string content
    mov rdi, [rbx + r12*8]
    call print_str
    ; print '"'
    mov rax, 1
    mov rdi, 1
    lea rsi, [str_quote]
    mov rdx, 1
    syscall
    inc r12
    cmp r12, r13
    jge .pas_done
    ; print ", "
    mov rax, 1
    mov rdi, 1
    lea rsi, [arr_sep]
    mov rdx, 2
    syscall
    jmp .pas_loop

.pas_done:
    mov rax, 1
    mov rdi, 1
    lea rsi, [arr_close]
    mov rdx, 1
    syscall

    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; print_arr_bool(rdi=arr_ptr)
; Print array of bools as [true, false, true]
global print_arr_bool
print_arr_bool:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    mov rbx, rdi

    ; print "["
    mov rax, 1
    mov rdi, 1
    lea rsi, [arr_open]
    mov rdx, 1
    syscall

    mov r13, [rbx - 8]     ; length
    test r13, r13
    jz .pab_done

    xor r12, r12            ; i = 0
.pab_loop:
    ; print element
    mov rdi, [rbx + r12*8]
    call print_bool
    inc r12
    cmp r12, r13
    jge .pab_done
    ; print ", "
    mov rax, 1
    mov rdi, 1
    lea rsi, [arr_sep]
    mov rdx, 2
    syscall
    jmp .pab_loop

.pab_done:
    ; print "]"
    mov rax, 1
    mov rdi, 1
    lea rsi, [arr_close]
    mov rdx, 1
    syscall

    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; print_arr_f64(rdi=arr_ptr)
; Print array of f64 as [1.000000, 2.000000]
global print_arr_f64
print_arr_f64:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    mov rbx, rdi

    mov rax, 1
    mov rdi, 1
    lea rsi, [arr_open]
    mov rdx, 1
    syscall

    mov r13, [rbx - 8]
    test r13, r13
    jz .paf_done

    xor r12, r12
.paf_loop:
    ; load float bits and call print_f64
    mov rdi, [rbx + r12*8]
    call print_f64
    inc r12
    cmp r12, r13
    jge .paf_done
    ; print ", "
    mov rax, 1
    mov rdi, 1
    lea rsi, [arr_sep]
    mov rdx, 2
    syscall
    jmp .paf_loop

.paf_done:
    mov rax, 1
    mov rdi, 1
    lea rsi, [arr_close]
    mov rdx, 1
    syscall

    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; exit(int code)
global exit
exit:
    mov rax, 60
    syscall

; str_len(const char *str)
global str_len
str_len:
    xor rax, rax
.lp: cmp byte [rdi+rax], 0
    je .dn
    inc rax
    jmp .lp
.dn: ret

; sys_write(int fd, const void *buf, int count)
global sys_write
sys_write:
    mov rax, 1
    syscall
    ret

; sys_read(int fd, void *buf, int count)
global sys_read
sys_read:
    mov rax, 0
    syscall
    ret

; sys_exit(int code)
global sys_exit
sys_exit:
    mov rax, 60
    syscall

; sys_getpid()
global sys_getpid
sys_getpid:
    mov rax, 39
    syscall
    ret

; ===== Error Handling Runtime =====

section .data
panic_prefix: db "panic at "
panic_prefix_len equ $ - panic_prefix
panic_colon: db ":"
panic_colon_len equ $ - panic_colon
panic_sep: db ": "
panic_sep_len equ $ - panic_sep
newline_char: db 0x0A

section .text
global panic_handler
panic_handler:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    mov rbx, rdi
    mov r12, rsi
    mov r13, rdx

    mov rax, 1
    mov rdi, 2
    lea rsi, [panic_prefix]
    mov rdx, panic_prefix_len
    syscall

    test r13, r13
    jz .panic_line
    mov rdi, r13
    call str_len
    mov rdx, rax
    mov rax, 1
    mov rdi, 2
    mov rsi, r13
    syscall
    jmp .panic_colon1

.panic_line:
    mov rax, 1
    mov rdi, 2
    lea rsi, [.input_label]
    mov rdx, 7
    syscall

.panic_colon1:
    mov rax, 1
    mov rdi, 2
    lea rsi, [panic_colon]
    mov rdx, panic_colon_len
    syscall

    mov rdi, r12
    call print_int_to_stderr

    mov rax, 1
    mov rdi, 2
    lea rsi, [panic_sep]
    mov rdx, panic_sep_len
    syscall

    test rbx, rbx
    jz .panic_newline
    mov rdi, rbx
    call str_len
    mov rdx, rax
    mov rax, 1
    mov rdi, 2
    mov rsi, rbx
    syscall

.panic_newline:
    mov rax, 1
    mov rdi, 2
    lea rsi, [newline_char]
    mov rdx, 1
    syscall

    mov rdi, 1
    mov rax, 60
    syscall

section .data
.input_label: db "<input>", 0

section .text
print_int_to_stderr:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov rax, rdi
    test rax, rax
    jns .its_pos
    push rax
    mov byte [rsp-1], '-'
    lea rsi, [rsp-1]
    mov rdx, 1
    mov rax, 1
    mov rdi, 2
    syscall
    pop rax
    neg rax
.its_pos:
    lea rsi, [rbp-1]
    mov byte [rsi], 0
    mov rcx, 10
    test rax, rax
    jnz .its_cv
    dec rsi
    mov byte [rsi], '0'
    jmp .its_pr
.its_cv: test rax, rax
    jz .its_pr
    xor rdx, rdx
    div rcx
    add dl, '0'
    dec rsi
    mov [rsi], dl
    jmp .its_cv
.its_pr: lea rdx, [rbp-1]
    sub rdx, rsi
    mov rax, 1
    mov rdi, 2
    syscall
    leave
    ret

; assert_handler(long condition, const char *msg)
section .data
assert_prefix: db "assertion failed: ", 0
assert_prefix_len equ $ - assert_prefix

section .text
global assert_handler
assert_handler:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    mov rbx, rdi
    mov r12, rsi

    test rbx, rbx
    jnz .assert_ok

    mov rax, 1
    mov rdi, 2
    lea rsi, [assert_prefix]
    mov rdx, assert_prefix_len
    syscall

    test r12, r12
    jz .assert_newline
    mov rdi, r12
    call str_len
    mov rdx, rax
    mov rax, 1
    mov rdi, 2
    mov rsi, r12
    syscall

.assert_newline:
    mov rax, 1
    mov rdi, 2
    lea rsi, [newline_char]
    mov rdx, 1
    syscall

    mov rdi, 2
    mov rax, 60
    syscall

.assert_ok:
    pop r12
    pop rbx
    pop rbp
    ret

; ===== Array Runtime =====
; Heap-allocated arrays. Layout at pointer:
;   [ptr - 32] = elem_size  (qword) — bytes per element
;   [ptr - 24] = refcount   (qword)
;   [ptr - 16] = capacity   (qword)
;   [ptr - 8]  = length     (qword)
;   [ptr + 0]  = elem[0], [ptr + elem_size] = elem[1], ...
; Pointer returned points to elem[0].

section .text

; _bump_alloc(size) -> ptr using brk syscall. Never frees.
; NOTE: syscall clobbers rcx and r11, so we save aligned base on stack.
global _bump_alloc
_bump_alloc:
    push rbx
    mov rbx, rdi            ; save size
    ; get current break
    xor rdi, rdi
    mov rax, 12             ; sys_brk
    syscall
    ; rax = current break (aligned base)
    add rax, 15
    and rax, -16
    push rax                ; save aligned base on stack (safe from syscall clobbers)
    ; set new break = aligned + size
    lea rdi, [rax + rbx]
    mov rax, 12
    syscall
    cmp rax, -1
    je .alloc_fail
    pop rax                 ; restore aligned base
    pop rbx
    ret
.alloc_fail:
    add rsp, 8              ; discard saved base
    lea rdi, [alloc_fail_msg]
    xor rsi, rsi
    xor rdx, rdx
    call panic_handler

; _memmove(dst, src, n)
_memmove:
    push rbx
    xor rax, rax
.mm_loop:
    cmp rax, rdx
    jge .mm_done
    mov bl, [rsi + rax]
    mov [rdi + rax], bl
    inc rax
    jmp .mm_loop
.mm_done:
    mov rax, rdi
    pop rbx
    ret

; ===== Watermark Allocator =====
; save_watermark() -> saved_break using brk(0)
; Restore watermark to automatically free all bump-allocated memory since save.
global save_watermark
save_watermark:
    xor rdi, rdi
    mov rax, 12
    syscall                 ; rax = current break
    ret

; restore_watermark(saved_break)
; Only restores if no live arrays (all refcounts == 1 in the freed region).
; For simplicity: always restore. Caller must ensure no arrays survive.
global restore_watermark
restore_watermark:
    mov rax, rdi            ; saved break
    mov rdi, rax
    mov rax, 12             ; sys_brk
    syscall
    ret

; create(int elem_size, int count) -> array_ptr
; rdi = elem_size, rsi = count
global create
create:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    mov rbx, rdi
    mov r12, rsi
    imul rdi, rsi
    add rdi, 32             ; 32-byte header (elem_size + refcount + capacity + length)
    call _bump_alloc
    test rax, rax
    jz .ac_fail
    mov [rax], rbx          ; elem_size
    mov qword [rax + 8], 1  ; refcount = 1
    mov [rax + 16], r12     ; capacity = count
    mov [rax + 24], r12     ; length = count
    add rax, 32             ; skip header
    pop r12
    pop rbx
    pop rbp
    ret
.ac_fail:
    lea rdi, [alloc_fail_msg]
    xor rsi, rsi
    xor rdx, rdx
    call panic_handler

; with_capacity(int elem_size, int cap) -> array_ptr
; Allocates capacity=cap, length=0. For building arrays with push.
; rdi = elem_size, rsi = capacity
global with_capacity
with_capacity:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    mov rbx, rdi
    mov r12, rsi
    imul rdi, rsi
    add rdi, 32
    call _bump_alloc
    test rax, rax
    jz .awc_fail
    mov [rax], rbx          ; elem_size
    mov qword [rax + 8], 1  ; refcount = 1
    mov [rax + 16], r12     ; capacity
    mov qword [rax + 24], 0 ; length = 0
    add rax, 32
    pop r12
    pop rbx
    pop rbp
    ret
.awc_fail:
    lea rdi, [alloc_fail_msg]
    xor rsi, rsi
    xor rdx, rdx
    call panic_handler

; get(array_ptr, int index) -> elem
; rdi = array_ptr, rsi = index
global get
get:
    push rbx
    mov rbx, rdi
    mov rdx, [rdi - 8]      ; length
    test rsi, rsi
    js .oob
    cmp rsi, rdx
    jge .oob
    mov rax, [rdi - 32]     ; elem_size
    imul rax, rsi           ; offset = index * elem_size
    mov rax, [rbx + rax]    ; load element
    pop rbx
    ret
.oob:
    lea rdi, [oob_msg]
    xor rsi, rsi
    xor rdx, rdx
    call panic_handler

; len(array_ptr) -> length
global len
len:
    mov rax, [rdi - 8]      ; length
    ret

; push(array_ptr, elem, elem_size) -> new_array_ptr
; rdi = array_ptr, rsi = elem, rcx = elem_size (bytes per element)
; NOTE: named array_push to avoid conflict with x86 'push' instruction
global array_push
array_push:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov rbx, rdi
    mov r12, rsi
    mov r15, rcx            ; r15 = elem_size
    mov r13, [rdi - 8]      ; length
    mov r14, [rdi - 16]     ; capacity
    cmp r13, r14
    jl .push_store
    mov rcx, r14
    test rcx, rcx
    jnz .push_not_zero
    mov rcx, 4
    jmp .push_have_cap
.push_not_zero:
    shl rcx, 1
.push_have_cap:
    push rcx
    mov rdi, rcx
    imul rdi, r15
    add rdi, 32             ; 32-byte header
    call _bump_alloc
    pop rcx
    test rax, rax
    jz .push_fail
    mov r8, rax            ; save new ptr
    mov rdi, rax
    lea rsi, [rbx - 32]    ; copy from old header
    mov rdx, r13
    imul rdx, r15          ; rdx = length * elem_size
    add rdx, 32            ; + header
    call _memmove
    mov rax, r8            ; restore new ptr
    mov [rax], r15         ; store elem_size at offset 0
    mov [rax+16], rcx      ; store new capacity at offset 16
    mov rbx, rax
    add rbx, 32            ; rbx = pointer to elem[0]
.push_store:
    ; Store element at [rbx + r13 * elem_size]
    mov rax, r13
    imul rax, r15
    mov [rbx + rax], r12
    inc r13
    mov [rbx - 8], r13     ; update length
    mov rax, rbx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
.push_fail:
    lea rdi, [alloc_fail_msg]
    xor rsi, rsi
    xor rdx, rdx
    call panic_handler

; pop(array_ptr) -> elem
global pop
pop:
    push rbx
    mov rbx, rdi
    mov rax, [rdi - 8]      ; length
    test rax, rax
    jz .pop_empty
    dec rax
    mov [rdi - 8], rax      ; update length
    mov rcx, [rdi - 32]     ; elem_size
    imul rcx, rax
    mov rax, [rbx + rcx]    ; load element
    pop rbx
    ret
.pop_empty:
    lea rdi, [pop_empty_msg]
    xor rsi, rsi
    xor rdx, rdx
    call panic_handler

; slice(array_ptr, long start, long end) -> new_array_ptr
; rdi = array_ptr, rsi = start, rdx = end
global slice
slice:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    mov rbx, rdi
    mov r12, rsi
    mov r13, rdx
    mov rcx, [rbx - 8]      ; length
    cmp r12, 0
    jl .slice_oob
    cmp r13, rcx
    jg .slice_oob
    cmp r12, r13
    jg .slice_oob
    mov r14, r13
    sub r14, r12
    mov rdi, [rbx - 32]     ; elem_size
    mov rsi, r14
    call create
    push rax
    mov rdi, rax
    ; source = rbx + r12 * elem_size
    mov rcx, [rbx - 32]     ; elem_size
    imul rcx, r12
    lea rsi, [rbx + rcx]
    mov rdx, r14
    imul rdx, [rbx - 32]    ; count * elem_size
    call _memmove
    pop rax
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
.slice_oob:
    lea rdi, [slice_oob_msg]
    xor rsi, rsi
    xor rdx, rdx
    call panic_handler

; concat(array_ptr a, array_ptr b) -> new_array_ptr
; rdi = a, rsi = b
global concat
concat:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    mov rbx, rdi
    mov r12, rsi
    mov r13, [rbx - 8]      ; a.length
    add r13, [r12 - 8]      ; + b.length
    mov rdi, [rbx - 32]     ; elem_size (from a)
    mov rsi, r13
    call create
    push rax
    mov rdi, rax
    mov rsi, rbx
    mov rdx, [rbx - 8]     ; a.length
    imul rdx, [rbx - 32]    ; * elem_size
    call _memmove
    pop rax
    push rax
    mov rcx, [rbx - 8]     ; a.length
    imul rcx, [rbx - 32]    ; * elem_size
    lea rdi, [rax + rcx]
    mov rsi, r12
    mov rdx, [r12 - 8]      ; b.length
    imul rdx, [r12 - 32]    ; * elem_size
    call _memmove
    pop rax
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; map(array_ptr, closure_ptr) -> new_array_ptr
; closure_ptr points to [fn_ptr:8][env_ptr:8]
; fn is called as: fn(env_ptr, elem) -> result
; rdi = array_ptr, rsi = closure_ptr
global map
map:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 400            ; frame must be large enough for closure's local vars
    mov rbx, rdi            ; array_ptr
    mov r12, rsi            ; closure_ptr
    mov r13, [rbx - 8]     ; length
    ; extract fn_ptr and env_ptr from closure object
    mov r14, [r12]          ; fn_ptr (first qword of closure object)
    mov r15, [r12 + 8]      ; env_ptr (second qword)
    ; allocate new array
    mov rdi, 8
    mov rsi, r13
    call create
    mov [rbp - 48], rax     ; store new_ptr
    ; Use stack slot for loop counter (avoids push/pop alignment issues)
    mov qword [rbp - 56], 0 ; counter = 0
.map_iter:
    mov rcx, [rbp - 56]     ; load counter
    cmp rcx, r13
    jge .map_done
    mov rdi, r15            ; env_ptr
    mov rsi, [rbx + rcx*8]  ; elem → rsi (closure convention: rdi=env, rsi=arg0)
    call r14                 ; fn(env_ptr, elem)
    mov rcx, [rbp - 56]     ; reload counter (call may clobber rcx)
    mov r8, [rbp - 48]
    mov [r8 + rcx*8], rax
    inc qword [rbp - 56]
    jmp .map_iter
.map_done:
    mov rax, [rbp - 48]
    add rsp, 400
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; filter(array_ptr, closure_ptr) -> new_array_ptr
; closure_ptr points to [fn_ptr:8][env_ptr:8]
; fn is called as: fn(env_ptr, elem) -> keep? (non-zero = keep)
; rdi = array_ptr, rsi = closure_ptr
global filter
filter:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 24             ; three slots: [rbp-48]=new_ptr, [rbp-56]=result_count, [rbp-64]=i
    mov rbx, rdi            ; array_ptr
    mov r12, rsi            ; closure_ptr
    mov r13, [rbx - 8]     ; length
    mov r14, [r12]          ; fn_ptr
    mov r15, [r12 + 8]      ; env_ptr
    ; allocate result array (same capacity)
    mov rdi, 8
    mov rsi, r13
    call create
    mov [rbp - 48], rax     ; new_ptr
    mov qword [rbp - 56], 0 ; result_count = 0
    mov qword [rbp - 64], 0 ; i = 0
.filter_iter:
    mov rcx, [rbp - 64]     ; reload i
    cmp rcx, r13
    jge .filter_done
    mov rdi, r15            ; env_ptr
    mov rsi, [rbx + rcx*8]  ; elem
    call r14                 ; fn(env_ptr, elem)
    test rax, rax
    jz .filter_skip
    ; keep element: copy via register
    mov r8, [rbp - 48]
    mov r9, [rbp - 56]
    mov rcx, [rbp - 64]     ; reload i
    mov r10, [rbx + rcx*8]
    mov [r8 + r9*8], r10
    inc qword [rbp - 56]
.filter_skip:
    inc qword [rbp - 64]
    jmp .filter_iter
.filter_done:
    ; fix result length
    mov r8, [rbp - 48]
    mov r9, [rbp - 56]
    mov [r8 - 8], r9        ; length = result_count
    mov rax, [rbp - 48]
    add rsp, 24
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; reduce(array_ptr, init, closure_ptr) -> accumulator
; closure_ptr points to [fn_ptr:8][env_ptr:8]
; fn is called as: fn(env_ptr, acc, elem) -> new_acc
; rdi = array_ptr, rsi = init, rdx = closure_ptr
global reduce
reduce:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 8             ; one slot for loop counter
    mov rbx, rdi            ; array_ptr
    mov r12, rdx            ; closure_ptr
    mov r13, [rbx - 8]     ; length
    mov r14, [r12]          ; fn_ptr
    mov r15, [r12 + 8]      ; env_ptr
    mov rax, rsi            ; acc = init
    mov qword [rbp - 48], 0 ; i = 0
.reduce_iter:
    mov rcx, [rbp - 48]     ; reload i
    cmp rcx, r13
    jge .reduce_done
    mov rdi, r15            ; env_ptr
    mov rsi, rax             ; acc
    mov rdx, [rbx + rcx*8]  ; elem
    call r14                 ; fn(env_ptr, acc, elem)
    inc qword [rbp - 48]
    jmp .reduce_iter
.reduce_done:
    add rsp, 8
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; free(array_ptr)
; Decrements refcount. When refcount reaches 0, memory is NOT freed individually
; because bump allocator doesn't support per-object deallocation.
; All memory is reclaimed at process exit. For scoped lifetimes, use
; save_watermark()/restore_watermark() instead.
global free
free:
    test rdi, rdi
    jz .free_done
    mov rax, [rdi - 24]     ; load refcount
    dec rax
    mov [rdi - 24], rax     ; store decremented refcount
.free_done:
    ret

; retain(array_ptr)
; Increments refcount. Call when passing array to another function.
global retain
retain:
    test rdi, rdi
    jz .retain_done
    inc qword [rdi - 24]
.retain_done:
    mov rax, rdi
    ret

section .data
oob_msg:        db "index out of bounds", 0
pop_empty_msg:  db "pop from empty array", 0
slice_oob_msg:  db "slice index out of bounds", 0
alloc_fail_msg: db "out of memory", 0
global div_zero_msg
div_zero_msg:   db "division by zero", 0
