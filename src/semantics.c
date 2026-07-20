/* ELang Type System & Semantic Analysis */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "semantics.h"
#include "config.h"  /* for MAX_IDENT_LEN */

/* ===== External Function Handling ===== */
/* The compiler doesn't know about library functions.
   All unresolved function calls are treated as extern.
   The linker resolves them at link time. */


/* ===== Type Info ===== */

type_info_t *type_new(type_kind_t kind) {
    type_info_t *t = SAFE_CALLOC(1, sizeof(type_info_t));
    t->kind = kind;
    t->array_len = -1;
    return t;
}

type_info_t *type_new_pointer(type_info_t *base) {
    type_info_t *t = type_new(TYPE_POINTER);
    t->base = base;
    return t;
}

type_info_t *type_new_array(type_info_t *base, int len) {
    type_info_t *t = type_new(TYPE_ARRAY);
    t->base = base;
    t->array_len = len;
    return t;
}

type_info_t *type_new_fn(type_info_t **params, int param_count, type_info_t *ret) {
    type_info_t *t = type_new(TYPE_FN);
    t->base = ret;
    t->fn.params = params;
    t->fn.count = param_count;
    return t;
}

type_info_t *type_new_result(type_info_t *ok_type, type_info_t *err_type) {
    type_info_t *t = type_new(TYPE_RESULT);
    t->result.ok_type = ok_type;
    t->result.err_type = err_type;
    return t;
}

type_info_t *type_copy(type_info_t *t) {
    if (!t) return NULL;
    type_info_t *c = type_new(t->kind);
    c->base = type_copy(t->base);
    c->array_len = t->array_len;
    if (t->fn.count > 0) {
        c->fn.count = t->fn.count;
        c->fn.params = SAFE_MALLOC(sizeof(type_info_t*) * t->fn.count);
        for (int i = 0; i < t->fn.count; i++)
            c->fn.params[i] = type_copy(t->fn.params[i]);
    }
    if (t->kind == TYPE_RESULT) {
        c->result.ok_type = type_copy(t->result.ok_type);
        c->result.err_type = type_copy(t->result.err_type);
    }
    if (t->struct_name) c->struct_name = SAFE_STRDUP(t->struct_name);
    return c;
}

int type_equal(type_info_t *a, type_info_t *b) {
    if (!a || !b) return a == b;
    /* TYPE_UNKNOWN matches any type (wildcard) */
    if (a->kind == TYPE_UNKNOWN || b->kind == TYPE_UNKNOWN) return 1;
    if (a->kind != b->kind) {
        /* Allow integer coercion: any int type is compatible with any other int type */
        if (type_is_integer(a) && type_is_integer(b)) return 1;
        return 0;
    }
    switch (a->kind) {
        case TYPE_POINTER: return type_equal(a->base, b->base);
        case TYPE_ARRAY:
            if (!type_equal(a->base, b->base)) return 0;
            return a->array_len == b->array_len || a->array_len == -1 || b->array_len == -1;
        case TYPE_FN:
            if (!type_equal(a->base, b->base)) return 0;
            if (a->fn.count != b->fn.count) return 0;
            for (int i = 0; i < a->fn.count; i++)
                if (!type_equal(a->fn.params[i], b->fn.params[i])) return 0;
            return 1;
        case TYPE_RESULT:
            if (!type_equal(a->result.ok_type, b->result.ok_type)) return 0;
            if (!type_equal(a->result.err_type, b->result.err_type)) return 0;
            return 1;
        case TYPE_ENUM:
            /* Enum types are equal if they have the same name */
            if (!a->struct_name || !b->struct_name) return 0;
            return strcmp(a->struct_name, b->struct_name) == 0;
        case TYPE_STRUCT:
            /* Struct types are equal if they have the same name */
            if (!a->struct_name || !b->struct_name) return 0;
            return strcmp(a->struct_name, b->struct_name) == 0;
        default: return 1;
    }
}

int type_is_numeric(type_info_t *t) {
    if (!t) return 0;
    return t->kind >= TYPE_I8 && t->kind <= TYPE_F64;
}

int type_is_integer(type_info_t *t) {
    if (!t) return 0;
    return (t->kind >= TYPE_I8 && t->kind <= TYPE_I64) ||
           (t->kind >= TYPE_U8 && t->kind <= TYPE_U64);
}

int type_is_signed(type_info_t *t) {
    if (!t) return 0;
    return t->kind >= TYPE_I8 && t->kind <= TYPE_I64;
}

const char *type_name_buf(type_info_t *t, char *buf, size_t size) {
    if (!t) { snprintf(buf, size, "?"); return buf; }
    switch (t->kind) {
        case TYPE_UNKNOWN: snprintf(buf, size, "?"); return buf;
        case TYPE_VOID: snprintf(buf, size, "void"); return buf;
        case TYPE_BOOL: snprintf(buf, size, "bool"); return buf;
        case TYPE_CHAR: snprintf(buf, size, "char"); return buf;
        case TYPE_I8: snprintf(buf, size, "i8"); return buf;
        case TYPE_I16: snprintf(buf, size, "i16"); return buf;
        case TYPE_I32: snprintf(buf, size, "i32"); return buf;
        case TYPE_I64: snprintf(buf, size, "i64"); return buf;
        case TYPE_U8: snprintf(buf, size, "u8"); return buf;
        case TYPE_U16: snprintf(buf, size, "u16"); return buf;
        case TYPE_U32: snprintf(buf, size, "u32"); return buf;
        case TYPE_U64: snprintf(buf, size, "u64"); return buf;
        case TYPE_F32: snprintf(buf, size, "f32"); return buf;
        case TYPE_F64: snprintf(buf, size, "f64"); return buf;
        case TYPE_STRING: snprintf(buf, size, "string"); return buf;
        case TYPE_POINTER: {
            char tmp[MAX_IDENT_LEN];
            type_name_buf(t->base, tmp, sizeof(tmp));
            snprintf(buf, size, "*%s", tmp);
            return buf;
        }
        case TYPE_ARRAY: {
            char tmp[MAX_IDENT_LEN];
            type_name_buf(t->base, tmp, sizeof(tmp));
            if (t->array_len >= 0)
                snprintf(buf, size, "[%s; %d]", tmp, t->array_len);
            else
                snprintf(buf, size, "[%s]", tmp);
            return buf;
        }
        case TYPE_FN: snprintf(buf, size, "fn"); return buf;
        case TYPE_STRUCT: snprintf(buf, size, "%s", t->struct_name ? t->struct_name : "struct"); return buf;
        case TYPE_ENUM: snprintf(buf, size, "%s", t->struct_name ? t->struct_name : "enum"); return buf;
        case TYPE_OK_RESULT: snprintf(buf, size, "Ok"); return buf;
        case TYPE_ERR_RESULT: snprintf(buf, size, "Err"); return buf;
        case TYPE_RESULT: {
            char ok_buf[64], err_buf[64];
            type_name_buf(t->result.ok_type, ok_buf, sizeof(ok_buf));
            type_name_buf(t->result.err_type, err_buf, sizeof(err_buf));
            snprintf(buf, size, "Result<%s, %s>", ok_buf, err_buf);
            return buf;
        }
    }
    snprintf(buf, size, "?");
    return buf;
}

const char *type_name(type_info_t *t) {
    if (!t) return "?";
    switch (t->kind) {
        case TYPE_UNKNOWN: return "?";
        case TYPE_VOID: return "void";
        case TYPE_BOOL: return "bool";
        case TYPE_CHAR: return "char";
        case TYPE_I8: return "i8"; case TYPE_I16: return "i16";
        case TYPE_I32: return "i32"; case TYPE_I64: return "i64";
        case TYPE_U8: return "u8"; case TYPE_U16: return "u16";
        case TYPE_U32: return "u32"; case TYPE_U64: return "u64";
        case TYPE_F32: return "f32"; case TYPE_F64: return "f64";
        case TYPE_STRING: return "string";
        case TYPE_FN: return "fn";
        case TYPE_STRUCT: return t->struct_name ? t->struct_name : "struct";
        case TYPE_ENUM: return t->struct_name ? t->struct_name : "enum";
        case TYPE_OK_RESULT: return "Ok";
        case TYPE_ERR_RESULT: return "Err";
        default: break;
    }
    return "?";
}

/* ===== Scope / Symbol Table ===== */

scope_t *scope_new(scope_t *parent) {
    scope_t *s = SAFE_CALLOC(1, sizeof(scope_t));
    s->parent = parent;
    s->scope_depth = parent ? parent->scope_depth + 1 : 0;
    return s;
}

void scope_free(scope_t *s) {
    if (!s) return;
    for (int i = 0; i < s->count; i++) {
        free(s->symbols[i].name);
        /* don't free type — it's owned by AST nodes */
    }
    free(s->symbols);
    free(s);
}

void scope_add(scope_t *s, const char *name, type_info_t *type, int is_mut) {
    if (s->count >= s->cap) {
        s->cap = s->cap ? s->cap * 2 : 16;
        s->symbols = SAFE_REALLOC(s->symbols, sizeof(symbol_t) * s->cap);
    }
    s->symbols[s->count].name = SAFE_STRDUP(name);
    s->symbols[s->count].type = type;
    s->symbols[s->count].stack_offset = 0;
    s->symbols[s->count].is_global = 0;
    s->symbols[s->count].is_mut = is_mut;
    s->count++;
}

symbol_t *scope_find(scope_t *s, const char *name) {
    for (scope_t *cur = s; cur; cur = cur->parent) {
        for (int i = cur->count - 1; i >= 0; i--) {
            if (strcmp(cur->symbols[i].name, name) == 0)
                return &cur->symbols[i];
        }
    }
    return NULL;
}

/* ===== Semantic Context ===== */

void sem_init(sem_ctx_t *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->current_scope = scope_new(NULL);
}

void sem_free(sem_ctx_t *ctx) {
    while (ctx->current_scope) {
        scope_t *parent = ctx->current_scope->parent;
        scope_free(ctx->current_scope);
        ctx->current_scope = parent;
    }
    for (int i = 0; i < ctx->error_count; i++) free(ctx->errors[i]);
    free(ctx->errors);
    for (int i = 0; i < ctx->fn_count; i++) {
        free(ctx->fn_table[i].name);
        free(ctx->fn_table[i].module);
    }
    free(ctx->fn_table);
}

void sem_error(sem_ctx_t *ctx, int line, int col, const char *fmt, ...) {
    char buf[512];
    va_list a;
    va_start(a, fmt);
    vsnprintf(buf, sizeof(buf), fmt, a);
    va_end(a);

    if (ctx->error_count >= ctx->error_cap) {
        ctx->error_cap = ctx->error_cap ? ctx->error_cap * 2 : 8;
        ctx->errors = SAFE_REALLOC(ctx->errors, sizeof(char*) * ctx->error_cap);
    }
    char *msg;
    asprintf(&msg, "Type error at %d:%d: %s", line, col, buf);
    ctx->errors[ctx->error_count++] = msg;
    fprintf(stderr, "%s\n", msg);
    ctx->has_errors = 1;
}

/* ===== Type from AST ===== */

type_info_t *sem_resolve_type(sem_ctx_t *ctx, ast_node_t *type_node) {
    if (!type_node) return NULL;
    switch (type_node->type) {
        case AST_IDENT: {
            /* resolve type name to type */
            char name[MAX_IDENT_LEN];
            snprintf(name, sizeof(name), "%.*s", (int)type_node->as.ident.name_len, type_node->as.ident.name);
            /* check built-in type names */
            if (strcmp(name, "void") == 0) return type_new(TYPE_VOID);
            if (strcmp(name, "bool") == 0) return type_new(TYPE_BOOL);
            if (strcmp(name, "char") == 0) return type_new(TYPE_CHAR);
            if (strcmp(name, "i8") == 0) return type_new(TYPE_I8);
            if (strcmp(name, "i16") == 0) return type_new(TYPE_I16);
            if (strcmp(name, "i32") == 0) return type_new(TYPE_I32);
            if (strcmp(name, "i64") == 0) return type_new(TYPE_I64);
            if (strcmp(name, "u8") == 0) return type_new(TYPE_U8);
            if (strcmp(name, "u16") == 0) return type_new(TYPE_U16);
            if (strcmp(name, "u32") == 0) return type_new(TYPE_U32);
            if (strcmp(name, "u64") == 0) return type_new(TYPE_U64);
            if (strcmp(name, "f32") == 0) return type_new(TYPE_F32);
            if (strcmp(name, "f64") == 0) return type_new(TYPE_F64);
            if (strcmp(name, "string") == 0) return type_new(TYPE_STRING);
            /* struct/enum types — check scope for registered types */
            symbol_t *sym = scope_find(ctx->current_scope, name);
            if (sym && sym->type && (sym->type->kind == TYPE_STRUCT || sym->type->kind == TYPE_ENUM)) {
                return type_copy(sym->type);
            }
            /* unknown type — create as struct for now */
            type_info_t *t = type_new(TYPE_STRUCT);
            t->struct_name = SAFE_STRDUP(name);
            return t;
        }
        case AST_BINARY_OP: {
            /* pointer type: *base */
            if (type_node->as.binary.op == TOKEN_STAR) {
                type_info_t *base = sem_resolve_type(ctx, type_node->as.binary.left);
                return type_new_pointer(base);
            }
            break;
        }
        case AST_RESULT_TYPE: {
            /* Result<T, E> type */
            type_info_t *ok = sem_resolve_type(ctx, type_node->as.result_type.ok_type);
            type_info_t *err = sem_resolve_type(ctx, type_node->as.result_type.err_type);
            return type_new_result(ok, err);
        }
        default: break;
    }
    return type_new(TYPE_UNKNOWN);
}

/* ===== Type Inference ===== */

type_info_t *sem_infer_expr(sem_ctx_t *ctx, ast_node_t *node) {
    if (!node) return NULL;

    switch (node->type) {
        case AST_INT_LIT: return type_new(TYPE_I64);
        case AST_FLOAT_LIT: return type_new(TYPE_F64);
        case AST_BOOL_LIT: return type_new(TYPE_BOOL);
        case AST_CHAR_LIT: return type_new(TYPE_CHAR);
        case AST_STRING_LIT: return type_new(TYPE_STRING);

        case AST_IDENT: {
            char name[MAX_IDENT_LEN];
            snprintf(name, sizeof(name), "%.*s", (int)node->as.ident.name_len, node->as.ident.name);
            symbol_t *sym = scope_find(ctx->current_scope, name);
            if (sym) return sym->type;
            return type_new(TYPE_UNKNOWN);
        }

        case AST_BINARY_OP: {
            /* Check for enum literal: Color::Red */
            if (node->as.binary.op == TOKEN_COLONCOLON &&
                node->as.binary.left->type == AST_IDENT) {
                char enum_name[MAX_IDENT_LEN];
                snprintf(enum_name, sizeof(enum_name), "%.*s",
                    (int)node->as.binary.left->as.ident.name_len,
                    node->as.binary.left->as.ident.name);
                /* Look up enum type in scope */
                symbol_t *sym = scope_find(ctx->current_scope, enum_name);
                if (sym && sym->type && sym->type->kind == TYPE_ENUM) {
                    return type_copy(sym->type);
                }
                return type_new(TYPE_I64); /* fallback */
            }
            type_info_t *left = sem_infer_expr(ctx, node->as.binary.left);
            type_info_t *right = sem_infer_expr(ctx, node->as.binary.right);
            if (!left || !right) return type_new(TYPE_UNKNOWN);

            /* comparison operators always return bool */
            switch (node->as.binary.op) {
                case TOKEN_EQ: case TOKEN_NEQ:
                case TOKEN_LT: case TOKEN_GT:
                case TOKEN_LTE: case TOKEN_GTE:
                case TOKEN_AND: case TOKEN_OR:
                    return type_new(TYPE_BOOL);
                default: break;
            }

            /* arithmetic: use left operand's type */
            if (type_is_numeric(left) && type_is_numeric(right)) return type_copy(left);
            /* string concatenation */
            if (left->kind == TYPE_STRING && right->kind == TYPE_STRING) return type_copy(left);
            /* range */
            if (node->as.binary.op == TOKEN_DOTDOT) return type_copy(left);
            return type_new(TYPE_UNKNOWN);
        }

        case AST_CALL: {
            /* look up function type */
            if (node->as.call.callee->type == AST_IDENT) {
                char name[MAX_IDENT_LEN];
                snprintf(name, sizeof(name), "%.*s",
                    (int)node->as.call.callee->as.ident.name_len,
                    node->as.call.callee->as.ident.name);
                symbol_t *sym = scope_find(ctx->current_scope, name);
                if (sym && sym->type && sym->type->kind == TYPE_FN) {
                    return type_copy(sym->type->base); /* return type */
                }
                /* function not found — treat as extern (linker will resolve) */
                return type_new(TYPE_UNKNOWN);
            }
            /* handle module::func() calls */
            if (node->as.call.callee->type == AST_BINARY_OP &&
                node->as.call.callee->as.binary.op == TOKEN_COLONCOLON) {
                ast_node_t *mod = node->as.call.callee->as.binary.left;
                ast_node_t *fn = node->as.call.callee->as.binary.right;
                /* build renamed name: module_func */
                char name[128];
                snprintf(name, sizeof(name), "%.*s_%.*s",
                    (int)mod->as.ident.name_len, mod->as.ident.name,
                    (int)fn->as.ident.name_len, fn->as.ident.name);
                symbol_t *sym = scope_find(ctx->current_scope, name);
                if (sym && sym->type && sym->type->kind == TYPE_FN) {
                    return type_copy(sym->type->base);
                }
            }
            return type_new(TYPE_UNKNOWN);
        }

        case AST_OK_EXPR: {
            /* Ok(val) — returns Result<T, E> where T is val's type */
            type_info_t *val_type = node->as.ok_expr.value
                ? sem_infer_expr(ctx, node->as.ok_expr.value)
                : type_new(TYPE_UNKNOWN);
            return type_new_result(val_type ? type_copy(val_type) : type_new(TYPE_UNKNOWN),
                                   type_new(TYPE_UNKNOWN));
        }

        case AST_ERR_EXPR: {
            /* Err(val) — returns Result<T, E> where E is val's type */
            type_info_t *val_type = node->as.err_expr.value
                ? sem_infer_expr(ctx, node->as.err_expr.value)
                : type_new(TYPE_UNKNOWN);
            return type_new_result(type_new(TYPE_UNKNOWN),
                                   val_type ? type_copy(val_type) : type_new(TYPE_UNKNOWN));
        }

        case AST_MATCH: {
            /* Match expression returns the type of the case bodies */
            if (node->as.match_expr.case_count > 0) {
                return sem_infer_expr(ctx, node->as.match_expr.cases[0].result);
            }
            return type_new(TYPE_UNKNOWN);
        }
        case AST_PIPE: {
            if (node->as.pipe.right->type == AST_CALL)
                return sem_infer_expr(ctx, node->as.pipe.right);
            return type_new(TYPE_UNKNOWN);
        }

        case AST_TRY_EXPR: {
            /* expr? — returns the inner Ok type */
            type_info_t *inner = sem_infer_expr(ctx, node->as.try_expr.operand);
            if (inner && inner->kind == TYPE_RESULT) {
                /* Unwrap Result<T, E> → T */
                return inner->result.ok_type ? type_copy(inner->result.ok_type) : type_new(TYPE_UNKNOWN);
            }
            return inner ? type_copy(inner) : type_new(TYPE_UNKNOWN);
        }

        case AST_CATCH_EXPR: {
            /* expr catch { handler } — returns handler's type */
            return sem_infer_expr(ctx, node->as.catch_expr.handler);
        }

        case AST_PANIC_EXPR: {
            /* panic(msg) — returns void (never returns actually) */
            return type_new(TYPE_VOID);
        }

        case AST_ASSERT_EXPR: {
            /* assert(cond, msg) — returns void */
            return type_new(TYPE_VOID);
        }

        case AST_TUPLE: {
            /* tuple type */
            type_info_t *t = type_new(TYPE_UNKNOWN);
            if (node->as.tuple.count > 0)
                t = sem_infer_expr(ctx, node->as.tuple.elements[0]);
            return t; /* simplified: return first element type */
        }

        case AST_ARRAY_LITERAL: {
            /* [1, 2, 3] — infer type from first element, count from count */
            if (node->as.array_literal.count == 0)
                return type_new_array(type_new(TYPE_I64), 0);
            type_info_t *elem_type = sem_infer_expr(ctx, node->as.array_literal.elements[0]);
            return type_new_array(elem_type ? type_copy(elem_type) : type_new(TYPE_I64),
                                  node->as.array_literal.count);
        }

        case AST_INDEX: {
            /* arr[i] — return element type of the array */
            type_info_t *arr_type = sem_infer_expr(ctx, node->as.binary.left);
            if (arr_type && arr_type->kind == TYPE_ARRAY && arr_type->base)
                return type_copy(arr_type->base);
            return type_new(TYPE_UNKNOWN);
        }

        case AST_LEN_EXPR: {
            /* arr.len — return i64 */
            sem_infer_expr(ctx, node->as.len_expr.operand);
            return type_new(TYPE_I64);
        }

        case AST_BLOCK: {
            if (node->as.block.count == 0) return type_new(TYPE_VOID);
            return sem_infer_expr(ctx, node->as.block.stmts[node->as.block.count - 1]);
        }

        default: return type_new(TYPE_UNKNOWN);
    }
}

/* ===== Constant Folding ===== */

void sem_fold_constants(ast_node_t *node) {
    if (!node) return;

    /* fold children first */
    switch (node->type) {
        case AST_BINARY_OP:
            sem_fold_constants(node->as.binary.left);
            sem_fold_constants(node->as.binary.right);
            /* fold int+int */
            if (node->as.binary.left->type == AST_INT_LIT &&
                node->as.binary.right->type == AST_INT_LIT) {
                long a = node->as.binary.left->as.int_val;
                long b = node->as.binary.right->as.int_val;
                long result = 0;
                switch (node->as.binary.op) {
                    case TOKEN_PLUS: result = a + b; break;
                    case TOKEN_MINUS: result = a - b; break;
                    case TOKEN_STAR: result = a * b; break;
                    case TOKEN_SLASH: if (b == 0) return; result = a / b; break;
                    case TOKEN_PERCENT: if (b == 0) return; result = a % b; break;
                    case TOKEN_EQ: result = a == b; break;
                    case TOKEN_NEQ: result = a != b; break;
                    case TOKEN_LT: result = a < b; break;
                    case TOKEN_GT: result = a > b; break;
                    case TOKEN_LTE: result = a <= b; break;
                    case TOKEN_GTE: result = a >= b; break;
                    default: return;
                }
                /* save children before overwriting the union */
                ast_node_t *left = node->as.binary.left;
                ast_node_t *right = node->as.binary.right;
                node->type = AST_INT_LIT;
                node->as.int_val = result;
                free_node(left);
                free_node(right);
            }
            /* fold string + string */
            if (node->as.binary.op == TOKEN_PLUS &&
                node->as.binary.left->type == AST_STRING_LIT &&
                node->as.binary.right->type == AST_STRING_LIT) {
                size_t l1 = node->as.binary.left->as.string_val.length;
                size_t l2 = node->as.binary.right->as.string_val.length;
                char *buf = SAFE_MALLOC(l1 + l2 + 1);
                memcpy(buf, node->as.binary.left->as.string_val.value, l1);
                memcpy(buf + l1, node->as.binary.right->as.string_val.value, l2);
                buf[l1 + l2] = '\0';
                free(node->as.binary.left->as.string_val.value);
                free(node->as.binary.right->as.string_val.value);
                /* save children before overwriting the union */
                ast_node_t *left = node->as.binary.left;
                ast_node_t *right = node->as.binary.right;
                node->type = AST_STRING_LIT;
                node->as.string_val.value = buf;
                node->as.string_val.length = l1 + l2;
                free_node(left);
                free_node(right);
            }
            break;
        case AST_IF:
            sem_fold_constants(node->as.if_stmt.condition);
            sem_fold_constants(node->as.if_stmt.then_block);
            sem_fold_constants(node->as.if_stmt.else_block);
            /* fold if(true) { a } else { b } → a */
            if (node->as.if_stmt.condition->type == AST_BOOL_LIT) {
                if (node->as.if_stmt.condition->as.bool_val) {
                    if (node->as.if_stmt.then_block) {
                        ast_node_t *blk = node->as.if_stmt.then_block;
                        if (blk->type == AST_BLOCK && blk->as.block.count > 0) {
                            /* replace with last expression */
                            ast_node_t *last = blk->as.block.stmts[blk->as.block.count - 1];
                            blk->as.block.stmts[blk->as.block.count - 1] = NULL;
                            blk->as.block.count--;
                            free_node(node->as.if_stmt.condition);
                            free_node(node->as.if_stmt.else_block);
                            free_node(blk); /* frees remaining statements + the block itself */
                            *node = *last;
                            free(last);
                        }
                    }
                }
            }
            break;
        case AST_RETURN: sem_fold_constants(node->as.ret.value); break;
        case AST_BLOCK:
            for (int i = 0; i < node->as.block.count; i++)
                sem_fold_constants(node->as.block.stmts[i]);
            break;
        case AST_LET: sem_fold_constants(node->as.let.value); break;
        case AST_ASSIGN: sem_fold_constants(node->as.assign.value); break;
        case AST_CALL:
            for (int i = 0; i < node->as.call.arg_count; i++)
                sem_fold_constants(node->as.call.args[i]);
            break;
        case AST_PIPE:
            sem_fold_constants(node->as.pipe.left);
            sem_fold_constants(node->as.pipe.right);
            break;
        case AST_TRY_EXPR: sem_fold_constants(node->as.try_expr.operand); break;
        case AST_CATCH_EXPR:
            sem_fold_constants(node->as.catch_expr.operand);
            sem_fold_constants(node->as.catch_expr.handler);
            break;
        case AST_PANIC_EXPR: sem_fold_constants(node->as.panic_expr.message); break;
        case AST_ASSERT_EXPR:
            sem_fold_constants(node->as.assert_expr.condition);
            sem_fold_constants(node->as.assert_expr.message);
            break;
        case AST_DEFER: sem_fold_constants(node->as.defer_stmt.expr); break;
        case AST_ARRAY_LITERAL:
            for (int i = 0; i < node->as.array_literal.count; i++)
                sem_fold_constants(node->as.array_literal.elements[i]);
            break;
        case AST_LEN_EXPR: sem_fold_constants(node->as.len_expr.operand); break;
        case AST_MATCH:
            sem_fold_constants(node->as.match_expr.value);
            for (int i = 0; i < node->as.match_expr.case_count; i++) {
                sem_fold_constants(node->as.match_expr.cases[i].pattern);
                sem_fold_constants(node->as.match_expr.cases[i].result);
            }
            break;
        case AST_FN_DECL:
            sem_fold_constants(node->as.fn_decl.body);
            break;
        case AST_RESULT_TYPE:
            sem_fold_constants(node->as.result_type.ok_type);
            sem_fold_constants(node->as.result_type.err_type);
            break;
        case AST_PROGRAM:
            for (int i = 0; i < node->as.program.count; i++)
                sem_fold_constants(node->as.program.declarations[i]);
            break;
        default: break;
    }
}

/* ===== Semantic Analysis ===== */

static void sem_stmt(sem_ctx_t *ctx, ast_node_t *node);
static void sem_block(sem_ctx_t *ctx, ast_node_t *block);

static void sem_block(sem_ctx_t *ctx, ast_node_t *block) {
    if (!block || block->type != AST_BLOCK) return;
    scope_t *old = ctx->current_scope;
    ctx->current_scope = scope_new(old);
    for (int i = 0; i < block->as.block.count; i++)
        sem_stmt(ctx, block->as.block.stmts[i]);
    scope_t *tmp = ctx->current_scope;
    ctx->current_scope = old;
    scope_free(tmp);
}

static void sem_stmt(sem_ctx_t *ctx, ast_node_t *node) {
    if (!node) return;
    switch (node->type) {
        case AST_LET: {
            type_info_t *val_type = NULL;
            if (node->as.let.value) {
                val_type = sem_infer_expr(ctx, node->as.let.value);
                sem_fold_constants(node->as.let.value);
                if (node->as.let.value->type == AST_INT_LIT)
                    val_type = type_new(TYPE_I64);
            }
            type_info_t *ann_type = NULL;
            if (node->as.let.type_expr)
                ann_type = sem_resolve_type(ctx, node->as.let.type_expr);
            type_info_t *final_type = ann_type ? ann_type : val_type;
            if (!final_type) final_type = type_new(TYPE_UNKNOWN);
            /* check annotation matches value */
            if (ann_type && val_type && !type_equal(ann_type, val_type)) {
                if (val_type->kind != TYPE_UNKNOWN) {
                    sem_error(ctx, node->line, node->col,
                        "type mismatch: expected %s, got %s",
                        type_name_buf(ann_type, (char[128]){0}, 128),
                        type_name_buf(val_type, (char[128]){0}, 128));
                }
            }
            scope_add(ctx->current_scope, node->as.let.name, final_type, 1);
            break;
        }
        case AST_ASSIGN: {
            type_info_t *val_type = sem_infer_expr(ctx, node->as.assign.value);
            sem_fold_constants(node->as.assign.value);
            if (node->as.assign.target->type == AST_IDENT) {
                char name[MAX_IDENT_LEN];
                snprintf(name, sizeof(name), "%.*s",
                    (int)node->as.assign.target->as.ident.name_len,
                    node->as.assign.target->as.ident.name);
                symbol_t *sym = scope_find(ctx->current_scope, name);
                if (!sym) {
                    sem_error(ctx, node->line, node->col, "undefined variable '%s'", name);
                } else if (!sym->is_mut) {
                    sem_error(ctx, node->line, node->col, "cannot reassign immutable variable '%s'", name);
                } else if (sym->type && val_type && !type_equal(sym->type, val_type)) {
                    if (val_type->kind != TYPE_UNKNOWN)
                        sem_error(ctx, node->line, node->col,
                            "type mismatch: expected %s, got %s",
                            type_name_buf(sym->type, (char[128]){0}, 128),
                            type_name_buf(val_type, (char[128]){0}, 128));
                }
            }
            break;
        }
        case AST_RETURN: {
            if (node->as.ret.value) {
                type_info_t *ret_type = sem_infer_expr(ctx, node->as.ret.value);
                if (ctx->current_fn_return && ret_type && !type_equal(ctx->current_fn_return, ret_type)) {
                    if (ret_type->kind != TYPE_UNKNOWN)
                        sem_error(ctx, node->line, node->col,
                            "return type mismatch: expected %s, got %s",
                            type_name_buf(ctx->current_fn_return, (char[128]){0}, 128),
                            type_name_buf(ret_type, (char[128]){0}, 128));
                }
            }
            break;
        }
        case AST_FN_DECL: {
            type_info_t *ret_type = NULL;
            if (node->as.fn_decl.return_type)
                ret_type = sem_resolve_type(ctx, node->as.fn_decl.return_type);

            /* build function type */
            type_info_t **param_types = SAFE_MALLOC(sizeof(type_info_t*) * node->as.fn_decl.param_count);
            for (int i = 0; i < node->as.fn_decl.param_count; i++) {
                param_types[i] = node->as.fn_decl.params[i].type_expr
                    ? sem_resolve_type(ctx, node->as.fn_decl.params[i].type_expr)
                    : type_new(TYPE_UNKNOWN);
            }
            type_info_t *fn_type = type_new_fn(param_types, node->as.fn_decl.param_count, ret_type);

            /* add to scope */
            scope_add(ctx->current_scope, node->as.fn_decl.name, fn_type, 0);

            /* add to fn_table for cross-module */
            if (ctx->fn_count >= ctx->fn_cap) {
                ctx->fn_cap = ctx->fn_cap ? ctx->fn_cap * 2 : 16;
                ctx->fn_table = SAFE_REALLOC(ctx->fn_table, sizeof(*ctx->fn_table) * ctx->fn_cap);
            }
            ctx->fn_table[ctx->fn_count].name = SAFE_STRDUP(node->as.fn_decl.name);
            ctx->fn_table[ctx->fn_count].module = NULL;
            ctx->fn_table[ctx->fn_count].type = fn_type;
            ctx->fn_table[ctx->fn_count].is_export = 0;
            ctx->fn_count++;

            /* analyze function body */
            type_info_t *saved_ret = ctx->current_fn_return;
            ctx->current_fn_return = ret_type;
            scope_t *old = ctx->current_scope;
            ctx->current_scope = scope_new(old);
            /* add params to scope */
            for (int i = 0; i < node->as.fn_decl.param_count; i++) {
                scope_add(ctx->current_scope,
                    node->as.fn_decl.params[i].name,
                    param_types[i], 1);
            }
            sem_block(ctx, node->as.fn_decl.body);
            scope_t *tmp = ctx->current_scope;
            ctx->current_scope = old;
            scope_free(tmp);
            ctx->current_fn_return = saved_ret;
            break;
        }
        case AST_IF:
            sem_fold_constants(node->as.if_stmt.condition);
            sem_block(ctx, node->as.if_stmt.then_block);
            if (node->as.if_stmt.else_block) {
                if (node->as.if_stmt.else_block->type == AST_IF)
                    sem_stmt(ctx, node->as.if_stmt.else_block);
                else
                    sem_block(ctx, node->as.if_stmt.else_block);
            }
            break;
        case AST_WHILE:
            sem_fold_constants(node->as.while_stmt.condition);
            sem_block(ctx, node->as.while_stmt.body);
            break;
        case AST_FOR:
            sem_fold_constants(node->as.for_stmt.iterable);
            {
                scope_t *old = ctx->current_scope;
                ctx->current_scope = scope_new(old);
                scope_add(ctx->current_scope, node->as.for_stmt.vars[0], type_new(TYPE_I64), 1);
                if (node->as.for_stmt.var_count > 1)
                    scope_add(ctx->current_scope, node->as.for_stmt.vars[1], type_new(TYPE_UNKNOWN), 1);
                sem_block(ctx, node->as.for_stmt.body);
                scope_t *tmp = ctx->current_scope;
                ctx->current_scope = old;
                scope_free(tmp);
            }
            break;
        case AST_MATCH: {
            sem_fold_constants(node->as.match_expr.value);
            type_info_t *val_type = sem_infer_expr(ctx, node->as.match_expr.value);
            for (int i = 0; i < node->as.match_expr.case_count; i++) {
                sem_fold_constants(node->as.match_expr.cases[i].pattern);
                sem_fold_constants(node->as.match_expr.cases[i].result);
            }
            (void)val_type;
            break;
        }
        case AST_DEFER:
            sem_fold_constants(node->as.defer_stmt.expr);
            break;
        case AST_TRY_EXPR:
            sem_fold_constants(node->as.try_expr.operand);
            break;
        case AST_CATCH_EXPR:
            sem_fold_constants(node->as.catch_expr.operand);
            sem_block(ctx, node->as.catch_expr.handler);
            break;
        case AST_PANIC_EXPR:
            sem_fold_constants(node->as.panic_expr.message);
            break;
        case AST_ASSERT_EXPR:
            sem_fold_constants(node->as.assert_expr.condition);
            sem_fold_constants(node->as.assert_expr.message);
            break;
        case AST_STRUCT_DECL: {
            /* Register struct fields in type system */
            type_info_t *t = type_new(TYPE_STRUCT);
            t->struct_name = SAFE_STRNDUP(node->as.struct_decl.name, node->as.struct_decl.name_len);
            t->fields.count = node->as.struct_decl.field_count;
            t->fields.names = SAFE_MALLOC(sizeof(char*) * t->fields.count);
            t->fields.offsets = SAFE_MALLOC(sizeof(int) * t->fields.count);
            t->fields.types = SAFE_MALLOC(sizeof(type_info_t*) * t->fields.count);
            for (int i = 0; i < t->fields.count; i++) {
                t->fields.names[i] = SAFE_STRNDUP(
                    node->as.struct_decl.fields[i].name,
                    node->as.struct_decl.fields[i].name_len);
                t->fields.offsets[i] = i * 8;  /* each field is 8 bytes */
                t->fields.types[i] = node->as.struct_decl.fields[i].type_expr
                    ? sem_resolve_type(ctx, node->as.struct_decl.fields[i].type_expr)
                    : type_new(TYPE_UNKNOWN);
            }
            /* Register in scope */
            scope_add(ctx->current_scope, t->struct_name, t, 0);
            break;
        }
        case AST_ENUM_DECL: {
            /* Register enum type in type system */
            type_info_t *t = type_new(TYPE_ENUM);
            t->struct_name = SAFE_STRNDUP(node->as.enum_decl.name, node->as.enum_decl.name_len);
            /* Register in scope */
            scope_add(ctx->current_scope, t->struct_name, t, 0);
            break;
        }
        case AST_IMPORT_DECL:
            /* Import nodes are resolved and removed before sem_analyze.
             * This case exists only for safety if imports remain. */
            break;
        case AST_CALL:
            /* check function calls for unknown functions */
            sem_infer_expr(ctx, node);
            break;
        case AST_BLOCK:
            sem_block(ctx, node);
            break;
        default:
            sem_fold_constants(node);
            break;
    }
}

void sem_analyze(sem_ctx_t *ctx, ast_node_t *program) {
    if (!program || program->type != AST_PROGRAM) return;
    for (int i = 0; i < program->as.program.count; i++)
        sem_stmt(ctx, program->as.program.declarations[i]);
}
