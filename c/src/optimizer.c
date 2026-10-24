#include <stdlib.h>
#include <string.h>

#include "optimizer.h"

typedef struct {
    Arena *arena;
    size_t var_count;
    size_t temp_count;
    Value *values;
    unsigned *stamps;
    unsigned current;
} Known;

static size_t known_slot(const Known *known, const Operand *operand)
{
    return operand->kind == OPD_VAR ? (size_t)operand->id : known->var_count + (size_t)operand->id;
}

static int is_register(const Operand *operand)
{
    return operand->kind == OPD_VAR || operand->kind == OPD_TEMP;
}

static void known_clear(Known *known)
{
    known->current++;
}

static void known_set(Known *known, const Operand *operand, Value value)
{
    size_t slot = known_slot(known, operand);
    known->values[slot] = value;
    known->stamps[slot] = known->current;
}

static void known_forget(Known *known, const Operand *operand)
{
    known->stamps[known_slot(known, operand)] = 0;
}

static int known_resolve(const Known *known, Operand *operand)
{
    if (!is_register(operand)) {
        return 0;
    }
    size_t slot = known_slot(known, operand);
    if (known->stamps[slot] != known->current) {
        return 0;
    }
    *operand = operand_const(known->values[slot]);
    return 1;
}

static void compact(InstrList *instrs)
{
    size_t kept = 0;
    for (size_t index = 0; index < instrs->count; index++) {
        if (instrs->items[index].op != OP_NOP) {
            instrs->items[kept++] = instrs->items[index];
        }
    }
    instrs->count = kept;
}

static int propagate(Known *known, InstrList *instrs)
{
    int changed = 0;
    known->current++;
    for (size_t index = 0; index < instrs->count; index++) {
        Instr *instr = &instrs->items[index];
        if (instr->op == OP_LABEL) {
            known_clear(known);
            continue;
        }
        if (instr->op == OP_JMP) {
            continue;
        }
        if (known_resolve(known, &instr->a)) {
            changed = 1;
        }
        if (known_resolve(known, &instr->b)) {
            changed = 1;
        }
        Op op = instr->op;
        if (OP_IS_BINARY(op) && instr->a.kind == OPD_CONST && instr->b.kind == OPD_CONST) {
            int divides = op == OP_DIV || op == OP_MOD;
            if (!(divides && value_is_zero(instr->b.constant))) {
                Value folded;
                value_binary(op, instr->a.constant, instr->b.constant, &folded);
                instr->op = OP_COPY;
                instr->a = operand_const(folded);
                memset(&instr->b, 0, sizeof instr->b);
                changed = 1;
            }
        } else if ((op == OP_NEG || op == OP_NOT || op == OP_ITOF) && instr->a.kind == OPD_CONST) {
            Value folded;
            if (op == OP_NEG) {
                folded = value_neg(instr->a.constant);
            } else if (op == OP_NOT) {
                folded = value_not(instr->a.constant);
            } else {
                folded = value_to_float(instr->a.constant);
            }
            instr->op = OP_COPY;
            instr->a = operand_const(folded);
            changed = 1;
        } else if ((op == OP_JZ || op == OP_JNZ) && instr->a.kind == OPD_CONST) {
            int truthy = value_truthy(instr->a.constant);
            int take = op == OP_JZ ? !truthy : truthy;
            if (take) {
                instr->op = OP_JMP;
                memset(&instr->a, 0, sizeof instr->a);
            } else {
                instr->op = OP_NOP;
            }
            changed = 1;
        }
        if (instr->op == OP_COPY) {
            if (instr->a.kind == OPD_CONST) {
                known_set(known, &instr->dst, instr->a.constant);
            } else {
                known_forget(known, &instr->dst);
            }
        } else if (op_is_defining(instr->op)) {
            known_forget(known, &instr->dst);
        }
    }
    compact(instrs);
    return changed;
}

static int remove_unreachable(InstrList *instrs)
{
    size_t kept = 0;
    int dead = 0;
    for (size_t index = 0; index < instrs->count; index++) {
        Instr instr = instrs->items[index];
        if (instr.op == OP_LABEL) {
            dead = 0;
            instrs->items[kept++] = instr;
            continue;
        }
        if (dead) {
            continue;
        }
        instrs->items[kept++] = instr;
        if (instr.op == OP_JMP) {
            dead = 1;
        }
    }
    int changed = kept != instrs->count;
    instrs->count = kept;
    return changed;
}

static int remove_trivial_jumps(InstrList *instrs)
{
    int changed = 0;
    for (size_t index = 0; index < instrs->count; index++) {
        Instr *instr = &instrs->items[index];
        if (instr->op != OP_JMP) {
            continue;
        }
        size_t probe = index + 1;
        while (probe < instrs->count && instrs->items[probe].op == OP_LABEL) {
            if (instrs->items[probe].target == instr->target) {
                instr->op = OP_NOP;
                changed = 1;
                break;
            }
            probe++;
        }
    }
    compact(instrs);
    return changed;
}

static void count_uses(const InstrList *instrs, size_t temp_count, int *uses)
{
    memset(uses, 0, temp_count * sizeof *uses);
    for (size_t index = 0; index < instrs->count; index++) {
        const Instr *instr = &instrs->items[index];
        if (instr->a.kind == OPD_TEMP) {
            uses[instr->a.id]++;
        }
        if (instr->b.kind == OPD_TEMP) {
            uses[instr->b.id]++;
        }
    }
}

static int merge_copies(Known *known, InstrList *instrs)
{
    int *uses = arena_alloc(known->arena, (known->temp_count + 1) * sizeof *uses);
    count_uses(instrs, known->temp_count, uses);
    int changed = 0;
    for (size_t index = 0; index + 1 < instrs->count; index++) {
        Instr *first = &instrs->items[index];
        Instr *second = &instrs->items[index + 1];
        if (!op_is_defining(first->op) || first->dst.kind != OPD_TEMP) {
            continue;
        }
        if (second->op != OP_COPY || second->a.kind != OPD_TEMP || second->a.id != first->dst.id) {
            continue;
        }
        if (uses[first->dst.id] != 1 || first->dst.type != second->dst.type) {
            continue;
        }
        first->dst = second->dst;
        second->op = OP_NOP;
        changed = 1;
    }
    compact(instrs);
    return changed;
}

static int remove_dead_temps(Known *known, InstrList *instrs)
{
    int *uses = arena_alloc(known->arena, (known->temp_count + 1) * sizeof *uses);
    count_uses(instrs, known->temp_count, uses);
    size_t kept = 0;
    for (size_t index = 0; index < instrs->count; index++) {
        Instr instr = instrs->items[index];
        if (op_is_pure(instr.op) && instr.dst.kind == OPD_TEMP && uses[instr.dst.id] == 0) {
            continue;
        }
        instrs->items[kept++] = instr;
    }
    int changed = kept != instrs->count;
    instrs->count = kept;
    return changed;
}

static int remove_unused_labels(InstrList *instrs)
{
    int max_label = -1;
    for (size_t index = 0; index < instrs->count; index++) {
        if (instrs->items[index].target > max_label) {
            max_label = instrs->items[index].target;
        }
    }
    if (max_label < 0) {
        return 0;
    }
    char *targeted = calloc((size_t)max_label + 1, 1);
    if (targeted == NULL) {
        return 0;
    }
    for (size_t index = 0; index < instrs->count; index++) {
        Op op = instrs->items[index].op;
        if (op == OP_JMP || op == OP_JZ || op == OP_JNZ) {
            targeted[instrs->items[index].target] = 1;
        }
    }
    size_t kept = 0;
    for (size_t index = 0; index < instrs->count; index++) {
        Instr instr = instrs->items[index];
        if (instr.op == OP_LABEL && !targeted[instr.target]) {
            continue;
        }
        instrs->items[kept++] = instr;
    }
    int changed = kept != instrs->count;
    instrs->count = kept;
    free(targeted);
    return changed;
}

void optimize_code(Arena *arena, const InstrList *input, size_t var_count, InstrList *output)
{
    output->items = NULL;
    output->count = 0;
    output->cap = 0;
    for (size_t index = 0; index < input->count; index++) {
        vec_push(arena, output, input->items[index]);
    }
    Known known;
    known.arena = arena;
    known.var_count = var_count;
    known.temp_count = tac_temp_count(input);
    size_t slots = var_count + known.temp_count + 1;
    known.values = arena_alloc(arena, slots * sizeof *known.values);
    known.stamps = arena_alloc(arena, slots * sizeof *known.stamps);
    known.current = 0;
    for (;;) {
        int changed = 0;
        changed |= propagate(&known, output);
        changed |= remove_unreachable(output);
        changed |= remove_trivial_jumps(output);
        changed |= merge_copies(&known, output);
        changed |= remove_dead_temps(&known, output);
        changed |= remove_unused_labels(output);
        if (!changed) {
            return;
        }
    }
}
