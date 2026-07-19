/* ELang Code Generator */
#ifndef ELANG_CODEGEN_H
#define ELANG_CODEGEN_H

#include "ast.h"
#include <stdio.h>

/* --- Shared constants --- */
#define MAX_IDENT_LEN       64    /* max identifier buffer size */
#define MAX_EXTNAME_LEN     128   /* max extern name buffer size */
#define MAX_PARSE_DEPTH     2000  /* parser recursion limit */

typedef struct {
    int label;
    char *value;
    size_t length;
} string_entry_t;

typedef struct {
    FILE *output;
    int label_count, string_count;
    struct { char *name; int stack_offset, size; } *symbols;
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
} codegen_t;

void codegen_init(codegen_t *cg, FILE *output);
int codegen_program(codegen_t *cg, ast_node_t *program);
void codegen_free(codegen_t *cg);

#endif
