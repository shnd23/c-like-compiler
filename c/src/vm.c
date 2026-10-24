#include <stdlib.h>

#include "vm.h"

#define STACK_LIMIT 64

VmResult vm_run(const Bytecode *program, FILE *out, long long max_steps)
{
    VmResult result;
    result.steps = 0;
    result.fault = NULL;

    size_t slot_count = program->slot_count ? program->slot_count : 1;
    Value *slots = calloc(slot_count, sizeof *slots);
    if (slots == NULL) {
        result.fault = "out of memory";
        return result;
    }
    Value stack[STACK_LIMIT];
    int top = 0;
    size_t pc = 0;
    char buffer[64];

    for (;;) {
        if (result.steps >= max_steps) {
            result.fault = "step limit exceeded (possible infinite loop)";
            break;
        }
        result.steps++;
        const BcInstr *instr = &program->code.items[pc++];
        BcOp code = instr->code;
        if (code <= BC_NE) {
            Value right = stack[--top];
            Value left = stack[--top];
            Value value;
            if (value_binary((Op)code, left, right, &value) != 0) {
                result.fault = "division by zero";
                break;
            }
            stack[top++] = value;
            continue;
        }
        if (top > STACK_LIMIT - 2) {
            result.fault = "stack overflow";
            break;
        }
        switch (code) {
        case BC_NEG:
            stack[top - 1] = value_neg(stack[top - 1]);
            break;
        case BC_NOT:
            stack[top - 1] = value_not(stack[top - 1]);
            break;
        case BC_ITOF:
            stack[top - 1] = value_to_float(stack[top - 1]);
            break;
        case BC_PUSH:
            stack[top++] = instr->value;
            break;
        case BC_LOAD:
            stack[top++] = slots[instr->arg];
            break;
        case BC_STORE:
            slots[instr->arg] = stack[--top];
            break;
        case BC_JMP:
            pc = (size_t)instr->arg;
            break;
        case BC_JZ:
            if (value_is_zero(stack[--top])) {
                pc = (size_t)instr->arg;
            }
            break;
        case BC_JNZ:
            if (!value_is_zero(stack[--top])) {
                pc = (size_t)instr->arg;
            }
            break;
        case BC_PRINT:
            value_format(stack[--top], buffer, sizeof buffer);
            fputs(buffer, out);
            fputc('\n', out);
            break;
        case BC_HALT:
            free(slots);
            return result;
        default:
            result.fault = "invalid instruction";
            break;
        }
        if (result.fault != NULL) {
            break;
        }
    }
    free(slots);
    return result;
}
