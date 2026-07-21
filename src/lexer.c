/* ELang Lexer */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"
#include "config.h"

static void advance(lexer_t *l) {
    if (l->current == '\n') { l->line++; l->col = 1; }
    else l->col++;
    l->pos++;
    l->current = l->pos < l->length ? l->source[l->pos] : '\0';
}

static char peek_char(lexer_t *l) {
    return l->pos + 1 < l->length ? l->source[l->pos + 1] : '\0';
}

static token_type_t check_keyword(const char *w, size_t len) {
    struct { const char *w; size_t len; token_type_t t; } kw[] = {
        {"fn",2,TOKEN_FN},{"return",6,TOKEN_RETURN},{"if",2,TOKEN_IF},{"else",4,TOKEN_ELSE},
        {"while",5,TOKEN_WHILE},{"for",3,TOKEN_FOR},{"in",2,TOKEN_IN},{"loop",4,TOKEN_LOOP},
        {"break",5,TOKEN_BREAK},{"continue",8,TOKEN_CONTINUE},{"match",5,TOKEN_MATCH},
        {"let",3,TOKEN_LET},{"mut",3,TOKEN_MUT},{"const",5,TOKEN_CONST},
        {"struct",6,TOKEN_STRUCT},{"enum",4,TOKEN_ENUM},{"impl",4,TOKEN_IMPL},{"import",6,TOKEN_IMPORT},
        {"export",6,TOKEN_EXPORT},{"as",2,TOKEN_AS},{"true",4,TOKEN_TRUE},{"false",5,TOKEN_FALSE},
        {"nil",3,TOKEN_NIL},{"type",4,TOKEN_TYPE},{"void",4,TOKEN_VOID},
        {"i8",2,TOKEN_I8},{"i16",3,TOKEN_I16},{"i32",3,TOKEN_I32},{"i64",3,TOKEN_I64},
        {"u8",2,TOKEN_U8},{"u16",3,TOKEN_U16},{"u32",3,TOKEN_U32},{"u64",3,TOKEN_U64},
        {"f32",3,TOKEN_F32},{"f64",3,TOKEN_F64},{"bool",4,TOKEN_BOOL},{"char",4,TOKEN_CHAR},
        {"string",6,TOKEN_STRING},{"error",5,TOKEN_ERROR_TYPE},
        {"alloc",5,TOKEN_ALLOC},{"free",4,TOKEN_FREE},{"syscall",7,TOKEN_SYSCALL},
        {"panic",5,TOKEN_PANIC},{"catch",5,TOKEN_CATCH},{"assert",6,TOKEN_ASSERT},
        {"Ok",2,TOKEN_OK},{"Err",3,TOKEN_ERR},{"Result",6,TOKEN_RESULT},
        {"defer",5,TOKEN_DEFER},
    };
    for (size_t i = 0; i < sizeof(kw)/sizeof(kw[0]); i++)
        if (len == kw[i].len && memcmp(w, kw[i].w, len) == 0) return kw[i].t;
    return TOKEN_IDENT;
}

void lexer_init(lexer_t *l, const char *source) {
    l->source = source; l->pos = 0; l->length = strlen(source);
    l->line = 1; l->col = 1; l->current = source[0];
}

token_t lexer_next_token(lexer_t *l) {
    while (l->current == ' ' || l->current == '\t' || l->current == '\r') advance(l);
    while (l->current == '/' && peek_char(l) == '/') {
        while (l->current != '\n' && l->current != '\0') advance(l);
        while (l->current == ' ' || l->current == '\t') advance(l);
    }
    /* Block comments: /star ... star/backslash (with nesting support) */
    while (l->current == '/' && peek_char(l) == '*') {
        advance(l); advance(l); /* skip /star */
        int depth = 1;
        while (depth > 0 && l->current != '\0') {
            if (l->current == '/' && peek_char(l) == '*') {
                advance(l); advance(l);
                depth++;
            } else if (l->current == '*' && peek_char(l) == '/') {
                advance(l); advance(l);
                depth--;
            } else {
                advance(l);
            }
        }
        while (l->current == ' ' || l->current == '\t') advance(l);
    }
    if (l->current == '\0') return token_create(TOKEN_EOF, "", 0, l->line, l->col);
    int line = l->line, col = l->col;
    if (l->current == '\n') { advance(l); return token_create(TOKEN_NEWLINE, "\\n", 1, line, col); }
    if (isdigit(l->current)) {
        size_t sp = l->pos;
        /* Check for prefix: 0x (hex), 0o (octal), 0b (binary) */
        if (l->current == '0' && l->pos + 1 < l->length) {
            char next = l->source[l->pos + 1];
            if (next == 'x' || next == 'X') {
                /* Hex literal: 0x... */
                advance(l); advance(l); /* skip 0x */
                while (isxdigit(l->current) || l->current == '_') advance(l);
                return token_create(TOKEN_INT_LIT, &l->source[sp], l->pos - sp, line, col);
            }
            if (next == 'o' || next == 'O') {
                /* Octal literal: 0o... */
                advance(l); advance(l); /* skip 0o */
                while (((l->current >= '0' && l->current <= '7') || l->current == '_') && l->current != '\0') advance(l);
                return token_create(TOKEN_INT_LIT, &l->source[sp], l->pos - sp, line, col);
            }
            if (next == 'b' || next == 'B') {
                /* Binary literal: 0b... */
                advance(l); advance(l); /* skip 0b */
                while (((l->current == '0' || l->current == '1') || l->current == '_') && l->current != '\0') advance(l);
                return token_create(TOKEN_INT_LIT, &l->source[sp], l->pos - sp, line, col);
            }
        }
        /* Decimal literal */
        while (isdigit(l->current) || l->current == '_') advance(l);
        if (l->current == '.' && isdigit(peek_char(l))) {
            advance(l);
            while (isdigit(l->current) || l->current == '_') advance(l);
            return token_create(TOKEN_FLOAT_LIT, &l->source[sp], l->pos - sp, line, col);
        }
        return token_create(TOKEN_INT_LIT, &l->source[sp], l->pos - sp, line, col);
    }
    if (l->current == '"') {
        advance(l);
        size_t sp = l->pos;
        while (l->current != '"' && l->current != '\0') {
            if (l->current == '\\') {
                advance(l);
                switch (l->current) {
                    case 'n': case 't': case 'r': case '0':
                    case '\\': case '"': case '\'':
                        break;
                    default:
                        fprintf(stderr, "Lexer warning at %d:%d: unknown escape '\\%c'\n",
                                l->line, l->col, l->current);
                }
            }
            advance(l);
        }
        size_t len = l->pos - sp;
        if (l->current != '"') {
            fprintf(stderr, "Lexer error at %d:%d: unterminated string literal\n", line, col);
            return token_create(TOKEN_ERROR, &l->source[sp], len, line, col);
        }
        advance(l);
        return token_create(TOKEN_STRING_LIT, &l->source[sp], len, line, col);
    }
    if (l->current == '\'') {
        advance(l);
        size_t sp = l->pos;
        while (l->current != '\'' && l->current != '\0') {
            if (l->current == '\\') {
                advance(l);
                switch (l->current) {
                    case 'n': case 't': case 'r': case '0':
                    case '\\': case '"': case '\'':
                        break;
                    default:
                        fprintf(stderr, "Lexer warning at %d:%d: unknown escape '\\%c'\n",
                                l->line, l->col, l->current);
                }
            }
            advance(l);
        }
        size_t len = l->pos - sp;
        if (l->current != '\'') {
            fprintf(stderr, "Lexer error at %d:%d: unterminated character literal\n", line, col);
            return token_create(TOKEN_ERROR, &l->source[sp], len, line, col);
        }
        advance(l);
        return token_create(TOKEN_CHAR_LIT, &l->source[sp], len, line, col);
    }
    if (isalpha(l->current) || l->current == '_') {
        size_t sp = l->pos;
        while (isalnum(l->current) || l->current == '_') advance(l);
        return token_create(check_keyword(&l->source[sp], l->pos - sp), &l->source[sp], l->pos - sp, line, col);
    }
    char c = l->current; advance(l);
    switch (c) {
        case '+': if (l->current=='='){advance(l);return token_create(TOKEN_PLUS_ASSIGN,"+=",2,line,col);} return token_create(TOKEN_PLUS,"+",1,line,col);
        case '-': if (l->current=='='){advance(l);return token_create(TOKEN_MINUS_ASSIGN,"-=",2,line,col);}
                  if (l->current=='>'){advance(l);return token_create(TOKEN_ARROW,"->",2,line,col);}
                  return token_create(TOKEN_MINUS,"-",1,line,col);
        case '*': if (l->current=='='){advance(l);return token_create(TOKEN_STAR_ASSIGN,"*=",2,line,col);} return token_create(TOKEN_STAR,"*",1,line,col);
        case '/': if (l->current=='='){advance(l);return token_create(TOKEN_SLASH_ASSIGN,"/=",2,line,col);} return token_create(TOKEN_SLASH,"/",1,line,col);
        case '%': if (l->current=='='){advance(l);return token_create(TOKEN_PERCENT_ASSIGN,"%=",2,line,col);} return token_create(TOKEN_PERCENT,"%",1,line,col);
        case '=': if (l->current=='='){advance(l);return token_create(TOKEN_EQ,"==",2,line,col);}
                  if (l->current=='>'){advance(l);return token_create(TOKEN_FAT_ARROW,"=>",2,line,col);}
                  return token_create(TOKEN_ASSIGN,"=",1,line,col);
        case '!': if (l->current=='='){advance(l);return token_create(TOKEN_NEQ,"!=",2,line,col);} return token_create(TOKEN_NOT,"!",1,line,col);
        case '<': if (l->current=='='){advance(l);return token_create(TOKEN_LTE,"<=",2,line,col);}
                  if (l->current=='<'){advance(l);return token_create(TOKEN_LSHIFT,"<<",2,line,col);}
                  return token_create(TOKEN_LT,"<",1,line,col);
        case '>': if (l->current=='='){advance(l);return token_create(TOKEN_GTE,">=",2,line,col);}
                  if (l->current=='>'){advance(l);return token_create(TOKEN_RSHIFT,">>",2,line,col);}
                  return token_create(TOKEN_GT,">",1,line,col);
        case '&': if (l->current=='&'){advance(l);return token_create(TOKEN_AND,"&&",2,line,col);} return token_create(TOKEN_AMP,"&",1,line,col);
        case '|': if (l->current=='>'){advance(l);return token_create(TOKEN_PIPE_ARROW,"|>",2,line,col);}
                  if (l->current=='|'){advance(l);return token_create(TOKEN_OR,"||",2,line,col);} return token_create(TOKEN_PIPE,"|",1,line,col);
        case '.': if (l->current=='.'){advance(l);return token_create(TOKEN_DOTDOT,"..",2,line,col);} return token_create(TOKEN_DOT,".",1,line,col);
        case '(': return token_create(TOKEN_LPAREN,"(",1,line,col);
        case ')': return token_create(TOKEN_RPAREN,")",1,line,col);
        case '{': return token_create(TOKEN_LBRACE,"{",1,line,col);
        case '}': return token_create(TOKEN_RBRACE,"}",1,line,col);
        case '[': return token_create(TOKEN_LBRACKET,"[",1,line,col);
        case ']': return token_create(TOKEN_RBRACKET,"]",1,line,col);
        case ':': if (l->current==':'){advance(l);return token_create(TOKEN_COLONCOLON,"::",2,line,col);}
                  return token_create(TOKEN_COLON,":",1,line,col);
        case ';': return token_create(TOKEN_SEMICOLON,";",1,line,col);
        case ',': return token_create(TOKEN_COMMA,",",1,line,col);
        case '?': return token_create(TOKEN_QUESTION,"?",1,line,col);
        default: return token_create(TOKEN_ERROR, &l->source[l->pos-1], 1, line, col);
    }
}

char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Error: cannot open '%s'\n", path); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = SAFE_MALLOC(sz + 1);
    size_t read = fread(buf, 1, sz, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}
