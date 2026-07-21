/* Internal codegen declarations — shared across codegen modules */
#ifndef ELANG_CODEGEN_INTERNAL_H
#define ELANG_CODEGEN_INTERNAL_H

#include "codegen.h"
#include "semantics.h"

/* --- Frame layout constants --- */
#define FRAME_REGS          5
#define FRAME_HEADER        (FRAME_REGS * 8)
#define WATERMARK_SLOT      FRAME_HEADER
#define FRAME_BASE          (FRAME_HEADER + 8)
#define CLO_MIN_FRAME       56
#define PUSH_RESERVE        64

/* --- Capture detection --- */
typedef struct {
    char **names;
    size_t *name_lens;
    int count, cap;
} ident_list_t;

/* --- Shared utility functions --- */
void codegen_error(codegen_t *cg, const char *fmt, ...);
void buf_check(codegen_t *cg, int written, size_t bufsize, const char *context);
void emit(codegen_t *cg, const char *fmt, ...);
void emit_raw(codegen_t *cg, const char *s);
int  new_label(codegen_t *cg);
void add_extern(codegen_t *cg, const char *name);
void emit_name(codegen_t *cg, const char *name, size_t len);
int  is_extern(codegen_t *cg, const char *name, size_t len);
int  is_user_defined(codegen_t *cg, const char *name, size_t len);
int  is_nasm_keyword(const char *name, size_t len);

/* --- Symbol table --- */
int  find_sym(codegen_t *cg, const char *name);
int  find_sym_is_float(codegen_t *cg, const char *name, int *is_float);
int  add_sym(codegen_t *cg, const char *name, int size);
int  add_sym_float(codegen_t *cg, const char *name, int size, int is_float);

/* --- Scope --- */
typedef struct { int symbol_count, stack_size; } scope_mark_t;
scope_mark_t scope_enter(codegen_t *cg);
void scope_exit(codegen_t *cg, scope_mark_t m);

/* --- Strings --- */
int  find_string_label(codegen_t *cg, const char *value, size_t length);
void emit_string_data(codegen_t *cg, int label, const char *value, size_t length);
void collect_strings(codegen_t *cg, ast_node_t *n);

/* --- Floats --- */
int  is_float_type(codegen_t *cg, ast_node_t *n);
int  add_float_const(codegen_t *cg, double val);
void collect_floats(codegen_t *cg, ast_node_t *n);

/* --- Capture detection --- */
void idlist_add(ident_list_t *l, const char *name, size_t len);
void collect_idents(ast_node_t *n, ident_list_t *out);

/* --- Defer --- */
void push_defer(codegen_t *cg, ast_node_t *expr);
void emit_defers(codegen_t *cg, int from_depth);

/* --- Call helpers --- */
int  emit_args_to_regs(codegen_t *cg, ast_node_t **args, const char **arg_regs, int reg_count);
void emit_call_target(codegen_t *cg, ast_node_t *callee);
void emit_call(codegen_t *cg, ast_node_t *callee, ast_node_t **args, int narg, int closure_convention);
void emit_closure_call(codegen_t *cg, int clo_stack_off, ast_node_t **args, int narg);
int  resolve_call_args(codegen_t *cg, ast_node_t *call_node, ast_node_t ***out_args, int *out_count);

/* --- Modules --- */
void register_module(codegen_t *cg, const char *name, size_t name_len, int is_stdlib);
int  is_stdlib_module(codegen_t *cg, const char *name, size_t name_len);

/* --- Code generation (forward) --- */
void gen_expr(codegen_t *cg, ast_node_t *n);
void gen_stmt(codegen_t *cg, ast_node_t *n);
void gen_node(codegen_t *cg, ast_node_t *n);

#endif
