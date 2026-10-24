#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ops.h"

Value value_int(int64_t number)
{
    Value value;
    memset(&value, 0, sizeof value);
    value.u.i = number;
    return value;
}

Value value_float(double number)
{
    Value value;
    memset(&value, 0, sizeof value);
    value.is_float = 1;
    value.u.f = number;
    return value;
}

int value_is_zero(Value value)
{
    return value.is_float ? value.u.f == 0.0 : value.u.i == 0;
}

int value_truthy(Value value)
{
    return !value_is_zero(value);
}

Value value_to_float(Value value)
{
    return value.is_float ? value : value_float((double)value.u.i);
}

Value value_neg(Value value)
{
    if (value.is_float) {
        return value_float(-value.u.f);
    }
    return value_int((int64_t)(0 - (uint64_t)value.u.i));
}

Value value_not(Value value)
{
    return value_int(value_is_zero(value) ? 1 : 0);
}

static int64_t wrap_add(int64_t a, int64_t b)
{
    return (int64_t)((uint64_t)a + (uint64_t)b);
}

static int64_t wrap_sub(int64_t a, int64_t b)
{
    return (int64_t)((uint64_t)a - (uint64_t)b);
}

static int64_t wrap_mul(int64_t a, int64_t b)
{
    return (int64_t)((uint64_t)a * (uint64_t)b);
}

static int compare_ints(Op op, int64_t a, int64_t b)
{
    switch (op) {
    case OP_LT:
        return a < b;
    case OP_GT:
        return a > b;
    case OP_LE:
        return a <= b;
    case OP_GE:
        return a >= b;
    case OP_EQ:
        return a == b;
    default:
        return a != b;
    }
}

static int compare_floats(Op op, double a, double b)
{
    switch (op) {
    case OP_LT:
        return a < b;
    case OP_GT:
        return a > b;
    case OP_LE:
        return a <= b;
    case OP_GE:
        return a >= b;
    case OP_EQ:
        return a == b;
    default:
        return a != b;
    }
}

int value_binary(Op op, Value left, Value right, Value *result)
{
    if (left.is_float != right.is_float) {
        left = value_to_float(left);
        right = value_to_float(right);
    }
    if (left.is_float) {
        double a = left.u.f;
        double b = right.u.f;
        switch (op) {
        case OP_ADD:
            *result = value_float(a + b);
            return 0;
        case OP_SUB:
            *result = value_float(a - b);
            return 0;
        case OP_MUL:
            *result = value_float(a * b);
            return 0;
        case OP_DIV:
            if (b == 0.0) {
                return 1;
            }
            *result = value_float(a / b);
            return 0;
        case OP_MOD:
            return 1;
        default:
            *result = value_int(compare_floats(op, a, b));
            return 0;
        }
    }
    int64_t a = left.u.i;
    int64_t b = right.u.i;
    switch (op) {
    case OP_ADD:
        *result = value_int(wrap_add(a, b));
        return 0;
    case OP_SUB:
        *result = value_int(wrap_sub(a, b));
        return 0;
    case OP_MUL:
        *result = value_int(wrap_mul(a, b));
        return 0;
    case OP_DIV:
        if (b == 0) {
            return 1;
        }
        *result = value_int(b == -1 ? wrap_sub(0, a) : a / b);
        return 0;
    case OP_MOD:
        if (b == 0) {
            return 1;
        }
        *result = value_int(b == -1 ? 0 : a % b);
        return 0;
    default:
        *result = value_int(compare_ints(op, a, b));
        return 0;
    }
}

void value_format(Value value, char *buffer, size_t size)
{
    if (!value.is_float) {
        snprintf(buffer, size, "%lld", (long long)value.u.i);
        return;
    }
    if (isnan(value.u.f)) {
        snprintf(buffer, size, "nan");
        return;
    }
    snprintf(buffer, size, "%.10g", value.u.f);
    if (strpbrk(buffer, ".en") == NULL) {
        size_t length = strlen(buffer);
        if (length + 3 <= size) {
            memcpy(buffer + length, ".0", 3);
        }
    }
}

static const char *const OP_SYMBOLS[] = {"+", "-", "*", "/", "%", "<", ">", "<=", ">=", "==", "!="};

const char *op_symbol(Op op)
{
    return OP_IS_BINARY(op) ? OP_SYMBOLS[op] : "?";
}

int op_from_symbol(const char *symbol, Op *op)
{
    for (int index = 0; index <= OP_NE; index++) {
        if (strcmp(OP_SYMBOLS[index], symbol) == 0) {
            *op = (Op)index;
            return 1;
        }
    }
    return 0;
}

const char *type_name(TypeKind type)
{
    switch (type) {
    case TYPE_INT:
        return "int";
    case TYPE_FLOAT:
        return "float";
    case TYPE_ERROR:
        return "error";
    default:
        return "none";
    }
}
