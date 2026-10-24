#ifndef MINIC_PIPELINE_H
#define MINIC_PIPELINE_H

#include "ast.h"
#include "bytecode.h"
#include "errors.h"
#include "ir.h"
#include "symbols.h"
#include "token.h"

typedef struct {
    Arena arena;
    TokenList tokens;
    Node *program;
    SymbolTable table;
    InstrList raw_tac;
    InstrList tac;
    Bytecode bytecode;
    ErrorList errors;
} Compilation;

int compile_source(Compilation *result, const char *source, size_t length, int optimize);
void compilation_free(Compilation *result);

#endif
