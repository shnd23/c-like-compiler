#include <string.h>

#include "lexer.h"

typedef struct {
    Arena *arena;
    const char *source;
    size_t length;
    size_t pos;
    int line;
    int col;
    ErrorList *errors;
} Lexer;

static int is_digit(int ch)
{
    return ch >= '0' && ch <= '9';
}

static int is_alpha(int ch)
{
    return ch == '_' || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static int is_alnum(int ch)
{
    return is_alpha(ch) || is_digit(ch);
}

static int is_space(int ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f' || ch == '\v';
}

static int peek(const Lexer *lexer, size_t offset)
{
    size_t index = lexer->pos + offset;
    return index < lexer->length ? (unsigned char)lexer->source[index] : -1;
}

static int advance(Lexer *lexer)
{
    unsigned char ch = (unsigned char)lexer->source[lexer->pos++];
    if (ch == '\n') {
        lexer->line++;
        lexer->col = 1;
    } else if ((ch & 0xC0) != 0x80) {
        lexer->col++;
    }
    return ch;
}

static void report(Lexer *lexer, int line, int col, char *message)
{
    errlist_add(lexer->arena, lexer->errors, line, col, message);
}

static void push_token(Lexer *lexer, TokenList *tokens, TokenKind kind, size_t start, int line,
                       int col)
{
    Token token;
    token.kind = kind;
    token.text = arena_strndup(lexer->arena, lexer->source + start, lexer->pos - start);
    token.line = line;
    token.col = col;
    vec_push(lexer->arena, tokens, token);
}

static void skip_trivia(Lexer *lexer)
{
    while (lexer->pos < lexer->length) {
        int ch = peek(lexer, 0);
        if (is_space(ch)) {
            advance(lexer);
        } else if (ch == '/' && peek(lexer, 1) == '/') {
            while (lexer->pos < lexer->length && peek(lexer, 0) != '\n') {
                advance(lexer);
            }
        } else if (ch == '/' && peek(lexer, 1) == '*') {
            int line = lexer->line;
            int col = lexer->col;
            advance(lexer);
            advance(lexer);
            int closed = 0;
            while (lexer->pos < lexer->length) {
                if (peek(lexer, 0) == '*' && peek(lexer, 1) == '/') {
                    advance(lexer);
                    advance(lexer);
                    closed = 1;
                    break;
                }
                advance(lexer);
            }
            if (!closed) {
                report(lexer, line, col, arena_strdup(lexer->arena, "unterminated block comment"));
            }
        } else {
            break;
        }
    }
}

static int is_two_char_operator(int first, int second)
{
    static const char *const PAIRS[] = {"==", "!=", "<=", ">=", "&&", "||"};
    for (size_t index = 0; index < sizeof PAIRS / sizeof PAIRS[0]; index++) {
        if (PAIRS[index][0] == first && PAIRS[index][1] == second) {
            return 1;
        }
    }
    return 0;
}

static int is_keyword(const char *word, size_t length)
{
    static const char *const KEYWORDS[] = {"int", "float", "if", "else", "while", "print"};
    for (size_t index = 0; index < sizeof KEYWORDS / sizeof KEYWORDS[0]; index++) {
        if (strlen(KEYWORDS[index]) == length && memcmp(KEYWORDS[index], word, length) == 0) {
            return 1;
        }
    }
    return 0;
}

static void read_word(Lexer *lexer, TokenList *tokens, int line, int col)
{
    size_t start = lexer->pos;
    while (is_alnum(peek(lexer, 0))) {
        advance(lexer);
    }
    TokenKind kind = is_keyword(lexer->source + start, lexer->pos - start) ? TK_KEYWORD : TK_IDENT;
    push_token(lexer, tokens, kind, start, line, col);
}

static int integer_too_large(const char *word, size_t length)
{
    size_t skip = 0;
    while (skip < length && word[skip] == '0') {
        skip++;
    }
    size_t digits = length - skip;
    if (digits > 19) {
        return 1;
    }
    if (digits < 19) {
        return 0;
    }
    return memcmp(word + skip, "9223372036854775807", 19) > 0;
}

static void read_number(Lexer *lexer, TokenList *tokens, int line, int col)
{
    size_t start = lexer->pos;
    while (is_digit(peek(lexer, 0))) {
        advance(lexer);
    }
    int is_float = 0;
    int missing_digits = 0;
    if (peek(lexer, 0) == '.') {
        is_float = 1;
        advance(lexer);
        int digits = 0;
        while (is_digit(peek(lexer, 0))) {
            advance(lexer);
            digits++;
        }
        missing_digits = digits == 0;
    }
    int bad_suffix = 0;
    if (is_alpha(peek(lexer, 0))) {
        while (is_alnum(peek(lexer, 0))) {
            advance(lexer);
        }
        bad_suffix = 1;
    }
    size_t length = lexer->pos - start;
    const char *word = lexer->source + start;
    if (bad_suffix) {
        report(lexer, line, col,
               arena_printf(lexer->arena,
                            "bad identifier '%.*s': identifiers cannot start with a digit",
                            (int)length, word));
        return;
    }
    if (missing_digits) {
        report(lexer, line, col,
               arena_printf(lexer->arena, "'%.*s' is missing digits after the decimal point",
                            (int)length, word));
        return;
    }
    if (is_float) {
        push_token(lexer, tokens, TK_FLOAT, start, line, col);
        return;
    }
    if (integer_too_large(word, length)) {
        report(lexer, line, col,
               arena_printf(lexer->arena, "integer literal '%.*s' is too large", (int)length, word));
        return;
    }
    push_token(lexer, tokens, TK_INTEGER, start, line, col);
}

static void next_token(Lexer *lexer, TokenList *tokens)
{
    int line = lexer->line;
    int col = lexer->col;
    size_t start = lexer->pos;
    int ch = peek(lexer, 0);
    if (is_alpha(ch)) {
        read_word(lexer, tokens, line, col);
        return;
    }
    if (is_digit(ch)) {
        read_number(lexer, tokens, line, col);
        return;
    }
    if (is_two_char_operator(ch, peek(lexer, 1))) {
        advance(lexer);
        advance(lexer);
        push_token(lexer, tokens, TK_OPERATOR, start, line, col);
        return;
    }
    if (ch > 0 && strchr("+-*/%=<>!", ch) != NULL) {
        advance(lexer);
        push_token(lexer, tokens, TK_OPERATOR, start, line, col);
        return;
    }
    if (ch > 0 && strchr("(){};", ch) != NULL) {
        advance(lexer);
        push_token(lexer, tokens, TK_DELIM, start, line, col);
        return;
    }
    advance(lexer);
    while (lexer->pos < lexer->length && (peek(lexer, 0) & 0xC0) == 0x80) {
        advance(lexer);
    }
    size_t width = lexer->pos - start;
    if (ch == '&' || ch == '|') {
        report(lexer, line, col,
               arena_printf(lexer->arena, "unexpected character '%c' (did you mean '%c%c'?)", ch,
                            ch, ch));
    } else {
        report(lexer, line, col,
               arena_printf(lexer->arena, "unexpected character '%.*s'", (int)width,
                            lexer->source + start));
    }
}

void lex_source(Arena *arena, const char *source, size_t length, TokenList *tokens,
                ErrorList *errors)
{
    Lexer lexer;
    lexer.arena = arena;
    lexer.source = source;
    lexer.length = length;
    lexer.pos = 0;
    lexer.line = 1;
    lexer.col = 1;
    lexer.errors = errors;
    for (;;) {
        skip_trivia(&lexer);
        if (lexer.pos >= lexer.length) {
            Token eof;
            eof.kind = TK_EOF;
            eof.text = arena_strdup(arena, "$");
            eof.line = lexer.line;
            eof.col = lexer.col;
            vec_push(arena, tokens, eof);
            return;
        }
        next_token(&lexer, tokens);
    }
}
