#include "errors.h"

void errlist_add(Arena *arena, ErrorList *errors, int line, int col, char *message)
{
    CompileError error;
    error.line = line;
    error.col = col;
    error.message = message;
    vec_push(arena, errors, error);
}

void errlist_sort(ErrorList *errors)
{
    for (size_t index = 1; index < errors->count; index++) {
        CompileError current = errors->items[index];
        size_t slot = index;
        while (slot > 0) {
            const CompileError *before = &errors->items[slot - 1];
            if (before->line < current.line ||
                (before->line == current.line && before->col <= current.col)) {
                break;
            }
            errors->items[slot] = errors->items[slot - 1];
            slot--;
        }
        errors->items[slot] = current;
    }
}

static int is_continuation(unsigned char byte)
{
    return (byte & 0xC0) == 0x80;
}

void format_error(FILE *out, const char *filename, const char *source, size_t length,
                  const CompileError *error)
{
    size_t start = 0;
    int current = 1;
    while (current < error->line && start < length) {
        if (source[start] == '\n') {
            current++;
        }
        start++;
    }
    size_t end = start;
    if (current != error->line) {
        end = start = length;
    }
    while (end < length && source[end] != '\n') {
        end++;
    }
    while (end > start && source[end - 1] == '\r') {
        end--;
    }
    fprintf(out, "%s:%d:%d: error: %s\n", filename, error->line, error->col, error->message);
    fprintf(out, "%5d | %.*s\n", error->line, (int)(end - start), source + start);
    fputs("      | ", out);
    int wanted = error->col > 1 ? error->col - 1 : 0;
    int seen = 0;
    for (size_t index = start; index < end && seen < wanted; index++) {
        unsigned char byte = (unsigned char)source[index];
        if (is_continuation(byte)) {
            continue;
        }
        fputc(byte == '\t' ? '\t' : ' ', out);
        seen++;
    }
    fputs("^\n", out);
}
