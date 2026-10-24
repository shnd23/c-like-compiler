#include <string.h>

#include "bytecode.h"

static const char *const BC_NAMES[] = {"ADD",  "SUB", "MUL",  "DIV",  "MOD",   "LT",   "GT",  "LE",
                                       "GE",   "EQ",  "NE",   "NEG",  "NOT",   "ITOF", "PUSH", "LOAD",
                                       "STORE", "JMP", "JZ",   "JNZ",  "PRINT", "HALT"};

typedef struct {
    Arena *arena;
    Bytecode *program;
    size_t var_count;
} Lowering;

static void emit(Lowering *lowering, BcOp code, Value value, int arg)
{
    BcInstr instr;
    instr.code = code;
    instr.value = value;
    instr.arg = arg;
    vec_push(lowering->arena, &lowering->program->code, instr);
}

static int slot_of(const Lowering *lowering, const Operand *operand)
{
    if (operand->kind == OPD_VAR) {
        return operand->id;
    }
    return (int)lowering->var_count + operand->id;
}

static void emit_load(Lowering *lowering, const Operand *operand)
{
    if (operand->kind == OPD_CONST) {
        emit(lowering, BC_PUSH, operand->constant, 0);
    } else {
        emit(lowering, BC_LOAD, value_int(0), slot_of(lowering, operand));
    }
}

void lower_to_bytecode(Arena *arena, const InstrList *instrs, const SymbolTable *table,
                       Bytecode *program)
{
    memset(program, 0, sizeof *program);
    size_t var_count = table->symbols.count;
    size_t temp_count = tac_temp_count(instrs);
    program->var_count = var_count;
    program->slot_count = var_count + temp_count;
    program->slot_names = arena_alloc(arena, (program->slot_count + 1) * sizeof(char *));
    for (size_t index = 0; index < var_count; index++) {
        program->slot_names[index] = table->symbols.items[index].ir_name;
    }
    for (size_t index = 0; index < temp_count; index++) {
        program->slot_names[var_count + index] = arena_printf(arena, "$t%zu", index);
    }

    Lowering lowering;
    lowering.arena = arena;
    lowering.program = program;
    lowering.var_count = var_count;

    int max_label = -1;
    for (size_t index = 0; index < instrs->count; index++) {
        if (instrs->items[index].target > max_label) {
            max_label = instrs->items[index].target;
        }
    }
    int *label_address = arena_alloc(arena, ((size_t)max_label + 2) * sizeof(int));
    VEC(size_t) fixups;
    memset(&fixups, 0, sizeof fixups);
    Value zero = value_int(0);

    for (size_t index = 0; index < instrs->count; index++) {
        const Instr *instr = &instrs->items[index];
        Op op = instr->op;
        if (op == OP_LABEL) {
            label_address[instr->target] = (int)program->code.count;
        } else if (op == OP_COPY) {
            emit_load(&lowering, &instr->a);
            emit(&lowering, BC_STORE, zero, slot_of(&lowering, &instr->dst));
        } else if (OP_IS_BINARY(op)) {
            emit_load(&lowering, &instr->a);
            emit_load(&lowering, &instr->b);
            emit(&lowering, (BcOp)op, zero, 0);
            emit(&lowering, BC_STORE, zero, slot_of(&lowering, &instr->dst));
        } else if (op == OP_NEG || op == OP_NOT || op == OP_ITOF) {
            emit_load(&lowering, &instr->a);
            emit(&lowering, op == OP_NEG ? BC_NEG : op == OP_NOT ? BC_NOT : BC_ITOF, zero, 0);
            emit(&lowering, BC_STORE, zero, slot_of(&lowering, &instr->dst));
        } else if (op == OP_JMP) {
            size_t at = program->code.count;
            vec_push(arena, &fixups, at);
            emit(&lowering, BC_JMP, zero, instr->target);
        } else if (op == OP_JZ || op == OP_JNZ) {
            emit_load(&lowering, &instr->a);
            size_t at = program->code.count;
            vec_push(arena, &fixups, at);
            emit(&lowering, op == OP_JZ ? BC_JZ : BC_JNZ, zero, instr->target);
        } else if (op == OP_PRINT) {
            emit_load(&lowering, &instr->a);
            emit(&lowering, BC_PRINT, zero, 0);
        }
    }
    emit(&lowering, BC_HALT, zero, 0);
    for (size_t index = 0; index < fixups.count; index++) {
        BcInstr *jump = &program->code.items[fixups.items[index]];
        jump->arg = label_address[jump->arg];
    }
}

void format_bytecode(FILE *out, const Bytecode *program)
{
    char buffer[64];
    for (size_t address = 0; address < program->code.count; address++) {
        const BcInstr *instr = &program->code.items[address];
        const char *name = BC_NAMES[instr->code];
        switch (instr->code) {
        case BC_PUSH:
            value_format(instr->value, buffer, sizeof buffer);
            fprintf(out, "%04zu  %-6s%s\n", address, name, buffer);
            break;
        case BC_LOAD:
        case BC_STORE:
            fprintf(out, "%04zu  %-6s%d %s\n", address, name, instr->arg,
                    program->slot_names[instr->arg]);
            break;
        case BC_JMP:
        case BC_JZ:
        case BC_JNZ:
            fprintf(out, "%04zu  %-6s%04d\n", address, name, instr->arg);
            break;
        default:
            fprintf(out, "%04zu  %s\n", address, name);
            break;
        }
    }
}
