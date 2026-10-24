#include <string.h>

#include "bytecode.h"
#include "codegen.h"
#include "lexer.h"
#include "optimizer.h"
#include "parser.h"
#include "pipeline.h"
#include "semantic.h"

int compile_source(Compilation *result, const char *source, size_t length, int optimize)
{
    memset(result, 0, sizeof *result);
    arena_init(&result->arena);
    Arena *arena = &result->arena;

    lex_source(arena, source, length, &result->tokens, &result->errors);
    if (result->errors.count > 0) {
        return 0;
    }

    result->program = parse_program(arena, &result->tokens, &result->errors);
    if (result->errors.count > 0) {
        return 0;
    }

    symtab_init(&result->table, arena);
    analyze_program(arena, result->program, &result->table, &result->errors);
    if (result->errors.count > 0) {
        return 0;
    }

    generate_code(arena, result->program, &result->table, &result->raw_tac);
    if (optimize) {
        optimize_code(arena, &result->raw_tac, result->table.symbols.count, &result->tac);
    } else {
        for (size_t index = 0; index < result->raw_tac.count; index++) {
            vec_push(arena, &result->tac, result->raw_tac.items[index]);
        }
    }
    lower_to_bytecode(arena, &result->tac, &result->table, &result->bytecode);
    return 1;
}

void compilation_free(Compilation *result)
{
    arena_free(&result->arena);
    memset(result, 0, sizeof *result);
}
