; ELang Standard Library (Extended)
; Available with: using "std"
; Requires core.asm to be linked as well
section .text

; str_cmp(const char *a, const char *b) -> i64
; Compare two strings lexicographically
; Returns: <0 if a<b, 0 if a==b, >0 if a>b
global str_cmp
str_cmp:
.lp:
    movzx rax, byte [rdi]
    movzx rcx, byte [rsi]
    cmp al, cl
    jne .diff
    test al, al
    jz .eq
    inc rdi
    inc rsi
    jmp .lp
.diff:
    sub rax, rcx
    ret
.eq:
    xor rax, rax
    ret

; str_dup(const char *s) -> char*
; Duplicate a string - for now just return the original (no heap alloc)
global str_dup
str_dup:
    mov rax, rdi
    ret

; str_cat(const char *a, const char *b) -> char*
; Concatenate two strings - returns pointer to a (modified in place, NOT safe)
; NOTE: This is a simplified version that doesn't allocate new memory
global str_cat
str_cat:
    ; find end of a
    mov rax, rdi
.find_end:
    cmp byte [rax], 0
    je .copy_b
    inc rax
    jmp .find_end
.copy_b:
    ; copy b to end of a
    mov cl, [rsi]
    mov [rax], cl
    test cl, cl
    jz .done
    inc rax
    inc rsi
    jmp .copy_b
.done:
    mov rax, rdi
    ret

; read_input() -> char*
; Read a line from stdin using brk
global read_input
read_input:
    push rbx
    ; allocate buffer via brk(0)
    mov rdi, 0
    mov rax, 12
    syscall
    mov rbx, rax            ; rbx = buffer
    ; read from stdin
    mov rdi, 0              ; fd = stdin
    mov rsi, rbx            ; buf
    mov rdx, 4096           ; max count
    mov rax, 0              ; sys_read
    syscall
    ; null terminate (rax = bytes read)
    mov byte [rbx+rax], 0
    ; set new break after the string
    lea rdi, [rbx+rax+1]
    mov rax, 12
    syscall
    mov rax, rbx            ; return buffer
    pop rbx
    ret

; sys_brk(int addr) -> int
; Change the program break
global sys_brk
sys_brk:
    mov rax, 12
    syscall
    ret
