#include "symbols.h"

void symtab_init(SymbolTable *table, Arena *arena)
{
    memset(table, 0, sizeof *table);
    table->arena = arena;
    size_t zero = 0;
    vec_push(arena, &table->marks, zero);
}

void symtab_push_scope(SymbolTable *table)
{
    size_t mark = table->active.count;
    vec_push(table->arena, &table->marks, mark);
}

void symtab_pop_scope(SymbolTable *table)
{
    table->marks.count--;
    table->active.count = table->marks.items[table->marks.count];
}

Symbol *symtab_lookup(SymbolTable *table, const char *name)
{
    for (size_t index = table->active.count; index > 0; index--) {
        Symbol *symbol = &table->symbols.items[table->active.items[index - 1]];
        if (strcmp(symbol->name, name) == 0) {
            return symbol;
        }
    }
    return NULL;
}

Symbol *symtab_lookup_current(SymbolTable *table, const char *name)
{
    size_t floor = table->marks.items[table->marks.count - 1];
    for (size_t index = table->active.count; index > floor; index--) {
        Symbol *symbol = &table->symbols.items[table->active.items[index - 1]];
        if (strcmp(symbol->name, name) == 0) {
            return symbol;
        }
    }
    return NULL;
}

Symbol *symtab_declare(SymbolTable *table, const char *name, TypeKind type, int line, int col)
{
    if (symtab_lookup_current(table, name) != NULL) {
        return NULL;
    }
    int count = 0;
    for (size_t index = 0; index < table->symbols.count; index++) {
        if (strcmp(table->symbols.items[index].name, name) == 0) {
            count++;
        }
    }
    Symbol symbol;
    memset(&symbol, 0, sizeof symbol);
    symbol.name = arena_strdup(table->arena, name);
    symbol.type = type;
    symbol.line = line;
    symbol.col = col;
    symbol.depth = (int)table->marks.count - 1;
    symbol.uid = (int)table->symbols.count;
    symbol.ir_name =
        count == 0 ? symbol.name : arena_printf(table->arena, "%s.%d", symbol.name, count);
    vec_push(table->arena, &table->symbols, symbol);
    int uid = symbol.uid;
    vec_push(table->arena, &table->active, uid);
    return &table->symbols.items[uid];
}

void format_symbols(FILE *out, const SymbolTable *table)
{
    fprintf(out, "  %-4s%-12s%-7s%-7s%s\n", "ID", "NAME", "TYPE", "DEPTH", "LINE");
    for (size_t index = 0; index < table->symbols.count; index++) {
        const Symbol *symbol = &table->symbols.items[index];
        fprintf(out, "  %-4d%-12s%-7s%-7d%d\n", symbol->uid, symbol->ir_name,
                type_name(symbol->type), symbol->depth, symbol->line);
    }
}
