#ifndef MINIC_BYTECODE_H
#define MINIC_BYTECODE_H

#include <stdio.h>

#include "ir.h"

typedef enum {
    BC_ADD,
    BC_SUB,
    BC_MUL,
    BC_DIV,
    BC_MOD,
    BC_LT,
    BC_GT,
    BC_LE,
    BC_GE,
    BC_EQ,
    BC_NE,
    BC_NEG,
    BC_NOT,
    BC_ITOF,
    BC_PUSH,
    BC_LOAD,
    BC_STORE,
    BC_JMP,
    BC_JZ,
    BC_JNZ,
    BC_PRINT,
    BC_HALT
} BcOp;

typedef struct {
    BcOp code;
    Value value;
    int arg;
} BcInstr;

typedef struct {
    VEC(BcInstr) code;
    char **slot_names;
    size_t slot_count;
    size_t var_count;
} Bytecode;

void lower_to_bytecode(Arena *arena, const InstrList *instrs, const SymbolTable *table,
                       Bytecode *program);
void format_bytecode(FILE *out, const Bytecode *program);

#endif
