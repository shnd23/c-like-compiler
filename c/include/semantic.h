#ifndef MINIC_SEMANTIC_H
#define MINIC_SEMANTIC_H

#include "ast.h"
#include "errors.h"
#include "symbols.h"

void analyze_program(Arena *arena, Node *program, SymbolTable *table, ErrorList *errors);

#endif
