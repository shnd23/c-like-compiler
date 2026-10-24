#ifndef MINIC_SYMBOLS_H
#define MINIC_SYMBOLS_H

#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "ops.h"

typedef struct {
    char *name;
    TypeKind type;
    int line;
    int col;
    int depth;
    int uid;
    char *ir_name;
} Symbol;

typedef struct {
    Arena *arena;
    VEC(Symbol) symbols;
    VEC(int) active;
    VEC(size_t) marks;
} SymbolTable;

void symtab_init(SymbolTable *table, Arena *arena);
void symtab_push_scope(SymbolTable *table);
void symtab_pop_scope(SymbolTable *table);
Symbol *symtab_lookup(SymbolTable *table, const char *name);
Symbol *symtab_lookup_current(SymbolTable *table, const char *name);
Symbol *symtab_declare(SymbolTable *table, const char *name, TypeKind type, int line, int col);
void format_symbols(FILE *out, const SymbolTable *table);

#endif
