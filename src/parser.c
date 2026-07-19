/* ELang Parser */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "codegen.h"  /* for MAX_IDENT_LEN, MAX_PARSE_DEPTH */

#define ELANG_MAX_PARSE_DEPTH 2000

static void advance(parser_t *p) { p->current = p->peek; p->peek = lexer_next_token(&p->lexer); }
static int match(parser_t *p, token_type_t t) { if (p->current.type == t) { advance(p); return 1; } return 0; }
static int expect(parser_t *p, token_type_t t) {
    if (p->current.type == t) { advance(p); return 1; }
    fprintf(stderr, "Parse error at %d:%d: expected %s, got %s\n",
            p->current.line, p->current.col, token_type_name(t), token_type_name(p->current.type));
    p->has_error = 1;
    return 0;
}
static char *tok_str(token_t *t) { char *s = malloc(t->length + 1); memcpy(s, t->value, t->length); s[t->length] = '\0'; return s; }

/* parse a simple type identifier (i64, string, etc.) without expression parsing */
static ast_node_t *parse_type_ident(parser_t *p) {
    token_t t = p->current;
    switch (t.type) {
        case TOKEN_I8: case TOKEN_I16: case TOKEN_I32: case TOKEN_I64:
        case TOKEN_U8: case TOKEN_U16: case TOKEN_U32: case TOKEN_U64:
        case TOKEN_F32: case TOKEN_F64: case TOKEN_BOOL: case TOKEN_CHAR:
        case TOKEN_STRING: case TOKEN_VOID: case TOKEN_NIL: case TOKEN_ERROR_TYPE:
        case TOKEN_IDENT: {
            ast_node_t *n = ast_new(AST_IDENT, t.line, t.col);
            n->as.ident.name = tok_str(&t); n->as.ident.name_len = t.length;
            advance(p);
            return n;
        }
        default:
            fprintf(stderr, "Parse error at %d:%d: expected type, got %s\n",
                    t.line, t.col, token_type_name(t.type));
            advance(p);
            return ast_new(AST_IDENT, t.line, t.col);
    }
}

static ast_node_t *parse_expr(parser_t *p);
static ast_node_t *parse_stmt(parser_t *p);
static ast_node_t *parse_block(parser_t *p);
static ast_node_t *parse_match(parser_t *p);

static void skip_nl(parser_t *p) { while (p->current.type == TOKEN_NEWLINE) advance(p); }

static ast_node_t *parse_primary(parser_t *p) {
    token_t t = p->current;
    switch (t.type) {
        case TOKEN_INT_LIT: { ast_node_t *n = ast_new(AST_INT_LIT, t.line, t.col);
            n->as.int_val = strtol(t.value, NULL, 0); advance(p); return n; }
        case TOKEN_FLOAT_LIT: { ast_node_t *n = ast_new(AST_FLOAT_LIT, t.line, t.col);
            n->as.float_val = strtod(t.value, NULL); advance(p); return n; }
        case TOKEN_STRING_LIT: { ast_node_t *n = ast_new(AST_STRING_LIT, t.line, t.col);
            n->as.string_val.value = tok_str(&t); n->as.string_val.length = t.length; advance(p); return n; }
        case TOKEN_TRUE: { ast_node_t *n = ast_new(AST_BOOL_LIT, t.line, t.col); n->as.bool_val = 1; advance(p); return n; }
        case TOKEN_FALSE: { ast_node_t *n = ast_new(AST_BOOL_LIT, t.line, t.col); n->as.bool_val = 0; advance(p); return n; }
        case TOKEN_I8: case TOKEN_I16: case TOKEN_I32: case TOKEN_I64:
        case TOKEN_U8: case TOKEN_U16: case TOKEN_U32: case TOKEN_U64:
        case TOKEN_F32: case TOKEN_F64: case TOKEN_BOOL: case TOKEN_CHAR:
        case TOKEN_STRING: case TOKEN_VOID: case TOKEN_NIL: case TOKEN_ERROR_TYPE: {
            ast_node_t *n = ast_new(AST_IDENT, t.line, t.col);
            n->as.ident.name = tok_str(&t); n->as.ident.name_len = t.length; advance(p); return n; }
        case TOKEN_IDENT: {
            ast_node_t *n = ast_new(AST_IDENT, t.line, t.col);
            n->as.ident.name = tok_str(&t); n->as.ident.name_len = t.length; advance(p);
            /* handle module::name qualified access */
            if (p->current.type == TOKEN_COLONCOLON) {
                advance(p);
                if (p->current.type == TOKEN_IDENT) {
                    ast_node_t *member = ast_new(AST_IDENT, p->current.line, p->current.col);
                    member->as.ident.name = tok_str(&p->current);
                    member->as.ident.name_len = p->current.length;
                    advance(p);
                    ast_node_t *qual = ast_new(AST_BINARY_OP, t.line, t.col);
                    qual->as.binary.op = TOKEN_COLONCOLON;
                    qual->as.binary.left = n; qual->as.binary.right = member;
                    n = qual;
                }
            }
            if (p->current.type == TOKEN_LPAREN) {
                ast_node_t *call = ast_new(AST_CALL, t.line, t.col);
                call->as.call.callee = n; call->as.call.args = NULL; call->as.call.arg_count = 0;
                advance(p);
                while (p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) {
                    if (call->as.call.arg_count > 0) expect(p, TOKEN_COMMA);
                    ast_node_t *arg = parse_expr(p);
                    if (arg) { call->as.call.arg_count++;
                        call->as.call.args = realloc(call->as.call.args, sizeof(void*) * call->as.call.arg_count);
                        call->as.call.args[call->as.call.arg_count - 1] = arg; }
                }
                expect(p, TOKEN_RPAREN);
                /* handle postfix ? after calls */
                if (p->current.type == TOKEN_QUESTION) {
                    advance(p);
                    ast_node_t *try_n = ast_new(AST_TRY_EXPR, t.line, t.col);
                    try_n->as.try_expr.operand = call;
                    return try_n;
                }
                return call;
            }
            /* handle postfix ? after qualified access */
            if (p->current.type == TOKEN_QUESTION) {
                advance(p);
                ast_node_t *try_n = ast_new(AST_TRY_EXPR, t.line, t.col);
                try_n->as.try_expr.operand = n;
                return try_n;
            }
            return n;
        }
        case TOKEN_OK: { ast_node_t *n = ast_new(AST_OK_EXPR, t.line, t.col); advance(p);
            if (match(p, TOKEN_LPAREN)) { n->as.ok_expr.value = parse_expr(p); expect(p, TOKEN_RPAREN); }
            return n; }
        case TOKEN_ERR: { ast_node_t *n = ast_new(AST_ERR_EXPR, t.line, t.col); advance(p);
            if (match(p, TOKEN_LPAREN)) { n->as.err_expr.value = parse_expr(p); expect(p, TOKEN_RPAREN); }
            return n; }
        case TOKEN_MINUS: {
            advance(p);
            ast_node_t *operand = parse_primary(p);
            ast_node_t *n = ast_new(AST_BINARY_OP, t.line, t.col);
            n->as.binary.op = TOKEN_MINUS;
            ast_node_t *zero = ast_new(AST_INT_LIT, t.line, t.col);
            zero->as.int_val = 0;
            n->as.binary.left = zero; n->as.binary.right = operand;
            return n;
        }
        case TOKEN_LPAREN: {
            advance(p);
            if (p->current.type == TOKEN_RPAREN) { advance(p); return ast_new(AST_BLOCK, t.line, t.col); }
            ast_node_t *first = parse_expr(p);
            if (match(p, TOKEN_COMMA)) {
                /* tuple: (a, b, c) */
                ast_node_t *n = ast_new(AST_TUPLE, t.line, t.col);
                n->as.tuple.elements = malloc(sizeof(void*) * 2);
                n->as.tuple.elements[0] = first; n->as.tuple.count = 1;
                do { skip_nl(p);
                    ast_node_t *el = parse_expr(p);
                    n->as.tuple.count++;
                    n->as.tuple.elements = realloc(n->as.tuple.elements, sizeof(void*) * n->as.tuple.count);
                    n->as.tuple.elements[n->as.tuple.count - 1] = el;
                } while (match(p, TOKEN_COMMA));
                expect(p, TOKEN_RPAREN); return n;
            }
            expect(p, TOKEN_RPAREN); return first;
        }
        case TOKEN_LBRACKET: {
            /* array literal: [1, 2, 3] */
            ast_node_t *n = ast_new(AST_ARRAY_LITERAL, t.line, t.col);
            n->as.array_literal.elements = NULL; n->as.array_literal.count = 0;
            advance(p);
            while (p->current.type != TOKEN_RBRACKET && p->current.type != TOKEN_EOF) {
                if (n->as.array_literal.count > 0) expect(p, TOKEN_COMMA);
                skip_nl(p);
                ast_node_t *el = parse_expr(p);
                if (el) {
                    n->as.array_literal.count++;
                    n->as.array_literal.elements = realloc(n->as.array_literal.elements, sizeof(void*) * n->as.array_literal.count);
                    n->as.array_literal.elements[n->as.array_literal.count - 1] = el;
                }
            }
            expect(p, TOKEN_RBRACKET);
            return n;
        }
        case TOKEN_MATCH: return parse_match(p);
        case TOKEN_IF: {
            /* if expression: if cond { val } else { val } or if cond { val } else expr */
            ast_node_t *n = ast_new(AST_IF, t.line, t.col);
            advance(p);
            n->as.if_stmt.condition = parse_expr(p);
            skip_nl(p);
            n->as.if_stmt.then_block = parse_block(p);
            skip_nl(p);
            if (match(p, TOKEN_ELSE)) {
                skip_nl(p);
                if (p->current.type == TOKEN_IF)
                    n->as.if_stmt.else_block = parse_primary(p);
                else
                    n->as.if_stmt.else_block = parse_block(p);
            }
            return n;
        }
        case TOKEN_PANIC: {
            ast_node_t *n = ast_new(AST_PANIC_EXPR, t.line, t.col);
            advance(p); expect(p, TOKEN_LPAREN);
            n->as.panic_expr.message = parse_expr(p);
            expect(p, TOKEN_RPAREN); return n; }
        case TOKEN_ASSERT: {
            ast_node_t *n = ast_new(AST_ASSERT_EXPR, t.line, t.col);
            advance(p); expect(p, TOKEN_LPAREN);
            n->as.assert_expr.condition = parse_expr(p);
            expect(p, TOKEN_COMMA);
            n->as.assert_expr.message = parse_expr(p);
            expect(p, TOKEN_RPAREN); return n; }
        case TOKEN_FN: {
            /* fn x => expr  or  fn(x) => expr — closure expression */
            advance(p); /* consume 'fn' */
            ast_node_t *n = ast_new(AST_CLOSURE, t.line, t.col);
            n->as.closure.params = NULL;
            n->as.closure.param_lens = NULL;
            n->as.closure.param_count = 0;
            n->as.closure.captures = NULL;
            n->as.closure.capture_lens = NULL;
            n->as.closure.capture_count = 0;
            /* parse parameters: fn x => ... or fn(x) => ... */
            if (p->current.type == TOKEN_LPAREN) {
                advance(p); /* skip ( */
                while (p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) {
                    if (n->as.closure.param_count > 0) expect(p, TOKEN_COMMA);
                    n->as.closure.param_count++;
                    n->as.closure.params = realloc(n->as.closure.params, sizeof(char*) * n->as.closure.param_count);
                    n->as.closure.param_lens = realloc(n->as.closure.param_lens, sizeof(size_t) * n->as.closure.param_count);
                    n->as.closure.params[n->as.closure.param_count - 1] = tok_str(&p->current);
                    n->as.closure.param_lens[n->as.closure.param_count - 1] = p->current.length;
                    advance(p);
                }
                expect(p, TOKEN_RPAREN);
            } else {
                /* single param without parens: fn x => ... */
                n->as.closure.param_count = 1;
                n->as.closure.params = malloc(sizeof(char*));
                n->as.closure.param_lens = malloc(sizeof(size_t));
                n->as.closure.params[0] = tok_str(&p->current);
                n->as.closure.param_lens[0] = p->current.length;
                advance(p);
            }
            expect(p, TOKEN_FAT_ARROW);
            skip_nl(p);
            n->as.closure.body = parse_expr(p);
            return n; }
        case TOKEN_ERROR:
            fprintf(stderr, "Parse error at %d:%d: invalid token\n", t.line, t.col);
            p->has_error = 1;
            advance(p); return NULL;
        default: fprintf(stderr, "Parse error at %d:%d: unexpected %s\n", t.line, t.col, token_type_name(t.type));
            p->has_error = 1;
            advance(p); return NULL;
    }
}

static ast_node_t *parse_binary(parser_t *p, int min_prec) {
    if (++p->depth > ELANG_MAX_PARSE_DEPTH) {
        fprintf(stderr, "Parse error at %d:%d: expression nested too deeply\n",
                p->current.line, p->current.col);
        p->has_error = 1;
        p->depth--;
        return NULL;
    }
    ast_node_t *left = parse_primary(p);
    /* handle postfix index access: expr[expr] */
    while (left && p->current.type == TOKEN_LBRACKET) {
        advance(p);
        ast_node_t *index = parse_expr(p);
        expect(p, TOKEN_RBRACKET);
        ast_node_t *n = ast_new(AST_INDEX, left->line, left->col);
        n->as.binary.left = left;
        n->as.binary.right = index;
        n->as.binary.op = TOKEN_LBRACKET;
        left = n;
    }
    /* handle postfix .field, .method(), and .len */
    while (left && p->current.type == TOKEN_DOT &&
           p->peek.type == TOKEN_IDENT) {
        advance(p); /* dot */
        if (p->current.length == 3 && memcmp(p->current.value, "len", 3) == 0 &&
            p->peek.type != TOKEN_LPAREN) {
            /* .len — array length */
            advance(p);
            ast_node_t *n = ast_new(AST_LEN_EXPR, left->line, left->col);
            n->as.len_expr.operand = left;
            left = n;
        } else if (p->peek.type == TOKEN_LPAREN) {
            /* .method(args) — method call: method(obj, args...) */
            ast_node_t *method_name = ast_new(AST_IDENT, p->current.line, p->current.col);
            method_name->as.ident.name = tok_str(&p->current);
            method_name->as.ident.name_len = p->current.length;
            advance(p); /* method name */
            advance(p); /* ( */
            /* Build arg list: [object, arg0, arg1, ...] */
            ast_node_t *call = ast_new(AST_CALL, left->line, left->col);
            call->as.call.callee = method_name;
            call->as.call.args = NULL;
            call->as.call.arg_count = 0;
            /* first arg = the object (self) */
            call->as.call.arg_count = 1;
            call->as.call.args = malloc(sizeof(ast_node_t*));
            call->as.call.args[0] = left;
            /* parse remaining args */
            while (p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) {
                if (call->as.call.arg_count > 1) expect(p, TOKEN_COMMA);
                ast_node_t *arg = parse_expr(p);
                if (arg) {
                    call->as.call.arg_count++;
                    call->as.call.args = realloc(call->as.call.args,
                        sizeof(ast_node_t*) * call->as.call.arg_count);
                    call->as.call.args[call->as.call.arg_count - 1] = arg;
                }
            }
            expect(p, TOKEN_RPAREN);
            left = call;
        } else {
            /* .field — member access */
            ast_node_t *n = ast_new(AST_MEMBER, left->line, left->col);
            n->as.member.object = left;
            n->as.member.field = tok_str(&p->current);
            n->as.member.field_len = p->current.length;
            advance(p);
            left = n;
        }
    }
    while (1) {
        token_type_t op = p->current.type; int prec = 0;
        switch (op) {
            case TOKEN_PIPE_ARROW: prec=1; break;
            case TOKEN_OR: prec=2; break; case TOKEN_AND: prec=3; break;
            case TOKEN_EQ: case TOKEN_NEQ:
            case TOKEN_LT: case TOKEN_GT: case TOKEN_LTE: case TOKEN_GTE:
            case TOKEN_DOTDOT: prec=4; break;
            case TOKEN_PLUS: case TOKEN_MINUS: prec=5; break;
            case TOKEN_STAR: case TOKEN_SLASH: case TOKEN_PERCENT: prec=6; break;
            case TOKEN_COLONCOLON: prec=7; break;
            default: p->depth--; return left;
        }
        if (prec < min_prec) { p->depth--; return left; }
        advance(p);
        ast_node_t *right = parse_binary(p, prec + 1);
        if (op == TOKEN_DOTDOT) {
            ast_node_t *n = ast_new(AST_RANGE, op, 0);
            n->as.range.left = left; n->as.range.right = right;
            p->depth--;
            return n;
        }
        if (op == TOKEN_PIPE_ARROW) {
            ast_node_t *n = ast_new(AST_PIPE, op, 0);
            n->as.pipe.left = left; n->as.pipe.right = right;
            left = n;
            continue;
        }
        ast_node_t *n = ast_new(AST_BINARY_OP, op, 0);
        n->as.binary.op = op; n->as.binary.left = left; n->as.binary.right = right;
        left = n;
    }
}

static ast_node_t *parse_expr(parser_t *p) {
    ast_node_t *e = parse_binary(p, 0);
    if (e && p->current.type == TOKEN_ASSIGN) {
        advance(p);
        ast_node_t *v = parse_expr(p);
        ast_node_t *a = ast_new(AST_ASSIGN, e->line, e->col);
        a->as.assign.target = e; a->as.assign.value = v; return a;
    }
    /* handle postfix: expr catch { handler } */
    if (e && p->current.type == TOKEN_CATCH) {
        advance(p); skip_nl(p);
        ast_node_t *handler = parse_block(p);
        ast_node_t *c = ast_new(AST_CATCH_EXPR, e->line, e->col);
        c->as.catch_expr.operand = e;
        c->as.catch_expr.handler = handler;
        return c;
    }
    return e;
}

static ast_node_t *parse_let(parser_t *p) {
    ast_node_t *n = ast_new(AST_LET, p->current.line, p->current.col);
    advance(p); n->as.let.is_mut = match(p, TOKEN_MUT);
    if (p->current.type == TOKEN_LPAREN) {
        /* tuple unpacking: let (a, b) = ... */
        advance(p);
        ast_node_t *tup = ast_new(AST_TUPLE_ASSIGN, p->current.line, p->current.col);
        tup->as.tuple_assign.names = NULL; tup->as.tuple_assign.name_count = 0;
        while (p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) {
            if (tup->as.tuple_assign.name_count > 0) expect(p, TOKEN_COMMA);
            tup->as.tuple_assign.name_count++;
            tup->as.tuple_assign.names = realloc(tup->as.tuple_assign.names, sizeof(char*) * tup->as.tuple_assign.name_count);
            tup->as.tuple_assign.names[tup->as.tuple_assign.name_count - 1] = tok_str(&p->current);
            advance(p);
        }
        expect(p, TOKEN_RPAREN);
        expect(p, TOKEN_ASSIGN);
        tup->as.tuple_assign.value = parse_expr(p);
        /* also populate the LET node with the tuple assign as its value */
        n->as.let.name = strdup("_tuple_"); n->as.let.name_len = 7;
        n->as.let.value = tup;
        return n;
    }
    n->as.let.name = tok_str(&p->current); n->as.let.name_len = p->current.length; advance(p);
    if (!match(p, TOKEN_COLON)) {
        fprintf(stderr, "Parse error at %d:%d: expected ':' after variable name '%.*s'\n",
            p->current.line, p->current.col, (int)n->as.let.name_len, n->as.let.name);
        p->has_error = 1;
    }
    n->as.let.type_expr = parse_binary(p, 0);
    if (match(p, TOKEN_ASSIGN)) n->as.let.value = parse_expr(p);
    return n;
}

static ast_node_t *parse_if(parser_t *p) {
    ast_node_t *n = ast_new(AST_IF, p->current.line, p->current.col);
    advance(p); n->as.if_stmt.condition = parse_expr(p);
    skip_nl(p); n->as.if_stmt.then_block = parse_block(p);
    skip_nl(p);
    if (match(p, TOKEN_ELSE)) { skip_nl(p);
        n->as.if_stmt.else_block = p->current.type == TOKEN_IF ? parse_if(p) : parse_block(p); }
    return n;
}

static ast_node_t *parse_while(parser_t *p) {
    ast_node_t *n = ast_new(AST_WHILE, p->current.line, p->current.col);
    advance(p); n->as.while_stmt.condition = parse_expr(p);
    skip_nl(p); n->as.while_stmt.body = parse_block(p); return n;
}

static ast_node_t *parse_for(parser_t *p) {
    ast_node_t *n = ast_new(AST_FOR, p->current.line, p->current.col);
    advance(p); /* consume 'for' keyword */

    /* parse variable names with types: "x: i64" or "i: i64, x: i64" */
    n->as.for_stmt.vars = NULL;
    n->as.for_stmt.var_lens = NULL;
    n->as.for_stmt.var_types = NULL;
    n->as.for_stmt.var_count = 0;

    /* parse first variable */
    n->as.for_stmt.var_count = 1;
    n->as.for_stmt.vars = malloc(sizeof(char*));
    n->as.for_stmt.var_lens = malloc(sizeof(size_t));
    n->as.for_stmt.var_types = malloc(sizeof(ast_node_t*));
    n->as.for_stmt.vars[0] = tok_str(&p->current);
    n->as.for_stmt.var_lens[0] = p->current.length;
    advance(p);
    if (!match(p, TOKEN_COLON)) {
        fprintf(stderr, "Parse error at %d:%d: expected ':' after variable name\n",
            p->current.line, p->current.col);
        p->has_error = 1;
    }
    n->as.for_stmt.var_types[0] = parse_binary(p, 0);

    /* optional second variable for enumerate: for i: i64, x: i64 in arr */
    if (p->current.type == TOKEN_COMMA) {
        advance(p);
        n->as.for_stmt.var_count = 2;
        n->as.for_stmt.vars = realloc(n->as.for_stmt.vars, sizeof(char*) * 2);
        n->as.for_stmt.var_lens = realloc(n->as.for_stmt.var_lens, sizeof(size_t) * 2);
        n->as.for_stmt.var_types = realloc(n->as.for_stmt.var_types, sizeof(ast_node_t*) * 2);
        n->as.for_stmt.vars[1] = tok_str(&p->current);
        n->as.for_stmt.var_lens[1] = p->current.length;
        advance(p);
        if (!match(p, TOKEN_COLON)) {
            fprintf(stderr, "Parse error at %d:%d: expected ':' after variable name\n",
                p->current.line, p->current.col);
            p->has_error = 1;
        }
        n->as.for_stmt.var_types[1] = parse_binary(p, 0);
    }

    if (!expect(p, TOKEN_IN)) return n;
    n->as.for_stmt.iterable = parse_expr(p);
    skip_nl(p);
    /* support => for single-expression for bodies */
    if (p->current.type == TOKEN_FAT_ARROW) {
        advance(p); skip_nl(p);
        ast_node_t *body_stmt = parse_stmt(p);
        ast_node_t *blk = ast_new(AST_BLOCK, body_stmt->line, body_stmt->col);
        blk->as.block.stmts = malloc(sizeof(void*));
        blk->as.block.stmts[0] = body_stmt; blk->as.block.count = 1;
        n->as.for_stmt.body = blk;
    } else {
        n->as.for_stmt.body = parse_block(p);
    }
    return n;
}

static ast_node_t *parse_match(parser_t *p) {
    ast_node_t *n = ast_new(AST_MATCH, p->current.line, p->current.col);
    advance(p); n->as.match_expr.value = parse_expr(p);
    skip_nl(p); expect(p, TOKEN_LBRACE);
    n->as.match_expr.cases = NULL; n->as.match_expr.case_count = 0;
    skip_nl(p);
    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        skip_nl(p); if (p->current.type == TOKEN_RBRACE) break;
        ast_node_t *pat = parse_expr(p); skip_nl(p);
        if (!match(p, TOKEN_FAT_ARROW)) { free_node(pat); continue; }
        skip_nl(p); ast_node_t *res = parse_expr(p);
        n->as.match_expr.case_count++;
        n->as.match_expr.cases = realloc(n->as.match_expr.cases,
            sizeof(struct { ast_node_t *pattern, *result; }) * n->as.match_expr.case_count);
        n->as.match_expr.cases[n->as.match_expr.case_count - 1].pattern = pat;
        n->as.match_expr.cases[n->as.match_expr.case_count - 1].result = res;
        skip_nl(p); match(p, TOKEN_COMMA); skip_nl(p);
    }
    expect(p, TOKEN_RBRACE); return n;
}

static ast_node_t *parse_fn_decl(parser_t *p) {
    ast_node_t *n = ast_new(AST_FN_DECL, p->current.line, p->current.col);
    advance(p); n->as.fn_decl.name = tok_str(&p->current); n->as.fn_decl.name_len = p->current.length;
    advance(p); expect(p, TOKEN_LPAREN);
    n->as.fn_decl.params = NULL; n->as.fn_decl.param_count = 0;
    while (p->current.type != TOKEN_RPAREN) {
        if (n->as.fn_decl.param_count > 0) expect(p, TOKEN_COMMA);
        if (p->current.type == TOKEN_IDENT) {
            n->as.fn_decl.param_count++;
            n->as.fn_decl.params = realloc(n->as.fn_decl.params,
                sizeof(struct { char *name; size_t name_len; ast_node_t *type_expr; }) * n->as.fn_decl.param_count);
            int last = n->as.fn_decl.param_count - 1;
            n->as.fn_decl.params[last].name = tok_str(&p->current); n->as.fn_decl.params[last].name_len = p->current.length;
            advance(p);
            n->as.fn_decl.params[last].type_expr = match(p, TOKEN_COLON) ? parse_expr(p) : NULL;
        }
    }
    expect(p, TOKEN_RPAREN);
    /* parse return type: stop before => */
    if (match(p, TOKEN_ARROW)) {
        skip_nl(p);
        if (p->current.type != TOKEN_FAT_ARROW) {
            /* check for Result<T, E> */
            if (p->current.type == TOKEN_RESULT) {
                ast_node_t *n_result = ast_new(AST_RESULT_TYPE, p->current.line, p->current.col);
                advance(p); /* consume Result */
                expect(p, TOKEN_LT);
                n_result->as.result_type.ok_type = parse_type_ident(p);
                expect(p, TOKEN_COMMA);
                n_result->as.result_type.err_type = parse_type_ident(p);
                expect(p, TOKEN_GT);
                n->as.fn_decl.return_type = n_result;
            } else {
                n->as.fn_decl.return_type = parse_expr(p);
            }
        }
    }
    skip_nl(p);
    /* support => for single-expression function bodies */
    if (p->current.type == TOKEN_FAT_ARROW) {
        advance(p); skip_nl(p);
        ast_node_t *body_stmt = parse_stmt(p);
        ast_node_t *blk = ast_new(AST_BLOCK, body_stmt->line, body_stmt->col);
        blk->as.block.stmts = malloc(sizeof(void*));
        /* wrap expression statements in implicit return */
        if (body_stmt->type != AST_RETURN && body_stmt->type != AST_LET &&
            body_stmt->type != AST_WHILE && body_stmt->type != AST_FOR &&
            body_stmt->type != AST_STRUCT_DECL &&
            body_stmt->type != AST_ENUM_DECL && body_stmt->type != AST_IMPORT_DECL &&
            body_stmt->type != AST_DEFER && body_stmt->type != AST_ASSIGN &&
            body_stmt->type != AST_FN_DECL && body_stmt->type != AST_BLOCK) {
            ast_node_t *ret = ast_new(AST_RETURN, body_stmt->line, body_stmt->col);
            ret->as.ret.value = body_stmt;
            body_stmt = ret;
        }
        blk->as.block.stmts[0] = body_stmt; blk->as.block.count = 1;
        n->as.fn_decl.body = blk;
    } else {
        ast_node_t *body = parse_block(p);
        /* implicit return: if last stmt is an expression and return type is not void, wrap in return */
        int is_void = 0;
        if (n->as.fn_decl.return_type && n->as.fn_decl.return_type->type == AST_IDENT) {
            char rname[MAX_IDENT_LEN];
            snprintf(rname, sizeof(rname), "%.*s",
                (int)n->as.fn_decl.return_type->as.ident.name_len,
                n->as.fn_decl.return_type->as.ident.name);
            if (strcmp(rname, "void") == 0) is_void = 1;
        }
        if (!is_void && body && body->as.block.count > 0) {
            ast_node_t *last = body->as.block.stmts[body->as.block.count - 1];
            if (last && last->type != AST_RETURN && last->type != AST_LET &&
                last->type != AST_WHILE && last->type != AST_FOR &&
                last->type != AST_STRUCT_DECL && last->type != AST_ENUM_DECL &&
                last->type != AST_IMPORT_DECL && last->type != AST_DEFER &&
                last->type != AST_ASSIGN && last->type != AST_FN_DECL) {
                ast_node_t *ret = ast_new(AST_RETURN, last->line, last->col);
                ret->as.ret.value = last;
                body->as.block.stmts[body->as.block.count - 1] = ret;
            }
        }
        n->as.fn_decl.body = body;
    }
    /* Validate main: must have -> u8 return type */
    if (n->as.fn_decl.name_len == 4 && memcmp(n->as.fn_decl.name, "main", 4) == 0) {
        if (!n->as.fn_decl.return_type) {
            fprintf(stderr, "Parse error at %d:%d: fn main must have a return type: fn main() -> u8\n",
                n->line, n->col);
            p->has_error = 1;
        } else if (n->as.fn_decl.return_type->type == AST_IDENT) {
            char rname[MAX_IDENT_LEN];
            snprintf(rname, sizeof(rname), "%.*s",
                (int)n->as.fn_decl.return_type->as.ident.name_len,
                n->as.fn_decl.return_type->as.ident.name);
            if (strcmp(rname, "u8") != 0) {
                fprintf(stderr, "Parse error at %d:%d: fn main must return u8, not %s\n",
                    n->line, n->col, rname);
                p->has_error = 1;
            }
        } else {
            fprintf(stderr, "Parse error at %d:%d: fn main must return u8\n",
                n->line, n->col);
            p->has_error = 1;
        }
    }
    return n;
}

static ast_node_t *parse_struct(parser_t *p) {
    ast_node_t *n = ast_new(AST_STRUCT_DECL, p->current.line, p->current.col);
    advance(p); n->as.struct_decl.name = tok_str(&p->current); n->as.struct_decl.name_len = p->current.length;
    advance(p); expect(p, TOKEN_LBRACE);
    n->as.struct_decl.fields = NULL; n->as.struct_decl.field_count = 0;
    skip_nl(p);
    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        skip_nl(p); if (p->current.type == TOKEN_RBRACE) break;
        n->as.struct_decl.field_count++;
        n->as.struct_decl.fields = realloc(n->as.struct_decl.fields,
            sizeof(struct { char *name; size_t name_len; ast_node_t *type_expr; }) * n->as.struct_decl.field_count);
        int last = n->as.struct_decl.field_count - 1;
        n->as.struct_decl.fields[last].name = tok_str(&p->current); n->as.struct_decl.fields[last].name_len = p->current.length;
        advance(p);
        if (match(p, TOKEN_COLON)) {
            n->as.struct_decl.fields[last].type_expr = parse_expr(p);
        } else {
            n->as.struct_decl.fields[last].type_expr = NULL;
        }
        skip_nl(p); match(p, TOKEN_COMMA); skip_nl(p);
    }
    expect(p, TOKEN_RBRACE); return n;
}

static ast_node_t *parse_import(parser_t *p) {
    ast_node_t *n = ast_new(AST_IMPORT_DECL, p->current.line, p->current.col);
    advance(p); n->as.import.path = tok_str(&p->current); n->as.import.path_len = p->current.length;
    advance(p); return n;
}

static ast_node_t *parse_enum(parser_t *p) {
    ast_node_t *n = ast_new(AST_ENUM_DECL, p->current.line, p->current.col);
    advance(p); /* consume 'enum' */
    n->as.enum_decl.name = tok_str(&p->current); n->as.enum_decl.name_len = p->current.length;
    advance(p);
    expect(p, TOKEN_LBRACE);
    n->as.enum_decl.variants = NULL; n->as.enum_decl.variant_count = 0;
    skip_nl(p);
    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        skip_nl(p);
        if (p->current.type == TOKEN_RBRACE) break;
        if (n->as.enum_decl.variant_count > 0) {
            /* accept comma or newline as separator */
            if (p->current.type == TOKEN_COMMA) advance(p);
            skip_nl(p);
        }
        if (p->current.type == TOKEN_RBRACE) break;
        /* parse variant: Name or Name(Type) or Name = value */
        n->as.enum_decl.variant_count++;
        n->as.enum_decl.variants = realloc(n->as.enum_decl.variants,
            sizeof(struct { char *name; size_t name_len; ast_node_t *value; })
            * n->as.enum_decl.variant_count);
        int last = n->as.enum_decl.variant_count - 1;
        n->as.enum_decl.variants[last].name = tok_str(&p->current);
        n->as.enum_decl.variants[last].name_len = p->current.length;
        advance(p);
        n->as.enum_decl.variants[last].value = NULL;
        if (match(p, TOKEN_LPAREN)) {
            /* variant with data: Some(i64) */
            n->as.enum_decl.variants[last].value = parse_type_ident(p);
            expect(p, TOKEN_RPAREN);
        } else if (match(p, TOKEN_ASSIGN)) {
            /* manual ordinal: Jan = 1 */
            n->as.enum_decl.variants[last].value = parse_expr(p);
        }
    }
    expect(p, TOKEN_RBRACE);
    return n;
}

static ast_node_t *parse_impl(parser_t *p) {
    ast_node_t *n = ast_new(AST_IMPL_DECL, p->current.line, p->current.col);
    advance(p); /* consume 'impl' */
    n->as.impl_decl.type_name = tok_str(&p->current);
    n->as.impl_decl.type_name_len = p->current.length;
    advance(p);
    expect(p, TOKEN_LBRACE);
    n->as.impl_decl.methods = NULL; n->as.impl_decl.method_count = 0;
    skip_nl(p);
    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        skip_nl(p);
        if (p->current.type == TOKEN_RBRACE) break;
        if (p->current.type == TOKEN_FN) {
            ast_node_t *method = parse_fn_decl(p);
            if (method) {
                n->as.impl_decl.method_count++;
                n->as.impl_decl.methods = realloc(n->as.impl_decl.methods,
                    sizeof(ast_node_t*) * n->as.impl_decl.method_count);
                n->as.impl_decl.methods[n->as.impl_decl.method_count - 1] = method;
            }
        } else {
            fprintf(stderr, "Parse error at %d:%d: expected 'fn' in impl block\n",
                p->current.line, p->current.col);
            p->has_error = 1;
            break;
        }
    }
    expect(p, TOKEN_RBRACE);
    return n;
}

static ast_node_t *parse_export(parser_t *p) {
    advance(p);
    if (p->current.type == TOKEN_FN) {
        ast_node_t *n = parse_fn_decl(p);
        if (n) n->as.fn_decl.is_export = 1;
        return n;
    }
    return NULL;
}

static ast_node_t *parse_block(parser_t *p) {
    skip_nl(p); expect(p, TOKEN_LBRACE); skip_nl(p);
    ast_node_t *n = ast_new(AST_BLOCK, p->current.line, p->current.col);
    n->as.block.stmts = NULL; n->as.block.count = 0;
    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        skip_nl(p); if (p->current.type == TOKEN_RBRACE) break;
        ast_node_t *s = parse_stmt(p);
        if (s) { n->as.block.count++;
            n->as.block.stmts = realloc(n->as.block.stmts, sizeof(void*) * n->as.block.count);
            n->as.block.stmts[n->as.block.count - 1] = s; }
    }
    expect(p, TOKEN_RBRACE); return n;
}

static ast_node_t *parse_stmt(parser_t *p) {
    skip_nl(p);
    switch (p->current.type) {
        case TOKEN_LET: return parse_let(p);
        case TOKEN_IF: return parse_if(p);
        case TOKEN_WHILE: return parse_while(p);
        case TOKEN_FOR: return parse_for(p);
        case TOKEN_MATCH: return parse_match(p);
        case TOKEN_FN: return parse_fn_decl(p);
        case TOKEN_STRUCT: return parse_struct(p);
        case TOKEN_ENUM: return parse_enum(p);
        case TOKEN_IMPL: return parse_impl(p);
        case TOKEN_IMPORT: return parse_import(p);
        case TOKEN_EXPORT: return parse_export(p);
        case TOKEN_USING: {
            ast_node_t *n = ast_new(AST_USING, p->current.line, p->current.col);
            advance(p);
            if (p->current.type == TOKEN_STRING_LIT) {
                n->as.using_decl.path = tok_str(&p->current);
                n->as.using_decl.path_len = p->current.length;
                advance(p);
            }
            return n;
        }
        case TOKEN_DEFER: {
            ast_node_t *n = ast_new(AST_DEFER, p->current.line, p->current.col);
            advance(p); skip_nl(p);
            n->as.defer_stmt.expr = parse_expr(p);
            return n;
        }
        case TOKEN_RETURN: {
            ast_node_t *n = ast_new(AST_RETURN, p->current.line, p->current.col);
            advance(p); skip_nl(p);
            if (p->current.type != TOKEN_SEMICOLON && p->current.type != TOKEN_RBRACE &&
                p->current.type != TOKEN_NEWLINE && p->current.type != TOKEN_EOF)
                n->as.ret.value = parse_expr(p);
            return n;
        }
        case TOKEN_LBRACE: return parse_block(p);
        case TOKEN_NEWLINE: advance(p); return NULL;
        default: return parse_expr(p);
    }
}

void parser_init(parser_t *p, const char *source) {
    lexer_init(&p->lexer, source); p->current = lexer_next_token(&p->lexer); p->peek = lexer_next_token(&p->lexer);
    p->has_error = 0; p->depth = 0;
}

ast_node_t *parser_parse(parser_t *p) {
    ast_node_t *prog = ast_new(AST_PROGRAM, 1, 1);
    prog->as.program.declarations = NULL; prog->as.program.count = 0;
    while (p->current.type != TOKEN_EOF && !p->has_error) {
        skip_nl(p); if (p->current.type == TOKEN_EOF) break;
        ast_node_t *d = parse_stmt(p);
        if (d) { prog->as.program.count++;
            prog->as.program.declarations = realloc(prog->as.program.declarations, sizeof(void*) * prog->as.program.count);
            prog->as.program.declarations[prog->as.program.count - 1] = d; }
    }
    return prog;
}
