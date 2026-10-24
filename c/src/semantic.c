#include <string.h>

#include "semantic.h"

typedef struct {
    Arena *arena;
    SymbolTable *table;
    ErrorList *errors;
} Analyzer;

static void report(Analyzer *analyzer, const Node *node, char *message)
{
    errlist_add(analyzer->arena, analyzer->errors, node->line, node->col, message);
}

static TypeKind visit_expr(Analyzer *analyzer, Node *node);

static void check_assignable(Analyzer *analyzer, TypeKind target, TypeKind value, const char *name,
                             const Node *node)
{
    if (value == TYPE_ERROR) {
        return;
    }
    if (target == TYPE_INT && value == TYPE_FLOAT) {
        report(analyzer, node,
               arena_printf(analyzer->arena, "cannot assign float to int variable '%s'", name));
    }
}

static TypeKind compute_type(Analyzer *analyzer, Node *node)
{
    switch (node->kind) {
    case NODE_NUMBER:
        return node->u.number.is_float ? TYPE_FLOAT : TYPE_INT;
    case NODE_VAR: {
        Symbol *symbol = symtab_lookup(analyzer->table, node->u.var.name);
        if (symbol == NULL) {
            report(analyzer, node,
                   arena_printf(analyzer->arena, "undeclared variable '%s'", node->u.var.name));
            return TYPE_ERROR;
        }
        node->u.var.symbol = symbol->uid;
        return symbol->type;
    }
    case NODE_UNARY: {
        TypeKind operand = visit_expr(analyzer, node->u.unary.operand);
        if (operand == TYPE_ERROR) {
            return TYPE_ERROR;
        }
        return node->u.unary.op[0] == '!' ? TYPE_INT : operand;
    }
    case NODE_BINARY: {
        TypeKind left = visit_expr(analyzer, node->u.binary.left);
        TypeKind right = visit_expr(analyzer, node->u.binary.right);
        if (left == TYPE_ERROR || right == TYPE_ERROR) {
            return TYPE_ERROR;
        }
        const char *op = node->u.binary.op;
        if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0 || strcmp(op, "<") == 0 ||
            strcmp(op, ">") == 0 || strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0 ||
            strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
            return TYPE_INT;
        }
        if (strcmp(op, "%") == 0) {
            if (left == TYPE_FLOAT || right == TYPE_FLOAT) {
                report(analyzer, node,
                       arena_strdup(analyzer->arena, "operator '%' requires integer operands"));
                return TYPE_ERROR;
            }
            return TYPE_INT;
        }
        return (left == TYPE_FLOAT || right == TYPE_FLOAT) ? TYPE_FLOAT : TYPE_INT;
    }
    default:
        return TYPE_ERROR;
    }
}

static TypeKind visit_expr(Analyzer *analyzer, Node *node)
{
    node->type = compute_type(analyzer, node);
    return node->type;
}

static void visit_stmt(Analyzer *analyzer, Node *node);

static void visit_decl(Analyzer *analyzer, Node *node)
{
    if (node->u.decl.init != NULL) {
        TypeKind value = visit_expr(analyzer, node->u.decl.init);
        check_assignable(analyzer, node->u.decl.var_type, value, node->u.decl.name, node);
    }
    const Symbol *previous = symtab_lookup_current(analyzer->table, node->u.decl.name);
    if (previous != NULL) {
        report(analyzer, node,
               arena_printf(analyzer->arena,
                            "'%s' is already declared in this scope (first declared at line %d)",
                            node->u.decl.name, previous->line));
        return;
    }
    const Symbol *symbol = symtab_declare(analyzer->table, node->u.decl.name,
                                          node->u.decl.var_type, node->line, node->col);
    node->u.decl.symbol = symbol->uid;
}

static void visit_assign(Analyzer *analyzer, Node *node)
{
    const Symbol *symbol = symtab_lookup(analyzer->table, node->u.assign.name);
    TypeKind value = visit_expr(analyzer, node->u.assign.value);
    if (symbol == NULL) {
        report(analyzer, node,
               arena_printf(analyzer->arena, "undeclared variable '%s'", node->u.assign.name));
        return;
    }
    node->u.assign.symbol = symbol->uid;
    check_assignable(analyzer, symbol->type, value, node->u.assign.name, node);
}

static void visit_stmt(Analyzer *analyzer, Node *node)
{
    switch (node->kind) {
    case NODE_VARDECL:
        visit_decl(analyzer, node);
        break;
    case NODE_ASSIGN:
        visit_assign(analyzer, node);
        break;
    case NODE_IF:
        visit_expr(analyzer, node->u.branch.cond);
        visit_stmt(analyzer, node->u.branch.then_branch);
        if (node->u.branch.else_branch != NULL) {
            visit_stmt(analyzer, node->u.branch.else_branch);
        }
        break;
    case NODE_WHILE:
        visit_expr(analyzer, node->u.loop.cond);
        visit_stmt(analyzer, node->u.loop.body);
        break;
    case NODE_PRINT:
    case NODE_EXPRSTMT:
        visit_expr(analyzer, node->u.child);
        break;
    case NODE_BLOCK:
        symtab_push_scope(analyzer->table);
        for (size_t index = 0; index < node->u.list.count; index++) {
            visit_stmt(analyzer, node->u.list.items[index]);
        }
        symtab_pop_scope(analyzer->table);
        break;
    default:
        break;
    }
}

void analyze_program(Arena *arena, Node *program, SymbolTable *table, ErrorList *errors)
{
    Analyzer analyzer;
    analyzer.arena = arena;
    analyzer.table = table;
    analyzer.errors = errors;
    for (size_t index = 0; index < program->u.list.count; index++) {
        visit_stmt(&analyzer, program->u.list.items[index]);
    }
    errlist_sort(errors);
}
