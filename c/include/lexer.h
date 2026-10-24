#ifndef MINIC_LEXER_H
#define MINIC_LEXER_H

#include "errors.h"
#include "token.h"

void lex_source(Arena *arena, const char *source, size_t length, TokenList *tokens,
                ErrorList *errors);

#endif
