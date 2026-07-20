/* ELang Code Generator - x86_64 NASM */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "codegen.h"
#include "semantics.h"

/* --- Frame layout constants --- */
#define FRAME_REGS          5     /* callee-saved: rbx, r12, r13, r14, r15 */
#define FRAME_HEADER        (FRAME_REGS * 8)  /* 40 bytes */
#define WATERMARK_SLOT      FRAME_HEADER      /* rbp-48: watermark after saved regs */
#define FRAME_BASE          (FRAME_HEADER + 8) /* 56: first usable local slot */
#define CLO_MIN_FRAME       56    /* minimum closure frame size */
#define PUSH_RESERVE        64    /* extra stack space for push depth in emit_call */

/* --- Capture detection --- */
/* Collects all identifiers in AST that are NOT in the exclude list. */
typedef struct {
    char **names;
    size_t *name_lens;
    int count, cap;
} ident_list_t;

static void idlist_add(ident_list_t *l, const char *name, size_t len) {
    for (int i = 0; i < l->count; i++)
        if (l->name_lens[i] == len && memcmp(l->names[i], name, len) == 0) return;
    if (l->count >= l->cap) { l->cap = l->cap ? l->cap * 2 : 8;
        l->names = SAFE_REALLOC(l->names, sizeof(char*) * l->cap);
        l->name_lens = SAFE_REALLOC(l->name_lens, sizeof(size_t) * l->cap); }
    l->names[l->count] = SAFE_STRNDUP(name, len);
    l->name_lens[l->count] = len;
    l->count++;
}

static void collect_idents(ast_node_t *n, ident_list_t *out) {
    if (!n) return;
    switch (n->type) {
        case AST_IDENT: idlist_add(out, n->as.ident.name, n->as.ident.name_len); break;
        case AST_BINARY_OP: collect_idents(n->as.binary.left, out); collect_idents(n->as.binary.right, out); break;
        case AST_UNARY_OP: collect_idents(n->as.unary.operand, out); break;
        case AST_CALL: collect_idents(n->as.call.callee, out);
            for (int i = 0; i < n->as.call.arg_count; i++) collect_idents(n->as.call.args[i], out);
            break;
        case AST_PIPE: collect_idents(n->as.pipe.left, out); collect_idents(n->as.pipe.right, out); break;
        case AST_IF: collect_idents(n->as.if_stmt.condition, out); collect_idents(n->as.if_stmt.then_block, out);
            collect_idents(n->as.if_stmt.else_block, out); break;
        case AST_WHILE: collect_idents(n->as.while_stmt.condition, out); collect_idents(n->as.while_stmt.body, out); break;
        case AST_FOR: collect_idents(n->as.for_stmt.iterable, out); collect_idents(n->as.for_stmt.body, out); break;
        case AST_BLOCK: for (int i = 0; i < n->as.block.count; i++) collect_idents(n->as.block.stmts[i], out); break;
        case AST_LET: collect_idents(n->as.let.value, out); break;
        case AST_ASSIGN: collect_idents(n->as.assign.target, out); collect_idents(n->as.assign.value, out); break;
        case AST_RETURN: collect_idents(n->as.ret.value, out); break;
        case AST_MATCH: collect_idents(n->as.match_expr.value, out);
            for (int i = 0; i < n->as.match_expr.case_count; i++) {
                collect_idents(n->as.match_expr.cases[i].pattern, out);
                collect_idents(n->as.match_expr.cases[i].result, out); } break;
        case AST_DEFER: collect_idents(n->as.defer_stmt.expr, out); break;
        case AST_TRY_EXPR: collect_idents(n->as.try_expr.operand, out); break;
        case AST_CATCH_EXPR: collect_idents(n->as.catch_expr.operand, out); collect_idents(n->as.catch_expr.handler, out); break;
        case AST_PANIC_EXPR: collect_idents(n->as.panic_expr.message, out); break;
        case AST_ASSERT_EXPR: collect_idents(n->as.assert_expr.condition, out); collect_idents(n->as.assert_expr.message, out); break;
        case AST_TUPLE: for (int i = 0; i < n->as.tuple.count; i++) collect_idents(n->as.tuple.elements[i], out); break;
        case AST_ARRAY_LITERAL: for (int i = 0; i < n->as.array_literal.count; i++) collect_idents(n->as.array_literal.elements[i], out); break;
        case AST_INDEX: collect_idents(n->as.binary.left, out); collect_idents(n->as.binary.right, out); break;
        case AST_LEN_EXPR: collect_idents(n->as.len_expr.operand, out); break;
        case AST_OK_EXPR: collect_idents(n->as.ok_expr.value, out); break;
        case AST_ERR_EXPR: collect_idents(n->as.err_expr.value, out); break;
        case AST_CLOSURE: collect_idents(n->as.closure.body, out); break;
        default: break;
    }
}

/* Codegen error — prints message and sets error flag */
static void codegen_error(codegen_t *cg, const char *fmt, ...) {
    fprintf(stderr, "codegen error: ");
    va_list a; va_start(a, fmt); vfprintf(stderr, fmt, a); va_end(a);
    fprintf(stderr, "\n");
    cg->has_error = 1;
    cg->error_count++;
}

/* Check snprintf result for truncation */
static void buf_check(codegen_t *cg, int written, size_t bufsize, const char *context) {
    if (written < 0 || (size_t)written >= bufsize) {
        codegen_error(cg, "identifier too long (%s), max %zu chars", context, bufsize - 1);
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
static void register_module(codegen_t *cg, const char *name, size_t name_len, int is_stdlib);
static int is_stdlib_module(codegen_t *cg, const char *name, size_t name_len);
static int is_float_type(codegen_t *cg, ast_node_t *n);

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

/* Find symbol and check if it's float */
static int find_sym_is_float(codegen_t *cg, const char *name, int *is_float) {
    for (int i = cg->symbol_count - 1; i >= 0; i--) {
        if (strcmp(cg->symbols[i].name, name) == 0) {
            *is_float = cg->symbols[i].is_float;
            return cg->symbols[i].stack_offset;
        }
    }
    *is_float = 0;
    return -1;
}

static int add_sym_float(codegen_t *cg, const char *name, int size, int is_float) {
    cg->stack_size += size;
    if (cg->stack_size > cg->max_stack_size) cg->max_stack_size = cg->stack_size;
    int off = cg->stack_size;
    cg->symbol_count++;
    cg->symbols = SAFE_REALLOC(cg->symbols, sizeof(*cg->symbols) * cg->symbol_count);
    cg->symbols[cg->symbol_count - 1].name = SAFE_STRDUP(name);
    cg->symbols[cg->symbol_count - 1].stack_offset = off;
    cg->symbols[cg->symbol_count - 1].size = size;
    cg->symbols[cg->symbol_count - 1].is_float = is_float;
    return off;
}

static int add_sym(codegen_t *cg, const char *name, int size) {
    return add_sym_float(cg, name, size, 0);
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

/* Find string label by value. Returns label number, or -1 if not found.
 * Caller should use fprintf(cg->output, "str_%d", label) to format. */
static int find_string_label(codegen_t *cg, const char *value, size_t length) {
    for (int i = 0; i < cg->string_entries; i++) {
        if (cg->strings[i].length == length && memcmp(cg->strings[i].value, value, length) == 0) {
            return cg->strings[i].label;
        }
    }
    return -1;
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
        if (!d) continue;
        if (d->type == AST_FN_DECL &&
            d->as.fn_decl.name_len == len &&
            memcmp(d->as.fn_decl.name, name, len) == 0)
            return 1;
        if (d->type == AST_STRUCT_DECL &&
            d->as.struct_decl.name_len == len &&
            memcmp(d->as.struct_decl.name, name, len) == 0)
            return 1;
        if (d->type == AST_IMPL_DECL) {
            for (int mi = 0; mi < d->as.impl_decl.method_count; mi++) {
                ast_node_t *method = d->as.impl_decl.methods[mi];
                if (method->type != AST_FN_DECL) continue;
                /* Build prefixed name: Type_method */
                char prefixed[MAX_IDENT_LEN];
                int plen = snprintf(prefixed, sizeof(prefixed), "%.*s_%.*s",
                    (int)d->as.impl_decl.type_name_len, d->as.impl_decl.type_name,
                    (int)method->as.fn_decl.name_len, method->as.fn_decl.name);
                if (plen == (int)len && memcmp(prefixed, name, len) == 0)
                    return 1;
            }
        }
    }
    return 0;
}

/* Check if name is a NASM keyword — used to avoid conflicts */
static int is_nasm_keyword(const char *name, size_t len) {
    static const char *kw[] = {
        "abs","pop","ret","call","jmp","jnz","jz","jge","jle","jg","jl","je","jne",
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

static void emit_call_target(codegen_t *cg, ast_node_t *callee) {
    if (callee->type == AST_BINARY_OP &&
        callee->as.binary.op == TOKEN_COLONCOLON) {
        ast_node_t *mod = callee->as.binary.left;
        ast_node_t *fn = callee->as.binary.right;
        char mod_name[MAX_IDENT_LEN];
        buf_check(cg, snprintf(mod_name, sizeof(mod_name), "%.*s",
            (int)mod->as.ident.name_len, mod->as.ident.name),
            sizeof(mod_name), "module name");
        if (is_stdlib_module(cg, mod_name, strlen(mod_name))) {
            /* stdlib module: call function directly (no prefix, separate .o) */
            char fn_name[MAX_IDENT_LEN];
            buf_check(cg, snprintf(fn_name, sizeof(fn_name), "%.*s",
                (int)fn->as.ident.name_len, fn->as.ident.name),
                sizeof(fn_name), "function name");
            add_extern(cg, fn_name);
            fprintf(cg->output, "    call %s\n", fn_name);
        } else {
            /* Project module: function is merged into same binary, call with _ prefix */
            fprintf(cg->output, "    call _%.*s_%.*s\n",
                (int)mod->as.ident.name_len, mod->as.ident.name,
                (int)fn->as.ident.name_len, fn->as.ident.name);
        }
    } else {
        char fn_name[MAX_IDENT_LEN];
        buf_check(cg, snprintf(fn_name, sizeof(fn_name), "%.*s",
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
        if (reg_count > 0) { gen_expr(cg, args[0]); cg->output = real_output;
            if (is_float_type(cg, args[0])) emit(cg, "movq rax, xmm0");
            emit(cg, "push rax"); }
        if (reg_count > 1) { gen_expr(cg, args[1]); cg->output = real_output;
            if (is_float_type(cg, args[1])) emit(cg, "movq rax, xmm0");
            emit(cg, "mov rbx, rax"); }
        for (int i = 2; i < reg_count; i++) {
            gen_expr(cg, args[i]); cg->output = real_output;
            if (is_float_type(cg, args[i])) emit(cg, "movq rax, xmm0");
            emit(cg, "mov %s, rax", regs[i]);
        }
        if (reg_count > 0) { emit(cg, "pop rax"); emit(cg, "mov rsi, rax"); }
        if (reg_count > 1) emit(cg, "mov rdx, rbx");
    } else {
        /* Standard: arg0→rdi, arg1→rsi, arg2→rdx, ... */
        if (reg_count > 0) { gen_expr(cg, args[0]); cg->output = real_output;
            if (is_float_type(cg, args[0])) emit(cg, "movq rax, xmm0");
            emit(cg, "push rax"); }
        if (reg_count > 1) { gen_expr(cg, args[1]); cg->output = real_output;
            if (is_float_type(cg, args[1])) emit(cg, "movq rax, xmm0");
            emit(cg, "mov rbx, rax"); }
        for (int i = 2; i < reg_count; i++) {
            gen_expr(cg, args[i]); cg->output = real_output;
            if (is_float_type(cg, args[i])) emit(cg, "movq rax, xmm0");
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

/* Emit a closure call: load [fn_ptr, env_ptr] from closure object on stack.
 * clo_stack_off = stack offset where closure object [fn_ptr:8][env_ptr:8] lives.
 * rdi = env_ptr, rsi = arg0, rdx = arg1, ... (closure calling convention). */
static void emit_closure_call(codegen_t *cg, int clo_stack_off, ast_node_t **args, int narg) {
    const char *regs[] = {"rsi","rdx","rcx","r8","r9"};
    int reg_count = narg < 5 ? narg : 5;
    int stack_args = narg > 5 ? narg - 5 : 0;
    FILE *real_output = cg->output;

    /* Save rbx (twice for alignment) */
    emit(cg, "push rbx");
    emit(cg, "push rbx");

    /* Load closure object: fn_ptr → r10, env_ptr → rdi */
    emit(cg, "mov rax, [rbp-%d]", clo_stack_off);
    emit(cg, "mov r10, [rax]");     /* fn_ptr → r10 (preserved across gen_expr) */
    emit(cg, "mov rdi, [rax+8]");   /* env_ptr → rdi */

    /* Evaluate args: arg0→r11 spill, arg1→rbx, arg2+→regs */
    if (reg_count > 0) { gen_expr(cg, args[0]); cg->output = real_output; emit(cg, "push rax"); }
    if (reg_count > 1) { gen_expr(cg, args[1]); cg->output = real_output; emit(cg, "mov rbx, rax"); }
    for (int i = 2; i < reg_count; i++) {
        gen_expr(cg, args[i]); cg->output = real_output;
        emit(cg, "mov %s, rax", regs[i]);
    }
    if (reg_count > 0) { emit(cg, "pop rax"); emit(cg, "mov rsi, rax"); }
    if (reg_count > 1) emit(cg, "mov rdx, rbx");

    /* Restore rbx */
    emit(cg, "pop rbx");
    emit(cg, "pop rbx");

    /* Align rsp to 16 bytes */
    int total = cg->stack_size + 8 * stack_args + 16;
    int align = (16 - (total % 16)) % 16;
    if (align) emit(cg, "sub rsp, %d", align);

    emit(cg, "call r10");

    int cleanup = align + 8 * stack_args;
    if (cleanup) emit(cg, "add rsp, %d", cleanup);
}

/* --- Defer support --- */
static void push_defer(codegen_t *cg, ast_node_t *expr) {
    if (cg->defer_count >= cg->defer_cap) {
        cg->defer_cap = cg->defer_cap ? cg->defer_cap * 2 : 8;
        cg->defers = SAFE_REALLOC(cg->defers, sizeof(*cg->defers) * cg->defer_cap);
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

/* Resolve named arguments and default values for a function call.
 * Builds a temporary resolved argument list WITHOUT modifying the AST.
 * Returns resolved args in *out_args and count in *out_count.
 * Caller must free *out_args (but NOT the individual ast nodes). */
static int resolve_call_args(codegen_t *cg, ast_node_t *call_node,
                             ast_node_t ***out_args, int *out_count) {
    *out_args = NULL;
    *out_count = 0;
    if (call_node->type != AST_CALL) return 0;
    if (call_node->as.call.callee->type != AST_IDENT) return 0;

    /* Find the function declaration */
    ast_node_t *fn_decl = NULL;
    for (int i = 0; i < cg->prog->as.program.count; i++) {
        ast_node_t *d = cg->prog->as.program.declarations[i];
        if (d && d->type == AST_FN_DECL &&
            d->as.fn_decl.name_len == call_node->as.call.callee->as.ident.name_len &&
            memcmp(d->as.fn_decl.name, call_node->as.call.callee->as.ident.name,
                d->as.fn_decl.name_len) == 0) {
            fn_decl = d;
            break;
        }
    }
    if (!fn_decl) return 0;

    int param_count = fn_decl->as.fn_decl.param_count;
    if (param_count == 0) return 0;

    /* Check if any argument is a keyword (AST_BINARY_OP with TOKEN_COLON) */
    int has_keywords = 0;
    for (int i = 0; i < call_node->as.call.arg_count; i++) {
        ast_node_t *arg = call_node->as.call.args[i];
        if (arg && arg->type == AST_BINARY_OP && arg->as.binary.op == TOKEN_COLON) {
            has_keywords = 1;
            break;
        }
    }
    int has_defaults = 0;
    for (int p = 0; p < param_count; p++) {
        if (fn_decl->as.fn_decl.params[p].default_value) { has_defaults = 1; break; }
    }
    if (!has_keywords && !has_defaults) return 0;
    if (!has_keywords && call_node->as.call.arg_count == param_count) return 0;

    /* Build resolved argument array [param_count] */
    ast_node_t **resolved = SAFE_CALLOC(param_count, sizeof(ast_node_t*));

    /* First pass: place keyword arguments by name */
    int positional_idx = 0;
    for (int i = 0; i < call_node->as.call.arg_count; i++) {
        ast_node_t *arg = call_node->as.call.args[i];
        if (arg && arg->type == AST_BINARY_OP && arg->as.binary.op == TOKEN_COLON &&
            arg->as.binary.left->type == AST_IDENT) {
            ast_node_t *key = arg->as.binary.left;
            for (int p = 0; p < param_count; p++) {
                if ((int)strlen(fn_decl->as.fn_decl.params[p].name) == key->as.ident.name_len &&
                    memcmp(fn_decl->as.fn_decl.params[p].name, key->as.ident.name, key->as.ident.name_len) == 0) {
                    resolved[p] = arg->as.binary.right;
                    break;
                }
            }
        } else {
            while (positional_idx < param_count && resolved[positional_idx])
                positional_idx++;
            if (positional_idx < param_count) {
                resolved[positional_idx] = arg;
                positional_idx++;
            }
        }
    }

    /* Second pass: fill in defaults for remaining NULLs */
    for (int p = 0; p < param_count; p++) {
        if (!resolved[p] && fn_decl->as.fn_decl.params[p].default_value) {
            resolved[p] = fn_decl->as.fn_decl.params[p].default_value;
        }
    }

    *out_args = resolved;
    *out_count = param_count;
    return 1;
}

/* --- Float type detection --- */
static int is_float_type(codegen_t *cg, ast_node_t *n) {
    if (!n) return 0;
    if (n->type == AST_FLOAT_LIT) return 1;
    if (n->type == AST_IDENT) {
        for (int vi = 0; vi < cg->var_type_count; vi++) {
            if ((int)strlen(cg->var_types[vi].name) == n->as.ident.name_len &&
                memcmp(cg->var_types[vi].name, n->as.ident.name, n->as.ident.name_len) == 0) {
                const char *sn = cg->var_types[vi].struct_name;
                if (sn && (strcmp(sn, "f32") == 0 || strcmp(sn, "f64") == 0))
                    return 1;
            }
        }
    }
    return 0;
}

/* Register a float constant and return its label */
static int add_float_const(codegen_t *cg, double val) {
    /* Check if already registered */
    for (int i = 0; i < cg->float_entries_count; i++) {
        uint64_t a, b;
        memcpy(&a, &cg->float_entries[i].value, sizeof(a));
        memcpy(&b, &val, sizeof(b));
        if (a == b) return cg->float_entries[i].label;
    }
    int lbl = new_label(cg);
    cg->float_entries_count++;
    cg->float_entries = SAFE_REALLOC(cg->float_entries, sizeof(*cg->float_entries) * cg->float_entries_count);
    cg->float_entries[cg->float_entries_count - 1].label = lbl;
    cg->float_entries[cg->float_entries_count - 1].value = val;
    return lbl;
}

/* Collect float constants from AST */
static void collect_floats(codegen_t *cg, ast_node_t *n) {
    if (!n) return;
    switch (n->type) {
        case AST_FLOAT_LIT: add_float_const(cg, n->as.float_val); break;
        case AST_BINARY_OP: collect_floats(cg, n->as.binary.left); collect_floats(cg, n->as.binary.right); break;
        case AST_LET: collect_floats(cg, n->as.let.value); break;
        case AST_ASSIGN: collect_floats(cg, n->as.assign.value); break;
        case AST_FN_DECL: collect_floats(cg, n->as.fn_decl.body);
            for (int i = 0; i < n->as.fn_decl.param_count; i++)
                collect_floats(cg, n->as.fn_decl.params[i].default_value);
            break;
        case AST_BLOCK: for (int i = 0; i < n->as.block.count; i++) collect_floats(cg, n->as.block.stmts[i]); break;
        case AST_RETURN: collect_floats(cg, n->as.ret.value); break;
        case AST_IF: collect_floats(cg, n->as.if_stmt.condition); collect_floats(cg, n->as.if_stmt.then_block); collect_floats(cg, n->as.if_stmt.else_block); break;
        case AST_WHILE: collect_floats(cg, n->as.while_stmt.condition); collect_floats(cg, n->as.while_stmt.body); break;
        case AST_FOR: collect_floats(cg, n->as.for_stmt.iterable); collect_floats(cg, n->as.for_stmt.body); break;
        case AST_CALL: collect_floats(cg, n->as.call.callee); for (int i = 0; i < n->as.call.arg_count; i++) collect_floats(cg, n->as.call.args[i]); break;
        case AST_PIPE: collect_floats(cg, n->as.pipe.left); collect_floats(cg, n->as.pipe.right); break;
        case AST_MATCH: collect_floats(cg, n->as.match_expr.value); break;
        case AST_PROGRAM: for (int i = 0; i < n->as.program.count; i++) collect_floats(cg, n->as.program.declarations[i]); break;
        default: break;
    }
}

/* --- Compile-time print dispatch --- */
/* Detects print(...) calls and generates type-specific calls.
 * Supports: sep: ", " and end: "\n" keyword arguments. */
static void gen_print_call(codegen_t *cg, ast_node_t *n) {
    /* Separate print args from keyword args (sep, end) */
    ast_node_t *print_args[64];
    int print_argc = 0;
    const char *sep_str = NULL;
    size_t sep_len = 0;
    const char *end_str = NULL;
    size_t end_len = 0;

    for (int i = 0; i < n->as.call.arg_count; i++) {
        ast_node_t *arg = n->as.call.args[i];
        /* Check for keyword arg: AST_BINARY_OP with TOKEN_COLON (name: value) */
        if (arg && arg->type == AST_BINARY_OP && arg->as.binary.op == TOKEN_COLON &&
            arg->as.binary.left->type == AST_IDENT) {
            ast_node_t *key = arg->as.binary.left;
            ast_node_t *val = arg->as.binary.right;
            if (val && val->type == AST_STRING_LIT) {
                if (key->as.ident.name_len == 3 && memcmp(key->as.ident.name, "sep", 3) == 0) {
                    sep_str = val->as.string_val.value;
                    sep_len = val->as.string_val.length;
                    continue;
                }
                if (key->as.ident.name_len == 3 && memcmp(key->as.ident.name, "end", 3) == 0) {
                    end_str = val->as.string_val.value;
                    end_len = val->as.string_val.length;
                    continue;
                }
            }
        }
        if (print_argc < 64)
            print_args[print_argc++] = arg;
    }

    /* Generate calls for each print argument */
    for (int i = 0; i < print_argc; i++) {
        ast_node_t *arg = print_args[i];
        if (!arg) continue;

        /* Determine print function by type */
        const char *fn = NULL;
        switch (arg->type) {
            case AST_INT_LIT: fn = "print_i64"; break;
            case AST_FLOAT_LIT: fn = "print_f64"; break;
            case AST_STRING_LIT: fn = "print_str"; break;
            case AST_BOOL_LIT: fn = "print_bool"; break;
            case AST_BINARY_OP:
                /* Could be unary minus: 0 - N → print as i64 */
                if (arg->as.binary.op == TOKEN_MINUS &&
                    arg->as.binary.left->type == AST_INT_LIT &&
                    arg->as.binary.left->as.int_val == 0) {
                    fn = "print_i64";
                } else if (arg->as.binary.op == TOKEN_COLONCOLON) {
                    fn = "print_ptr"; /* module::func */
                } else {
                    fn = "print_i64"; /* arithmetic expression */
                }
                break;
            case AST_IDENT: {
                /* Try to resolve type from var_types */
                int found_type = 0;
                for (int vi = 0; vi < cg->var_type_count; vi++) {
                    if ((int)strlen(cg->var_types[vi].name) == arg->as.ident.name_len &&
                        memcmp(cg->var_types[vi].name, arg->as.ident.name, arg->as.ident.name_len) == 0) {
                        const char *sn = cg->var_types[vi].struct_name;
                        if (sn && strcmp(sn, "string") == 0) fn = "print_str";
                        else if (sn && strcmp(sn, "bool") == 0) fn = "print_bool";
                        else if (sn && (strcmp(sn, "f32") == 0 || strcmp(sn, "f64") == 0)) fn = "print_f64";
                        else fn = "print_i64";
                        found_type = 1;
                        break;
                    }
                }
                if (!found_type) {
                    /* Check if it's a user-defined function (print as pointer) */
                    if (is_user_defined(cg, arg->as.ident.name, arg->as.ident.name_len))
                        fn = "print_ptr";
                    else
                        fn = "print_i64"; /* fallback: print as integer */
                }
                break;
            }
            case AST_CALL:
                /* function call result — print as integer (return value) */
                fn = "print_i64";
                break;
            case AST_ARRAY_LITERAL:
                /* Dispatch by element type */
                if (arg->as.array_literal.count > 0) {
                    ast_node_t *first = arg->as.array_literal.elements[0];
                    if (first->type == AST_STRING_LIT) fn = "print_arr_str";
                    else if (first->type == AST_BOOL_LIT) fn = "print_arr_bool";
                    else fn = "print_arr_i64";
                } else {
                    fn = "print_arr_i64"; /* empty array */
                }
                break;
            case AST_OK_EXPR:
            case AST_ERR_EXPR:
                fn = "print_i64"; /* Result type — print raw value */
                break;
            case AST_TUPLE: {
                /* Print tuple: (elem0, elem1, ...) */
                add_extern(cg, "print_str");
                add_extern(cg, "str_lparen");
                add_extern(cg, "str_rparen");
                add_extern(cg, "str_comma");
                fprintf(cg->output, "    lea rdi, [rel str_lparen]\n");
                fprintf(cg->output, "    call print_str\n");
                for (int ti = 0; ti < arg->as.tuple.count; ti++) {
                    if (ti > 0) {
                        fprintf(cg->output, "    lea rdi, [rel str_comma]\n");
                        fprintf(cg->output, "    call print_str\n");
                    }
                    ast_node_t *el = arg->as.tuple.elements[ti];
                    const char *efn = "print_i64";
                    if (el->type == AST_STRING_LIT) efn = "print_str";
                    else if (el->type == AST_BOOL_LIT) efn = "print_bool";
                    else if (el->type == AST_FLOAT_LIT) efn = "print_f64";
                    gen_expr(cg, el);
                    if (strcmp(efn, "print_f64") == 0) emit(cg, "movq rax, xmm0");
                    emit(cg, "mov rdi, rax");
                    add_extern(cg, efn);
                    fprintf(cg->output, "    call %s\n", efn);
                }
                fprintf(cg->output, "    lea rdi, [rel str_rparen]\n");
                fprintf(cg->output, "    call print_str\n");
                fn = NULL; /* already handled */
                break;
            }
            default:
                fn = "print_i64";
                break;
        }

        /* Generate: evaluate arg → rdi, call fn (skip if already handled, e.g. tuple) */
        if (fn == NULL) continue;
        gen_expr(cg, arg);
        if (strcmp(fn, "print_f64") == 0) {
            /* Float value is in xmm0 — extract bits to rax, then to rdi */
            emit(cg, "movq rax, xmm0");
        }
        emit(cg, "mov rdi, rax");
        add_extern(cg, fn);
        fprintf(cg->output, "    call %s\n", fn);

        /* Print separator between arguments */
        if (i < print_argc - 1) {
            if (sep_str) {
                /* Find or create string label for separator */
                int lbl = find_string_label(cg, sep_str, sep_len);
                if (lbl >= 0) {
                    fprintf(cg->output, "    lea rdi, [str_%d]\n", lbl);
                    add_extern(cg, "print_str");
                    fprintf(cg->output, "    call print_str\n");
                }
            }
        }
    }

    /* Print end string (default: newline) */
    if (end_str) {
        if (end_len == 0) {
            /* end: "" — no output */
        } else {
            int lbl = find_string_label(cg, end_str, end_len);
            if (lbl >= 0) {
                fprintf(cg->output, "    lea rdi, [str_%d]\n", lbl);
                add_extern(cg, "print_str");
                fprintf(cg->output, "    call print_str\n");
            }
        }
    } else {
        /* Default: newline */
        add_extern(cg, "print_newline");
        fprintf(cg->output, "    call print_newline\n");
    }
}

static void gen_expr(codegen_t *cg, ast_node_t *n) {
    if (!n || cg->has_error) return;
    switch (n->type) {
        case AST_INT_LIT: emit(cg, "mov rax, %ld", n->as.int_val); break;
        case AST_FLOAT_LIT: {
            /* Load float constant from .data section */
            int lbl = add_float_const(cg, n->as.float_val);
            emit(cg, "movsd xmm0, [rel fconst_%d]", lbl);
            break;
        }
        case AST_BOOL_LIT: emit(cg, "mov rax, %d", n->as.bool_val ? 1 : 0); break;
        case AST_STRING_LIT: {
            int lbl = find_string_label(cg, n->as.string_val.value, n->as.string_val.length);
            if (lbl >= 0) fprintf(cg->output, "    lea rax, [str_%d]\n", lbl);
            break; }
        case AST_IDENT: {
            int sym_float = 0;
            int off = find_sym_is_float(cg, n->as.ident.name, &sym_float);
            if (off >= 0) {
                /* Variable — load from stack */
                if (sym_float) {
                    emit(cg, "movsd xmm0, [rbp-%d]", off);
                } else {
                    emit(cg, "mov rax, [rbp-%d]", off);
                }
            } else if (is_user_defined(cg, n->as.ident.name, n->as.ident.name_len)) {
                /* Function name used as value — wrap in closure object [fn_ptr, NULL].
                 * This ensures the function follows the closure calling convention:
                 * rdi=env_ptr (NULL), rsi=arg0, rdx=arg1, ... */
                char fn_name[MAX_IDENT_LEN];
                buf_check(cg, snprintf(fn_name, sizeof(fn_name), "%.*s",
                    (int)n->as.ident.name_len, n->as.ident.name),
                    sizeof(fn_name), "function name");
                /* Allocate closure object inline (16 bytes) */
                int clo_off = add_sym(cg, "__fn_clo", 16);
                fprintf(cg->output, "    lea rax, [_%s]\n", fn_name);
                emit(cg, "mov [rbp-%d], rax", clo_off);     /* fn_ptr */
                emit(cg, "mov qword [rbp-%d + 8], 0", clo_off); /* env_ptr = NULL */
                emit(cg, "lea rax, [rbp-%d]", clo_off);     /* return ptr to closure object */
            } else {
                codegen_error(cg, "undefined '%.*s'",
                    (int)n->as.ident.name_len, n->as.ident.name);
                return;
            }
            break; }
        case AST_BINARY_OP:
            /* Check for enum literal: Color::Red */
            if (n->as.binary.op == TOKEN_COLONCOLON &&
                n->as.binary.left->type == AST_IDENT) {
                char enum_name[MAX_IDENT_LEN];
                buf_check(cg, snprintf(enum_name, sizeof(enum_name), "%.*s",
                    (int)n->as.binary.left->as.ident.name_len,
                    n->as.binary.left->as.ident.name),
                    sizeof(enum_name), "enum name");
                /* Find variant index */
                int tag = -1;
                for (int ei = 0; ei < cg->prog->as.program.count; ei++) {
                    ast_node_t *d = cg->prog->as.program.declarations[ei];
                    if (d && d->type == AST_ENUM_DECL &&
                        d->as.enum_decl.name_len == strlen(enum_name) &&
                        memcmp(d->as.enum_decl.name, enum_name,
                            d->as.enum_decl.name_len) == 0) {
                        for (int vi = 0; vi < d->as.enum_decl.variant_count; vi++) {
                            if (d->as.enum_decl.variants[vi].name_len ==
                                    n->as.binary.right->as.ident.name_len &&
                                memcmp(d->as.enum_decl.variants[vi].name,
                                    n->as.binary.right->as.ident.name,
                                    d->as.enum_decl.variants[vi].name_len) == 0) {
                                tag = vi; break;
                            }
                        }
                        break;
                    }
                }
                if (tag >= 0) {
                    emit(cg, "mov rax, %d", tag);
                } else {
                    codegen_error(cg, "unknown enum variant '%.*s'",
                        (int)n->as.binary.right->as.ident.name_len,
                        n->as.binary.right->as.ident.name);
                }
                break;
            }
            gen_expr(cg, n->as.binary.left);
            int left_is_float = (cg->output != NULL); /* placeholder */
            /* Check if either operand is float */
            int use_float = is_float_type(cg, n->as.binary.left) || is_float_type(cg, n->as.binary.right);

            if (use_float) {
                /* Float binary operations via SSE2 */
                /* left value is already in xmm0 from gen_expr (if float literal) */
                /* For non-float left: convert rax to xmm0 */
                if (!is_float_type(cg, n->as.binary.left)) {
                    emit(cg, "cvtsi2sd xmm0, rax");
                }
                emit(cg, "movsd [rbp-8], xmm0"); /* save left */
                gen_expr(cg, n->as.binary.right);
                if (!is_float_type(cg, n->as.binary.right)) {
                    emit(cg, "cvtsi2sd xmm0, rax");
                }
                emit(cg, "movsd xmm1, xmm0"); /* xmm1 = right */
                emit(cg, "movsd xmm0, [rbp-8]"); /* xmm0 = left */
                switch (n->as.binary.op) {
                    case TOKEN_PLUS: emit(cg, "addsd xmm0, xmm1"); break;
                    case TOKEN_MINUS: emit(cg, "subsd xmm0, xmm1"); break;
                    case TOKEN_STAR: emit(cg, "mulsd xmm0, xmm1"); break;
                    case TOKEN_SLASH: emit(cg, "divsd xmm0, xmm1"); break;
                    case TOKEN_EQ: emit(cg, "cmpsd xmm0, xmm1, 0"); emit(cg, "movd eax, xmm0"); emit(cg, "movzx rax, al"); break;
                    case TOKEN_NEQ: emit(cg, "cmpsd xmm0, xmm1, 0"); emit(cg, "movd eax, xmm0"); emit(cg, "xor al, 1"); emit(cg, "movzx rax, al"); break;
                    case TOKEN_LT: emit(cg, "cmpsd xmm0, xmm1, 1"); emit(cg, "movd eax, xmm0"); emit(cg, "movzx rax, al"); break;
                    case TOKEN_GT: emit(cg, "cmpsd xmm1, xmm0, 1"); emit(cg, "movd eax, xmm0"); emit(cg, "movzx rax, al"); break;
                    case TOKEN_LTE: emit(cg, "cmpsd xmm0, xmm1, 1"); emit(cg, "movd eax, xmm0"); emit(cg, "movzx rax, al");
                        /* Also check equality */
                        emit(cg, "movsd xmm0, [rbp-8]");
                        emit(cg, "cmpsd xmm0, xmm1, 0");
                        emit(cg, "movd ecx, xmm0");
                        emit(cg, "or al, cl");
                        emit(cg, "movzx rax, al");
                        break;
                    case TOKEN_GTE: emit(cg, "cmpsd xmm1, xmm0, 1"); emit(cg, "movd eax, xmm0"); emit(cg, "movzx rax, al");
                        emit(cg, "movsd xmm0, [rbp-8]");
                        emit(cg, "cmpsd xmm0, xmm1, 0");
                        emit(cg, "movd ecx, xmm0");
                        emit(cg, "or al, cl");
                        emit(cg, "movzx rax, al");
                        break;
                    default: break;
                }
            } else {
                /* Integer binary operations */
                emit(cg, "push rax");
                gen_expr(cg, n->as.binary.right);
                { int tmp_off = add_sym(cg, "__binop", 8);
                  emit(cg, "mov [rbp-%d], rax", tmp_off);
                  emit(cg, "pop rax");
                  emit(cg, "mov rcx, [rbp-%d]", tmp_off); }
                switch (n->as.binary.op) {
                    case TOKEN_PLUS: emit(cg, "add rax, rcx"); break;
                    case TOKEN_MINUS: emit(cg, "sub rax, rcx"); break;
                    case TOKEN_STAR: emit(cg, "imul rax, rcx"); break;
                    case TOKEN_SLASH:
                        emit(cg, "test rcx, rcx");
                        { int dz_label = new_label(cg);
                          emit(cg, "jnz L%d", dz_label);
                          emit(cg, "lea rdi, [rel div_zero_msg]");
                          emit(cg, "xor rsi, rsi");
                          emit(cg, "xor rdx, rdx");
                          add_extern(cg, "panic_handler");
                          add_extern(cg, "div_zero_msg");
                          fprintf(cg->output, "    call panic_handler\n");
                          fprintf(cg->output, "L%d:\n", dz_label); }
                        emit(cg, "cqo"); emit(cg, "idiv rcx"); break;
                    case TOKEN_EQ: emit(cg, "cmp rax, rcx"); emit(cg, "sete al"); emit(cg, "movzx rax, al"); break;
                    case TOKEN_NEQ: emit(cg, "cmp rax, rcx"); emit(cg, "setne al"); emit(cg, "movzx rax, al"); break;
                    case TOKEN_LT: emit(cg, "cmp rax, rcx"); emit(cg, "setl al"); emit(cg, "movzx rax, al"); break;
                    case TOKEN_GT: emit(cg, "cmp rax, rcx"); emit(cg, "setg al"); emit(cg, "movzx rax, al"); break;
                    case TOKEN_LTE: emit(cg, "cmp rax, rcx"); emit(cg, "setle al"); emit(cg, "movzx rax, al"); break;
                    case TOKEN_GTE: emit(cg, "cmp rax, rcx"); emit(cg, "setge al"); emit(cg, "movzx rax, al"); break;
                    case TOKEN_AND: emit(cg, "and rax, rcx"); break;
                    case TOKEN_OR: emit(cg, "or rax, rcx"); break;
                    case TOKEN_PERCENT:
                        emit(cg, "test rcx, rcx");
                        { int dz_label = new_label(cg);
                          emit(cg, "jnz L%d", dz_label);
                          emit(cg, "lea rdi, [rel div_zero_msg]");
                          emit(cg, "xor rsi, rsi");
                          emit(cg, "xor rdx, rdx");
                          add_extern(cg, "panic_handler");
                          fprintf(cg->output, "    call panic_handler\n");
                          fprintf(cg->output, "L%d:\n", dz_label); }
                        emit(cg, "xor rdx, rdx"); emit(cg, "div rcx"); emit(cg, "mov rax, rdx"); break;
                    default: break;
                }
            } break;
        case AST_CALL: {
            /* --- Universal print dispatch --- */
            if (n->as.call.callee->type == AST_IDENT &&
                n->as.call.callee->as.ident.name_len == 5 &&
                memcmp(n->as.call.callee->as.ident.name, "print", 5) == 0) {
                gen_print_call(cg, n);
                break;
            }

            /* --- Resolve named args + defaults for user-defined functions --- */
            ast_node_t **resolved_args = NULL;
            int resolved_count = 0;
            int was_resolved = resolve_call_args(cg, n, &resolved_args, &resolved_count);

            int is_user = (n->as.call.callee->type == AST_IDENT &&
                is_user_defined(cg, n->as.call.callee->as.ident.name,
                    n->as.call.callee->as.ident.name_len));
            int is_struct = 0;
            if (is_user && n->as.call.callee->type == AST_IDENT) {
                for (int i = 0; i < cg->prog->as.program.count; i++) {
                    ast_node_t *d = cg->prog->as.program.declarations[i];
                    if (d && d->type == AST_STRUCT_DECL &&
                        d->as.struct_decl.name_len == n->as.call.callee->as.ident.name_len &&
                        memcmp(d->as.struct_decl.name, n->as.call.callee->as.ident.name,
                            d->as.struct_decl.name_len) == 0) {
                        is_struct = 1; break;
                    }
                }
            }
            /* Check if this is an enum auto method (c.tag(), c.name()) */
            int is_enum_method = 0;
            char enum_method_name[MAX_IDENT_LEN] = {0};
            if (!is_user && !is_struct && n->as.call.callee->type == AST_IDENT &&
                n->as.call.arg_count > 0 && n->as.call.args[0]->type == AST_IDENT) {
                for (int vi = 0; vi < cg->var_type_count; vi++) {
                    if (strlen(cg->var_types[vi].name) == n->as.call.args[0]->as.ident.name_len &&
                        memcmp(cg->var_types[vi].name, n->as.call.args[0]->as.ident.name,
                            n->as.call.args[0]->as.ident.name_len) == 0) {
                        char *type_name = cg->var_types[vi].struct_name;
                        char method_name[32] = {0};
                        int mlen = (int)n->as.call.callee->as.ident.name_len;
                        if (mlen < 32) {
                            memcpy(method_name, n->as.call.callee->as.ident.name, mlen);
                            method_name[mlen] = '\0';
                        }
                        if (strcmp(method_name, "tag") == 0 ||
                            strcmp(method_name, "name") == 0 ||
                            strcmp(method_name, "count") == 0) {
                            for (int ei = 0; ei < cg->prog->as.program.count; ei++) {
                                ast_node_t *d = cg->prog->as.program.declarations[ei];
                                if (d && d->type == AST_ENUM_DECL &&
                                    d->as.enum_decl.name_len == strlen(type_name) &&
                                    memcmp(d->as.enum_decl.name, type_name,
                                        d->as.enum_decl.name_len) == 0) {
                                    snprintf(enum_method_name, sizeof(enum_method_name),
                                        "%s_%s", type_name, method_name);
                                    is_enum_method = 1;
                                    break;
                                }
                            }
                        }
                        break;
                    }
                }
            }
            /* Check if this is a struct method call (first arg is struct type) */
            int is_method = 0;
            char method_type[MAX_IDENT_LEN] = {0};
            if (!is_user && !is_struct && !is_enum_method && n->as.call.callee->type == AST_IDENT &&
                n->as.call.arg_count > 0 && n->as.call.args[0]->type == AST_IDENT) {
                for (int vi = 0; vi < cg->var_type_count; vi++) {
                    if (strlen(cg->var_types[vi].name) == n->as.call.args[0]->as.ident.name_len &&
                        memcmp(cg->var_types[vi].name, n->as.call.args[0]->as.ident.name,
                            n->as.call.args[0]->as.ident.name_len) == 0) {
                        snprintf(method_type, sizeof(method_type), "%s_%.*s",
                            cg->var_types[vi].struct_name,
                            (int)n->as.call.callee->as.ident.name_len,
                            n->as.call.callee->as.ident.name);
                        if (is_user_defined(cg, method_type, strlen(method_type)))
                            is_method = 1;
                        break;
                    }
                }
            }
            /* Dispatch — use resolved args if available */
            ast_node_t **call_args = was_resolved ? resolved_args : n->as.call.args;
            int call_argc = was_resolved ? resolved_count : n->as.call.arg_count;

            if (is_enum_method) {
                ast_node_t temp = {.type = AST_IDENT,
                    .as.ident.name = enum_method_name,
                    .as.ident.name_len = strlen(enum_method_name)};
                emit_call(cg, &temp, call_args, call_argc, 0);
            } else if (is_method) {
                ast_node_t method_ident = *n->as.call.callee;
                method_ident.as.ident.name = method_type;
                method_ident.as.ident.name_len = strlen(method_type);
                ast_node_t *saved_callee = n->as.call.callee;
                n->as.call.callee = &method_ident;
                emit_call(cg, n->as.call.callee, call_args, call_argc, 1);
                n->as.call.callee = saved_callee;
            } else if (is_struct) {
                emit_call(cg, n->as.call.callee, call_args, call_argc, 0);
            } else if (is_user) {
                emit_call(cg, n->as.call.callee, call_args, call_argc, 1);
            } else if (n->as.call.callee->type == AST_IDENT) {
                int clo_off = find_sym(cg, n->as.call.callee->as.ident.name);
                if (clo_off >= 0) {
                    emit_closure_call(cg, clo_off, call_args, call_argc);
                } else {
                    emit_call(cg, n->as.call.callee, call_args, call_argc, 0);
                }
            } else {
                /* module::func() or other complex callee */
                int use_closure = 0;
                if (n->as.call.callee->type == AST_BINARY_OP &&
                    n->as.call.callee->as.binary.op == TOKEN_COLONCOLON) {
                    /* project module functions use closure convention */
                    ast_node_t *mod = n->as.call.callee->as.binary.left;
                    if (!is_stdlib_module(cg, mod->as.ident.name, mod->as.ident.name_len))
                        use_closure = 1;
                }
                emit_call(cg, n->as.call.callee, call_args, call_argc, use_closure);
            }
            if (was_resolved) free(resolved_args);
            break; }
        case AST_PIPE: {
            /* x |> f(a)  =>  f(x, a) — left goes as first arg */
            ast_node_t *callee = n->as.pipe.right;
            if (callee->type == AST_CALL) {
                /* Build combined arg list: [pipe_left, explicit args...] */
                int total = callee->as.call.arg_count + 1;
                ast_node_t **all_args = SAFE_MALLOC(sizeof(ast_node_t*) * total);
                all_args[0] = n->as.pipe.left;
                for (int i = 0; i < callee->as.call.arg_count; i++)
                    all_args[i + 1] = callee->as.call.args[i];
                int pipe_is_user = 0;
                if (callee->as.call.callee->type == AST_IDENT)
                    pipe_is_user = is_user_defined(cg, callee->as.call.callee->as.ident.name,
                        callee->as.call.callee->as.ident.name_len);
                else if (callee->as.call.callee->type == AST_BINARY_OP &&
                         callee->as.call.callee->as.binary.op == TOKEN_COLONCOLON) {
                    /* project module::func uses closure convention */
                    ast_node_t *mod = callee->as.call.callee->as.binary.left;
                    if (!is_stdlib_module(cg, mod->as.ident.name, mod->as.ident.name_len))
                        pipe_is_user = 1;
                }
                emit_call(cg, callee->as.call.callee, all_args, total, pipe_is_user ? 1 : 0);
                free(all_args);
            } else {
                /* x |> f  =>  f(x) */
                if (callee->type != AST_IDENT) {
                    codegen_error(cg, "|> right side must be a function call or identifier");
                    break;
                }
                int pipe_is_user = is_user_defined(cg, callee->as.ident.name, callee->as.ident.name_len);
                ast_node_t *single_arg = n->as.pipe.left;
                emit_call(cg, callee, &single_arg, 1, pipe_is_user ? 1 : 0);
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
                add_extern(cg, "array_push");
                fprintf(cg->output, "    call array_push\n");
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
        case AST_MEMBER: {
            /* p.x — field access */
            gen_expr(cg, n->as.member.object); /* rax = object pointer */
            /* Find struct name from var_types table */
            char *struct_name = NULL;
            if (n->as.member.object->type == AST_IDENT) {
                for (int vi = 0; vi < cg->var_type_count; vi++) {
                    if (strlen(cg->var_types[vi].name) == n->as.member.object->as.ident.name_len &&
                        memcmp(cg->var_types[vi].name, n->as.member.object->as.ident.name,
                            n->as.member.object->as.ident.name_len) == 0) {
                        struct_name = cg->var_types[vi].struct_name;
                        break;
                    }
                }
            }
            /* Find field offset */
            int field_offset = -1;
            if (struct_name) {
                for (int si = 0; si < cg->prog->as.program.count; si++) {
                    ast_node_t *d = cg->prog->as.program.declarations[si];
                    if (d && d->type == AST_STRUCT_DECL &&
                        d->as.struct_decl.name_len == strlen(struct_name) &&
                        memcmp(d->as.struct_decl.name, struct_name,
                            d->as.struct_decl.name_len) == 0) {
                        for (int fi = 0; fi < d->as.struct_decl.field_count; fi++) {
                            if (d->as.struct_decl.fields[fi].name_len == n->as.member.field_len &&
                                memcmp(d->as.struct_decl.fields[fi].name, n->as.member.field,
                                    n->as.member.field_len) == 0) {
                                field_offset = fi * 8;
                                break;
                            }
                        }
                        break;
                    }
                }
            }
            if (field_offset < 0) {
                codegen_error(cg, "unknown field '%.*s'",
                    (int)n->as.member.field_len, n->as.member.field);
                break;
            }
            emit(cg, "mov rax, [rax + %d]", field_offset);
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

            /* --- Detect captures --- */
            ident_list_t all_idents = {0};
            collect_idents(n->as.closure.body, &all_idents);

            /* Filter: keep only idents that exist in outer scope AND are not params */
            int capture_count = 0;
            char **capture_names = NULL;
            int *capture_offsets = NULL; /* stack offset of each captured var in outer scope */

            for (int ci = 0; ci < all_idents.count; ci++) {
                /* Skip closure params */
                int is_param = 0;
                for (int pi = 0; pi < n->as.closure.param_count; pi++)
                    if (n->as.closure.param_lens[pi] == all_idents.name_lens[ci] &&
                        memcmp(n->as.closure.params[pi], all_idents.names[ci], all_idents.name_lens[ci]) == 0)
                        { is_param = 1; break; }
                if (is_param) { free(all_idents.names[ci]); all_idents.names[ci] = NULL; continue; }

                /* Check if exists in outer scope */
                int off = find_sym(cg, all_idents.names[ci]);
                if (off < 0) { free(all_idents.names[ci]); all_idents.names[ci] = NULL; continue; }

                /* It's a capture — deduplicate */
                int dup = 0;
                for (int di = 0; di < capture_count; di++)
                    if (strlen(capture_names[di]) == all_idents.name_lens[ci] &&
                        memcmp(capture_names[di], all_idents.names[ci], all_idents.name_lens[ci]) == 0)
                        { dup = 1; break; }
                if (dup) { free(all_idents.names[ci]); all_idents.names[ci] = NULL; continue; }

                capture_names = SAFE_REALLOC(capture_names, sizeof(char*) * (capture_count + 1));
                capture_offsets = SAFE_REALLOC(capture_offsets, sizeof(int) * (capture_count + 1));
                capture_names[capture_count] = all_idents.names[ci]; /* take ownership */
                capture_offsets[capture_count] = off;
                all_idents.names[ci] = NULL; /* ownership transferred */
                capture_count++;
            }
            free(all_idents.name_lens);
            /* free remaining unowned names */
            for (int ci = 0; ci < all_idents.count; ci++)
                if (all_idents.names[ci]) free(all_idents.names[ci]);
            free(all_idents.names);

            /* --- Build env struct on heap (caller side) --- */
            int env_slot = -1;
            if (capture_count > 0) {
                /* with_capacity(8, capture_count) → rax = env_ptr */
                emit(cg, "mov rdi, 8");
                emit(cg, "mov rsi, %d", capture_count);
                add_extern(cg, "with_capacity");
                fprintf(cg->output, "    call with_capacity\n");
                env_slot = add_sym(cg, "__env", 8);
                emit(cg, "mov [rbp-%d], rax", env_slot);
                for (int ci = 0; ci < capture_count; ci++) {
                    emit(cg, "mov rdi, [rbp-%d]", env_slot);
                    emit(cg, "mov rax, [rbp-%d]", capture_offsets[ci]);
                    emit(cg, "mov rsi, rax");
                    add_extern(cg, "array_push");
                    fprintf(cg->output, "    call array_push\n");
                    emit(cg, "mov [rbp-%d], rax", env_slot);
                }
            }

            /* --- Buffer closure body --- */
            FILE *saved_output = cg->output;
            char *buf = NULL;
            size_t buf_len = 0;
            FILE *mem = open_memstream(&buf, &buf_len);
            cg->output = mem;

            int saved_stack = cg->stack_size;
            int saved_max = cg->max_stack_size;

            fprintf(cg->output, "_fn%d:\n", clo_label);
            emit(cg, "push rbp"); emit(cg, "mov rbp, rsp");
            emit(cg, "push rbx"); emit(cg, "push r12"); emit(cg, "push r13"); emit(cg, "push r14"); emit(cg, "push r15");
            const char *cr[] = {"rdi","rsi","rdx","rcx","r8","r9"};
            cg->stack_size = FRAME_HEADER;

            /* Save env_ptr into a stack slot before it's overwritten by args */
            int clo_env_slot = -1;
            if (capture_count > 0) {
                clo_env_slot = add_sym(cg, "__clo_env", 8);
                emit(cg, "mov [rbp-%d], rdi", clo_env_slot);
            }

            /* Store args as symbols */
            int clo_param_count = n->as.closure.param_count < 5 ? n->as.closure.param_count : 5;
            for (int ai = 0; ai < clo_param_count; ai++) {
                int poff = add_sym(cg, n->as.closure.params[ai], 8);
                emit(cg, "mov [rbp-%d], %s", poff, cr[ai + 1]);
            }

            /* Load captures from env_ptr into local stack slots */
            for (int ci = 0; ci < capture_count; ci++) {
                int coff = add_sym(cg, capture_names[ci], 8);
                emit(cg, "mov rax, [rbp-%d]", clo_env_slot);
                emit(cg, "mov rax, [rax + %d]", ci * 8);
                emit(cg, "mov [rbp-%d], rax", coff);
            }

            /* Placeholder sub rsp — patched after body is generated */
            long clo_sub_rsp_pos = ftell(cg->output);
            fprintf(cg->output, "    sub rsp, 0x00000000\n");
            gen_expr(cg, n->as.closure.body);
            /* Patch sub rsp with actual size needed */
            int clo_stack_needed = cg->max_stack_size;
            if (clo_stack_needed < CLO_MIN_FRAME + FRAME_HEADER) clo_stack_needed = CLO_MIN_FRAME + FRAME_HEADER;
            long clo_cur_pos = ftell(cg->output);
            fseek(cg->output, clo_sub_rsp_pos, SEEK_SET);
            fprintf(cg->output, "    sub rsp, 0x%08X", (unsigned)(clo_stack_needed - FRAME_HEADER));
            fseek(cg->output, clo_cur_pos, SEEK_SET);
            emit(cg, "mov rsp, rbp"); emit(cg, "pop rbp"); emit(cg, "ret");

            fclose(mem);
            cg->output = saved_output;

            /* Restore caller's stack state */
            cg->stack_size = saved_stack;
            cg->max_stack_size = saved_max;

            /* Store buffered body for later emission */
            if (cg->closure_body_count >= cg->closure_body_cap) {
                cg->closure_body_cap = cg->closure_body_cap ? cg->closure_body_cap * 2 : 8;
                cg->closure_bodies = SAFE_REALLOC(cg->closure_bodies,
                    sizeof(*cg->closure_bodies) * cg->closure_body_cap);
            }
            cg->closure_bodies[cg->closure_body_count].label = clo_label;
            cg->closure_bodies[cg->closure_body_count].asm_text = buf;
            cg->closure_bodies[cg->closure_body_count].asm_len = buf_len;
            cg->closure_body_count++;

            /* Create closure object: [fn_ptr, env_ptr] */
            int clo_off = add_sym(cg, "__clo", 16);
            emit(cg, "lea rax, [_fn%d]", clo_label);
            emit(cg, "mov [rbp-%d], rax", clo_off);
            if (env_slot >= 0)
                emit(cg, "mov rax, [rbp-%d]", env_slot);
            else
                emit(cg, "xor rax, rax");
            emit(cg, "mov [rbp-%d + 8], rax", clo_off);
            emit(cg, "lea rax, [rbp-%d]", clo_off);

            /* Cleanup capture names */
            for (int ci = 0; ci < capture_count; ci++) free(capture_names[ci]);
            free(capture_names);
            free(capture_offsets);
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
    int saved_var_type_count = cg->var_type_count;
    const char *aregs[] = {"rsi","rdx","rcx","r8","r9","r10"};
    for (int i = 0; i < n->as.fn_decl.param_count && i < 6; i++) {
        int off = add_sym(cg, n->as.fn_decl.params[i].name, 8);
        emit(cg, "mov [rbp-%d], %s", off, aregs[i]);
        /* Track parameter struct type for dot access (e.g., self: Point) */
        if (n->as.fn_decl.params[i].type_expr &&
            n->as.fn_decl.params[i].type_expr->type == AST_IDENT) {
            if (cg->var_type_count >= cg->var_type_cap) {
                cg->var_type_cap = cg->var_type_cap ? cg->var_type_cap * 2 : 8;
                cg->var_types = SAFE_REALLOC(cg->var_types,
                    sizeof(*cg->var_types) * cg->var_type_cap);
            }
            cg->var_types[cg->var_type_count].name = SAFE_STRNDUP(
                n->as.fn_decl.params[i].name, n->as.fn_decl.params[i].name_len);
            cg->var_types[cg->var_type_count].struct_name = SAFE_STRNDUP(
                n->as.fn_decl.params[i].type_expr->as.ident.name,
                n->as.fn_decl.params[i].type_expr->as.ident.name_len);
            cg->var_type_count++;
        }
    }
    cg->returned = 0;
    cg->scope_depth = 0;
    int saved_defer_count = cg->defer_count;
    cg->sub_rsp_pos = ftell(cg->output);
    fprintf(cg->output, "    sub rsp, 0x00000000\n");
    gen_node(cg, n->as.fn_decl.body);
    if (!cg->returned) emit(cg, "xor rax, rax");
    int stack_needed = cg->max_stack_size - FRAME_BASE + 16; /* 16B headroom for push depth */
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
    /* Restore var_types to pre-function state */
    for (int i = saved_var_type_count; i < cg->var_type_count; i++) {
        free(cg->var_types[i].name);
        free(cg->var_types[i].struct_name);
    }
    cg->var_type_count = saved_var_type_count;
}

static void gen_struct_decl(codegen_t *cg, ast_node_t *n) {
    fprintf(cg->output, "\n; struct %.*s\n", (int)n->as.struct_decl.name_len, n->as.struct_decl.name);
    /* Constructor name: _<Name> — callable as Point(1, 2) */
    fprintf(cg->output, "global ");
    emit_name(cg, n->as.struct_decl.name, n->as.struct_decl.name_len);
    fprintf(cg->output, "\n");
    emit_name(cg, n->as.struct_decl.name, n->as.struct_decl.name_len);
    fprintf(cg->output, ":\n");
    emit(cg, "push rbp"); emit(cg, "mov rbp, rsp");

    /* Save field values from registers to stack before allocation */
    int field_count = n->as.struct_decl.field_count;
    const char *regs[] = {"rdi","rsi","rdx","rcx","r8","r9"};
    for (int i = 0; i < field_count && i < 6; i++)
        emit(cg, "push %s", regs[i]);

    /* Heap allocation: 16 (header) + field_count * 8 (fields) */
    emit(cg, "mov rdi, %d", 16 + field_count * 8);
    add_extern(cg, "_bump_alloc");
    fprintf(cg->output, "    call _bump_alloc\n");
    /* rax = ptr to allocated block */

    /* Store header at start of block */
    emit(cg, "mov qword [rax], 1");         /* refcount = 1 */
    emit(cg, "mov qword [rax+8], %d", field_count);

    /* Pop field values and store after header */
    for (int i = field_count - 1; i >= 0; i--) {
        emit(cg, "pop rbx");
        emit(cg, "mov [rax + %d], rbx", 16 + i * 8);
    }

    /* Return pointer to data (after header) */
    emit(cg, "add rax, 16");

    emit(cg, "mov rsp, rbp"); emit(cg, "pop rbp"); emit(cg, "ret");
}

static void gen_stmt(codegen_t *cg, ast_node_t *n) {
    if (!n || cg->has_error) return;
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
                int is_val_float = is_float_type(cg, n->as.let.value);
                int off = add_sym_float(cg, n->as.let.name, 8, is_val_float);
                if (n->as.let.value) {
                    gen_expr(cg, n->as.let.value);
                    if (is_val_float) {
                        emit(cg, "movsd [rbp-%d], xmm0", off);
                    } else {
                        emit(cg, "mov [rbp-%d], rax", off);
                    }
                } else {
                    emit(cg, "mov qword [rbp-%d], 0", off);
                }
                /* Track struct/enum type for dot access */
                if (n->as.let.type_expr && n->as.let.type_expr->type == AST_IDENT) {
                    if (cg->var_type_count >= cg->var_type_cap) {
                        cg->var_type_cap = cg->var_type_cap ? cg->var_type_cap * 2 : 8;
                        cg->var_types = SAFE_REALLOC(cg->var_types,
                            sizeof(*cg->var_types) * cg->var_type_cap);
                    }
                    cg->var_types[cg->var_type_count].name = SAFE_STRDUP(n->as.let.name);
                    /* If value is an enum literal, use enum type instead of annotation */
                    if (n->as.let.value && n->as.let.value->type == AST_BINARY_OP &&
                        n->as.let.value->as.binary.op == TOKEN_COLONCOLON &&
                        n->as.let.value->as.binary.left->type == AST_IDENT) {
                        cg->var_types[cg->var_type_count].struct_name = SAFE_STRNDUP(
                            n->as.let.value->as.binary.left->as.ident.name,
                            n->as.let.value->as.binary.left->as.ident.name_len);
                    } else {
                        cg->var_types[cg->var_type_count].struct_name = SAFE_STRNDUP(
                            n->as.let.type_expr->as.ident.name,
                            n->as.let.type_expr->as.ident.name_len);
                    }
                    cg->var_type_count++;
                }
                /* Also track enum type if value is an enum literal */
                else if (n->as.let.value && n->as.let.value->type == AST_BINARY_OP &&
                    n->as.let.value->as.binary.op == TOKEN_COLONCOLON &&
                    n->as.let.value->as.binary.left->type == AST_IDENT) {
                    if (cg->var_type_count >= cg->var_type_cap) {
                        cg->var_type_cap = cg->var_type_cap ? cg->var_type_cap * 2 : 8;
                        cg->var_types = SAFE_REALLOC(cg->var_types,
                            sizeof(*cg->var_types) * cg->var_type_cap);
                    }
                    cg->var_types[cg->var_type_count].name = SAFE_STRDUP(n->as.let.name);
                    cg->var_types[cg->var_type_count].struct_name = SAFE_STRNDUP(
                        n->as.let.value->as.binary.left->as.ident.name,
                        n->as.let.value->as.binary.left->as.ident.name_len);
                    cg->var_type_count++;
                }
            }
            break; }
        case AST_ASSIGN: {
            int off = find_sym(cg, n->as.assign.target->as.ident.name);
            if (off < 0) { codegen_error(cg, "undefined '%.*s'",
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
        case AST_IMPL_DECL: {
            /* impl Type { fn method() {} } — generate methods with type prefix */
            char type_name[MAX_IDENT_LEN];
            buf_check(cg, snprintf(type_name, sizeof(type_name), "%.*s",
                (int)n->as.impl_decl.type_name_len, n->as.impl_decl.type_name),
                sizeof(type_name), "impl type name");
            for (int mi = 0; mi < n->as.impl_decl.method_count; mi++) {
                ast_node_t *method = n->as.impl_decl.methods[mi];
                if (method->type != AST_FN_DECL) continue;
                /* Generate method with prefix: _Type_method */
                fprintf(cg->output, "\n; %s::", type_name);
                fprintf(cg->output, "%.*s\n", (int)method->as.fn_decl.name_len, method->as.fn_decl.name);
                fprintf(cg->output, "global ");
                emit_name(cg, type_name, strlen(type_name));
                fprintf(cg->output, "_");
                fprintf(cg->output, "%.*s", (int)method->as.fn_decl.name_len, method->as.fn_decl.name);
                fprintf(cg->output, "\n");
                emit_name(cg, type_name, strlen(type_name));
                fprintf(cg->output, "_");
                fprintf(cg->output, "%.*s:\n", (int)method->as.fn_decl.name_len, method->as.fn_decl.name);
                /* Copy method body with modified name */
                ast_node_t saved = *method;
                char *old_name = method->as.fn_decl.name;
                size_t old_len = method->as.fn_decl.name_len;
                char new_name[MAX_IDENT_LEN];
                snprintf(new_name, sizeof(new_name), "%s_%.*s", type_name,
                    (int)old_len, old_name);
                method->as.fn_decl.name = SAFE_STRDUP(new_name);
                method->as.fn_decl.name_len = strlen(new_name);
                gen_fn_decl(cg, method);
                free(method->as.fn_decl.name);
                method->as.fn_decl.name = old_name;
                method->as.fn_decl.name_len = old_len;
            }
            break;
        }
        case AST_ENUM_DECL: {
            /* Generate enum: constants, name table, auto methods */
            int count = n->as.enum_decl.variant_count;
            char enum_name[MAX_IDENT_LEN];
            buf_check(cg, snprintf(enum_name, sizeof(enum_name), "%.*s",
                (int)n->as.enum_decl.name_len, n->as.enum_decl.name),
                sizeof(enum_name), "enum name");

            /* Emit name table in .data */
            fprintf(cg->output, "; enum %.*s — %d variants\n",
                (int)n->as.enum_decl.name_len, n->as.enum_decl.name, count);

            /* Collect variant names for string data */
            for (int i = 0; i < count; i++) {
                int vlen = (int)n->as.enum_decl.variants[i].name_len;
                fprintf(cg->output, "  _enum_%s_name_%d: db ", enum_name, i);
                for (int c = 0; c < vlen; c++)
                    fprintf(cg->output, "0x%02X, ", (unsigned char)n->as.enum_decl.variants[i].name[c]);
                fprintf(cg->output, "0\n");
            }

            /* Name pointer table */
            fprintf(cg->output, "  _enum_%s_names: dq ", enum_name);
            for (int i = 0; i < count; i++)
                fprintf(cg->output, "_enum_%s_name_%d, ", enum_name, i);
            fprintf(cg->output, "0\n");

            /* tag(self) -> tag value */
            fprintf(cg->output, "global %s_tag\n", enum_name);
            fprintf(cg->output, "%s_tag:\n", enum_name);
            emit(cg, "mov rax, rdi");
            emit(cg, "ret");

            /* name(self) -> string name */
            fprintf(cg->output, "global %s_name\n", enum_name);
            fprintf(cg->output, "%s_name:\n", enum_name);
            emit(cg, "lea rax, [_enum_%s_names]", enum_name);
            emit(cg, "mov rax, [rax + rdi*8]");
            emit(cg, "ret");

            /* count() -> variant count */
            fprintf(cg->output, "global %s_count\n", enum_name);
            fprintf(cg->output, "%s_count:\n", enum_name);
            emit(cg, "mov rax, %d", count);
            emit(cg, "ret");

            /* Register enum functions as extern for linker */
            char fn_name[MAX_IDENT_LEN];
            snprintf(fn_name, sizeof(fn_name), "%s_tag", enum_name);
            add_extern(cg, fn_name);
            snprintf(fn_name, sizeof(fn_name), "%s_name", enum_name);
            add_extern(cg, fn_name);
            snprintf(fn_name, sizeof(fn_name), "%s_count", enum_name);
            add_extern(cg, fn_name);

            /* Register enum variants as constants in symbol table */
            for (int i = 0; i < count; i++) {
                /* Add to extern_names so they're callable */
                char variant_name[MAX_IDENT_LEN];
                snprintf(variant_name, sizeof(variant_name), "%.*s",
                    (int)n->as.enum_decl.variants[i].name_len,
                    n->as.enum_decl.variants[i].name);
                add_extern(cg, variant_name);
            }
            break;
        }
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
        case AST_IMPORT_DECL: {
            /* register module in table for call resolution */
            char ns[256];
            if (n->as.import.alias) {
                snprintf(ns, sizeof(ns), "%.*s", (int)n->as.import.alias_len, n->as.import.alias);
            } else {
                /* use last path component */
                const char *path = n->as.import.path;
                size_t len = n->as.import.path_len;
                const char *slash = memrchr(path, '/', len);
                if (slash)
                    snprintf(ns, sizeof(ns), "%.*s", (int)(len - (slash - path + 1)), slash + 1);
                else
                    snprintf(ns, sizeof(ns), "%.*s", (int)len, path);
            }
            register_module(cg, ns, strlen(ns), n->as.import.is_stdlib);
            fprintf(cg->output, "; import %s \"%.*s\"", n->as.import.is_stdlib ? "lib" : "mod",
                (int)n->as.import.path_len, n->as.import.path);
            if (n->as.import.alias)
                fprintf(cg->output, " as %.*s", (int)n->as.import.alias_len, n->as.import.alias);
            fprintf(cg->output, "\n");
            break; }
        case AST_TRY_EXPR:
        case AST_CATCH_EXPR:
            gen_expr(cg, n);
            break;
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
    if (!n || cg->has_error) return;
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
        cg->strings = SAFE_REALLOC(cg->strings, sizeof(string_entry_t) * cg->string_entries);
        cg->strings[cg->string_entries - 1].label = lbl;
        cg->strings[cg->string_entries - 1].value = SAFE_STRNDUP(n->as.string_val.value, n->as.string_val.length);
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
        case AST_FN_DECL: collect_strings(cg, n->as.fn_decl.body);
            for (int i = 0; i < n->as.fn_decl.param_count; i++)
                collect_strings(cg, n->as.fn_decl.params[i].default_value);
            break;
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
        case AST_IMPL_DECL:
            for (int mi = 0; mi < n->as.impl_decl.method_count; mi++)
                collect_strings(cg, n->as.impl_decl.methods[mi]);
            break;
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
    cg->has_error = 0; cg->error_count = 0;
    cg->var_types = NULL; cg->var_type_count = 0; cg->var_type_cap = 0;
    cg->closure_bodies = NULL; cg->closure_body_count = 0; cg->closure_body_cap = 0;
    cg->modules = NULL; cg->module_count = 0; cg->module_cap = 0;
    cg->float_entries = NULL; cg->float_entries_count = 0;
}

void codegen_free(codegen_t *cg) {
    for (int i = 0; i < cg->symbol_count; i++) free(cg->symbols[i].name);
    free(cg->symbols);
    for (int i = 0; i < cg->string_entries; i++) free(cg->strings[i].value);
    free(cg->strings);
    free(cg->defers);
    for (int i = 0; i < cg->extern_count; i++) free(cg->extern_names[i]);
    free(cg->extern_names);
    for (int i = 0; i < cg->var_type_count; i++) {
        free(cg->var_types[i].name);
        free(cg->var_types[i].struct_name);
    }
    free(cg->var_types);
    for (int i = 0; i < cg->module_count; i++) free(cg->modules[i].name);
    free(cg->modules);
    free(cg->float_entries);
}

static void add_extern(codegen_t *cg, const char *name) {
    cg->extern_count++;
    cg->extern_names = SAFE_REALLOC(cg->extern_names, sizeof(char*) * cg->extern_count);
    cg->extern_names[cg->extern_count - 1] = SAFE_STRDUP(name);
}

/* Register a module: name=is_stdlib */
static void register_module(codegen_t *cg, const char *name, size_t name_len, int is_stdlib) {
    if (cg->module_count >= cg->module_cap) {
        cg->module_cap = cg->module_cap ? cg->module_cap * 2 : 8;
        cg->modules = SAFE_REALLOC(cg->modules, sizeof(*cg->modules) * cg->module_cap);
    }
    cg->modules[cg->module_count].name = SAFE_STRNDUP(name, name_len);
    cg->modules[cg->module_count].is_stdlib = is_stdlib;
    cg->module_count++;
}

/* Check if a module name is a stdlib (not project-merged) */
static int is_stdlib_module(codegen_t *cg, const char *name, size_t name_len) {
    for (int i = 0; i < cg->module_count; i++)
        if (cg->modules[i].is_stdlib &&
            strlen(cg->modules[i].name) == name_len &&
            memcmp(cg->modules[i].name, name, name_len) == 0)
            return 1;
    return 0;
}

int codegen_program(codegen_t *cg, ast_node_t *prog) {
    cg->prog = prog;
    collect_strings(cg, prog);
    collect_floats(cg, prog);
    fprintf(cg->output, "; Generated by ELang compiler v%s\n", ELANG_VERSION);

    /* Collect all function calls for extern declarations */
    /* This is done during codegen, so we emit externs after gen_node */

    if (cg->string_entries > 0 || cg->source_file || cg->float_entries_count > 0) {
        emit_raw(cg, "section .data");
        for (int i = 0; i < cg->string_entries; i++)
            emit_string_data(cg, cg->strings[i].label, cg->strings[i].value, cg->strings[i].length);
        /* Emit float constants */
        for (int i = 0; i < cg->float_entries_count; i++) {
            uint64_t bits;
            memcpy(&bits, &cg->float_entries[i].value, sizeof(bits));
            fprintf(cg->output, "  fconst_%d: dq 0x%016llx\n",
                cg->float_entries[i].label, (unsigned long long)bits);
        }
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
        /* main returns u8 — use directly as exit code */
        emit_raw(cg, "    mov rdi, rax");
        emit_raw(cg, "    mov rax, 60");
        emit_raw(cg, "    syscall");
    }
    return cg->has_error ? 1 : 0;
}
