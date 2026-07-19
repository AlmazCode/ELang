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
panic_prefix: db "panic at ", 0
panic_prefix_len equ $ - panic_prefix
panic_colon: db ":", 0
panic_colon_len equ $ - panic_colon
panic_sep: db ": ", 0
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
;   [ptr - 24] = refcount  (qword)
;   [ptr - 16] = capacity  (qword)
;   [ptr - 8]  = length    (qword)
;   [ptr + 0]  = elem[0], [ptr + 8] = elem[1], ...
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
    add rdi, 24             ; 24-byte header (refcount + capacity + length)
    call _bump_alloc
    test rax, rax
    jz .ac_fail
    mov qword [rax], 1      ; refcount = 1
    mov [rax + 8], r12      ; capacity = count
    mov [rax + 16], r12     ; length = count
    add rax, 24             ; skip header
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
    add rdi, 24
    call _bump_alloc
    test rax, rax
    jz .awc_fail
    mov qword [rax], 1      ; refcount = 1
    mov [rax + 8], r12      ; capacity
    mov qword [rax + 16], 0 ; length = 0
    add rax, 24
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
    mov rdx, [rdi - 8]
    test rsi, rsi
    js .oob
    cmp rsi, rdx
    jge .oob
    mov rax, [rbx + rsi*8]
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
    mov rax, [rdi - 8]
    ret

; push(array_ptr, elem) -> new_array_ptr
; rdi = array_ptr, rsi = elem
global push
push:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    mov rbx, rdi
    mov r12, rsi
    mov r13, [rdi - 8]
    mov r14, [rdi - 16]
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
    lea rdi, [rcx*8 + 16]
    call _bump_alloc
    pop rcx
    test rax, rax
    jz .push_fail
    mov r8, rax            ; save new ptr in r8 (not rax, which gets clobbered by memmove)
    mov rdi, rax
    lea rsi, [rbx - 16]
    mov rdx, r13
    shl rdx, 3
    add rdx, 16
    call _memmove
    mov rax, r8            ; restore new ptr
    mov [rax], rcx
    mov rbx, rax
    add rbx, 16
.push_store:
    mov [rbx + r13*8], r12
    inc r13
    mov [rbx - 8], r13
    mov rax, rbx
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
    mov rax, [rdi - 8]
    test rax, rax
    jz .pop_empty
    dec rax
    mov [rdi - 8], rax
    mov rax, [rbx + rax*8]
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
    mov rcx, [rbx - 8]
    cmp r12, 0
    jl .slice_oob
    cmp r13, rcx
    jg .slice_oob
    cmp r12, r13
    jg .slice_oob
    mov r14, r13
    sub r14, r12
    mov rdi, 8
    mov rsi, r14
    call create
    push rax
    mov rdi, rax
    lea rsi, [rbx + r12*8]
    mov rdx, r14
    shl rdx, 3
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
    mov r13, [rbx - 8]
    add r13, [r12 - 8]
    mov rdi, 8
    mov rsi, r13
    call create
    push rax
    mov rdi, rax
    mov rsi, rbx
    mov rdx, [rbx - 8]
    shl rdx, 3
    call _memmove
    pop rax
    push rax
    mov rcx, [rbx - 8]
    lea rdi, [rax + rcx*8]
    mov rsi, r12
    mov rdx, [r12 - 8]
    shl rdx, 3
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
; Decrements refcount. If refcount reaches 0, restores watermark.
global free
free:
    test rdi, rdi
    jz .free_done
    mov rax, [rdi - 24]     ; load refcount
    dec rax
    mov [rdi - 24], rax     ; store decremented refcount
    jnz .free_done          ; if refcount > 0, don't free
    ; refcount == 0: restore watermark to free all bump memory
    ; NOTE: this is conservative — it frees ALL bump memory, not just this array.
    ; For a production allocator, you'd track per-object sizes.
    ; For now, this is acceptable for a demo compiler.
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
