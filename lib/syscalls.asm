; ELang Standard Library
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

; panic_handler(const char *msg)
; Prints "panic: <msg>\n" and exits with code 1
section .data
panic_prefix: db "panic: ", 0
panic_prefix_len equ $ - panic_prefix
newline_char: db 0x0A

section .text
global panic_handler
panic_handler:
    push rbp
    mov rbp, rsp
    push rbx
    mov rbx, rdi            ; save message pointer

    ; print "panic: "
    mov rax, 1              ; sys_write
    mov rdi, 1              ; stdout
    lea rsi, [panic_prefix]
    mov rdx, panic_prefix_len
    syscall

    ; print message
    test rbx, rbx
    jz .panic_newline
    mov rdi, rbx
    call str_len
    mov rdx, rax
    mov rax, 1
    mov rdi, 1
    mov rsi, rbx
    syscall

.panic_newline:
    ; print newline
    mov rax, 1
    mov rdi, 1
    lea rsi, [newline_char]
    mov rdx, 1
    syscall

    ; exit(1)
    mov rdi, 1
    mov rax, 60
    syscall

; assert_handler(long condition, const char *msg)
; If condition is 0, prints "assertion failed: <msg>\n" and exits with code 1
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
    mov rbx, rdi            ; save condition
    mov r12, rsi            ; save message pointer

    test rbx, rbx
    jnz .assert_ok

    ; print "assertion failed: "
    mov rax, 1
    mov rdi, 1
    lea rsi, [assert_prefix]
    mov rdx, assert_prefix_len
    syscall

    ; print message
    test r12, r12
    jz .assert_newline
    mov rdi, r12
    call str_len
    mov rdx, rax
    mov rax, 1
    mov rdi, 1
    mov rsi, r12
    syscall

.assert_newline:
    ; print newline
    mov rax, 1
    mov rdi, 1
    lea rsi, [newline_char]
    mov rdx, 1
    syscall

    ; exit(1)
    mov rdi, 1
    mov rax, 60
    syscall

.assert_ok:
    pop r12
    pop rbx
    pop rbp
    ret
