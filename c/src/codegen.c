#include <stdlib.h>
#include <string.h>

#include "codegen.h"

typedef struct {
    Arena *arena;
    const SymbolTable *table;
    InstrList *out;
    int temp_count;
    int label_count;
} Gen;

static Operand gen_expr(Gen *gen, const Node *node);
static void gen_stmt(Gen *gen, const Node *node);

static void emit(Gen *gen, Instr instr)
{
    vec_push(gen->arena, gen->out, instr);
}

static Operand new_temp(Gen *gen, TypeKind type)
{
    return operand_temp(gen->temp_count++, type);
}

static int new_label(Gen *gen)
{
    return gen->label_count++;
}

static Instr make_instr(Op op)
{
    Instr instr;
    memset(&instr, 0, sizeof instr);
    instr.op = op;
    return instr;
}

static void emit_label(Gen *gen, int label)
{
    Instr instr = make_instr(OP_LABEL);
    instr.target = label;
    emit(gen, instr);
}

static void emit_jump(Gen *gen, Op op, Operand condition, int label)
{
    Instr instr = make_instr(op);
    instr.a = condition;
    instr.target = label;
    emit(gen, instr);
}

static void emit_copy(Gen *gen, Operand dst, Operand source)
{
    Instr instr = make_instr(OP_COPY);
    instr.dst = dst;
    instr.a = source;
    emit(gen, instr);
}

static Operand coerce(Gen *gen, Operand operand, TypeKind target)
{
    if (operand.type == target) {
        return operand;
    }
    Operand temp = new_temp(gen, target);
    Instr instr = make_instr(OP_ITOF);
    instr.dst = temp;
    instr.a = operand;
    emit(gen, instr);
    return temp;
}

static Operand gen_logical(Gen *gen, const Node *node)
{
    int is_and = strcmp(node->u.binary.op, "&&") == 0;
    Op jump_op = is_and ? OP_JZ : OP_JNZ;
    Operand result = new_temp(gen, TYPE_INT);
    int short_label = new_label(gen);
    int end_label = new_label(gen);
    Operand left = gen_expr(gen, node->u.binary.left);
    emit_jump(gen, jump_op, left, short_label);
    Operand right = gen_expr(gen, node->u.binary.right);
    emit_jump(gen, jump_op, right, short_label);
    emit_copy(gen, result, operand_const(value_int(is_and ? 1 : 0)));
    Instr skip = make_instr(OP_JMP);
    skip.target = end_label;
    emit(gen, skip);
    emit_label(gen, short_label);
    emit_copy(gen, result, operand_const(value_int(is_and ? 0 : 1)));
    emit_label(gen, end_label);
    return result;
}

static int is_comparison(Op op)
{
    return op >= OP_LT && op <= OP_NE;
}

static Operand gen_expr(Gen *gen, const Node *node)
{
    switch (node->kind) {
    case NODE_NUMBER:
        if (node->u.number.is_float) {
            return operand_const(value_float(strtod(node->u.number.text, NULL)));
        }
        return operand_const(value_int((int64_t)strtoll(node->u.number.text, NULL, 10)));
    case NODE_VAR:
        return operand_var(&gen->table->symbols.items[node->u.var.symbol]);
    case NODE_UNARY: {
        Operand operand = gen_expr(gen, node->u.unary.operand);
        int negate = node->u.unary.op[0] == '-';
        Operand result = new_temp(gen, negate ? operand.type : TYPE_INT);
        Instr instr = make_instr(negate ? OP_NEG : OP_NOT);
        instr.dst = result;
        instr.a = operand;
        emit(gen, instr);
        return result;
    }
    case NODE_BINARY: {
        if (strcmp(node->u.binary.op, "&&") == 0 || strcmp(node->u.binary.op, "||") == 0) {
            return gen_logical(gen, node);
        }
        Operand left = gen_expr(gen, node->u.binary.left);
        Operand right = gen_expr(gen, node->u.binary.right);
        TypeKind operand_type =
            (left.type == TYPE_FLOAT || right.type == TYPE_FLOAT) ? TYPE_FLOAT : TYPE_INT;
        left = coerce(gen, left, operand_type);
        right = coerce(gen, right, operand_type);
        Op op = OP_ADD;
        op_from_symbol(node->u.binary.op, &op);
        Operand result = new_temp(gen, is_comparison(op) ? TYPE_INT : operand_type);
        Instr instr = make_instr(op);
        instr.dst = result;
        instr.a = left;
        instr.b = right;
        emit(gen, instr);
        return result;
    }
    default:
        return operand_const(value_int(0));
    }
}

static void gen_stmt(Gen *gen, const Node *node)
{
    switch (node->kind) {
    case NODE_VARDECL: {
        const Symbol *symbol = &gen->table->symbols.items[node->u.decl.symbol];
        Operand dst = operand_var(symbol);
        Operand value;
        if (node->u.decl.init != NULL) {
            value = coerce(gen, gen_expr(gen, node->u.decl.init), symbol->type);
        } else if (symbol->type == TYPE_FLOAT) {
            value = operand_const(value_float(0.0));
        } else {
            value = operand_const(value_int(0));
        }
        emit_copy(gen, dst, value);
        break;
    }
    case NODE_ASSIGN: {
        const Symbol *symbol = &gen->table->symbols.items[node->u.assign.symbol];
        Operand value = coerce(gen, gen_expr(gen, node->u.assign.value), symbol->type);
        emit_copy(gen, operand_var(symbol), value);
        break;
    }
    case NODE_IF: {
        Operand condition = gen_expr(gen, node->u.branch.cond);
        int else_label = new_label(gen);
        emit_jump(gen, OP_JZ, condition, else_label);
        gen_stmt(gen, node->u.branch.then_branch);
        if (node->u.branch.else_branch != NULL) {
            int end_label = new_label(gen);
            Instr skip = make_instr(OP_JMP);
            skip.target = end_label;
            emit(gen, skip);
            emit_label(gen, else_label);
            gen_stmt(gen, node->u.branch.else_branch);
            emit_label(gen, end_label);
        } else {
            emit_label(gen, else_label);
        }
        break;
    }
    case NODE_WHILE: {
        int start = new_label(gen);
        int end = new_label(gen);
        emit_label(gen, start);
        Operand condition = gen_expr(gen, node->u.loop.cond);
        emit_jump(gen, OP_JZ, condition, end);
        gen_stmt(gen, node->u.loop.body);
        Instr back = make_instr(OP_JMP);
        back.target = start;
        emit(gen, back);
        emit_label(gen, end);
        break;
    }
    case NODE_PRINT: {
        Instr instr = make_instr(OP_PRINT);
        instr.a = gen_expr(gen, node->u.child);
        emit(gen, instr);
        break;
    }
    case NODE_EXPRSTMT:
        gen_expr(gen, node->u.child);
        break;
    case NODE_BLOCK:
        for (size_t index = 0; index < node->u.list.count; index++) {
            gen_stmt(gen, node->u.list.items[index]);
        }
        break;
    default:
        break;
    }
}

void generate_code(Arena *arena, const Node *program, const SymbolTable *table, InstrList *out)
{
    Gen gen;
    gen.arena = arena;
    gen.table = table;
    gen.out = out;
    gen.temp_count = 0;
    gen.label_count = 0;
    for (size_t index = 0; index < program->u.list.count; index++) {
        gen_stmt(&gen, program->u.list.items[index]);
    }
}
