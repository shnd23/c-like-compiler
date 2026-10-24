#include "token.h"

const char *token_kind_name(TokenKind kind)
{
    switch (kind) {
    case TK_KEYWORD:
        return "KEYWORD";
    case TK_IDENT:
        return "IDENTIFIER";
    case TK_INTEGER:
        return "INTEGER";
    case TK_FLOAT:
        return "FLOAT";
    case TK_OPERATOR:
        return "OPERATOR";
    case TK_DELIM:
        return "DELIMITER";
    default:
        return "EOF";
    }
}

void format_tokens(FILE *out, const TokenList *tokens)
{
    for (size_t index = 0; index < tokens->count; index++) {
        const Token *token = &tokens->items[index];
        char position[48];
        snprintf(position, sizeof position, "%d:%d", token->line, token->col);
        fprintf(out, "%-8s%-12s%s\n", position, token_kind_name(token->kind), token->text);
    }
}
