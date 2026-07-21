/* ELang Code Generator */
#ifndef ELANG_CODEGEN_H
#define ELANG_CODEGEN_H

#include "config.h"
#include "ast.h"
#include <stdio.h>

typedef struct {
    int label;
    char *value;
    size_t length;
} string_entry_t;

typedef struct {
    FILE *output;
    int label_count, string_count;
    struct { char *name; int stack_offset, size; int is_float; } *symbols;
    int symbol_count, stack_size, max_stack_size;
    string_entry_t *strings;
    int string_entries;
    int is_main;
    int returned;
    struct { ast_node_t *expr; int scope_depth; } *defers;
    int defer_count, defer_cap, scope_depth;
    char **extern_names;
    int extern_count;
    int in_return_expr;
    int current_loop_end;   /* label of current loop's end (for break) */
    int current_loop_inc;   /* label of current loop's continue target */
    long sub_rsp_pos;   /* file offset of sub rsp placeholder for patching */
    const char *source_file; /* source filename for panic location */
    ast_node_t *prog;   /* AST root for function lookup */
    int has_error;      /* set on codegen error, checked before continuing */
    int error_count;    /* total errors seen */
    struct { char *name; char *struct_name; } *var_types; /* variable → struct type */
    int var_type_count, var_type_cap;
    /* Closure body buffer: bodies emitted after main code */
    struct { int label; char *asm_text; size_t asm_len; } *closure_bodies;
    int closure_body_count, closure_body_cap;
    int ret_buf_off;     /* stack offset for tuple return buffer (-1 = none) */
    int ret_tuple_count; /* number of elements in tuple return */
    /* Module table: tracks project modules (merged, prefixed) vs stdlib (separate .o) */
    struct { char *name; int is_stdlib; } *modules;
    int module_count, module_cap;
    /* Float constants table */
    struct { int label; double value; } *float_entries;
    int float_entries_count;
} codegen_t;

void codegen_init(codegen_t *cg, FILE *output);
int codegen_program(codegen_t *cg, ast_node_t *program);
void codegen_free(codegen_t *cg);

#endif
