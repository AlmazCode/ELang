/* ELang Code Generator — main orchestrator (gen_expr, gen_stmt, gen_node, codegen_program) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "codegen_internal.h"

/* Check if an expression has an unsigned integer type */
static int expr_is_unsigned(codegen_t *cg, ast_node_t *n) {
    (void)cg;
    if (n && n->typed) {
        return n->typed->kind >= TYPE_U8 && n->typed->kind <= TYPE_U64;
    }
    return 0;
}

/* Get element size in bytes for a type */
static int type_elem_size(type_info_t *t) {
    if (!t) return 8;
    switch (t->kind) {
        case TYPE_I8: case TYPE_U8: case TYPE_BOOL: case TYPE_CHAR: return 1;
        case TYPE_I16: case TYPE_U16: return 2;
        case TYPE_I32: case TYPE_U32: case TYPE_F32: return 4;
        default: return 8;
    }
}

/* ============================================================
 * gen_print_call — compile-time print dispatch
 * ============================================================ */

static void gen_print_call(codegen_t *cg, ast_node_t *n) {
    ast_node_t *print_args[64];
    int print_argc = 0;
    const char *sep_str = NULL;
    size_t sep_len = 0;
    const char *end_str = NULL;
    size_t end_len = 0;

    for (int i = 0; i < n->as.call.arg_count; i++) {
        ast_node_t *arg = n->as.call.args[i];
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

    for (int i = 0; i < print_argc; i++) {
        ast_node_t *arg = print_args[i];
        if (!arg) continue;

        const char *fn = NULL;
        switch (arg->type) {
            case AST_INT_LIT: fn = "print_i64"; break;
            case AST_FLOAT_LIT: fn = "print_f64"; break;
            case AST_STRING_LIT: fn = "print_str"; break;
            case AST_BOOL_LIT: fn = "print_bool"; break;
            case AST_BINARY_OP:
                if (arg->as.binary.op == TOKEN_MINUS &&
                    arg->as.binary.left->type == AST_INT_LIT &&
                    arg->as.binary.left->as.int_val == 0) {
                    fn = "print_i64";
                } else if (arg->as.binary.op == TOKEN_COLONCOLON) {
                    fn = "print_ptr";
                } else {
                    fn = "print_i64";
                }
                break;
            case AST_IDENT: {
                int found_type = 0;
                for (int vi = 0; vi < cg->var_type_count; vi++) {
                    if (strlen(cg->var_types[vi].name) == arg->as.ident.name_len &&
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
                    if (is_user_defined(cg, arg->as.ident.name, arg->as.ident.name_len))
                        fn = "print_ptr";
                    else
                        fn = "print_i64";
                }
                break;
            }
            case AST_CALL: fn = "print_i64"; break;
            case AST_ARRAY_LITERAL:
                if (arg->as.array_literal.count > 0) {
                    ast_node_t *first = arg->as.array_literal.elements[0];
                    if (first->type == AST_STRING_LIT) fn = "print_arr_str";
                    else if (first->type == AST_BOOL_LIT) fn = "print_arr_bool";
                    else if (first->type == AST_FLOAT_LIT) fn = "print_arr_f64";
                    else fn = "print_arr_i64";
                } else {
                    fn = "print_arr_i64";
                }
                break;
            case AST_OK_EXPR:
            case AST_ERR_EXPR:
                fn = "print_i64";
                break;
            case AST_TUPLE: {
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
                fn = NULL;
                break;
            }
            default:
                fn = "print_i64";
                break;
        }

        if (fn == NULL) continue;
        gen_expr(cg, arg);
        if (strcmp(fn, "print_f64") == 0) {
            emit(cg, "movq rax, xmm0");
        }
        emit(cg, "mov rdi, rax");
        add_extern(cg, fn);
        fprintf(cg->output, "    call %s\n", fn);

        if (i < print_argc - 1) {
            if (sep_str) {
                int lbl = find_string_label(cg, sep_str, sep_len);
                if (lbl >= 0) {
                    fprintf(cg->output, "    lea rdi, [str_%d]\n", lbl);
                    add_extern(cg, "print_str");
                    fprintf(cg->output, "    call print_str\n");
                }
            }
        }
    }

    if (end_str) {
        if (end_len == 0) {
            /* no output */
        } else {
            int lbl = find_string_label(cg, end_str, end_len);
            if (lbl >= 0) {
                fprintf(cg->output, "    lea rdi, [str_%d]\n", lbl);
                add_extern(cg, "print_str");
                fprintf(cg->output, "    call print_str\n");
            }
        }
    } else {
        add_extern(cg, "print_newline");
        fprintf(cg->output, "    call print_newline\n");
    }
}

/* ============================================================
 * gen_expr — expression code generation
 * ============================================================ */

void gen_expr(codegen_t *cg, ast_node_t *n) {
    if (!n || cg->has_error) return;
    switch (n->type) {
        case AST_INT_LIT: emit(cg, "mov rax, %ld", n->as.int_val); break;
        case AST_FLOAT_LIT: {
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
                if (sym_float) {
                    emit(cg, "movsd xmm0, [rbp-%d]", off);
                } else {
                    emit(cg, "mov rax, [rbp-%d]", off);
                }
            } else if (is_user_defined(cg, n->as.ident.name, n->as.ident.name_len)) {
                char fn_name[MAX_IDENT_LEN];
                buf_check(cg, snprintf(fn_name, sizeof(fn_name), "%.*s",
                    (int)n->as.ident.name_len, n->as.ident.name),
                    sizeof(fn_name), "function name");
                int clo_off = add_sym(cg, "__fn_clo", 16);
                fprintf(cg->output, "    lea rax, [_%s]\n", fn_name);
                emit(cg, "mov [rbp-%d], rax", clo_off);
                emit(cg, "mov qword [rbp-%d + 8], 0", clo_off);
                emit(cg, "lea rax, [rbp-%d]", clo_off);
            } else {
                codegen_error(cg, "undefined '%.*s'",
                    (int)n->as.ident.name_len, n->as.ident.name);
                return;
            }
            break; }
        case AST_BINARY_OP:
            if (n->as.binary.op == TOKEN_COLONCOLON &&
                n->as.binary.left->type == AST_IDENT) {
                char enum_name[MAX_IDENT_LEN];
                buf_check(cg, snprintf(enum_name, sizeof(enum_name), "%.*s",
                    (int)n->as.binary.left->as.ident.name_len,
                    n->as.binary.left->as.ident.name),
                    sizeof(enum_name), "enum name");
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
            {
            int use_float = is_float_type(cg, n->as.binary.left) || is_float_type(cg, n->as.binary.right);

            if (use_float) {
                if (!is_float_type(cg, n->as.binary.left)) {
                    emit(cg, "cvtsi2sd xmm0, rax");
                }
                emit(cg, "movsd [rbp-8], xmm0");
                gen_expr(cg, n->as.binary.right);
                if (!is_float_type(cg, n->as.binary.right)) {
                    emit(cg, "cvtsi2sd xmm0, rax");
                }
                emit(cg, "movsd xmm1, xmm0");
                emit(cg, "movsd xmm0, [rbp-8]");
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
                          emit(cg, "mov rsi, %d", n->line);
                          if (cg->source_file)
                              emit(cg, "lea rdx, [rel _src_file]");
                          else
                              emit(cg, "xor rdx, rdx");
                          add_extern(cg, "panic_handler");
                          add_extern(cg, "div_zero_msg");
                          fprintf(cg->output, "    call panic_handler\n");
                          fprintf(cg->output, "L%d:\n", dz_label); }
                        if (expr_is_unsigned(cg, n->as.binary.left)) {
                            emit(cg, "xor rdx, rdx"); emit(cg, "div rcx");
                        } else {
                            emit(cg, "cqo"); emit(cg, "idiv rcx");
                        } break;
                    case TOKEN_EQ: emit(cg, "cmp rax, rcx"); emit(cg, "sete al"); emit(cg, "movzx rax, al"); break;
                    case TOKEN_NEQ: emit(cg, "cmp rax, rcx"); emit(cg, "setne al"); emit(cg, "movzx rax, al"); break;
                    case TOKEN_LT: { int u = expr_is_unsigned(cg, n->as.binary.left);
                        emit(cg, "cmp rax, rcx"); emit(cg, u ? "setb al" : "setl al"); emit(cg, "movzx rax, al"); break; }
                    case TOKEN_GT: { int u = expr_is_unsigned(cg, n->as.binary.left);
                        emit(cg, "cmp rax, rcx"); emit(cg, u ? "seta al" : "setg al"); emit(cg, "movzx rax, al"); break; }
                    case TOKEN_LTE: { int u = expr_is_unsigned(cg, n->as.binary.left);
                        emit(cg, "cmp rax, rcx"); emit(cg, u ? "setbe al" : "setle al"); emit(cg, "movzx rax, al"); break; }
                    case TOKEN_GTE: { int u = expr_is_unsigned(cg, n->as.binary.left);
                        emit(cg, "cmp rax, rcx"); emit(cg, u ? "setae al" : "setge al"); emit(cg, "movzx rax, al"); break; }
                    case TOKEN_AND: emit(cg, "and rax, rcx"); break;
                    case TOKEN_OR: emit(cg, "or rax, rcx"); break;
                    case TOKEN_PERCENT:
                        emit(cg, "test rcx, rcx");
                        { int dz_label = new_label(cg);
                          emit(cg, "jnz L%d", dz_label);
                          emit(cg, "lea rdi, [rel div_zero_msg]");
                          emit(cg, "mov rsi, %d", n->line);
                          if (cg->source_file)
                              emit(cg, "lea rdx, [rel _src_file]");
                          else
                              emit(cg, "xor rdx, rdx");
                          add_extern(cg, "panic_handler");
                          fprintf(cg->output, "    call panic_handler\n");
                          fprintf(cg->output, "L%d:\n", dz_label); }
                        if (expr_is_unsigned(cg, n->as.binary.left)) {
                            emit(cg, "xor rdx, rdx"); emit(cg, "div rcx"); emit(cg, "mov rax, rdx");
                        } else {
                            emit(cg, "cqo"); emit(cg, "idiv rcx"); emit(cg, "mov rax, rdx");
                        } break;
                    default: break;
                }
            } } break;
        case AST_CALL: {
            if (n->as.call.callee->type == AST_IDENT &&
                n->as.call.callee->as.ident.name_len == 5 &&
                memcmp(n->as.call.callee->as.ident.name, "print", 5) == 0) {
                gen_print_call(cg, n);
                break;
            }

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
                int use_closure = 0;
                if (n->as.call.callee->type == AST_BINARY_OP &&
                    n->as.call.callee->as.binary.op == TOKEN_COLONCOLON) {
                    ast_node_t *mod = n->as.call.callee->as.binary.left;
                    if (!is_stdlib_module(cg, mod->as.ident.name, mod->as.ident.name_len))
                        use_closure = 1;
                }
                emit_call(cg, n->as.call.callee, call_args, call_argc, use_closure);
            }
            if (was_resolved) free(resolved_args);
            break; }
        case AST_PIPE: {
            ast_node_t *callee = n->as.pipe.right;
            if (callee->type == AST_CALL) {
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
                    ast_node_t *mod = callee->as.call.callee->as.binary.left;
                    if (!is_stdlib_module(cg, mod->as.ident.name, mod->as.ident.name_len))
                        pipe_is_user = 1;
                }
                /* Special case: array_push needs elem_size in rcx */
                if (callee->as.call.callee->type == AST_IDENT &&
                    callee->as.call.callee->as.ident.name_len == 10 &&
                    memcmp(callee->as.call.callee->as.ident.name, "array_push", 10) == 0) {
                    int es = 8;
                    if (n->as.pipe.left->typed && n->as.pipe.left->typed->kind == TYPE_ARRAY
                        && n->as.pipe.left->typed->base)
                        es = type_elem_size(n->as.pipe.left->typed->base);
                    /* Emit call manually: rdi=arr, rsi=elem, rcx=elem_size */
                    emit(cg, "push rbx");
                    emit(cg, "push rbx");
                    gen_expr(cg, all_args[0]); /* arr */
                    emit(cg, "mov rdi, rax");
                    gen_expr(cg, all_args[1]); /* elem */
                    emit(cg, "mov rsi, rax");
                    emit(cg, "mov rcx, %d", es);
                    emit(cg, "pop rbx");
                    emit(cg, "pop rbx");
                    add_extern(cg, "array_push");
                    fprintf(cg->output, "    call array_push\n");
                } else {
                    emit_call(cg, callee->as.call.callee, all_args, total, pipe_is_user ? 1 : 0);
                }
                free(all_args);
            } else {
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
            for (int i = n->as.tuple.count - 1; i >= 0; i--) {
                gen_expr(cg, n->as.tuple.elements[i]);
                emit(cg, "push rax");
            }
            break; }
        case AST_ARRAY_LITERAL: {
            int count = n->as.array_literal.count;
            int elem_size = 8;
            if (n->typed && n->typed->kind == TYPE_ARRAY && n->typed->base)
                elem_size = type_elem_size(n->typed->base);
            emit(cg, "mov rdi, %d", elem_size);
            emit(cg, "mov rsi, %d", count);
            add_extern(cg, "with_capacity");
            fprintf(cg->output, "    call with_capacity\n");
            for (int i = 0; i < count; i++) {
                emit(cg, "push rax");
                gen_expr(cg, n->as.array_literal.elements[i]);
                emit(cg, "mov rsi, rax");
                emit(cg, "pop rdi");
                emit(cg, "mov rcx, %d", elem_size);
                add_extern(cg, "array_push");
                fprintf(cg->output, "    call array_push\n");
            }
            break; }
        case AST_INDEX: {
            gen_expr(cg, n->as.binary.left);
            emit(cg, "mov rdi, rax");
            gen_expr(cg, n->as.binary.right);
            emit(cg, "mov rsi, rax");
            add_extern(cg, "get");
            fprintf(cg->output, "    call get\n");
            break; }
        case AST_LEN_EXPR: {
            gen_expr(cg, n->as.len_expr.operand);
            emit(cg, "mov rdi, rax");
            add_extern(cg, "len");
            fprintf(cg->output, "    call len\n");
            break; }
        case AST_MEMBER: {
            gen_expr(cg, n->as.member.object);
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
                                    d->as.struct_decl.fields[fi].name_len) == 0) {
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
            gen_expr(cg, n->as.try_expr.operand);
            emit(cg, "test rax, 1");
            int ok_label = new_label(cg);
            emit(cg, "je L%d", ok_label);
            emit_defers(cg, 0);
            emit(cg, "mov rsp, rbp"); emit(cg, "pop rbp"); emit(cg, "ret");
            fprintf(cg->output, "L%d:\n", ok_label);
            emit(cg, "shr rax, 1");
            break; }
        case AST_CATCH_EXPR: {
            int err_label = new_label(cg);
            int end_label = new_label(cg);
            gen_expr(cg, n->as.catch_expr.operand);
            emit(cg, "test rax, 1");
            emit(cg, "jne L%d", err_label);
            emit(cg, "shr rax, 1");
            emit(cg, "jmp L%d", end_label);
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
            int clo_label = new_label(cg);

            ident_list_t all_idents = {0};
            collect_idents(n->as.closure.body, &all_idents);

            int capture_count = 0;
            char **capture_names = NULL;
            int *capture_offsets = NULL;

            for (int ci = 0; ci < all_idents.count; ci++) {
                int is_param = 0;
                for (int pi = 0; pi < n->as.closure.param_count; pi++)
                    if (n->as.closure.param_lens[pi] == all_idents.name_lens[ci] &&
                        memcmp(n->as.closure.params[pi], all_idents.names[ci], all_idents.name_lens[ci]) == 0)
                        { is_param = 1; break; }
                if (is_param) { free(all_idents.names[ci]); all_idents.names[ci] = NULL; continue; }

                int off = find_sym(cg, all_idents.names[ci]);
                if (off < 0) { free(all_idents.names[ci]); all_idents.names[ci] = NULL; continue; }

                int dup = 0;
                for (int di = 0; di < capture_count; di++)
                    if (strlen(capture_names[di]) == all_idents.name_lens[ci] &&
                        memcmp(capture_names[di], all_idents.names[ci], all_idents.name_lens[ci]) == 0)
                        { dup = 1; break; }
                if (dup) { free(all_idents.names[ci]); all_idents.names[ci] = NULL; continue; }

                capture_names = SAFE_REALLOC(capture_names, sizeof(char*) * (capture_count + 1));
                capture_offsets = SAFE_REALLOC(capture_offsets, sizeof(int) * (capture_count + 1));
                capture_names[capture_count] = all_idents.names[ci];
                capture_offsets[capture_count] = off;
                all_idents.names[ci] = NULL;
                capture_count++;
            }
            free(all_idents.name_lens);
            for (int ci = 0; ci < all_idents.count; ci++)
                if (all_idents.names[ci]) free(all_idents.names[ci]);
            free(all_idents.names);

            int env_slot = -1;
            if (capture_count > 0) {
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
                    emit(cg, "mov rcx, 8");
                    add_extern(cg, "array_push");
                    fprintf(cg->output, "    call array_push\n");
                    emit(cg, "mov [rbp-%d], rax", env_slot);
                }
            }

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

            int clo_env_slot = -1;
            if (capture_count > 0) {
                clo_env_slot = add_sym(cg, "__clo_env", 8);
                emit(cg, "mov [rbp-%d], rdi", clo_env_slot);
            }

            int clo_param_count = n->as.closure.param_count < 5 ? n->as.closure.param_count : 5;
            for (int ai = 0; ai < clo_param_count; ai++) {
                int poff = add_sym(cg, n->as.closure.params[ai], 8);
                emit(cg, "mov [rbp-%d], %s", poff, cr[ai + 1]);
            }

            for (int ci = 0; ci < capture_count; ci++) {
                int coff = add_sym(cg, capture_names[ci], 8);
                emit(cg, "mov rax, [rbp-%d]", clo_env_slot);
                emit(cg, "mov rax, [rax + %d]", ci * 8);
                emit(cg, "mov [rbp-%d], rax", coff);
            }

            long clo_sub_rsp_pos = ftell(cg->output);
            fprintf(cg->output, "    sub rsp, 0x00000000\n");
            gen_expr(cg, n->as.closure.body);
            int clo_stack_needed = cg->max_stack_size;
            if (clo_stack_needed < CLO_MIN_FRAME + FRAME_HEADER) clo_stack_needed = CLO_MIN_FRAME + FRAME_HEADER;
            if ((unsigned)(clo_stack_needed - FRAME_HEADER) > 0xFFFFFFFF) {
                codegen_error(cg, "closure stack frame too large");
                return;
            }
            long clo_cur_pos = ftell(cg->output);
            fseek(cg->output, clo_sub_rsp_pos, SEEK_SET);
            fprintf(cg->output, "    sub rsp, 0x%08X", (unsigned)(clo_stack_needed - FRAME_HEADER));
            fseek(cg->output, clo_cur_pos, SEEK_SET);
            emit(cg, "mov rsp, rbp"); emit(cg, "pop rbp"); emit(cg, "ret");

            fclose(mem);
            cg->output = saved_output;

            cg->stack_size = saved_stack;
            cg->max_stack_size = saved_max;

            if (cg->closure_body_count >= cg->closure_body_cap) {
                cg->closure_body_cap = cg->closure_body_cap ? cg->closure_body_cap * 2 : 8;
                cg->closure_bodies = SAFE_REALLOC(cg->closure_bodies,
                    sizeof(*cg->closure_bodies) * cg->closure_body_cap);
            }
            cg->closure_bodies[cg->closure_body_count].label = clo_label;
            cg->closure_bodies[cg->closure_body_count].asm_text = buf;
            cg->closure_bodies[cg->closure_body_count].asm_len = buf_len;
            cg->closure_body_count++;

            int clo_off = add_sym(cg, "__clo", 16);
            emit(cg, "lea rax, [_fn%d]", clo_label);
            emit(cg, "mov [rbp-%d], rax", clo_off);
            if (env_slot >= 0)
                emit(cg, "mov rax, [rbp-%d]", env_slot);
            else
                emit(cg, "xor rax, rax");
            emit(cg, "mov [rbp-%d + 8], rax", clo_off);
            emit(cg, "lea rax, [rbp-%d]", clo_off);

            for (int ci = 0; ci < capture_count; ci++) free(capture_names[ci]);
            free(capture_names);
            free(capture_offsets);
            break; }
        default: break;
    }
}

/* ============================================================
 * Statement generators
 * ============================================================ */

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
    int saved_end = cg->current_loop_end;
    int saved_inc = cg->current_loop_inc;
    cg->current_loop_end = end;
    cg->current_loop_inc = loop;
    fprintf(cg->output, "L%d:\n", loop);
    gen_expr(cg, n->as.while_stmt.condition);
    emit(cg, "cmp rax, 0"); emit(cg, "je L%d", end);
    cg->returned = 0;
    gen_node(cg, n->as.while_stmt.body);
    cg->returned = 0;
    emit(cg, "jmp L%d", loop);
    fprintf(cg->output, "L%d:\n", end);
    cg->current_loop_end = saved_end;
    cg->current_loop_inc = saved_inc;
}

static void gen_for(codegen_t *cg, ast_node_t *n) {
    int loop = new_label(cg), end = new_label(cg), inc = new_label(cg);
    int saved_end = cg->current_loop_end;
    int saved_inc = cg->current_loop_inc;
    cg->current_loop_end = end;
    cg->current_loop_inc = inc;
    scope_mark_t for_mark = scope_enter(cg);

    if (n->as.for_stmt.iterable->type == AST_RANGE) {
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
    cg->current_loop_end = saved_end;
    cg->current_loop_inc = saved_inc;
}

static void gen_loop(codegen_t *cg, ast_node_t *n) {
    int loop = new_label(cg), end = new_label(cg);
    int saved_end = cg->current_loop_end;
    int saved_inc = cg->current_loop_inc;
    cg->current_loop_end = end;
    cg->current_loop_inc = loop;
    fprintf(cg->output, "L%d:\n", loop);
    cg->returned = 0;
    gen_node(cg, n->as.loop_stmt.body);
    cg->returned = 0;
    emit(cg, "jmp L%d", loop);
    fprintf(cg->output, "L%d:\n", end);
    cg->current_loop_end = saved_end;
    cg->current_loop_inc = saved_inc;
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
    int stack_needed = cg->max_stack_size - FRAME_BASE + 16;
    if (stack_needed < 0) stack_needed = 0;
    if ((unsigned)stack_needed > 0xFFFFFFFF) {
        codegen_error(cg, "stack frame too large (%d bytes)", stack_needed);
        return;
    }
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
    for (int i = saved_var_type_count; i < cg->var_type_count; i++) {
        free(cg->var_types[i].name);
        free(cg->var_types[i].struct_name);
    }
    cg->var_type_count = saved_var_type_count;
}

static void gen_struct_decl(codegen_t *cg, ast_node_t *n) {
    fprintf(cg->output, "\n; struct %.*s\n", (int)n->as.struct_decl.name_len, n->as.struct_decl.name);
    fprintf(cg->output, "global ");
    emit_name(cg, n->as.struct_decl.name, n->as.struct_decl.name_len);
    fprintf(cg->output, "\n");
    emit_name(cg, n->as.struct_decl.name, n->as.struct_decl.name_len);
    fprintf(cg->output, ":\n");
    emit(cg, "push rbp"); emit(cg, "mov rbp, rsp");

    int field_count = n->as.struct_decl.field_count;
    const char *regs[] = {"rdi","rsi","rdx","rcx","r8","r9"};
    for (int i = 0; i < field_count && i < 6; i++)
        emit(cg, "push %s", regs[i]);

    emit(cg, "mov rdi, %d", 16 + field_count * 8);
    add_extern(cg, "_bump_alloc");
    fprintf(cg->output, "    call _bump_alloc\n");

    emit(cg, "mov qword [rax], 1");
    emit(cg, "mov qword [rax+8], %d", field_count);

    for (int i = field_count - 1; i >= 0; i--) {
        emit(cg, "pop rbx");
        emit(cg, "mov [rax + %d], rbx", 16 + i * 8);
    }

    emit(cg, "add rax, 16");
    emit(cg, "mov rsp, rbp"); emit(cg, "pop rbp"); emit(cg, "ret");
}

/* ============================================================
 * gen_stmt — statement dispatch
 * ============================================================ */

void gen_stmt(codegen_t *cg, ast_node_t *n) {
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
                if (n->as.let.type_expr && n->as.let.type_expr->type == AST_IDENT) {
                    if (cg->var_type_count >= cg->var_type_cap) {
                        cg->var_type_cap = cg->var_type_cap ? cg->var_type_cap * 2 : 8;
                        cg->var_types = SAFE_REALLOC(cg->var_types,
                            sizeof(*cg->var_types) * cg->var_type_cap);
                    }
                    cg->var_types[cg->var_type_count].name = SAFE_STRDUP(n->as.let.name);
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
        case AST_LOOP: gen_loop(cg, n); break;
        case AST_BREAK:
            if (cg->current_loop_end < 0) {
                codegen_error(cg, "break outside of loop");
                break;
            }
            emit_defers(cg, cg->scope_depth);
            emit(cg, "jmp L%d", cg->current_loop_end);
            cg->returned = 1;
            break;
        case AST_CONTINUE:
            if (cg->current_loop_inc < 0) {
                codegen_error(cg, "continue outside of loop");
                break;
            }
            emit_defers(cg, cg->scope_depth);
            emit(cg, "jmp L%d", cg->current_loop_inc);
            cg->returned = 1;
            break;
        case AST_BLOCK: gen_block(cg, n); break;
        case AST_FN_DECL: gen_fn_decl(cg, n); break;
        case AST_STRUCT_DECL: gen_struct_decl(cg, n); break;
        case AST_IMPL_DECL: {
            char type_name[MAX_IDENT_LEN];
            buf_check(cg, snprintf(type_name, sizeof(type_name), "%.*s",
                (int)n->as.impl_decl.type_name_len, n->as.impl_decl.type_name),
                sizeof(type_name), "impl type name");
            for (int mi = 0; mi < n->as.impl_decl.method_count; mi++) {
                ast_node_t *method = n->as.impl_decl.methods[mi];
                if (method->type != AST_FN_DECL) continue;
                fprintf(cg->output, "\n; %s::", type_name);
                fprintf(cg->output, "%.*s\n", (int)method->as.fn_decl.name_len, method->as.fn_decl.name);
                fprintf(cg->output, "global ");
                emit_name(cg, type_name, strlen(type_name));
                fprintf(cg->output, "_");
                fprintf(cg->output, "%.*s\n", (int)method->as.fn_decl.name_len, method->as.fn_decl.name);
                emit_name(cg, type_name, strlen(type_name));
                fprintf(cg->output, "_");
                fprintf(cg->output, "%.*s:\n", (int)method->as.fn_decl.name_len, method->as.fn_decl.name);
                char *old_name = method->as.fn_decl.name;
                size_t old_len = method->as.fn_decl.name_len;
                char new_name[MAX_IDENT_LEN];
                buf_check(cg, snprintf(new_name, sizeof(new_name), "%s_%.*s", type_name,
                    (int)old_len, old_name), sizeof(new_name), "method name");
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
            int count = n->as.enum_decl.variant_count;
            char enum_name[MAX_IDENT_LEN];
            buf_check(cg, snprintf(enum_name, sizeof(enum_name), "%.*s",
                (int)n->as.enum_decl.name_len, n->as.enum_decl.name),
                sizeof(enum_name), "enum name");

            fprintf(cg->output, "; enum %.*s — %d variants\n",
                (int)n->as.enum_decl.name_len, n->as.enum_decl.name, count);

            for (int i = 0; i < count; i++) {
                int vlen = (int)n->as.enum_decl.variants[i].name_len;
                fprintf(cg->output, "  _enum_%s_name_%d: db ", enum_name, i);
                for (int c = 0; c < vlen; c++)
                    fprintf(cg->output, "0x%02X, ", (unsigned char)n->as.enum_decl.variants[i].name[c]);
                fprintf(cg->output, "0\n");
            }

            fprintf(cg->output, "  _enum_%s_names: dq ", enum_name);
            for (int i = 0; i < count; i++)
                fprintf(cg->output, "_enum_%s_name_%d, ", enum_name, i);
            fprintf(cg->output, "0\n");

            fprintf(cg->output, "global %s_tag\n", enum_name);
            fprintf(cg->output, "%s_tag:\n", enum_name);
            emit(cg, "mov rax, rdi");
            emit(cg, "ret");

            fprintf(cg->output, "global %s_name\n", enum_name);
            fprintf(cg->output, "%s_name:\n", enum_name);
            emit(cg, "lea rax, [_enum_%s_names]", enum_name);
            emit(cg, "mov rax, [rax + rdi*8]");
            emit(cg, "ret");

            fprintf(cg->output, "global %s_count\n", enum_name);
            fprintf(cg->output, "%s_count:\n", enum_name);
            emit(cg, "mov rax, %d", count);
            emit(cg, "ret");

            char fn_name[MAX_IDENT_LEN];
            buf_check(cg, snprintf(fn_name, sizeof(fn_name), "%s_tag", enum_name),
                sizeof(fn_name), "enum function name");
            add_extern(cg, fn_name);
            buf_check(cg, snprintf(fn_name, sizeof(fn_name), "%s_name", enum_name),
                sizeof(fn_name), "enum function name");
            add_extern(cg, fn_name);
            buf_check(cg, snprintf(fn_name, sizeof(fn_name), "%s_count", enum_name),
                sizeof(fn_name), "enum function name");
            add_extern(cg, fn_name);

            for (int i = 0; i < count; i++) {
                char variant_name[MAX_IDENT_LEN];
                buf_check(cg, snprintf(variant_name, sizeof(variant_name), "%.*s",
                    (int)n->as.enum_decl.variants[i].name_len,
                    n->as.enum_decl.variants[i].name),
                    sizeof(variant_name), "enum variant name");
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
            char ns[256];
            if (n->as.import.alias) {
                snprintf(ns, sizeof(ns), "%.*s", (int)n->as.import.alias_len, n->as.import.alias);
            } else {
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
            if (n->as.panic_expr.message) {
                gen_expr(cg, n->as.panic_expr.message);
                emit(cg, "mov rdi, rax");
            } else {
                emit(cg, "xor rdi, rdi");
            }
            emit(cg, "mov rsi, %d", n->line);
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
            gen_expr(cg, n->as.assert_expr.condition);
            emit(cg, "push rax");
            if (n->as.assert_expr.message) {
                gen_expr(cg, n->as.assert_expr.message);
                emit(cg, "mov rsi, rax");
            } else {
                emit(cg, "xor rsi, rsi");
            }
            emit(cg, "pop rdi");
            add_extern(cg, "assert_handler");
            fprintf(cg->output, "    call ");
            emit_name(cg, "assert_handler", 14);
            fprintf(cg->output, "\n");
            break; }
        default: gen_expr(cg, n); break;
    }
}

/* ============================================================
 * gen_node — top-level dispatch
 * ============================================================ */

void gen_node(codegen_t *cg, ast_node_t *n) {
    if (!n || cg->has_error) return;
    switch (n->type) {
        case AST_PROGRAM: for (int i = 0; i < n->as.program.count; i++) gen_node(cg, n->as.program.declarations[i]); break;
        default: gen_stmt(cg, n); break;
    }
}

/* ============================================================
 * codegen_init / codegen_free / codegen_program
 * ============================================================ */

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
    cg->current_loop_end = -1; cg->current_loop_inc = -1;
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

int codegen_program(codegen_t *cg, ast_node_t *prog) {
    cg->prog = prog;
    collect_strings(cg, prog);
    collect_floats(cg, prog);
    fprintf(cg->output, "; Generated by ELang compiler v%s\n", ELANG_VERSION);

    if (cg->string_entries > 0 || cg->source_file || cg->float_entries_count > 0) {
        emit_raw(cg, "section .data");
        for (int i = 0; i < cg->string_entries; i++)
            emit_string_data(cg, cg->strings[i].label, cg->strings[i].value, cg->strings[i].length);
        for (int i = 0; i < cg->float_entries_count; i++) {
            uint64_t bits;
            memcpy(&bits, &cg->float_entries[i].value, sizeof(bits));
            fprintf(cg->output, "  fconst_%d: dq 0x%016llx\n",
                cg->float_entries[i].label, (unsigned long long)bits);
        }
        if (cg->source_file) {
            fprintf(cg->output, "  _src_file: db ");
            for (const char *p = cg->source_file; *p; p++)
                fprintf(cg->output, "0x%02X, ", (unsigned char)*p);
            fprintf(cg->output, "0\n");
        }
    }
    emit_raw(cg, "section .text");
    gen_node(cg, prog);

    for (int i = 0; i < cg->closure_body_count; i++) {
        fwrite(cg->closure_bodies[i].asm_text, 1, cg->closure_bodies[i].asm_len, cg->output);
        fprintf(cg->output, "\n");
    }

    fprintf(cg->output, "\n; Extern declarations (resolved by linker)\n");
    for (int i = 0; i < cg->extern_count; i++) {
        fprintf(cg->output, "extern %s\n", cg->extern_names[i]);
    }

    if (cg->is_main) {
        emit_raw(cg, "global _start"); emit_raw(cg, "");
        emit_raw(cg, "_start:");
        emit_raw(cg, "    call _main");
        emit_raw(cg, "    mov rdi, rax");
        emit_raw(cg, "    mov rax, 60");
        emit_raw(cg, "    syscall");
    }
    return cg->has_error ? 1 : 0;
}
