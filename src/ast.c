/* ELang AST */
#include <stdlib.h>
#include <string.h>
#include "ast.h"

ast_node_t *ast_new(ast_type_t type, int line, int col) {
    ast_node_t *n = calloc(1, sizeof(ast_node_t));
    n->type = type; n->line = line; n->col = col; n->typed = NULL;
    return n;
}

void free_node(ast_node_t *n) {
    if (!n) return;
    switch (n->type) {
        case AST_STRING_LIT: free(n->as.string_val.value); break;
        case AST_IDENT: free(n->as.ident.name); break;
        case AST_BINARY_OP: free_node(n->as.binary.left); free_node(n->as.binary.right); break;
        case AST_UNARY_OP: free_node(n->as.unary.operand); break;
        case AST_RANGE: free_node(n->as.range.left); free_node(n->as.range.right); break;
        case AST_PIPE: free_node(n->as.pipe.left); free_node(n->as.pipe.right); break;
        case AST_DEFER: free_node(n->as.defer_stmt.expr); break;
        case AST_TUPLE: for (int i = 0; i < n->as.tuple.count; i++) free_node(n->as.tuple.elements[i]);
            free(n->as.tuple.elements); break;
        case AST_TUPLE_ASSIGN: for (int i = 0; i < n->as.tuple_assign.name_count; i++) free(n->as.tuple_assign.names[i]);
            free(n->as.tuple_assign.names); free_node(n->as.tuple_assign.value); break;
        case AST_USING: free(n->as.using_decl.path); break;
        case AST_TRY_EXPR: free_node(n->as.try_expr.operand); break;
        case AST_CATCH_EXPR: free_node(n->as.catch_expr.operand); free_node(n->as.catch_expr.handler); break;
        case AST_PANIC_EXPR: free_node(n->as.panic_expr.message); break;
        case AST_ASSERT_EXPR: free_node(n->as.assert_expr.condition); free_node(n->as.assert_expr.message); break;
        case AST_ARRAY_LITERAL: for (int i = 0; i < n->as.array_literal.count; i++) free_node(n->as.array_literal.elements[i]);
            free(n->as.array_literal.elements); break;
        case AST_LEN_EXPR: free_node(n->as.len_expr.operand); break;
        case AST_CALL: free_node(n->as.call.callee);
            for (int i = 0; i < n->as.call.arg_count; i++) free_node(n->as.call.args[i]);
            free(n->as.call.args); break;
        case AST_RETURN: free_node(n->as.ret.value); break;
        case AST_IF: free_node(n->as.if_stmt.condition); free_node(n->as.if_stmt.then_block);
            free_node(n->as.if_stmt.else_block); break;
        case AST_WHILE: free_node(n->as.while_stmt.condition); free_node(n->as.while_stmt.body); break;
        case AST_FOR: free(n->as.for_stmt.var); free_node(n->as.for_stmt.iterable);
            free_node(n->as.for_stmt.body); break;
        case AST_BLOCK: for (int i = 0; i < n->as.block.count; i++) free_node(n->as.block.stmts[i]);
            free(n->as.block.stmts); break;
        case AST_LET: free(n->as.let.name); free_node(n->as.let.type_expr); free_node(n->as.let.value); break;
        case AST_ASSIGN: free_node(n->as.assign.target); free_node(n->as.assign.value); break;
        case AST_FN_DECL: free(n->as.fn_decl.name);
            for (int i = 0; i < n->as.fn_decl.param_count; i++) {
                free(n->as.fn_decl.params[i].name); free_node(n->as.fn_decl.params[i].type_expr); }
            free(n->as.fn_decl.params); free_node(n->as.fn_decl.return_type); free_node(n->as.fn_decl.body); break;
        case AST_STRUCT_DECL: free(n->as.struct_decl.name);
            for (int i = 0; i < n->as.struct_decl.field_count; i++) {
                free(n->as.struct_decl.fields[i].name); free_node(n->as.struct_decl.fields[i].type_expr); }
            free(n->as.struct_decl.fields); break;
        case AST_ENUM_DECL: free(n->as.enum_decl.name);
            for (int i = 0; i < n->as.enum_decl.variant_count; i++) {
                free(n->as.enum_decl.variants[i].name); free_node(n->as.enum_decl.variants[i].value); }
            free(n->as.enum_decl.variants); break;
        case AST_IMPORT_DECL: free(n->as.import.path); break;
        case AST_MATCH: free_node(n->as.match_expr.value);
            for (int i = 0; i < n->as.match_expr.case_count; i++) {
                free_node(n->as.match_expr.cases[i].pattern); free_node(n->as.match_expr.cases[i].result); }
            free(n->as.match_expr.cases); break;
        case AST_STRUCT_LITERAL: free(n->as.struct_literal.name);
            for (int i = 0; i < n->as.struct_literal.field_count; i++) {
                free(n->as.struct_literal.fields[i].name); free_node(n->as.struct_literal.fields[i].value); }
            free(n->as.struct_literal.fields); break;
        case AST_ENUM_LITERAL: free(n->as.enum_literal.enum_name); free(n->as.enum_literal.variant); break;
        case AST_OK_EXPR: free_node(n->as.ok_expr.value); break;
        case AST_ERR_EXPR: free_node(n->as.err_expr.value); break;
        case AST_RESULT_TYPE: free_node(n->as.result_type.ok_type); free_node(n->as.result_type.err_type); break;
        case AST_PROGRAM: for (int i = 0; i < n->as.program.count; i++) free_node(n->as.program.declarations[i]);
            free(n->as.program.declarations); break;
        default: break;
    }
    free(n);
}

void ast_free(ast_node_t *n) { free_node(n); }
