#ifndef MINIC_IR_H
#define MINIC_IR_H

#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "ops.h"
#include "symbols.h"

typedef enum { OPD_NONE, OPD_CONST, OPD_VAR, OPD_TEMP } OperandKind;

typedef struct {
    OperandKind kind;
    TypeKind type;
    Value constant;
    int id;
} Operand;

typedef struct {
    Op op;
    Operand dst;
    Operand a;
    Operand b;
    int target;
} Instr;

typedef VEC(Instr) InstrList;

Operand operand_const(Value value);
Operand operand_var(const Symbol *symbol);
Operand operand_temp(int id, TypeKind type);
int op_is_defining(Op op);
int op_is_pure(Op op);
void format_tac(FILE *out, const SymbolTable *table, const InstrList *instrs);
size_t tac_temp_count(const InstrList *instrs);

#endif
