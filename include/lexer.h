/* ELang Lexer */
#ifndef ELANG_LEXER_H
#define ELANG_LEXER_H

#include "token.h"

typedef struct {
    const char *source;
    size_t pos, length;
    int line, col;
    char current;
} lexer_t;

void lexer_init(lexer_t *lexer, const char *source);
token_t lexer_next_token(lexer_t *lexer);
token_t lexer_peek_token(lexer_t *lexer);
char *read_file(const char *path);

#endif
