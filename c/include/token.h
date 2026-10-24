#ifndef MINIC_TOKEN_H
#define MINIC_TOKEN_H

#include <stdio.h>
#include <string.h>

#include "arena.h"

typedef enum {
    TK_EOF,
    TK_KEYWORD,
    TK_IDENT,
    TK_INTEGER,
    TK_FLOAT,
    TK_OPERATOR,
    TK_DELIM
} TokenKind;

typedef struct {
    TokenKind kind;
    char *text;
    int line;
    int col;
} Token;

typedef VEC(Token) TokenList;

const char *token_kind_name(TokenKind kind);
void format_tokens(FILE *out, const TokenList *tokens);

#endif
