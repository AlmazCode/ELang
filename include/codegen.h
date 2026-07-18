/* ELang Code Generator */
#ifndef ELANG_CODEGEN_H
#define ELANG_CODEGEN_H

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
    struct { char *name; int stack_offset, size; } *symbols;
    int symbol_count, stack_size;
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
} codegen_t;

void codegen_init(codegen_t *cg, FILE *output);
int codegen_program(codegen_t *cg, ast_node_t *program);
void codegen_free(codegen_t *cg);

#endif
