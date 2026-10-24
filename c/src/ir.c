#include "ir.h"

Operand operand_const(Value value)
{
    Operand operand;
    memset(&operand, 0, sizeof operand);
    operand.kind = OPD_CONST;
    operand.type = value.is_float ? TYPE_FLOAT : TYPE_INT;
    operand.constant = value;
    return operand;
}

Operand operand_var(const Symbol *symbol)
{
    Operand operand;
    memset(&operand, 0, sizeof operand);
    operand.kind = OPD_VAR;
    operand.type = symbol->type;
    operand.id = symbol->uid;
    return operand;
}

Operand operand_temp(int id, TypeKind type)
{
    Operand operand;
    memset(&operand, 0, sizeof operand);
    operand.kind = OPD_TEMP;
    operand.type = type;
    operand.id = id;
    return operand;
}

int op_is_defining(Op op)
{
    return OP_IS_BINARY(op) || op == OP_COPY || op == OP_NEG || op == OP_NOT || op == OP_ITOF;
}

int op_is_pure(Op op)
{
    if (op == OP_DIV || op == OP_MOD) {
        return 0;
    }
    return op_is_defining(op);
}

size_t tac_temp_count(const InstrList *instrs)
{
    size_t count = 0;
    for (size_t index = 0; index < instrs->count; index++) {
        const Instr *instr = &instrs->items[index];
        const Operand *slots[3] = {&instr->dst, &instr->a, &instr->b};
        for (int slot = 0; slot < 3; slot++) {
            if (slots[slot]->kind == OPD_TEMP && (size_t)slots[slot]->id + 1 > count) {
                count = (size_t)slots[slot]->id + 1;
            }
        }
    }
    return count;
}

static void write_operand(FILE *out, const SymbolTable *table, const Operand *operand)
{
    char buffer[64];
    switch (operand->kind) {
    case OPD_CONST:
        value_format(operand->constant, buffer, sizeof buffer);
        fputs(buffer, out);
        break;
    case OPD_VAR:
        fputs(table->symbols.items[operand->id].ir_name, out);
        break;
    case OPD_TEMP:
        fprintf(out, "$t%d", operand->id);
        break;
    default:
        fputs("?", out);
        break;
    }
}

static void format_instr(FILE *out, const SymbolTable *table, const Instr *instr)
{
    if (instr->op == OP_LABEL) {
        fprintf(out, "L%d:\n", instr->target);
        return;
    }
    fputs("    ", out);
    switch (instr->op) {
    case OP_COPY:
        write_operand(out, table, &instr->dst);
        fputs(" = ", out);
        write_operand(out, table, &instr->a);
        break;
    case OP_NEG:
    case OP_NOT:
    case OP_ITOF:
        write_operand(out, table, &instr->dst);
        fputs(instr->op == OP_NEG ? " = -" : instr->op == OP_NOT ? " = !" : " = float(", out);
        write_operand(out, table, &instr->a);
        if (instr->op == OP_ITOF) {
            fputc(')', out);
        }
        break;
    case OP_JZ:
    case OP_JNZ:
        fputs(instr->op == OP_JZ ? "ifz " : "ifnz ", out);
        write_operand(out, table, &instr->a);
        fprintf(out, " goto L%d", instr->target);
        break;
    case OP_JMP:
        fprintf(out, "goto L%d", instr->target);
        break;
    case OP_PRINT:
        fputs("print ", out);
        write_operand(out, table, &instr->a);
        break;
    default:
        write_operand(out, table, &instr->dst);
        fputs(" = ", out);
        write_operand(out, table, &instr->a);
        fprintf(out, " %s ", op_symbol(instr->op));
        write_operand(out, table, &instr->b);
        break;
    }
    fputc('\n', out);
}

void format_tac(FILE *out, const SymbolTable *table, const InstrList *instrs)
{
    for (size_t index = 0; index < instrs->count; index++) {
        format_instr(out, table, &instrs->items[index]);
    }
}
