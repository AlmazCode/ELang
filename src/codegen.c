/* ELang Code Generator - x86_64 NASM */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "codegen.h"

static void emit(codegen_t *cg, const char *fmt, ...) {
    fprintf(cg->output, "    "); va_list a; va_start(a, fmt); vfprintf(cg->output, fmt, a); va_end(a);
    fprintf(cg->output, "\n");
}
static void emit_raw(codegen_t *cg, const char *s) { fprintf(cg->output, "%s\n", s); }
static int new_label(codegen_t *cg) { return cg->label_count++; }

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
    int off = cg->stack_size;
    cg->symbol_count++;
    cg->symbols = realloc(cg->symbols, sizeof(*cg->symbols) * cg->symbol_count);
    cg->symbols[cg->symbol_count - 1].name = strdup(name);
    cg->symbols[cg->symbol_count - 1].stack_offset = off;
    cg->symbols[cg->symbol_count - 1].size = size;
    return off;
}

static const char *find_string_label(codegen_t *cg, const char *value, size_t length) {
    for (int i = 0; i < cg->string_entries; i++) {
        if (cg->strings[i].length == length && memcmp(cg->strings[i].value, value, length) == 0) {
            static char buf[32];
            snprintf(buf, sizeof(buf), "str_%d", cg->strings[i].label);
            return buf;
        }
    }
    return NULL;
}

static void emit_string_data(codegen_t *cg, int label, const char *value, size_t length) {
    fprintf(cg->output, "  str_%d: db ", label);
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
    fprintf(cg->output, ", 0\n");
}

/* --- Forward declarations --- */
static void gen_expr(codegen_t *cg, ast_node_t *n);
static void gen_stmt(codegen_t *cg, ast_node_t *n);
static void gen_node(codegen_t *cg, ast_node_t *n);

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
            if (off < 0) { fprintf(stderr, "codegen: undefined '%.*s'\n", (int)n->as.ident.name_len, n->as.ident.name); return; }
            emit(cg, "mov rax, [rbp-%d]", off); break; }
        case AST_BINARY_OP:
            gen_expr(cg, n->as.binary.left); emit(cg, "push rax");
            gen_expr(cg, n->as.binary.right); emit(cg, "mov rbx, rax"); emit(cg, "pop rax");
            switch (n->as.binary.op) {
                case TOKEN_PLUS: emit(cg, "add rax, rbx"); break;
                case TOKEN_MINUS: emit(cg, "sub rax, rbx"); break;
                case TOKEN_STAR: emit(cg, "imul rax, rbx"); break;
                case TOKEN_SLASH: emit(cg, "cqo"); emit(cg, "idiv rbx"); break;
                case TOKEN_EQ: emit(cg, "cmp rax, rbx"); emit(cg, "sete al"); emit(cg, "movzx rax, al"); break;
                case TOKEN_NEQ: emit(cg, "cmp rax, rbx"); emit(cg, "setne al"); emit(cg, "movzx rax, al"); break;
                case TOKEN_LT: emit(cg, "cmp rax, rbx"); emit(cg, "setl al"); emit(cg, "movzx rax, al"); break;
                case TOKEN_GT: emit(cg, "cmp rax, rbx"); emit(cg, "setg al"); emit(cg, "movzx rax, al"); break;
                case TOKEN_LTE: emit(cg, "cmp rax, rbx"); emit(cg, "setle al"); emit(cg, "movzx rax, al"); break;
                case TOKEN_GTE: emit(cg, "cmp rax, rbx"); emit(cg, "setge al"); emit(cg, "movzx rax, al"); break;
                case TOKEN_AND: emit(cg, "and rax, rbx"); break;
                case TOKEN_OR: emit(cg, "or rax, rbx"); break;
                case TOKEN_PERCENT: emit(cg, "xor rdx, rdx"); emit(cg, "div rbx"); emit(cg, "mov rax, rdx"); break;
                default: break;
            } break;
        case AST_CALL: {
            const char *regs[] = {"rdi","rsi","rdx","rcx","r8","r9"};
            int narg = n->as.call.arg_count;
            for (int i = (narg < 6 ? narg - 1 : 5); i >= 0; i--) {
                gen_expr(cg, n->as.call.args[i]); emit(cg, "mov %s, rax", regs[i]); }
            /* handle module::func() calls — emit as _module_func */
            if (n->as.call.callee->type == AST_BINARY_OP &&
                n->as.call.callee->as.binary.op == TOKEN_COLONCOLON) {
                ast_node_t *mod = n->as.call.callee->as.binary.left;
                ast_node_t *fn = n->as.call.callee->as.binary.right;
                fprintf(cg->output, "    call _%.*s_%.*s\n",
                    (int)mod->as.ident.name_len, mod->as.ident.name,
                    (int)fn->as.ident.name_len, fn->as.ident.name);
            } else {
                fprintf(cg->output, "    call ");
                emit_name(cg, n->as.call.callee->as.ident.name, n->as.call.callee->as.ident.name_len);
                fprintf(cg->output, "\n");
            }
            break; }
        case AST_PIPE: {
            /* x |> f(a)  =>  f(a, x) — left goes as last arg */
            ast_node_t *callee = n->as.pipe.right;
            if (callee->type == AST_CALL) {
                /* push left value, then eval args, then pop left into last reg */
                gen_expr(cg, n->as.pipe.left); emit(cg, "push rax");
                const char *regs[] = {"rdi","rsi","rdx","rcx","r8","r9"};
                int narg = callee->as.call.arg_count;
                for (int i = (narg < 6 ? narg - 1 : 5); i >= 0; i--) {
                    gen_expr(cg, callee->as.call.args[i]); emit(cg, "mov %s, rax", regs[i]); }
                /* now pop left value into the next arg register */
                int total = narg + 1;
                if (total <= 6) { emit(cg, "pop rax"); emit(cg, "mov %s, rax", regs[narg]); }
                else { emit(cg, "pop rax"); /* stack-based for >6 args */ }
                /* handle module::func in pipeline */
                if (callee->as.call.callee->type == AST_BINARY_OP &&
                    callee->as.call.callee->as.binary.op == TOKEN_COLONCOLON) {
                    ast_node_t *mod = callee->as.call.callee->as.binary.left;
                    ast_node_t *fn = callee->as.call.callee->as.binary.right;
                    fprintf(cg->output, "    call _%.*s_%.*s\n",
                        (int)mod->as.ident.name_len, mod->as.ident.name,
                        (int)fn->as.ident.name_len, fn->as.ident.name);
                } else {
                    fprintf(cg->output, "    call ");
                    emit_name(cg, callee->as.call.callee->as.ident.name, callee->as.call.callee->as.ident.name_len);
                    fprintf(cg->output, "\n");
                }
            } else {
                /* x |> f  =>  f(x) */
                gen_expr(cg, n->as.pipe.left); emit(cg, "mov rdi, rax");
                gen_expr(cg, callee);
                fprintf(cg->output, "    call "); emit_name(cg, callee->as.ident.name, callee->as.ident.name_len);
                fprintf(cg->output, "\n");
            }
            break; }
        case AST_OK_EXPR: gen_expr(cg, n->as.ok_expr.value); emit(cg, "shl rax, 1"); break;
        case AST_ERR_EXPR: gen_expr(cg, n->as.err_expr.value); emit(cg, "shl rax, 1"); emit(cg, "or rax, 1"); break;
        case AST_MATCH: gen_stmt(cg, n); break;
        case AST_WHEN: { int sr = cg->returned; gen_stmt(cg, n); cg->returned = sr; break; }
        case AST_IF: { int sr = cg->returned; gen_stmt(cg, n); cg->returned = sr; break; }
        case AST_TUPLE: {
            /* evaluate elements, push them right-to-left, result is a "tuple" on stack */
            for (int i = n->as.tuple.count - 1; i >= 0; i--) {
                gen_expr(cg, n->as.tuple.elements[i]);
                emit(cg, "push rax");
            }
            /* rax = stack pointer to first element (for now, just leave values on stack) */
            break; }
        case AST_TRY_EXPR: {
            /* expr? — check if result is Err (bit 0 == 1), if so propagate (return) */
            gen_expr(cg, n->as.try_expr.operand);
            emit(cg, "mov rbx, rax");
            emit(cg, "and rbx, 1");
            emit(cg, "cmp rbx, 1");
            int ok_label = new_label(cg);
            emit(cg, "jne L%d", ok_label);
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
            emit(cg, "mov rbx, rax");
            emit(cg, "and rbx, 1");
            emit(cg, "cmp rbx, 1");
            emit(cg, "je L%d", err_label);
            /* Ok case: extract value */
            emit(cg, "shr rax, 1");
            emit(cg, "jmp L%d", end_label);
            /* Err case: run handler */
            fprintf(cg->output, "L%d:\n", err_label);
            emit(cg, "shr rax, 1");
            int err_off = add_sym(cg, "__catch_err", 8);
            emit(cg, "mov [rbp-%d], rax", err_off);
            int saved_scope = cg->scope_depth;
            cg->scope_depth++;
            gen_node(cg, n->as.catch_expr.handler);
            if (!cg->returned) emit_defers(cg, cg->scope_depth);
            cg->scope_depth = saved_scope;
            fprintf(cg->output, "L%d:\n", end_label);
            break; }
        default: break;
    }
}

static void gen_stmt(codegen_t *cg, ast_node_t *n) {
    if (!n) return;
    if (n->type == AST_FN_DECL) { cg->returned = 0; cg->scope_depth = 0; cg->defer_count = 0; }
    if (cg->returned) return;
    switch (n->type) {
        case AST_LET: {
            if (n->as.let.value && n->as.let.value->type == AST_TUPLE_ASSIGN) {
                /* tuple unpacking: let (a, b) = expr */
                ast_node_t *ta = n->as.let.value;
                gen_expr(cg, ta->as.tuple_assign.value);
                /* elements are on stack, pop into variables */
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
        case AST_IF: {
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
            break; }
        case AST_WHEN: {
            /* when is like if but always leaves a value in rax */
            int el = new_label(cg), end = new_label(cg);
            gen_expr(cg, n->as.when_expr.condition);
            emit(cg, "cmp rax, 0"); emit(cg, "je L%d", el);
            cg->returned = 0;
            gen_node(cg, n->as.when_expr.then_block);
            emit(cg, "jmp L%d", end);
            fprintf(cg->output, "L%d:\n", el);
            if (n->as.when_expr.else_block) {
                cg->returned = 0;
                gen_node(cg, n->as.when_expr.else_block);
            }
            fprintf(cg->output, "L%d:\n", end);
            break; }
        case AST_WHILE: {
            int loop = new_label(cg), end = new_label(cg);
            fprintf(cg->output, "L%d:\n", loop);
            gen_expr(cg, n->as.while_stmt.condition);
            emit(cg, "cmp rax, 0"); emit(cg, "je L%d", end);
            cg->returned = 0;
            gen_node(cg, n->as.while_stmt.body);
            cg->returned = 0;
            emit(cg, "jmp L%d", loop);
            fprintf(cg->output, "L%d:\n", end); break; }
        case AST_FOR: {
            int loop = new_label(cg), end = new_label(cg), inc = new_label(cg);
            /* evaluate range bounds and store both on stack */
            gen_expr(cg, n->as.for_stmt.iterable->as.range.left);
            emit(cg, "push rax"); /* push start */
            gen_expr(cg, n->as.for_stmt.iterable->as.range.right);
            emit(cg, "push rax"); /* push end */
            int end_off = add_sym(cg, "__for_end", 8);
            emit(cg, "pop rax"); emit(cg, "mov [rbp-%d], rax", end_off); /* end → stack */
            int iter_off = add_sym(cg, n->as.for_stmt.var, 8);
            emit(cg, "pop rax"); emit(cg, "mov [rbp-%d], rax", iter_off); /* start → iter */
            fprintf(cg->output, "L%d:\n", loop);
            emit(cg, "mov rax, [rbp-%d]", iter_off);
            emit(cg, "mov rbx, [rbp-%d]", end_off); /* reload end from stack each iteration */
            emit(cg, "cmp rax, rbx");
            emit(cg, "jge L%d", end);
            cg->returned = 0;
            gen_node(cg, n->as.for_stmt.body);
            fprintf(cg->output, "L%d:\n", inc);
            emit(cg, "mov rax, [rbp-%d]", iter_off);
            emit(cg, "inc rax");
            emit(cg, "mov [rbp-%d], rax", iter_off);
            emit(cg, "jmp L%d", loop);
            fprintf(cg->output, "L%d:\n", end); break; }
        case AST_BLOCK: {
            int saved_depth = cg->scope_depth;
            cg->scope_depth++;
            for (int i = 0; i < n->as.block.count; i++) gen_node(cg, n->as.block.stmts[i]);
            /* emit defers added in this scope */
            if (!cg->returned) emit_defers(cg, cg->scope_depth);
            cg->scope_depth = saved_depth;
            break; }
        case AST_FN_DECL: {
            fprintf(cg->output, "\nglobal "); emit_name(cg, n->as.fn_decl.name, n->as.fn_decl.name_len);
            fprintf(cg->output, "\n"); emit_name(cg, n->as.fn_decl.name, n->as.fn_decl.name_len);
            fprintf(cg->output, ":\n");
            emit(cg, "push rbp"); emit(cg, "mov rbp, rsp");
            emit(cg, "push rbx"); emit(cg, "push r12"); emit(cg, "push r13"); emit(cg, "push r14"); emit(cg, "push r15");
            cg->stack_size = 40; /* 5 saved registers * 8 bytes */
            int saved = cg->stack_size;
            /* store params into stack frame */
            const char *aregs[] = {"rdi","rsi","rdx","rcx","r8","r9"};
            for (int i = 0; i < n->as.fn_decl.param_count && i < 6; i++) {
                int off = add_sym(cg, n->as.fn_decl.params[i].name, 8);
                emit(cg, "mov [rbp-%d], %s", off, aregs[i]); }
            cg->returned = 0;
            cg->scope_depth = 0;
            int saved_defer_count = cg->defer_count;
            /* allocate stack space for locals AFTER storing params */
            emit(cg, "sub rsp, 256");
            gen_node(cg, n->as.fn_decl.body);
            if (!cg->returned) {
                emit(cg, "xor rax, rax");
            }
            emit(cg, "add rsp, 256");
            emit(cg, "pop r15"); emit(cg, "pop r14"); emit(cg, "pop r13"); emit(cg, "pop r12"); emit(cg, "pop rbx");
            emit(cg, "mov rsp, rbp"); emit(cg, "pop rbp"); emit(cg, "ret");
            cg->stack_size = saved;
            cg->defer_count = saved_defer_count;
            break; }
        case AST_STRUCT_DECL: {
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
            break; }
        case AST_ENUM_DECL:
            fprintf(cg->output, "; enum %.*s\n", (int)n->as.enum_decl.name_len, n->as.enum_decl.name); break;
        case AST_MATCH: {
            int end_lbl = new_label(cg), ret_lbl = new_label(cg);
            int saved_stack = cg->stack_size;
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
            cg->stack_size = saved_stack;
            break; }
        case AST_USING:
            fprintf(cg->output, "; using \"%.*s\"\n", (int)n->as.using_decl.path_len, n->as.using_decl.path); break;
        case AST_IMPORT_DECL: fprintf(cg->output, "; import \"%.*s\"\n", (int)n->as.import.path_len, n->as.import.path); break;
        case AST_TRY_EXPR: {
            /* expr? — check if result is Err (bit 0 == 1), if so propagate (return) */
            gen_expr(cg, n->as.try_expr.operand);
            emit(cg, "mov rbx, rax");
            emit(cg, "and rbx, 1");
            emit(cg, "cmp rbx, 1");
            int ok_label = new_label(cg);
            emit(cg, "jne L%d", ok_label);
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
            emit(cg, "mov rbx, rax");
            emit(cg, "and rbx, 1");
            emit(cg, "cmp rbx, 1");
            emit(cg, "je L%d", err_label);
            /* Ok case: extract value */
            emit(cg, "shr rax, 1");
            emit(cg, "jmp L%d", end_label);
            /* Err case: run handler */
            fprintf(cg->output, "L%d:\n", err_label);
            emit(cg, "shr rax, 1");
            /* store error value in a temp variable for the handler */
            int err_off = add_sym(cg, "__catch_err", 8);
            emit(cg, "mov [rbp-%d], rax", err_off);
            int saved_scope = cg->scope_depth;
            cg->scope_depth++;
            gen_node(cg, n->as.catch_expr.handler);
            if (!cg->returned) emit_defers(cg, cg->scope_depth);
            cg->scope_depth = saved_scope;
            fprintf(cg->output, "L%d:\n", end_label);
            break; }
        case AST_PANIC_EXPR: {
            /* panic(msg) — call panic_handler */
            if (n->as.panic_expr.message) {
                gen_expr(cg, n->as.panic_expr.message);
                emit(cg, "mov rdi, rax");
            }
            fprintf(cg->output, "    call ");
            emit_name(cg, "panic_handler", 13);
            fprintf(cg->output, "\n");
            cg->returned = 1;
            break; }
        case AST_ASSERT_EXPR: {
            /* assert(cond, msg) — call assert_handler */
            gen_expr(cg, n->as.assert_expr.condition);
            emit(cg, "mov rdi, rax");
            if (n->as.assert_expr.message) {
                gen_expr(cg, n->as.assert_expr.message);
                emit(cg, "mov rsi, rax");
            }
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
        case AST_CALL: collect_strings(cg, n->as.call.callee);
            for (int i = 0; i < n->as.call.arg_count; i++) collect_strings(cg, n->as.call.args[i]); break;
        case AST_IF: collect_strings(cg, n->as.if_stmt.condition); collect_strings(cg, n->as.if_stmt.then_block);
            collect_strings(cg, n->as.if_stmt.else_block); break;
        case AST_WHEN: collect_strings(cg, n->as.when_expr.condition); collect_strings(cg, n->as.when_expr.then_block);
            collect_strings(cg, n->as.when_expr.else_block); break;
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
        case AST_TUPLE_ASSIGN: collect_strings(cg, n->as.tuple_assign.value); break;
        case AST_PROGRAM: for (int i = 0; i < n->as.program.count; i++) collect_strings(cg, n->as.program.declarations[i]); break;
        default: break;
    }
}

void codegen_init(codegen_t *cg, FILE *output) {
    cg->output = output; cg->label_count = 0; cg->string_count = 0;
    cg->symbols = NULL; cg->symbol_count = 0; cg->stack_size = 0;
    cg->strings = NULL; cg->string_entries = 0; cg->is_main = 1; cg->returned = 0;
    cg->defers = NULL; cg->defer_count = 0; cg->defer_cap = 0; cg->scope_depth = 0;
    cg->extern_names = NULL; cg->extern_count = 0; cg->in_return_expr = 0;
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
    collect_strings(cg, prog);
    emit_raw(cg, "; Generated by ELang compiler v0.2.0");
    const char *externs[] = {
        "print", "print_str", "print_int", "print_hex",
        "read_input", "exit",
        "str_len", "str_dup", "str_cmp", "str_cat",
        "sys_open", "sys_close", "sys_read", "sys_write",
        "sys_getpid", "sys_exit", "sys_brk",
        "panic_handler", "assert_handler"
    };
    for (int i = 0; i < (int)(sizeof(externs)/sizeof(externs[0])); i++) {
        fprintf(cg->output, "extern %s\n", externs[i]);
        add_extern(cg, externs[i]);
    }
    if (cg->string_entries > 0) {
        emit_raw(cg, "section .data");
        for (int i = 0; i < cg->string_entries; i++)
            emit_string_data(cg, cg->strings[i].label, cg->strings[i].value, cg->strings[i].length);
    }
    emit_raw(cg, "section .text"); emit_raw(cg, "global _start"); emit_raw(cg, "");
    gen_node(cg, prog);
    emit_raw(cg, ""); emit_raw(cg, "_start:");
    emit_raw(cg, "    call _main"); emit(cg, "mov rdi, rax");
    emit(cg, "mov rax, 60"); emit(cg, "syscall");
    return 0;
}
