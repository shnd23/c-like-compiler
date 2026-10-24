#ifndef MINIC_CODEGEN_H
#define MINIC_CODEGEN_H

#include "ast.h"
#include "ir.h"

void generate_code(Arena *arena, const Node *program, const SymbolTable *table, InstrList *out);

#endif
