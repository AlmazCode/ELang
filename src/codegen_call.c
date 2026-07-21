/* ELang Codegen — call helpers (emit_call, emit_closure_call, resolve_call_args) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen_internal.h"

/* --- Call target resolution --- */

void emit_call_target(codegen_t *cg, ast_node_t *callee) {
    if (callee->type == AST_BINARY_OP &&
        callee->as.binary.op == TOKEN_COLONCOLON) {
        ast_node_t *mod = callee->as.binary.left;
        ast_node_t *fn = callee->as.binary.right;
        char mod_name[MAX_IDENT_LEN];
        buf_check(cg, snprintf(mod_name, sizeof(mod_name), "%.*s",
            (int)mod->as.ident.name_len, mod->as.ident.name),
            sizeof(mod_name), "module name");
        if (is_stdlib_module(cg, mod_name, strlen(mod_name))) {
            char fn_name[MAX_IDENT_LEN];
            buf_check(cg, snprintf(fn_name, sizeof(fn_name), "%.*s",
                (int)fn->as.ident.name_len, fn->as.ident.name),
                sizeof(fn_name), "function name");
            add_extern(cg, fn_name);
            fprintf(cg->output, "    call %s\n", fn_name);
        } else {
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
            fprintf(cg->output, "    call ");
            emit_name(cg, callee->as.ident.name, callee->as.ident.name_len);
            fprintf(cg->output, "\n");
        } else {
            add_extern(cg, fn_name);
            if (is_nasm_keyword(fn_name, strlen(fn_name)))
                fprintf(cg->output, "    call _%s\n", fn_name);
            else
                fprintf(cg->output, "    call %s\n", fn_name);
        }
    }
}

/* --- Shared: evaluate args into register slots --- */

int emit_args_to_regs(codegen_t *cg, ast_node_t **args,
                      const char **arg_regs, int reg_count) {
    FILE *real_output = cg->output;
    if (reg_count > 0) { gen_expr(cg, args[0]); cg->output = real_output;
        if (is_float_type(cg, args[0])) emit(cg, "movq rax, xmm0");
        emit(cg, "push rax"); }
    if (reg_count > 1) { gen_expr(cg, args[1]); cg->output = real_output;
        if (is_float_type(cg, args[1])) emit(cg, "movq rax, xmm0");
        emit(cg, "mov rbx, rax"); }
    for (int i = 2; i < reg_count; i++) {
        gen_expr(cg, args[i]); cg->output = real_output;
        if (is_float_type(cg, args[i])) emit(cg, "movq rax, xmm0");
        emit(cg, "mov %s, rax", arg_regs[i]);
    }
    if (reg_count > 0) { emit(cg, "pop rax"); emit(cg, "mov %s, rax", arg_regs[0]); }
    if (reg_count > 1) emit(cg, "mov %s, rbx", arg_regs[1]);
    return 0;
}

/* --- emit_call --- */

void emit_call(codegen_t *cg, ast_node_t *callee, ast_node_t **args, int narg, int closure_convention) {
    static const char *std_regs[] = {"rdi","rsi","rdx","rcx","r8","r9"};
    static const char *clo_regs[] = {"rsi","rdx","rcx","r8","r9","r10"};
    int max_regs = 6;
    int reg_count = narg < max_regs ? narg : max_regs;
    int stack_args = narg > max_regs ? narg - max_regs : 0;

    emit(cg, "push rbx");
    emit(cg, "push rbx");

    if (closure_convention) {
        emit(cg, "xor rdi, rdi");
        emit_args_to_regs(cg, args, clo_regs, reg_count);
    } else {
        emit_args_to_regs(cg, args, std_regs, reg_count);
    }

    emit(cg, "pop rbx");
    emit(cg, "pop rbx");

    int total = cg->stack_size + 8 * stack_args + 16;
    int align = (16 - (total % 16)) % 16;
    if (align) emit(cg, "sub rsp, %d", align);

    emit_call_target(cg, callee);

    int cleanup = align + 8 * stack_args;
    if (cleanup) emit(cg, "add rsp, %d", cleanup);
}

/* --- emit_closure_call --- */

void emit_closure_call(codegen_t *cg, int clo_stack_off, ast_node_t **args, int narg) {
    static const char *clo_regs[] = {"rsi","rdx","rcx","r8","r9"};
    int reg_count = narg < 5 ? narg : 5;
    int stack_args = narg > 5 ? narg - 5 : 0;

    emit(cg, "push rbx");
    emit(cg, "push rbx");

    emit(cg, "mov rax, [rbp-%d]", clo_stack_off);
    emit(cg, "mov r10, [rax]");
    emit(cg, "mov rdi, [rax+8]");

    emit_args_to_regs(cg, args, clo_regs, reg_count);

    emit(cg, "pop rbx");
    emit(cg, "pop rbx");

    int total = cg->stack_size + 8 * stack_args + 16;
    int align = (16 - (total % 16)) % 16;
    if (align) emit(cg, "sub rsp, %d", align);

    emit(cg, "call r10");

    int cleanup = align + 8 * stack_args;
    if (cleanup) emit(cg, "add rsp, %d", cleanup);
}

/* --- resolve_call_args (named args + default values) --- */

int resolve_call_args(codegen_t *cg, ast_node_t *call_node,
                      ast_node_t ***out_args, int *out_count) {
    *out_args = NULL;
    *out_count = 0;
    if (call_node->type != AST_CALL) return 0;
    if (call_node->as.call.callee->type != AST_IDENT) return 0;

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

    ast_node_t **resolved = SAFE_CALLOC(param_count, sizeof(ast_node_t*));

    int positional_idx = 0;
    for (int i = 0; i < call_node->as.call.arg_count; i++) {
        ast_node_t *arg = call_node->as.call.args[i];
        if (arg && arg->type == AST_BINARY_OP && arg->as.binary.op == TOKEN_COLON &&
            arg->as.binary.left->type == AST_IDENT) {
            ast_node_t *key = arg->as.binary.left;
            for (int p = 0; p < param_count; p++) {
                if (strlen(fn_decl->as.fn_decl.params[p].name) == key->as.ident.name_len &&
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

    for (int p = 0; p < param_count; p++) {
        if (!resolved[p] && fn_decl->as.fn_decl.params[p].default_value) {
            resolved[p] = fn_decl->as.fn_decl.params[p].default_value;
        }
    }

    *out_args = resolved;
    *out_count = param_count;
    return 1;
}
