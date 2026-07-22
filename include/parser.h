/* ELang Parser */
#ifndef ELANG_PARSER_H
#define ELANG_PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    lexer_t lexer;
    token_t current, peek;
    int has_error;
    int ever_had_error; /* never reset — tracks if ANY parse error occurred */
    int depth;
} parser_t;

void parser_init(parser_t *parser, const char *source);
ast_node_t *parser_parse(parser_t *parser);

#endif
