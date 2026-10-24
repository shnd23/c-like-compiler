#ifndef MINIC_PARSER_H
#define MINIC_PARSER_H

#include "ast.h"
#include "errors.h"
#include "token.h"

Node *parse_program(Arena *arena, const TokenList *tokens, ErrorList *errors);

#endif
