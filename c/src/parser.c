#include <setjmp.h>
#include <stdarg.h>
#include <string.h>

#include "parser.h"

#define MAX_DEPTH 200

typedef struct {
    Arena *arena;
    const TokenList *tokens;
    size_t pos;
    int depth;
    ErrorList *errors;
    jmp_buf *handler;
    char *failure;
    int failure_line;
    int failure_col;
} Parser;

static const char *const LEVEL_OR[] = {"||", NULL};
static const char *const LEVEL_AND[] = {"&&", NULL};
static const char *const LEVEL_EQUALITY[] = {"==", "!=", NULL};
static const char *const LEVEL_RELATIONAL[] = {"<", ">", "<=", ">=", NULL};
static const char *const LEVEL_ADDITIVE[] = {"+", "-", NULL};
static const char *const LEVEL_MULTIPLICATIVE[] = {"*", "/", "%", NULL};
static const char *const *const LEVELS[] = {LEVEL_OR,       LEVEL_AND,      LEVEL_EQUALITY,
                                             LEVEL_RELATIONAL, LEVEL_ADDITIVE, LEVEL_MULTIPLICATIVE};
#define LEVEL_COUNT ((int)(sizeof LEVELS / sizeof LEVELS[0]))

static Node *parse_statement(Parser *parser);
static Node *parse_expression(Parser *parser);
static Node *parse_unary(Parser *parser);

static const Token *peek(const Parser *parser)
{
    return &parser->tokens->items[parser->pos];
}

static const Token *advance(Parser *parser)
{
    const Token *token = peek(parser);
    if (token->kind != TK_EOF) {
        parser->pos++;
    }
    return token;
}

static int is_text(const Token *token, const char *text)
{
    return (token->kind == TK_KEYWORD || token->kind == TK_OPERATOR || token->kind == TK_DELIM) &&
           strcmp(token->text, text) == 0;
}

static int check(const Parser *parser, const char *text)
{
    return is_text(peek(parser), text);
}

static void fail(Parser *parser, const Token *token, char *message)
{
    parser->failure = message;
    parser->failure_line = token->line;
    parser->failure_col = token->col;
    longjmp(*parser->handler, 1);
}

static const char *describe_token(Parser *parser, const Token *token)
{
    if (token->kind == TK_EOF) {
        return "end of input";
    }
    return arena_printf(parser->arena, "'%s'", token->text);
}

static const Token *expect(Parser *parser, const char *text)
{
    const Token *token = peek(parser);
    if (check(parser, text)) {
        return advance(parser);
    }
    fail(parser, token,
         arena_printf(parser->arena, "expected '%s' before %s", text,
                      describe_token(parser, token)));
    return token;
}

static const Token *expect_identifier(Parser *parser)
{
    const Token *token = peek(parser);
    if (token->kind == TK_IDENT) {
        return advance(parser);
    }
    fail(parser, token,
         arena_printf(parser->arena, "expected identifier before %s",
                      describe_token(parser, token)));
    return token;
}

static void enter(Parser *parser)
{
    parser->depth++;
    if (parser->depth > MAX_DEPTH) {
        fail(parser, peek(parser), arena_strdup(parser->arena, "nesting is too deep"));
    }
}

static void leave(Parser *parser)
{
    parser->depth--;
}

static int is_type_keyword(const Token *token)
{
    return token->kind == TK_KEYWORD &&
           (strcmp(token->text, "int") == 0 || strcmp(token->text, "float") == 0);
}

static int is_sync_keyword(const Token *token)
{
    static const char *const SYNC[] = {"int", "float", "if", "while", "print"};
    if (token->kind != TK_KEYWORD) {
        return 0;
    }
    for (size_t index = 0; index < sizeof SYNC / sizeof SYNC[0]; index++) {
        if (strcmp(token->text, SYNC[index]) == 0) {
            return 1;
        }
    }
    return 0;
}

static void synchronize(Parser *parser)
{
    while (peek(parser)->kind != TK_EOF) {
        const Token *token = peek(parser);
        if (is_text(token, ";") && token->kind == TK_DELIM) {
            advance(parser);
            return;
        }
        if (is_text(token, "}") && token->kind == TK_DELIM) {
            return;
        }
        if (is_sync_keyword(token)) {
            return;
        }
        advance(parser);
    }
}

static Node *parse_statements(Parser *parser, int in_block, int line, int col, NodeKind kind)
{
    Node *container = node_new(parser->arena, kind, line, col);
    while (peek(parser)->kind != TK_EOF) {
        if (in_block && check(parser, "}")) {
            break;
        }
        size_t start = parser->pos;
        int saved_depth = parser->depth;
        jmp_buf *previous = parser->handler;
        jmp_buf local;
        parser->handler = &local;
        if (setjmp(local) == 0) {
            Node *statement = parse_statement(parser);
            parser->handler = previous;
            vec_push(parser->arena, &container->u.list, statement);
        } else {
            parser->handler = previous;
            errlist_add(parser->arena, parser->errors, parser->failure_line, parser->failure_col,
                        parser->failure);
            parser->depth = saved_depth;
            synchronize(parser);
            if (parser->pos == start) {
                advance(parser);
            }
        }
    }
    return container;
}

static Node *parse_binary(Parser *parser, int level)
{
    if (level == LEVEL_COUNT) {
        return parse_unary(parser);
    }
    Node *left = parse_binary(parser, level + 1);
    for (;;) {
        const Token *token = peek(parser);
        int matches = 0;
        if (token->kind == TK_OPERATOR) {
            for (const char *const *candidate = LEVELS[level]; *candidate != NULL; candidate++) {
                if (strcmp(token->text, *candidate) == 0) {
                    matches = 1;
                    break;
                }
            }
        }
        if (!matches) {
            return left;
        }
        advance(parser);
        Node *right = parse_binary(parser, level + 1);
        Node *node = node_new(parser->arena, NODE_BINARY, token->line, token->col);
        strcpy(node->u.binary.op, token->text);
        node->u.binary.left = left;
        node->u.binary.right = right;
        left = node;
    }
}

static void reject_adjacent_operand(Parser *parser, const Token *previous)
{
    const Token *following = peek(parser);
    if ((following->kind == TK_IDENT || following->kind == TK_INTEGER ||
         following->kind == TK_FLOAT) &&
        following->line == previous->line) {
        fail(parser, following,
             arena_printf(parser->arena, "missing an operator before '%s'", following->text));
    }
}

static Node *parse_primary(Parser *parser)
{
    const Token *token = peek(parser);
    if (token->kind == TK_INTEGER || token->kind == TK_FLOAT) {
        advance(parser);
        reject_adjacent_operand(parser, token);
        Node *node = node_new(parser->arena, NODE_NUMBER, token->line, token->col);
        node->u.number.text = token->text;
        node->u.number.is_float = token->kind == TK_FLOAT;
        return node;
    }
    if (token->kind == TK_IDENT) {
        advance(parser);
        reject_adjacent_operand(parser, token);
        Node *node = node_new(parser->arena, NODE_VAR, token->line, token->col);
        node->u.var.name = token->text;
        node->u.var.symbol = -1;
        return node;
    }
    if (token->kind == TK_DELIM && check(parser, "(")) {
        advance(parser);
        Node *inner = parse_expression(parser);
        expect(parser, ")");
        return inner;
    }
    if (token->kind == TK_EOF) {
        fail(parser, token, arena_strdup(parser->arena, "unexpected end of input"));
    }
    fail(parser, token, arena_printf(parser->arena, "unexpected token '%s'", token->text));
    return NULL;
}

static Node *parse_unary(Parser *parser)
{
    enter(parser);
    const Token *token = peek(parser);
    Node *result;
    if (token->kind == TK_OPERATOR && (strcmp(token->text, "-") == 0 || strcmp(token->text, "!") == 0)) {
        advance(parser);
        Node *operand = parse_unary(parser);
        result = node_new(parser->arena, NODE_UNARY, token->line, token->col);
        strcpy(result->u.unary.op, token->text);
        result->u.unary.operand = operand;
    } else {
        result = parse_primary(parser);
    }
    leave(parser);
    return result;
}

static Node *parse_expression(Parser *parser)
{
    return parse_binary(parser, 0);
}

static Node *parse_body(Parser *parser, const char *owner)
{
    const Token *token = peek(parser);
    if (is_type_keyword(token)) {
        fail(parser, token,
             arena_printf(parser->arena, "a declaration cannot be the body of '%s'; wrap it in a block",
                          owner));
    }
    return parse_statement(parser);
}

static Node *parse_declaration(Parser *parser)
{
    const Token *keyword = advance(parser);
    const Token *name = expect_identifier(parser);
    Node *init = NULL;
    if (check(parser, "=")) {
        advance(parser);
        init = parse_expression(parser);
    }
    expect(parser, ";");
    Node *node = node_new(parser->arena, NODE_VARDECL, name->line, name->col);
    node->u.decl.var_type = strcmp(keyword->text, "int") == 0 ? TYPE_INT : TYPE_FLOAT;
    node->u.decl.name = name->text;
    node->u.decl.init = init;
    node->u.decl.symbol = -1;
    return node;
}

static Node *parse_assignment(Parser *parser)
{
    const Token *name = advance(parser);
    expect(parser, "=");
    Node *value = parse_expression(parser);
    expect(parser, ";");
    Node *node = node_new(parser->arena, NODE_ASSIGN, name->line, name->col);
    node->u.assign.name = name->text;
    node->u.assign.value = value;
    node->u.assign.symbol = -1;
    return node;
}

static Node *parse_if(Parser *parser)
{
    const Token *keyword = advance(parser);
    expect(parser, "(");
    Node *cond = parse_expression(parser);
    expect(parser, ")");
    Node *then_branch = parse_body(parser, "if");
    Node *else_branch = NULL;
    if (check(parser, "else")) {
        advance(parser);
        else_branch = parse_body(parser, "else");
    }
    Node *node = node_new(parser->arena, NODE_IF, keyword->line, keyword->col);
    node->u.branch.cond = cond;
    node->u.branch.then_branch = then_branch;
    node->u.branch.else_branch = else_branch;
    return node;
}

static Node *parse_while(Parser *parser)
{
    const Token *keyword = advance(parser);
    expect(parser, "(");
    Node *cond = parse_expression(parser);
    expect(parser, ")");
    Node *body = parse_body(parser, "while");
    Node *node = node_new(parser->arena, NODE_WHILE, keyword->line, keyword->col);
    node->u.loop.cond = cond;
    node->u.loop.body = body;
    return node;
}

static Node *parse_print(Parser *parser)
{
    const Token *keyword = advance(parser);
    expect(parser, "(");
    Node *value = parse_expression(parser);
    expect(parser, ")");
    expect(parser, ";");
    Node *node = node_new(parser->arena, NODE_PRINT, keyword->line, keyword->col);
    node->u.child = value;
    return node;
}

static Node *parse_block(Parser *parser)
{
    const Token *brace = expect(parser, "{");
    Node *block = parse_statements(parser, 1, brace->line, brace->col, NODE_BLOCK);
    expect(parser, "}");
    return block;
}

static Node *parse_expression_statement(Parser *parser)
{
    const Token *start = peek(parser);
    Node *expr = parse_expression(parser);
    expect(parser, ";");
    Node *node = node_new(parser->arena, NODE_EXPRSTMT, start->line, start->col);
    node->u.child = expr;
    return node;
}

static Node *parse_statement_inner(Parser *parser)
{
    const Token *token = peek(parser);
    if (token->kind == TK_KEYWORD) {
        if (is_type_keyword(token)) {
            return parse_declaration(parser);
        }
        if (strcmp(token->text, "if") == 0) {
            return parse_if(parser);
        }
        if (strcmp(token->text, "while") == 0) {
            return parse_while(parser);
        }
        if (strcmp(token->text, "print") == 0) {
            return parse_print(parser);
        }
    }
    if (check(parser, "else")) {
        errlist_add(parser->arena, parser->errors, token->line, token->col,
                    arena_strdup(parser->arena, "'else' without a matching 'if'"));
        advance(parser);
        parse_body(parser, "else");
        return node_new(parser->arena, NODE_BLOCK, token->line, token->col);
    }
    if (token->kind == TK_DELIM && check(parser, "{")) {
        return parse_block(parser);
    }
    if (token->kind == TK_IDENT) {
        const Token *following = &parser->tokens->items[parser->pos + 1];
        if (following->kind == TK_OPERATOR && strcmp(following->text, "=") == 0) {
            return parse_assignment(parser);
        }
    }
    return parse_expression_statement(parser);
}

static Node *parse_statement(Parser *parser)
{
    enter(parser);
    Node *node = parse_statement_inner(parser);
    leave(parser);
    return node;
}

Node *parse_program(Arena *arena, const TokenList *tokens, ErrorList *errors)
{
    Parser parser;
    memset(&parser, 0, sizeof parser);
    parser.arena = arena;
    parser.tokens = tokens;
    parser.errors = errors;
    return parse_statements(&parser, 0, 1, 1, NODE_PROGRAM);
}
