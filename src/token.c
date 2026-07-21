/* ELang Token */
#include "token.h"

token_t token_create(token_type_t t, const char *v, size_t l, int line, int col) {
    return (token_t){t, v, l, line, col};
}

const char *token_type_name(token_type_t t) {
    switch (t) {
        case TOKEN_INT_LIT: return "int"; case TOKEN_FLOAT_LIT: return "float";
        case TOKEN_STRING_LIT: return "string"; case TOKEN_IDENT: return "ident";
        case TOKEN_FN: return "fn"; case TOKEN_RETURN: return "return";
        case TOKEN_IF: return "if"; case TOKEN_ELSE: return "else";
        case TOKEN_WHILE: return "while"; case TOKEN_FOR: return "for";
        case TOKEN_MATCH: return "match"; case TOKEN_IN: return "in"; case TOKEN_LET: return "let";
        case TOKEN_STRUCT: return "struct"; case TOKEN_ENUM: return "enum";
        case TOKEN_IMPORT: return "import"; case TOKEN_EXPORT: return "export";
        case TOKEN_TRUE: return "true"; case TOKEN_FALSE: return "false";
        case TOKEN_OK: return "Ok"; case TOKEN_ERR: return "Err"; case TOKEN_RESULT: return "Result";
        case TOKEN_PLUS: return "+"; case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*"; case TOKEN_SLASH: return "/";
        case TOKEN_EQ: return "=="; case TOKEN_NEQ: return "!=";
        case TOKEN_LT: return "<"; case TOKEN_GT: return ">";
        case TOKEN_ASSIGN: return "="; case TOKEN_ARROW: return "->";
        case TOKEN_COLON: return ":"; case TOKEN_COMMA: return ",";
        case TOKEN_LPAREN: return "("; case TOKEN_RPAREN: return ")";
        case TOKEN_LBRACE: return "{"; case TOKEN_RBRACE: return "}";
        case TOKEN_LBRACKET: return "["; case TOKEN_RBRACKET: return "]";
        case TOKEN_SEMICOLON: return ";"; case TOKEN_DOT: return ".";
        case TOKEN_DOTDOT: return ".."; case TOKEN_FAT_ARROW: return "=>";
        case TOKEN_PIPE_ARROW: return "|>";
        case TOKEN_DEFER: return "defer";
        case TOKEN_COLONCOLON: return "::";
        case TOKEN_CATCH: return "catch"; case TOKEN_PANIC: return "panic";
        case TOKEN_ASSERT: return "assert"; case TOKEN_QUESTION: return "?";
        case TOKEN_NEWLINE: return "newline"; case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
        default: return "?";
    }
}
