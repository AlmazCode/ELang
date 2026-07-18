/* ELang Type System & Semantic Analysis */
#ifndef ELANG_SEMANTICS_H
#define ELANG_SEMANTICS_H

#include "ast.h"

/* --- Type Kinds --- */
typedef enum {
    TYPE_UNKNOWN, TYPE_VOID, TYPE_BOOL, TYPE_CHAR,
    TYPE_I8, TYPE_I16, TYPE_I32, TYPE_I64,
    TYPE_U8, TYPE_U16, TYPE_U32, TYPE_U64,
    TYPE_F32, TYPE_F64,
    TYPE_STRING, TYPE_POINTER, TYPE_ARRAY,
    TYPE_FN, TYPE_STRUCT, TYPE_ENUM,
    TYPE_OK_RESULT, TYPE_ERR_RESULT, TYPE_RESULT,
} type_kind_t;

/* --- Type Info --- */
typedef struct type_info type_info_t;

struct type_info {
    type_kind_t kind;
    type_info_t *base;             /* pointer/array element type, fn return type */
    int array_len;                 /* -1 = dynamic */
    struct {
        type_info_t **params;
        int count;
    } fn;
    struct {
        type_info_t *ok_type;      /* Result<T, E> — T */
        type_info_t *err_type;     /* Result<T, E> — E */
    } result;
    char *struct_name;             /* for named struct/enum types */
};

type_info_t *type_new(type_kind_t kind);
type_info_t *type_new_pointer(type_info_t *base);
type_info_t *type_new_array(type_info_t *base, int len);
type_info_t *type_new_fn(type_info_t **params, int param_count, type_info_t *ret);
type_info_t *type_new_result(type_info_t *ok_type, type_info_t *err_type);
type_info_t *type_copy(type_info_t *t);
int type_equal(type_info_t *a, type_info_t *b);
int type_is_numeric(type_info_t *t);
int type_is_integer(type_info_t *t);
int type_is_signed(type_info_t *t);
const char *type_name(type_info_t *t);
const char *type_name_buf(type_info_t *t, char *buf, size_t size);

/* --- Symbol Table --- */
typedef struct {
    char *name;
    type_info_t *type;
    int stack_offset;
    int is_global;
    int is_mut;
} symbol_t;

typedef struct scope {
    symbol_t *symbols;
    int count, cap;
    struct scope *parent;
    int scope_depth;
} scope_t;

scope_t *scope_new(scope_t *parent);
void scope_free(scope_t *s);
void scope_add(scope_t *s, const char *name, type_info_t *type, int is_mut);
symbol_t *scope_find(scope_t *s, const char *name);

/* --- Semantic Context --- */
typedef struct {
    scope_t *current_scope;
    type_info_t *current_fn_return;  /* current function's return type */
    int has_errors;
    char **errors;
    int error_count, error_cap;
    /* function table for cross-module resolution */
    struct { char *name; char *module; type_info_t *type; int is_export; } *fn_table;
    int fn_count, fn_cap;
    int std_imported;  /* 1 if using "std" was encountered */
} sem_ctx_t;

void sem_init(sem_ctx_t *ctx);
void sem_free(sem_ctx_t *ctx);
void sem_error(sem_ctx_t *ctx, int line, int col, const char *fmt, ...);

/* --- Main Analysis --- */
void sem_analyze(sem_ctx_t *ctx, ast_node_t *program);

/* --- Type Inference --- */
type_info_t *sem_infer_expr(sem_ctx_t *ctx, ast_node_t *node);
type_info_t *sem_resolve_type(sem_ctx_t *ctx, ast_node_t *type_node);

/* --- Constant Folding --- */
void sem_fold_constants(ast_node_t *node);

#endif
