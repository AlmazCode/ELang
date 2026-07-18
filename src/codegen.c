/* ELang Code Generator - x86_64 NASM */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "codegen.h"

/* --- Frame layout constants --- */
#define FRAME_REGS          5     /* callee-saved: rbx, r12, r13, r14, r15 */
#define FRAME_HEADER        (FRAME_REGS * 8)  /* 40 bytes */
#define WATERMARK_SLOT      FRAME_HEADER      /* rbp-48: watermark after saved regs */
#define FRAME_BASE          (FRAME_HEADER + 8) /* 56: first usable local slot */
#define CLO_MIN_FRAME       56    /* minimum closure frame size */
#define PUSH_RESERVE        64    /* extra stack space for push depth in emit_call */

/* Check snprintf result for truncation */
static void buf_check(int written, size_t bufsize, const char *context) {
    if (written < 0 || (size_t)written >= bufsize) {
        fprintf(stderr, "codegen error: identifier too long (%s), max %zu chars\n",
                context, bufsize - 1);
        exit(1);
    }
}

static void emit(codegen_t *cg, const char *fmt, ...) {
    fprintf(cg->output, "    "); va_list a; va_start(a, fmt); vfprintf(cg->output, fmt, a); va_end(a);
    fprintf(cg->output, "\n");
}
static void emit_raw(codegen_t *cg, const char *s) { fprintf(cg->output, "%s\n", s); }
static int new_label(codegen_t *cg) { return cg->label_count++; }

/* Forward declaration */
static void add_extern(codegen_t *cg, const char *name);

/* Check if name is an extern (stdlib) function — don't prefix those */
static int is_extern(codegen_t *cg, const char *name, size_t len) {
    for (int i = 0; i < cg->extern_count; i++)
        if (strlen(cg->extern_names[i]) == len && memcmp(cg->extern_names[i], name, len) == 0) return 1;
    return 0;
}

/* Emit function name: prefix user-defined names with _ to avoid NASM collisions */
static void emit_name(codegen_t *cg, const char *name, size_t len) {
    if (!is_extern(cg, name, len)) fprintf(cg->output, "_");
    fprintf(cg->output, "%.*s", (int)len, name);
}

static int find_sym(codegen_t *cg, const char *name) {
    for (int i = cg->symbol_count - 1; i >= 0; i--)
        if (strcmp(cg->symbols[i].name, name) == 0) return cg->symbols[i].stack_offset;
    return -1;
}

static int add_sym(codegen_t *cg, const char *name, int size) {
    cg->stack_size += size;
    if (cg->stack_size > cg->max_stack_size) cg->max_stack_size = cg->stack_size;
    int off = cg->stack_size;
    cg->symbol_count++;
    cg->symbols = realloc(cg->symbols, sizeof(*cg->symbols) * cg->symbol_count);
    cg->symbols[cg->symbol_count - 1].name = strdup(name);
    cg->symbols[cg->symbol_count - 1].stack_offset = off;
    cg->symbols[cg->symbol_count - 1].size = size;
    return off;
}

/* --- Forward declarations --- */
static void gen_expr(codegen_t *cg, ast_node_t *n);
static void gen_stmt(codegen_t *cg, ast_node_t *n);
static void gen_node(codegen_t *cg, ast_node_t *n);

typedef struct { int symbol_count, stack_size; } scope_mark_t;

static scope_mark_t scope_enter(codegen_t *cg) {
    scope_mark_t m = { cg->symbol_count, cg->stack_size };
    return m;
}

static void scope_exit(codegen_t *cg, scope_mark_t m) {
    for (int i = cg->symbol_count - 1; i >= m.symbol_count; i--)
        free(cg->symbols[i].name);
    cg->symbol_count = m.symbol_count;
    cg->stack_size = m.stack_size;
}

static const char *find_string_label(codegen_t *cg, const char *value, size_t length) {
    for (int i = 0; i < cg->string_entries; i++) {
        if (cg->strings[i].length == length && memcmp(cg->strings[i].value, value, length) == 0) {
            static char bufs[2][32];
            static int idx = 0;
            char *buf = bufs[idx++ & 1];
            snprintf(buf, sizeof(bufs[0]), "str_%d", cg->strings[i].label);
            return buf;
        }
    }
    return NULL;
}

static void emit_string_data(codegen_t *cg, int label, const char *value, size_t length) {
    fprintf(cg->output, "  str_%d: db ", label);
    if (length == 0) {
        fprintf(cg->output, "0");
    } else {
        for (size_t i = 0; i < length; i++) {
            if (value[i] == '\\' && i + 1 < length) {
                i++;
                switch (value[i]) {
                    case 'n': fprintf(cg->output, "0x0A"); break;
                    case 't': fprintf(cg->output, "0x09"); break;
                    case '0': fprintf(cg->output, "0x00"); break;
                    case '\\': fprintf(cg->output, "0x5C"); break;
                    case '"': fprintf(cg->output, "0x22"); break;
                    case 'r': fprintf(cg->output, "0x0D"); break;
                    default: fprintf(cg->output, "0x%02X", (unsigned char)value[i]); break;
                }
            } else if (value[i] == '"') {
                fprintf(cg->output, "0x22");
            } else if (value[i] >= 0x20 && (unsigned char)value[i] < 0x7F) {
                fprintf(cg->output, "'%c'", value[i]);
            } else {
                fprintf(cg->output, "0x%02X", (unsigned char)value[i]);
            }
            if (i < length - 1) fprintf(cg->output, ", ");
        }
    }
    fprintf(cg->output, ", 0\n");
}

/* Check if a function name is defined in the current AST (user-defined) */
static int is_user_defined(codegen_t *cg, const char *name, size_t len) {
    if (!cg->prog || cg->prog->type != AST_PROGRAM) return 0;
    for (int i = 0; i < cg->prog->as.program.count; i++) {
        ast_node_t *d = cg->prog->as.program.declarations[i];
        if (d && d->type == AST_FN_DECL &&
            d->as.fn_decl.name_len == len &&
            memcmp(d->as.fn_decl.name, name, len) == 0)
            return 1;
    }
    return 0;
}

/* Emit a call instruction for the given callee (handles module::func and plain func) */
static int is_nasm_keyword(const char *name, size_t len) {
    static const char *kw[] = {
        "abs","push","pop","ret","call","jmp","jnz","jz","jge","jle","jg","jl","je","jne",
        "nop","div","mul","sub","add","and","or","xor","shl","shr","sal","sar","rol","ror",
        "test","cmp","mov","lea","inc","dec","neg","not","int","sys","loop","rep","lock",
        "bits","section","global","extern","align","equ","db","dw","dd","dq","dt",
        "times","default","cpu","float",
        NULL
    };
    for (int i = 0; kw[i]; i++)
        if (strlen(kw[i]) == len && memcmp(kw[i], name, len) == 0) return 1;
    return 0;
}

/* Emit function name with _ prefix if it conflicts with NASM keywords */
static void emit_call_name(codegen_t *cg, const char *name, size_t len) {
    if (is_nasm_keyword(name, len)) {
        add_extern(cg, name);  /* extern stays unprefixed */
        fprintf(cg->output, "    call _%.*s\n", (int)len, name);
    } else {
        add_extern(cg, name);
        fprintf(cg->output, "    call %.*s\n", (int)len, name);
    }
}

static void emit_call_target(codegen_t *cg, ast_node_t *callee) {
    if (callee->type == AST_BINARY_OP &&
        callee->as.binary.op == TOKEN_COLONCOLON) {
        ast_node_t *mod = callee->as.binary.left;
        ast_node_t *fn = callee->as.binary.right;
        char mod_name[MAX_IDENT_LEN];
        buf_check(snprintf(mod_name, sizeof(mod_name), "%.*s",
            (int)mod->as.ident.name_len, mod->as.ident.name),
            sizeof(mod_name), "module name");
        if (strcmp(mod_name, "std") == 0) {
            char fn_name[MAX_IDENT_LEN];
            buf_check(snprintf(fn_name, sizeof(fn_name), "%.*s",
                (int)fn->as.ident.name_len, fn->as.ident.name),
                sizeof(fn_name), "function name");
            emit_call_name(cg, fn_name, strlen(fn_name));
        } else {
            char ext_name[MAX_EXTNAME_LEN];
            buf_check(snprintf(ext_name, sizeof(ext_name), "%.*s_%.*s",
                (int)mod->as.ident.name_len, mod->as.ident.name,
                (int)fn->as.ident.name_len, fn->as.ident.name),
                sizeof(ext_name), "module::function name");
            add_extern(cg, ext_name);
            fprintf(cg->output, "    call _%.*s_%.*s\n",
                (int)mod->as.ident.name_len, mod->as.ident.name,
                (int)fn->as.ident.name_len, fn->as.ident.name);
        }
    } else {
        char fn_name[MAX_IDENT_LEN];
        buf_check(snprintf(fn_name, sizeof(fn_name), "%.*s",
            (int)callee->as.ident.name_len,
            callee->as.ident.name),
            sizeof(fn_name), "function name");
        if (is_user_defined(cg, fn_name, strlen(fn_name))) {
            /* User-defined function — use emit_name for _ prefix */
            fprintf(cg->output, "    call ");
            emit_name(cg, callee->as.ident.name, callee->as.ident.name_len);
            fprintf(cg->output, "\n");
        } else {
            /* Extern function (runtime library) — add to list, no _ prefix */
            add_extern(cg, fn_name);
            if (is_nasm_keyword(fn_name, strlen(fn_name)))
                fprintf(cg->output, "    call _%s\n", fn_name);
            else
                fprintf(cg->output, "    call %s\n", fn_name);
        }
    }
}

/* Emit a function call with unlimited args via System V AMD64 ABI.
 * When closure_convention=1, rdi=NULL(env_ptr), args shifted (for user-defined fn).
 * No temp slots (avoids collision with closure objects).
 * Strategy: push arg0 twice (once for rbx, once for final reg), arg1→rbx, arg2+→regs. */
static void emit_call(codegen_t *cg, ast_node_t *callee, ast_node_t **args, int narg, int closure_convention) {
    const char *regs[] = {"rdi","rsi","rdx","rcx","r8","r9"};
    int reg_count = narg < 6 ? narg : 6;
    int stack_args = narg > 6 ? narg - 6 : 0;

    FILE *real_output = cg->output;

    /* Save rbx (callee-saved) — push twice to align */
    emit(cg, "push rbx");
    emit(cg, "push rbx");

    if (closure_convention) {
        /* rdi=NULL, args shifted: arg0→rsi, arg1→rdx, arg2→rcx, ... */
        emit(cg, "xor rdi, rdi");
        if (reg_count > 0) { gen_expr(cg, args[0]); cg->output = real_output; emit(cg, "push rax"); }
        if (reg_count > 1) { gen_expr(cg, args[1]); cg->output = real_output; emit(cg, "mov rbx, rax"); }
        for (int i = 2; i < reg_count; i++) {
            gen_expr(cg, args[i]); cg->output = real_output;
            emit(cg, "mov %s, rax", regs[i]);
        }
        if (reg_count > 0) { emit(cg, "pop rax"); emit(cg, "mov rsi, rax"); }
        if (reg_count > 1) emit(cg, "mov rdx, rbx");
    } else {
        /* Standard: arg0→rdi, arg1→rsi, arg2→rdx, ... */
        if (reg_count > 0) { gen_expr(cg, args[0]); cg->output = real_output; emit(cg, "push rax"); }
        if (reg_count > 1) { gen_expr(cg, args[1]); cg->output = real_output; emit(cg, "mov rbx, rax"); }
        for (int i = 2; i < reg_count; i++) {
            gen_expr(cg, args[i]); cg->output = real_output;
            emit(cg, "mov %s, rax", regs[i]);
        }
        if (reg_count > 0) { emit(cg, "pop rax"); emit(cg, "mov rdi, rax"); }
        if (reg_count > 1) emit(cg, "mov rsi, rbx");
    }

    /* Restore rbx (two pops to undo the two pushes) */
    emit(cg, "pop rbx");
    emit(cg, "pop rbx");

    /* Align rsp to 16 bytes before call.
     * Two pushes added16 bytes. total = stack_size + stack_args*8 + 16. */
    int total = cg->stack_size + 8 * stack_args + 16;
    int align = (16 - (total % 16)) % 16;
    if (align) emit(cg, "sub rsp, %d", align);

    emit_call_target(cg, callee);

    int cleanup = align + 8 * stack_args;
    if (cleanup) emit(cg, "add rsp, %d", cleanup);
}

/* --- Defer support --- */
static void push_defer(codegen_t *cg, ast_node_t *expr) {
    if (cg->defer_count >= cg->defer_cap) {
        cg->defer_cap = cg->defer_cap ? cg->defer_cap * 2 : 8;
        cg->defers = realloc(cg->defers, sizeof(*cg->defers) * cg->defer_cap);
    }
    cg->defers[cg->defer_count].expr = expr;
    cg->defers[cg->defer_count].scope_depth = cg->scope_depth;
    cg->defer_count++;
}

static void emit_defers(codegen_t *cg, int from_depth) {
    for (int i = cg->defer_count - 1; i >= 0; i--) {
        if (cg->defers[i].scope_depth >= from_depth) {
            gen_expr(cg, cg->defers[i].expr);
            /* remove from stack */
            memmove(&cg->defers[i], &cg->defers[i + 1], sizeof(*cg->defers) * (cg->defer_count - i - 1));
            cg->defer_count--;
        }
    }
}

static void gen_expr(codegen_t *cg, ast_node_t *n) {
    if (!n) return;
    switch (n->type) {
        case AST_INT_LIT: emit(cg, "mov rax, %ld", n->as.int_val); break;
        case AST_BOOL_LIT: emit(cg, "mov rax, %d", n->as.bool_val ? 1 : 0); break;
        case AST_STRING_LIT: {
            const char *lbl = find_string_label(cg, n->as.string_val.value, n->as.string_val.length);
            if (lbl) emit(cg, "lea rax, [%s]", lbl);
            break; }
        case AST_IDENT: {
            int off = find_sym(cg, n->as.ident.name);
            if (off >= 0) {
                /* Variable — load from stack */
                emit(cg, "mov rax, [rbp-%d]", off);
            } else if (is_user_defined(cg, n->as.ident.name, n->as.ident.name_len)) {
                /* Function name used as value — wrap in closure object [fn_ptr, NULL].
                 * This ensures the function follows the closure calling convention:
                 * rdi=env_ptr (NULL), rsi=arg0, rdx=arg1, ... */
                char fn_name[MAX_IDENT_LEN];
                buf_check(snprintf(fn_name, sizeof(fn_name), "%.*s",
                    (int)n->as.ident.name_len, n->as.ident.name),
                    sizeof(fn_name), "function name");
                /* Allocate closure object inline (16 bytes) */
                int clo_off = add_sym(cg, "__fn_clo", 16);
                fprintf(cg->output, "    lea rax, [_%s]\n", fn_name);
                emit(cg, "mov [rbp-%d], rax", clo_off);     /* fn_ptr */
                emit(cg, "mov qword [rbp-%d + 8], 0", clo_off); /* env_ptr = NULL */
                emit(cg, "lea rax, [rbp-%d]", clo_off);     /* return ptr to closure object */
            } else {
                fprintf(stderr, "codegen: undefined '%.*s'\n",
                    (int)n->as.ident.name_len, n->as.ident.name);
                return;
            }
            break; }
        case AST_BINARY_OP:
            gen_expr(cg, n->as.binary.left); emit(cg, "push rax");
            gen_expr(cg, n->as.binary.right);
            /* Use stack slot instead of rbx (rbx is callee-saved, can't use as scratch) */
            { int tmp_off = add_sym(cg, "__binop", 8);
              emit(cg, "mov [rbp-%d], rax", tmp_off);
              emit(cg, "pop rax");
              emit(cg, "mov rcx, [rbp-%d]", tmp_off); }
            switch (n->as.binary.op) {
                case TOKEN_PLUS: emit(cg, "add rax, rcx"); break;
                case TOKEN_MINUS: emit(cg, "sub rax, rcx"); break;
                case TOKEN_STAR: emit(cg, "imul rax, rcx"); break;
                case TOKEN_SLASH: emit(cg, "cqo"); emit(cg, "idiv rcx"); break;
                case TOKEN_EQ: emit(cg, "cmp rax, rcx"); emit(cg, "sete al"); emit(cg, "movzx rax, al"); break;
                case TOKEN_NEQ: emit(cg, "cmp rax, rcx"); emit(cg, "setne al"); emit(cg, "movzx rax, al"); break;
                case TOKEN_LT: emit(cg, "cmp rax, rcx"); emit(cg, "setl al"); emit(cg, "movzx rax, al"); break;
                case TOKEN_GT: emit(cg, "cmp rax, rcx"); emit(cg, "setg al"); emit(cg, "movzx rax, al"); break;
                case TOKEN_LTE: emit(cg, "cmp rax, rcx"); emit(cg, "setle al"); emit(cg, "movzx rax, al"); break;
                case TOKEN_GTE: emit(cg, "cmp rax, rcx"); emit(cg, "setge al"); emit(cg, "movzx rax, al"); break;
                case TOKEN_AND: emit(cg, "and rax, rcx"); break;
                case TOKEN_OR: emit(cg, "or rax, rcx"); break;
                case TOKEN_PERCENT: emit(cg, "xor rdx, rdx"); emit(cg, "div rcx"); emit(cg, "mov rax, rdx"); break;
                default: break;
            } break;
        case AST_CALL: {
            int is_user = (n->as.call.callee->type == AST_IDENT &&
                is_user_defined(cg, n->as.call.callee->as.ident.name,
                    n->as.call.callee->as.ident.name_len));
            emit_call(cg, n->as.call.callee, n->as.call.args, n->as.call.arg_count, is_user);
            break; }
        case AST_PIPE: {
            /* x |> f(a)  =>  f(x, a) — left goes as first arg */
            ast_node_t *callee = n->as.pipe.right;
            if (callee->type == AST_CALL) {
                /* Build combined arg list: [pipe_left, explicit args...] */
                int total = callee->as.call.arg_count + 1;
                ast_node_t **all_args = malloc(sizeof(ast_node_t*) * total);
                all_args[0] = n->as.pipe.left;
                for (int i = 0; i < callee->as.call.arg_count; i++)
                    all_args[i + 1] = callee->as.call.args[i];
                emit_call(cg, callee->as.call.callee, all_args, total, 0);
                free(all_args);
            } else {
                /* x |> f  =>  f(x) */
                if (callee->type != AST_IDENT) {
                    fprintf(stderr, "codegen error: |> right side must be a function call or identifier\n");
                    break;
                }
                ast_node_t *single_arg = n->as.pipe.left;
                emit_call(cg, callee, &single_arg, 1, 0);
            }
            break; }
        case AST_OK_EXPR: gen_expr(cg, n->as.ok_expr.value); emit(cg, "shl rax, 1"); break;
        case AST_ERR_EXPR: gen_expr(cg, n->as.err_expr.value); emit(cg, "shl rax, 1"); emit(cg, "or rax, 1"); break;
        case AST_MATCH: gen_stmt(cg, n); break;
        case AST_IF: { int sr = cg->returned; gen_stmt(cg, n); cg->returned = sr; break; }
        case AST_TUPLE: {
            /* evaluate elements, push them right-to-left, result is a "tuple" on stack */
            for (int i = n->as.tuple.count - 1; i >= 0; i--) {
                gen_expr(cg, n->as.tuple.elements[i]);
                emit(cg, "push rax");
            }
            /* rax = stack pointer to first element (for now, just leave values on stack) */
            break; }
        case AST_ARRAY_LITERAL: {
            /* [1, 2, 3] — heap-allocate via with_capacity + array_push */
            int count = n->as.array_literal.count;
            /* with_capacity(8, count) → rax = array_ptr */
            emit(cg, "mov rdi, 8");
            emit(cg, "mov rsi, %d", count);
            add_extern(cg, "with_capacity");
            fprintf(cg->output, "    call with_capacity\n");
            /* push each element: array_push(arr, elem) → arr = rax */
            for (int i = 0; i < count; i++) {
                emit(cg, "push rax"); /* save array_ptr */
                gen_expr(cg, n->as.array_literal.elements[i]);
                emit(cg, "mov rsi, rax"); /* elem → rsi */
                emit(cg, "pop rdi");      /* array_ptr → rdi */
                add_extern(cg, "push");
                fprintf(cg->output, "    call push\n");
            }
            /* rax = final array_ptr */
            break; }
        case AST_INDEX: {
            /* arr[i] — bounds-checked via array_get */
            gen_expr(cg, n->as.binary.left);  /* rax = array pointer */
            emit(cg, "mov rdi, rax");
            gen_expr(cg, n->as.binary.right); /* rax = index */
            emit(cg, "mov rsi, rax");
            add_extern(cg, "get");
            fprintf(cg->output, "    call get\n");
            /* rax = element */
            break; }
        case AST_LEN_EXPR: {
            /* arr.len — call array_len */
            gen_expr(cg, n->as.len_expr.operand); /* rax = array pointer */
            emit(cg, "mov rdi, rax");
            add_extern(cg, "len");
            fprintf(cg->output, "    call len\n");
            /* rax = length */
            break; }
        case AST_TRY_EXPR: {
            /* expr? — check if result is Err (bit 0 == 1), if so propagate (return) */
            gen_expr(cg, n->as.try_expr.operand);
            emit(cg, "test rax, 1");
            int ok_label = new_label(cg);
            emit(cg, "je L%d", ok_label);
            /* Err case: run defers and return the error value */
            emit_defers(cg, 0);
            emit(cg, "mov rsp, rbp"); emit(cg, "pop rbp"); emit(cg, "ret");
            fprintf(cg->output, "L%d:\n", ok_label);
            /* Ok case: extract value (shift right 1 to undo the shl 1) */
            emit(cg, "shr rax, 1");
            break; }
        case AST_CATCH_EXPR: {
            /* expr catch { handler } */
            int err_label = new_label(cg);
            int end_label = new_label(cg);
            gen_expr(cg, n->as.catch_expr.operand);
            emit(cg, "test rax, 1");
            emit(cg, "jne L%d", err_label);
            /* Ok case: extract value */
            emit(cg, "shr rax, 1");
            emit(cg, "jmp L%d", end_label);
            /* Err case: run handler */
            fprintf(cg->output, "L%d:\n", err_label);
            emit(cg, "shr rax, 1");
            scope_mark_t catch_mark = scope_enter(cg);
            int err_off = add_sym(cg, "__catch_err", 8);
            emit(cg, "mov [rbp-%d], rax", err_off);
            int saved_scope = cg->scope_depth;
            cg->scope_depth++;
            gen_node(cg, n->as.catch_expr.handler);
            if (!cg->returned) emit_defers(cg, cg->scope_depth);
            cg->scope_depth = saved_scope;
            scope_exit(cg, catch_mark);
            fprintf(cg->output, "L%d:\n", end_label);
            break; }
        case AST_CLOSURE: {
            /* fn x => expr — buffered closure body + closure object */
            int clo_label = new_label(cg);

            /* Buffer closure body using open_memstream */
            FILE *saved_output = cg->output;
            char *buf = NULL;
            size_t buf_len = 0;
            FILE *mem = open_memstream(&buf, &buf_len);
            cg->output = mem;

            /* Save stack state — closure codegen modifies stack_size via add_sym
             * for its parameters. We must restore after so caller's temp slots
             * don't collide with closure's local variables. */
            int saved_stack = cg->stack_size;
            int saved_max = cg->max_stack_size;

            fprintf(cg->output, "_fn%d:\n", clo_label);
            emit(cg, "push rbp"); emit(cg, "mov rbp, rsp");
            emit(cg, "push rbx"); emit(cg, "push r12"); emit(cg, "push r13"); emit(cg, "push r14"); emit(cg, "push r15");
            const char *cr[] = {"rdi","rsi","rdx","rcx","r8","r9"};
            /* Store args as symbols — stack_size accounts for them */
            int clo_param_count = n->as.closure.param_count < 5 ? n->as.closure.param_count : 5;
            cg->stack_size = FRAME_HEADER;
            for (int ai = 0; ai < clo_param_count; ai++) {
                int poff = add_sym(cg, n->as.closure.params[ai], 8);
                emit(cg, "mov [rbp-%d], %s", poff, cr[ai + 1]);
            }
            /* Allocate stack: need enough so params don't collide with caller's saved regs.
             * Closure's rbp points into caller's frame. Params at [rbp-X] must be in
             * closure's OWN stack space, not overlapping caller's saved registers.
             * Minimum: 40 (our saved regs) + params + body space + 48 (safety margin). */
            int clo_stack_needed = cg->stack_size;
            if (clo_stack_needed < CLO_MIN_FRAME) clo_stack_needed = CLO_MIN_FRAME;
            emit(cg, "sub rsp, %d", clo_stack_needed - 40);
            gen_expr(cg, n->as.closure.body);
            emit(cg, "mov rsp, rbp"); emit(cg, "pop rbp"); emit(cg, "ret");

            fclose(mem);
            cg->output = saved_output;

            /* Restore caller's stack state (closure's add_sym calls were in buffer) */
            cg->stack_size = saved_stack;
            cg->max_stack_size = saved_max;

            /* Store buffered body for later emission */
            if (cg->closure_body_count >= cg->closure_body_cap) {
                cg->closure_body_cap = cg->closure_body_cap ? cg->closure_body_cap * 2 : 8;
                cg->closure_bodies = realloc(cg->closure_bodies,
                    sizeof(*cg->closure_bodies) * cg->closure_body_cap);
            }
            cg->closure_bodies[cg->closure_body_count].label = clo_label;
            cg->closure_bodies[cg->closure_body_count].asm_text = buf;
            cg->closure_bodies[cg->closure_body_count].asm_len = buf_len;
            cg->closure_body_count++;

            /* Create closure object: [fn_ptr, env_ptr=NULL] */
            int clo_off = add_sym(cg, "__clo", 16);
            emit(cg, "lea rax, [_fn%d]", clo_label);
            emit(cg, "mov [rbp-%d], rax", clo_off);
            emit(cg, "mov qword [rbp-%d + 8], 0", clo_off);
            emit(cg, "lea rax, [rbp-%d]", clo_off);
            break; }
        default: break;
    }
}

/* --- Extracted statement generators --- */

static void gen_if(codegen_t *cg, ast_node_t *n) {
    int el = new_label(cg), en = new_label(cg);
    gen_expr(cg, n->as.if_stmt.condition);
    emit(cg, "cmp rax, 0"); emit(cg, "je L%d", el);
    int saved_ret = cg->returned;
    cg->returned = 0;
    gen_node(cg, n->as.if_stmt.then_block);
    int then_returned = cg->returned;
    if (n->as.if_stmt.else_block) {
        if (!then_returned) emit(cg, "jmp L%d", en);
        fprintf(cg->output, "L%d:\n", el);
        cg->returned = 0;
        gen_node(cg, n->as.if_stmt.else_block);
        fprintf(cg->output, "L%d:\n", en);
        cg->returned = then_returned && cg->returned;
    } else {
        fprintf(cg->output, "L%d:\n", el);
        cg->returned = 0;
    }
    if (cg->in_return_expr) cg->returned = saved_ret;
}

static void gen_while(codegen_t *cg, ast_node_t *n) {
    int loop = new_label(cg), end = new_label(cg);
    fprintf(cg->output, "L%d:\n", loop);
    gen_expr(cg, n->as.while_stmt.condition);
    emit(cg, "cmp rax, 0"); emit(cg, "je L%d", end);
    cg->returned = 0;
    gen_node(cg, n->as.while_stmt.body);
    cg->returned = 0;
    emit(cg, "jmp L%d", loop);
    fprintf(cg->output, "L%d:\n", end);
}

static void gen_for(codegen_t *cg, ast_node_t *n) {
    int loop = new_label(cg), end = new_label(cg), inc = new_label(cg);
    scope_mark_t for_mark = scope_enter(cg);

    if (n->as.for_stmt.iterable->type == AST_RANGE) {
        /* for i in start..end — range-based */
        gen_expr(cg, n->as.for_stmt.iterable->as.range.left);
        emit(cg, "push rax");
        gen_expr(cg, n->as.for_stmt.iterable->as.range.right);
        emit(cg, "push rax");
        int end_off = add_sym(cg, "__for_end", 8);
        emit(cg, "pop rax"); emit(cg, "mov [rbp-%d], rax", end_off);
        int iter_off = add_sym(cg, n->as.for_stmt.vars[0], 8);
        emit(cg, "pop rax"); emit(cg, "mov [rbp-%d], rax", iter_off);
        fprintf(cg->output, "L%d:\n", loop);
        emit(cg, "mov rax, [rbp-%d]", iter_off);
        { int tmp = add_sym(cg, "__cmp_tmp", 8);
          emit(cg, "mov [rbp-%d], rax", tmp);
          emit(cg, "mov rax, [rbp-%d]", end_off);
          emit(cg, "mov rcx, rax");
          emit(cg, "mov rax, [rbp-%d]", tmp);
          emit(cg, "cmp rax, rcx"); }
        emit(cg, "jge L%d", end);
        cg->returned = 0;
        gen_node(cg, n->as.for_stmt.body);
        fprintf(cg->output, "L%d:\n", inc);
        emit(cg, "mov rax, [rbp-%d]", iter_off);
        emit(cg, "inc rax");
        emit(cg, "mov [rbp-%d], rax", iter_off);
        emit(cg, "jmp L%d", loop);
    } else {
        /* for x in arr  OR  for i, x in arr */
        gen_expr(cg, n->as.for_stmt.iterable);
        int arr_off = add_sym(cg, "__for_arr", 8);
        emit(cg, "mov [rbp-%d], rax", arr_off);
        emit(cg, "mov rdi, rax");
        add_extern(cg, "len");
        fprintf(cg->output, "    call len\n");
        int len_off = add_sym(cg, "__for_len", 8);
        emit(cg, "mov [rbp-%d], rax", len_off);
        int idx_off = add_sym(cg, "__for_idx", 8);
        emit(cg, "mov qword [rbp-%d], 0", idx_off);
        int var_off = 0, idx_var_off = 0;
        if (n->as.for_stmt.var_count == 2) {
            idx_var_off = add_sym(cg, n->as.for_stmt.vars[0], 8);
            var_off = add_sym(cg, n->as.for_stmt.vars[1], 8);
        } else {
            var_off = add_sym(cg, n->as.for_stmt.vars[0], 8);
        }
        int cmp_tmp = add_sym(cg, "__cmp_tmp", 8);
        int cmp_idx = add_sym(cg, "__cmp_idx", 8);
        fprintf(cg->output, "L%d:\n", loop);
        emit(cg, "mov rax, [rbp-%d]", idx_off);
        emit(cg, "mov [rbp-%d], rax", cmp_tmp);
        emit(cg, "mov rax, [rbp-%d]", len_off);
        emit(cg, "mov [rbp-%d], rax", cmp_idx);
        emit(cg, "mov rax, [rbp-%d]", cmp_tmp);
        emit(cg, "mov rcx, [rbp-%d]", cmp_idx);
        emit(cg, "cmp rax, rcx");
        emit(cg, "jge L%d", end);
        emit(cg, "mov rdi, [rbp-%d]", arr_off);
        emit(cg, "mov rsi, [rbp-%d]", idx_off);
        add_extern(cg, "get");
        fprintf(cg->output, "    call get\n");
        if (n->as.for_stmt.var_count == 2) {
            emit(cg, "mov [rbp-%d], rax", var_off);
            emit(cg, "mov rax, [rbp-%d]", idx_off);
            emit(cg, "mov [rbp-%d], rax", idx_var_off);
        } else {
            emit(cg, "mov [rbp-%d], rax", var_off);
        }
        cg->returned = 0;
        gen_node(cg, n->as.for_stmt.body);
        fprintf(cg->output, "L%d:\n", inc);
        emit(cg, "mov rax, [rbp-%d]", idx_off);
        emit(cg, "inc rax");
        emit(cg, "mov [rbp-%d], rax", idx_off);
        emit(cg, "jmp L%d", loop);
    }
    fprintf(cg->output, "L%d:\n", end);
    scope_exit(cg, for_mark);
}

static void gen_block(codegen_t *cg, ast_node_t *n) {
    scope_mark_t mark = scope_enter(cg);
    int saved_depth = cg->scope_depth;
    cg->scope_depth++;
    for (int i = 0; i < n->as.block.count; i++) gen_node(cg, n->as.block.stmts[i]);
    if (!cg->returned) emit_defers(cg, cg->scope_depth);
    cg->scope_depth = saved_depth;
    scope_exit(cg, mark);
}

static void gen_fn_decl(codegen_t *cg, ast_node_t *n) {
    fprintf(cg->output, "\nglobal "); emit_name(cg, n->as.fn_decl.name, n->as.fn_decl.name_len);
    fprintf(cg->output, "\n"); emit_name(cg, n->as.fn_decl.name, n->as.fn_decl.name_len);
    fprintf(cg->output, ":\n");
    emit(cg, "push rbp"); emit(cg, "mov rbp, rsp");
    emit(cg, "push rbx"); emit(cg, "push r12"); emit(cg, "push r13"); emit(cg, "push r14"); emit(cg, "push r15");
    add_extern(cg, "save_watermark");
    fprintf(cg->output, "    call save_watermark\n");
    int wm_off = WATERMARK_SLOT;
    emit(cg, "mov [rbp-%d], rax", wm_off);
    cg->stack_size = FRAME_BASE;
    if (cg->max_stack_size < cg->stack_size)
        cg->max_stack_size = cg->stack_size;
    cg->max_stack_size += PUSH_RESERVE;
    int saved = cg->stack_size;
    int saved_max = cg->max_stack_size;
    const char *aregs[] = {"rsi","rdx","rcx","r8","r9","r10"};
    for (int i = 0; i < n->as.fn_decl.param_count && i < 6; i++) {
        int off = add_sym(cg, n->as.fn_decl.params[i].name, 8);
        emit(cg, "mov [rbp-%d], %s", off, aregs[i]); }
    cg->returned = 0;
    cg->scope_depth = 0;
    int saved_defer_count = cg->defer_count;
    cg->sub_rsp_pos = ftell(cg->output);
    fprintf(cg->output, "    sub rsp, 0x00000000\n");
    gen_node(cg, n->as.fn_decl.body);
    if (!cg->returned) emit(cg, "xor rax, rax");
    int stack_needed = cg->max_stack_size - FRAME_BASE;
    if (stack_needed < 0) stack_needed = 0;
    long cur_pos = ftell(cg->output);
    fseek(cg->output, cg->sub_rsp_pos, SEEK_SET);
    fprintf(cg->output, "    sub rsp, 0x%08X", (unsigned)stack_needed);
    fseek(cg->output, cur_pos, SEEK_SET);
    emit(cg, "add rsp, 0x%08X", (unsigned)stack_needed);
    emit(cg, "pop r15"); emit(cg, "pop r14"); emit(cg, "pop r13"); emit(cg, "pop r12"); emit(cg, "pop rbx");
    emit(cg, "mov rsp, rbp"); emit(cg, "pop rbp"); emit(cg, "ret");
    cg->stack_size = saved;
    cg->max_stack_size = saved_max;
    cg->defer_count = saved_defer_count;
}

static void gen_struct_decl(codegen_t *cg, ast_node_t *n) {
    fprintf(cg->output, "\n; struct %.*s\n", (int)n->as.struct_decl.name_len, n->as.struct_decl.name);
    fprintf(cg->output, "global struct_"); emit_name(cg, n->as.struct_decl.name, n->as.struct_decl.name_len);
    fprintf(cg->output, "\nstruct_"); emit_name(cg, n->as.struct_decl.name, n->as.struct_decl.name_len);
    fprintf(cg->output, ":\n");
    emit(cg, "push rbp"); emit(cg, "mov rbp, rsp");
    int total = 8 + n->as.struct_decl.field_count * 8;
    int aligned_total = (total + 15) & ~15;
    emit(cg, "sub rsp, %d", aligned_total);
    emit(cg, "mov qword [rbp-8], %d", n->as.struct_decl.field_count);
    const char *regs[] = {"rdi","rsi","rdx","rcx","r8","r9"};
    for (int i = 0; i < n->as.struct_decl.field_count && i < 6; i++)
        emit(cg, "mov [rbp-%d], %s", 16 + i * 8, regs[i]);
    emit(cg, "lea rax, [rbp-8]");
    emit(cg, "mov rsp, rbp"); emit(cg, "pop rbp"); emit(cg, "ret");
}

static void gen_stmt(codegen_t *cg, ast_node_t *n) {
    if (!n) return;
    if (n->type == AST_FN_DECL) { cg->returned = 0; cg->scope_depth = 0; cg->defer_count = 0; }
    if (cg->returned) return;
    switch (n->type) {
        case AST_LET: {
            if (n->as.let.value && n->as.let.value->type == AST_TUPLE_ASSIGN) {
                ast_node_t *ta = n->as.let.value;
                gen_expr(cg, ta->as.tuple_assign.value);
                for (int i = ta->as.tuple_assign.name_count - 1; i >= 0; i--) {
                    int off = add_sym(cg, ta->as.tuple_assign.names[i], 8);
                    emit(cg, "pop rax");
                    emit(cg, "mov [rbp-%d], rax", off);
                }
            } else {
                int off = add_sym(cg, n->as.let.name, 8);
                if (n->as.let.value) { gen_expr(cg, n->as.let.value); emit(cg, "mov [rbp-%d], rax", off); }
                else emit(cg, "mov qword [rbp-%d], 0", off);
            }
            break; }
        case AST_ASSIGN: {
            int off = find_sym(cg, n->as.assign.target->as.ident.name);
            if (off < 0) { fprintf(stderr, "codegen: undefined '%.*s'\n",
                (int)n->as.assign.target->as.ident.name_len, n->as.assign.target->as.ident.name); return; }
            gen_expr(cg, n->as.assign.value); emit(cg, "mov [rbp-%d], rax", off); break; }
        case AST_RETURN:
            emit_defers(cg, 0);
            if (n->as.ret.value) {
                cg->in_return_expr = 1;
                gen_expr(cg, n->as.ret.value);
                cg->in_return_expr = 0;
            }
            else emit(cg, "xor rax, rax");
            emit(cg, "mov rsp, rbp"); emit(cg, "pop rbp"); emit(cg, "ret");
            cg->returned = 1; break;
        case AST_DEFER:
            push_defer(cg, n->as.defer_stmt.expr);
            break;
        case AST_IF: gen_if(cg, n); break;
        case AST_WHILE: gen_while(cg, n); break;
        case AST_FOR: gen_for(cg, n); break;
        case AST_BLOCK: gen_block(cg, n); break;
        case AST_FN_DECL: gen_fn_decl(cg, n); break;
        case AST_STRUCT_DECL: gen_struct_decl(cg, n); break;
        case AST_ENUM_DECL:
            fprintf(cg->output, "; enum %.*s\n", (int)n->as.enum_decl.name_len, n->as.enum_decl.name); break;
        case AST_MATCH: {
            int end_lbl = new_label(cg), ret_lbl = new_label(cg);
            scope_mark_t match_mark = scope_enter(cg);
            gen_expr(cg, n->as.match_expr.value);
            emit(cg, "push rax");
            for (int i = 0; i < n->as.match_expr.case_count; i++) {
                int next = new_label(cg);
                emit(cg, "mov rax, [rsp]");
                ast_node_t *pat = n->as.match_expr.cases[i].pattern;
                if (pat->type == AST_OK_EXPR) {
                    emit(cg, "mov rbx, rax");
                    emit(cg, "and rbx, 1");
                    emit(cg, "cmp rbx, 0");
                    emit(cg, "jne L%d", next);
                    emit(cg, "shr rax, 1");
                    if (pat->as.ok_expr.value && pat->as.ok_expr.value->type == AST_IDENT) {
                        char *var_name = pat->as.ok_expr.value->as.ident.name;
                        int off = add_sym(cg, var_name, 8);
                        emit(cg, "mov [rbp-%d], rax", off);
                    }
                } else if (pat->type == AST_ERR_EXPR) {
                    emit(cg, "mov rbx, rax");
                    emit(cg, "and rbx, 1");
                    emit(cg, "cmp rbx, 1");
                    emit(cg, "jne L%d", next);
                    emit(cg, "shr rax, 1");
                    if (pat->as.err_expr.value && pat->as.err_expr.value->type == AST_IDENT) {
                        char *var_name = pat->as.err_expr.value->as.ident.name;
                        int off = add_sym(cg, var_name, 8);
                        emit(cg, "mov [rbp-%d], rax", off);
                    }
                } else {
                    emit(cg, "push rax");
                    gen_expr(cg, pat);
                    emit(cg, "mov rbx, rax");
                    emit(cg, "pop rax");
                    emit(cg, "cmp rax, rbx");
                    emit(cg, "jne L%d", next);
                }
                int saved_ret = cg->returned;
                cg->returned = 0;
                gen_node(cg, n->as.match_expr.cases[i].result);
                int case_returned = cg->returned;
                cg->returned = saved_ret;
                if (case_returned) {
                    emit(cg, "add rsp, 8");
                    emit(cg, "jmp L%d", ret_lbl);
                } else {
                    emit(cg, "push rax");
                    emit(cg, "jmp L%d", end_lbl);
                }
                fprintf(cg->output, "L%d:\n", next);
            }
            emit(cg, "add rsp, 8");
            fprintf(cg->output, "L%d:\n", end_lbl);
            emit(cg, "pop rax");
            fprintf(cg->output, "L%d:\n", ret_lbl);
            scope_exit(cg, match_mark);
            break; }
        case AST_USING:
            fprintf(cg->output, "; using \"%.*s\"\n", (int)n->as.using_decl.path_len, n->as.using_decl.path); break;
        case AST_IMPORT_DECL: fprintf(cg->output, "; import \"%.*s\"\n", (int)n->as.import.path_len, n->as.import.path); break;
        case AST_TRY_EXPR: {
            /* expr? — check if result is Err (bit 0 == 1), if so propagate (return) */
            gen_expr(cg, n->as.try_expr.operand);
            emit(cg, "test rax, 1");
            int ok_label = new_label(cg);
            emit(cg, "je L%d", ok_label);
            /* Err case: run defers and return the error value */
            emit_defers(cg, 0);
            emit(cg, "mov rsp, rbp"); emit(cg, "pop rbp"); emit(cg, "ret");
            fprintf(cg->output, "L%d:\n", ok_label);
            /* Ok case: extract value (shift right 1 to undo the shl 1) */
            emit(cg, "shr rax, 1");
            break; }
        case AST_CATCH_EXPR: {
            /* expr catch { handler } */
            int err_label = new_label(cg);
            int end_label = new_label(cg);
            gen_expr(cg, n->as.catch_expr.operand);
            emit(cg, "test rax, 1");
            emit(cg, "jne L%d", err_label);
            /* Ok case: extract value */
            emit(cg, "shr rax, 1");
            emit(cg, "jmp L%d", end_label);
            /* Err case: run handler */
            fprintf(cg->output, "L%d:\n", err_label);
            emit(cg, "shr rax, 1");
            scope_mark_t catch_mark = scope_enter(cg);
            int err_off = add_sym(cg, "__catch_err", 8);
            emit(cg, "mov [rbp-%d], rax", err_off);
            int saved_scope = cg->scope_depth;
            cg->scope_depth++;
            gen_node(cg, n->as.catch_expr.handler);
            if (!cg->returned) emit_defers(cg, cg->scope_depth);
            cg->scope_depth = saved_scope;
            scope_exit(cg, catch_mark);
            fprintf(cg->output, "L%d:\n", end_label);
            break; }
        case AST_PANIC_EXPR: {
            /* panic(msg) — call panic_handler(msg, line, file) */
            if (n->as.panic_expr.message) {
                gen_expr(cg, n->as.panic_expr.message);
                emit(cg, "mov rdi, rax");
            } else {
                emit(cg, "xor rdi, rdi");  /* NULL message */
            }
            /* arg2: line number */
            emit(cg, "mov rsi, %d", n->line);
            /* arg3: source filename (or NULL) */
            if (cg->source_file)
                emit(cg, "lea rdx, [_src_file]");
            else
                emit(cg, "xor rdx, rdx");
            add_extern(cg, "panic_handler");
            fprintf(cg->output, "    call ");
            emit_name(cg, "panic_handler", 13);
            fprintf(cg->output, "\n");
            cg->returned = 1;
            break; }
        case AST_ASSERT_EXPR: {
            /* assert(cond, msg) — call assert_handler */
            gen_expr(cg, n->as.assert_expr.condition);
            emit(cg, "push rax");           /* spill condition to stack */
            if (n->as.assert_expr.message) {
                gen_expr(cg, n->as.assert_expr.message);
                emit(cg, "mov rsi, rax");   /* message → rsi (arg2) */
            } else {
                emit(cg, "xor rsi, rsi");   /* NULL message */
            }
            emit(cg, "pop rdi");            /* restore condition → rdi (arg1) */
            add_extern(cg, "assert_handler");
            fprintf(cg->output, "    call ");
            emit_name(cg, "assert_handler", 14);
            fprintf(cg->output, "\n");
            break; }
        default: gen_expr(cg, n); break;
    }
}

static void gen_node(codegen_t *cg, ast_node_t *n) {
    if (!n) return;
    switch (n->type) {
        case AST_PROGRAM: for (int i = 0; i < n->as.program.count; i++) gen_node(cg, n->as.program.declarations[i]); break;
        default: gen_stmt(cg, n); break;
    }
}

static void collect_strings(codegen_t *cg, ast_node_t *n) {
    if (!n) return;
    if (n->type == AST_STRING_LIT) {
        int lbl = new_label(cg);
        cg->string_entries++;
        cg->strings = realloc(cg->strings, sizeof(string_entry_t) * cg->string_entries);
        cg->strings[cg->string_entries - 1].label = lbl;
        cg->strings[cg->string_entries - 1].value = strndup(n->as.string_val.value, n->as.string_val.length);
        cg->strings[cg->string_entries - 1].length = n->as.string_val.length;
    }
    switch (n->type) {
        case AST_BINARY_OP: collect_strings(cg, n->as.binary.left); collect_strings(cg, n->as.binary.right); break;
        case AST_UNARY_OP: collect_strings(cg, n->as.unary.operand); break;
        case AST_RANGE: collect_strings(cg, n->as.range.left); collect_strings(cg, n->as.range.right); break;
        case AST_PIPE: collect_strings(cg, n->as.pipe.left); collect_strings(cg, n->as.pipe.right); break;
        case AST_CALL:
            collect_strings(cg, n->as.call.callee);
            for (int i = 0; i < n->as.call.arg_count; i++)
                collect_strings(cg, n->as.call.args[i]);
            break;
        case AST_IF: collect_strings(cg, n->as.if_stmt.condition); collect_strings(cg, n->as.if_stmt.then_block);
            collect_strings(cg, n->as.if_stmt.else_block); break;
        case AST_WHILE: collect_strings(cg, n->as.while_stmt.condition); collect_strings(cg, n->as.while_stmt.body); break;
        case AST_FOR: collect_strings(cg, n->as.for_stmt.iterable); collect_strings(cg, n->as.for_stmt.body); break;
        case AST_BLOCK: for (int i = 0; i < n->as.block.count; i++) collect_strings(cg, n->as.block.stmts[i]); break;
        case AST_LET: collect_strings(cg, n->as.let.value); break;
        case AST_ASSIGN: collect_strings(cg, n->as.assign.value); break;
        case AST_RETURN: collect_strings(cg, n->as.ret.value); break;
        case AST_FN_DECL: collect_strings(cg, n->as.fn_decl.body); break;
        case AST_MATCH: collect_strings(cg, n->as.match_expr.value);
            for (int i = 0; i < n->as.match_expr.case_count; i++) {
                collect_strings(cg, n->as.match_expr.cases[i].pattern);
                collect_strings(cg, n->as.match_expr.cases[i].result); } break;
        case AST_OK_EXPR: collect_strings(cg, n->as.ok_expr.value); break;
        case AST_ERR_EXPR: collect_strings(cg, n->as.err_expr.value); break;
        case AST_DEFER: collect_strings(cg, n->as.defer_stmt.expr); break;
        case AST_TRY_EXPR: collect_strings(cg, n->as.try_expr.operand); break;
        case AST_CATCH_EXPR: collect_strings(cg, n->as.catch_expr.operand); collect_strings(cg, n->as.catch_expr.handler); break;
        case AST_PANIC_EXPR: collect_strings(cg, n->as.panic_expr.message); break;
        case AST_ASSERT_EXPR: collect_strings(cg, n->as.assert_expr.condition); collect_strings(cg, n->as.assert_expr.message); break;
        case AST_TUPLE: for (int i = 0; i < n->as.tuple.count; i++) collect_strings(cg, n->as.tuple.elements[i]); break;
        case AST_ARRAY_LITERAL: for (int i = 0; i < n->as.array_literal.count; i++) collect_strings(cg, n->as.array_literal.elements[i]); break;
        case AST_LEN_EXPR: collect_strings(cg, n->as.len_expr.operand); break;
        case AST_RESULT_TYPE: collect_strings(cg, n->as.result_type.ok_type); collect_strings(cg, n->as.result_type.err_type); break;
        case AST_TUPLE_ASSIGN: collect_strings(cg, n->as.tuple_assign.value); break;
        case AST_PROGRAM: for (int i = 0; i < n->as.program.count; i++) collect_strings(cg, n->as.program.declarations[i]); break;
        default: break;
    }
}

void codegen_init(codegen_t *cg, FILE *output) {
    cg->output = output; cg->label_count = 0; cg->string_count = 0;
    cg->symbols = NULL; cg->symbol_count = 0; cg->stack_size = 0; cg->max_stack_size = 0;
    cg->strings = NULL; cg->string_entries = 0; cg->is_main = 1; cg->returned = 0;
    cg->defers = NULL; cg->defer_count = 0; cg->defer_cap = 0; cg->scope_depth = 0;
    cg->extern_names = NULL; cg->extern_count = 0; cg->in_return_expr = 0;
    cg->sub_rsp_pos = 0; cg->source_file = NULL; cg->prog = NULL;
    cg->closure_bodies = NULL; cg->closure_body_count = 0; cg->closure_body_cap = 0;
}

void codegen_free(codegen_t *cg) {
    for (int i = 0; i < cg->symbol_count; i++) free(cg->symbols[i].name);
    free(cg->symbols);
    for (int i = 0; i < cg->string_entries; i++) free(cg->strings[i].value);
    free(cg->strings);
    free(cg->defers);
    for (int i = 0; i < cg->extern_count; i++) free(cg->extern_names[i]);
    free(cg->extern_names);
}

static void add_extern(codegen_t *cg, const char *name) {
    cg->extern_count++;
    cg->extern_names = realloc(cg->extern_names, sizeof(char*) * cg->extern_count);
    cg->extern_names[cg->extern_count - 1] = strdup(name);
}

int codegen_program(codegen_t *cg, ast_node_t *prog) {
    cg->prog = prog;
    collect_strings(cg, prog);
    emit_raw(cg, "; Generated by ELang compiler v0.42.0");

    /* Collect all function calls for extern declarations */
    /* This is done during codegen, so we emit externs after gen_node */

    if (cg->string_entries > 0 || cg->source_file) {
        emit_raw(cg, "section .data");
        for (int i = 0; i < cg->string_entries; i++)
            emit_string_data(cg, cg->strings[i].label, cg->strings[i].value, cg->strings[i].length);
        /* Emit source filename for panic_handler location reporting */
        if (cg->source_file) {
            fprintf(cg->output, "  _src_file: db ");
            for (const char *p = cg->source_file; *p; p++)
                fprintf(cg->output, "0x%02X, ", (unsigned char)*p);
            fprintf(cg->output, "0\n");
        }
    }
    emit_raw(cg, "section .text");
    gen_node(cg, prog);

    /* Emit buffered closure function bodies */
    for (int i = 0; i < cg->closure_body_count; i++) {
        fwrite(cg->closure_bodies[i].asm_text, 1, cg->closure_bodies[i].asm_len, cg->output);
        fprintf(cg->output, "\n");
    }

    /* Emit extern declarations for all called functions */
    /* The linker will resolve these */
    fprintf(cg->output, "\n; Extern declarations (resolved by linker)\n");
    for (int i = 0; i < cg->extern_count; i++) {
        fprintf(cg->output, "extern %s\n", cg->extern_names[i]);
    }

    /* Only emit _start for main programs, not libraries */
    if (cg->is_main) {
        emit_raw(cg, "global _start"); emit_raw(cg, "");
        emit_raw(cg, "_start:");
        emit_raw(cg, "    call _main");
        /* Check if main returned an unhandled Result (Err tag: bit 0 == 1) */
        emit_raw(cg, "    mov rbx, rax");
        emit_raw(cg, "    and rbx, 1");
        emit_raw(cg, "    cmp rbx, 1");
        int ok_label = new_label(cg);
        emit(cg, "jne L%d", ok_label);
        /* Err case: print "error: unhandled Result\n" to stderr, exit(1) */
        emit_raw(cg, "    ; unhandled Err from main — print to stderr");
        emit_raw(cg, "    mov rax, 1");          /* sys_write */
        emit_raw(cg, "    mov rdi, 2");          /* stderr */
        fprintf(cg->output, "    lea rsi, [_err_unhandled_msg]\n");
        emit_raw(cg, "    mov rdx, 32");         /* strlen("error: unhandled Result in main\n") */
        emit_raw(cg, "    syscall");
        emit_raw(cg, "    mov rdi, 1");
        emit_raw(cg, "    mov rax, 60");
        emit_raw(cg, "    syscall");
        /* Ok case: extract value and exit normally */
        fprintf(cg->output, "L%d:\n", ok_label);
        emit_raw(cg, "    shr rax, 1");
        emit_raw(cg, "    mov rdi, rax");
        emit_raw(cg, "    mov rax, 60");
        emit_raw(cg, "    syscall");
        /* Error message string */
        emit_raw(cg, "section .data");
        fprintf(cg->output, "  _err_unhandled_msg: db \"error: unhandled Result in main\", 0x0A, 0\n");
        emit_raw(cg, "section .text");
    }
    return 0;
}
