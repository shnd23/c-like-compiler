#ifndef MINIC_ERRORS_H
#define MINIC_ERRORS_H

#include <stdio.h>
#include <string.h>

#include "arena.h"

typedef struct {
    int line;
    int col;
    char *message;
} CompileError;

typedef VEC(CompileError) ErrorList;

void errlist_add(Arena *arena, ErrorList *errors, int line, int col, char *message);
void errlist_sort(ErrorList *errors);
void format_error(FILE *out, const char *filename, const char *source, size_t length,
                  const CompileError *error);

#endif
