#ifndef MINIC_OPS_H
#define MINIC_OPS_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_LT,
    OP_GT,
    OP_LE,
    OP_GE,
    OP_EQ,
    OP_NE,
    OP_NEG,
    OP_NOT,
    OP_ITOF,
    OP_COPY,
    OP_JZ,
    OP_JNZ,
    OP_JMP,
    OP_PRINT,
    OP_LABEL,
    OP_NOP
} Op;

typedef enum { TYPE_NONE, TYPE_INT, TYPE_FLOAT, TYPE_ERROR } TypeKind;

typedef struct {
    int is_float;
    union {
        int64_t i;
        double f;
    } u;
} Value;

#define OP_IS_BINARY(op) ((op) <= OP_NE)

Value value_int(int64_t number);
Value value_float(double number);
int value_is_zero(Value value);
int value_truthy(Value value);
Value value_neg(Value value);
Value value_not(Value value);
Value value_to_float(Value value);
int value_binary(Op op, Value left, Value right, Value *result);
void value_format(Value value, char *buffer, size_t size);

const char *op_symbol(Op op);
int op_from_symbol(const char *symbol, Op *op);
const char *type_name(TypeKind type);

#endif
