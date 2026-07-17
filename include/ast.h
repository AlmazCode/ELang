/* ELang AST */
#ifndef ELANG_AST_H
#define ELANG_AST_H

#include "token.h"
#include <stddef.h>

typedef struct type_info type_info_t;

typedef enum {
    AST_INT_LIT, AST_FLOAT_LIT, AST_STRING_LIT, AST_CHAR_LIT, AST_BOOL_LIT,
    AST_IDENT, AST_BINARY_OP, AST_UNARY_OP, AST_CALL, AST_INDEX, AST_MEMBER,
    AST_EXPR_STMT, AST_RETURN, AST_IF, AST_WHILE, AST_FOR, AST_LOOP,
    AST_BREAK, AST_CONTINUE, AST_BLOCK, AST_LET, AST_ASSIGN, AST_RANGE,
    AST_FN_DECL, AST_STRUCT_DECL, AST_ENUM_DECL, AST_IMPORT_DECL,
    AST_MATCH, AST_STRUCT_LITERAL, AST_ENUM_LITERAL, AST_OK_EXPR, AST_ERR_EXPR,
    AST_PIPE, AST_DEFER, AST_WHEN, AST_TUPLE, AST_TUPLE_ASSIGN,
    AST_USING, AST_PROGRAM,
    AST_TRY_EXPR, AST_CATCH_EXPR, AST_PANIC_EXPR, AST_ASSERT_EXPR,
    AST_ARRAY_LITERAL, AST_LEN_EXPR,
} ast_type_t;

typedef struct ast_node ast_node_t;

struct ast_node {
    ast_type_t type;
    int line, col;
    type_info_t *typed;  /* resolved type from semantic analysis */
    union {
        long int_val;
        double float_val;
        struct { char *value; size_t length; } string_val;
        char char_val;
        int bool_val;
        struct { char *name; size_t name_len; } ident;
        struct { token_type_t op; ast_node_t *left, *right; } binary;
        struct { token_type_t op; ast_node_t *operand; } unary;
        struct { ast_node_t *callee; ast_node_t **args; int arg_count; } call;
        struct { ast_node_t *value; } ret;
        struct { ast_node_t *condition, *then_block, *else_block; } if_stmt;
        struct { ast_node_t *condition, *body; } while_stmt;
        struct { char *var; size_t var_len; ast_node_t *iterable, *body; } for_stmt;
        struct { ast_node_t **stmts; int count; } block;
        struct { char *name; size_t name_len; int is_mut; ast_node_t *type_expr, *value; } let;
        struct { ast_node_t *target, *value; } assign;
        struct {
            char *name; size_t name_len;
            struct { char *name; size_t name_len; ast_node_t *type_expr; } *params;
            int param_count;
            ast_node_t *return_type, *body;
            int is_export;
        } fn_decl;
        struct { ast_node_t *left, *right; } range;
        struct { char *name; size_t name_len; int field_count;
            struct { char *name; size_t name_len; ast_node_t *type_expr; } *fields; } struct_decl;
        struct { char *name; size_t name_len; int variant_count;
            struct { char *name; size_t name_len; ast_node_t *value; } *variants; } enum_decl;
        struct { char *path; size_t path_len; } import;
        struct { ast_node_t *value;
            struct { ast_node_t *pattern, *result; } *cases; int case_count; } match_expr;
        struct { char *name; size_t name_len;
            struct { char *name; size_t name_len; ast_node_t *value; } *fields; int field_count; } struct_literal;
        struct { char *enum_name; size_t enum_name_len; char *variant; size_t variant_len; } enum_literal;
        struct { ast_node_t *value; } ok_expr;
        struct { ast_node_t *value; } err_expr;
        struct { ast_node_t *left, *right; } pipe;
        struct { ast_node_t *expr; } defer_stmt;
        struct { ast_node_t *condition, *then_block, *else_block; } when_expr;
        struct { ast_node_t **elements; int count; } tuple;
        struct { char **names; int name_count; ast_node_t *value; } tuple_assign;
        struct { char *path; size_t path_len; } using_decl;
        struct { ast_node_t **declarations; int count; } program;
        struct { ast_node_t *operand; } try_expr;           /* expr? — error propagation */
        struct { ast_node_t *operand, *handler; } catch_expr; /* expr catch { handler } */
        struct { ast_node_t *message; } panic_expr;          /* panic(msg) */
        struct { ast_node_t *condition, *message; } assert_expr; /* assert(cond, msg) */
        struct { ast_node_t **elements; int count; } array_literal;  /* [1, 2, 3] */
        struct { ast_node_t *operand; } len_expr;                    /* arr.len */
    } as;
};

ast_node_t *ast_new(ast_type_t type, int line, int col);
void ast_free(ast_node_t *node);
void free_node(ast_node_t *node);

#endif
