/* ELang Codegen Helpers — utility functions shared across codegen modules */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "codegen_internal.h"

/* --- Capture detection --- */

void idlist_add(ident_list_t *l, const char *name, size_t len) {
    for (int i = 0; i < l->count; i++)
        if (l->name_lens[i] == len && memcmp(l->names[i], name, len) == 0) return;
    if (l->count >= l->cap) { l->cap = l->cap ? l->cap * 2 : 8;
        l->names = SAFE_REALLOC(l->names, sizeof(char*) * l->cap);
        l->name_lens = SAFE_REALLOC(l->name_lens, sizeof(size_t) * l->cap); }
    l->names[l->count] = SAFE_STRNDUP(name, len);
    l->name_lens[l->count] = len;
    l->count++;
}

void collect_idents(ast_node_t *n, ident_list_t *out) {
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
        case AST_LOOP: collect_idents(n->as.loop_stmt.body, out); break;
        default: break;
    }
}

/* --- Codegen error --- */

void codegen_error(codegen_t *cg, const char *fmt, ...) {
    fprintf(stderr, "codegen error: ");
    va_list a; va_start(a, fmt); vfprintf(stderr, fmt, a); va_end(a);
    fprintf(stderr, "\n");
    cg->has_error = 1;
    cg->error_count++;
}

void buf_check(codegen_t *cg, int written, size_t bufsize, const char *context) {
    if (written < 0 || (size_t)written >= bufsize) {
        codegen_error(cg, "identifier too long (%s), max %zu chars", context, bufsize - 1);
    }
}

/* --- Emit helpers --- */

void emit(codegen_t *cg, const char *fmt, ...) {
    fprintf(cg->output, "    "); va_list a; va_start(a, fmt); vfprintf(cg->output, fmt, a); va_end(a);
    fprintf(cg->output, "\n");
}

void emit_raw(codegen_t *cg, const char *s) { fprintf(cg->output, "%s\n", s); }

int new_label(codegen_t *cg) { return cg->label_count++; }

/* --- Extern / name helpers --- */

void add_extern(codegen_t *cg, const char *name) {
    for (int i = 0; i < cg->extern_count; i++)
        if (strcmp(cg->extern_names[i], name) == 0) return;
    cg->extern_count++;
    cg->extern_names = SAFE_REALLOC(cg->extern_names, sizeof(char*) * cg->extern_count);
    cg->extern_names[cg->extern_count - 1] = SAFE_STRDUP(name);
}

int is_extern(codegen_t *cg, const char *name, size_t len) {
    for (int i = 0; i < cg->extern_count; i++)
        if (strlen(cg->extern_names[i]) == len && memcmp(cg->extern_names[i], name, len) == 0) return 1;
    return 0;
}

void emit_name(codegen_t *cg, const char *name, size_t len) {
    if (!is_extern(cg, name, len)) fprintf(cg->output, "_");
    fprintf(cg->output, "%.*s", (int)len, name);
}

int is_user_defined(codegen_t *cg, const char *name, size_t len) {
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

int is_nasm_keyword(const char *name, size_t len) {
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

/* --- Symbol table --- */

int find_sym(codegen_t *cg, const char *name) {
    for (int i = cg->symbol_count - 1; i >= 0; i--)
        if (strcmp(cg->symbols[i].name, name) == 0) return cg->symbols[i].stack_offset;
    return -1;
}

int find_sym_is_float(codegen_t *cg, const char *name, int *is_float) {
    for (int i = cg->symbol_count - 1; i >= 0; i--) {
        if (strcmp(cg->symbols[i].name, name) == 0) {
            *is_float = cg->symbols[i].is_float;
            return cg->symbols[i].stack_offset;
        }
    }
    *is_float = 0;
    return -1;
}

int add_sym_float(codegen_t *cg, const char *name, int size, int is_float) {
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

int add_sym(codegen_t *cg, const char *name, int size) {
    return add_sym_float(cg, name, size, 0);
}

/* --- Scope --- */

scope_mark_t scope_enter(codegen_t *cg) {
    scope_mark_t m = { cg->symbol_count, cg->stack_size };
    return m;
}

void scope_exit(codegen_t *cg, scope_mark_t m) {
    for (int i = cg->symbol_count - 1; i >= m.symbol_count; i--)
        free(cg->symbols[i].name);
    cg->symbol_count = m.symbol_count;
    cg->stack_size = m.stack_size;
}

/* --- String table --- */

int find_string_label(codegen_t *cg, const char *value, size_t length) {
    for (int i = 0; i < cg->string_entries; i++) {
        if (cg->strings[i].length == length && memcmp(cg->strings[i].value, value, length) == 0) {
            return cg->strings[i].label;
        }
    }
    return -1;
}

void emit_string_data(codegen_t *cg, int label, const char *value, size_t length) {
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

void collect_strings(codegen_t *cg, ast_node_t *n) {
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
        case AST_LOOP: collect_strings(cg, n->as.loop_stmt.body); break;
        case AST_PROGRAM: for (int i = 0; i < n->as.program.count; i++) collect_strings(cg, n->as.program.declarations[i]); break;
        default: break;
    }
}

/* --- Float helpers --- */

int is_float_type(codegen_t *cg, ast_node_t *n) {
    if (!n) return 0;
    if (n->type == AST_FLOAT_LIT) return 1;
    if (n->type == AST_IDENT) {
        for (int vi = 0; vi < cg->var_type_count; vi++) {
            if (strlen(cg->var_types[vi].name) == n->as.ident.name_len &&
                memcmp(cg->var_types[vi].name, n->as.ident.name, n->as.ident.name_len) == 0) {
                const char *sn = cg->var_types[vi].struct_name;
                if (sn && (strcmp(sn, "f32") == 0 || strcmp(sn, "f64") == 0))
                    return 1;
            }
        }
    }
    return 0;
}

int add_float_const(codegen_t *cg, double val) {
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

void collect_floats(codegen_t *cg, ast_node_t *n) {
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
        case AST_LOOP: collect_floats(cg, n->as.loop_stmt.body); break;
        case AST_CALL: collect_floats(cg, n->as.call.callee); for (int i = 0; i < n->as.call.arg_count; i++) collect_floats(cg, n->as.call.args[i]); break;
        case AST_PIPE: collect_floats(cg, n->as.pipe.left); collect_floats(cg, n->as.pipe.right); break;
        case AST_MATCH: collect_floats(cg, n->as.match_expr.value); break;
        case AST_PROGRAM: for (int i = 0; i < n->as.program.count; i++) collect_floats(cg, n->as.program.declarations[i]); break;
        default: break;
    }
}

/* --- Defer --- */

void push_defer(codegen_t *cg, ast_node_t *expr) {
    if (cg->defer_count >= cg->defer_cap) {
        cg->defer_cap = cg->defer_cap ? cg->defer_cap * 2 : 8;
        cg->defers = SAFE_REALLOC(cg->defers, sizeof(*cg->defers) * cg->defer_cap);
    }
    cg->defers[cg->defer_count].expr = expr;
    cg->defers[cg->defer_count].scope_depth = cg->scope_depth;
    cg->defer_count++;
}

void emit_defers(codegen_t *cg, int from_depth) {
    for (int i = cg->defer_count - 1; i >= 0; i--) {
        if (cg->defers[i].scope_depth >= from_depth) {
            gen_expr(cg, cg->defers[i].expr);
            memmove(&cg->defers[i], &cg->defers[i + 1], sizeof(*cg->defers) * (cg->defer_count - i - 1));
            cg->defer_count--;
        }
    }
}

/* --- Module helpers --- */

void register_module(codegen_t *cg, const char *name, size_t name_len, int is_stdlib) {
    if (cg->module_count >= cg->module_cap) {
        cg->module_cap = cg->module_cap ? cg->module_cap * 2 : 8;
        cg->modules = SAFE_REALLOC(cg->modules, sizeof(*cg->modules) * cg->module_cap);
    }
    cg->modules[cg->module_count].name = SAFE_STRNDUP(name, name_len);
    cg->modules[cg->module_count].is_stdlib = is_stdlib;
    cg->module_count++;
}

int is_stdlib_module(codegen_t *cg, const char *name, size_t name_len) {
    for (int i = 0; i < cg->module_count; i++)
        if (cg->modules[i].is_stdlib &&
            strlen(cg->modules[i].name) == name_len &&
            memcmp(cg->modules[i].name, name, name_len) == 0)
            return 1;
    return 0;
}
