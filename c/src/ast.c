#include "ast.h"

Node *node_new(Arena *arena, NodeKind kind, int line, int col)
{
    Node *node = arena_alloc(arena, sizeof *node);
    node->kind = kind;
    node->line = line;
    node->col = col;
    node->type = TYPE_NONE;
    return node;
}

static void print_typed_suffix(FILE *out, const Node *node)
{
    if (node->type != TYPE_NONE) {
        fprintf(out, " : %s", type_name(node->type));
    }
}

static void describe(FILE *out, const Node *node)
{
    switch (node->kind) {
    case NODE_PROGRAM:
        fputs("Program", out);
        break;
    case NODE_BLOCK:
        fputs("Block", out);
        break;
    case NODE_VARDECL:
        fprintf(out, "VarDecl %s %s", type_name(node->u.decl.var_type), node->u.decl.name);
        break;
    case NODE_ASSIGN:
        fprintf(out, "Assign %s", node->u.assign.name);
        break;
    case NODE_IF:
        fputs("If", out);
        break;
    case NODE_WHILE:
        fputs("While", out);
        break;
    case NODE_PRINT:
        fputs("Print", out);
        break;
    case NODE_EXPRSTMT:
        fputs("ExprStmt", out);
        break;
    case NODE_BINARY:
        fprintf(out, "Binary %s", node->u.binary.op);
        print_typed_suffix(out, node);
        break;
    case NODE_UNARY:
        fprintf(out, "Unary %s", node->u.unary.op);
        print_typed_suffix(out, node);
        break;
    case NODE_NUMBER:
        fprintf(out, "Number %s", node->u.number.text);
        print_typed_suffix(out, node);
        break;
    case NODE_VAR:
        fprintf(out, "Var %s", node->u.var.name);
        print_typed_suffix(out, node);
        break;
    }
    fputc('\n', out);
}

static void walk(FILE *out, const Node *node, int depth)
{
    for (int index = 0; index < depth; index++) {
        fputs("  ", out);
    }
    describe(out, node);
    switch (node->kind) {
    case NODE_PROGRAM:
    case NODE_BLOCK:
        for (size_t index = 0; index < node->u.list.count; index++) {
            walk(out, node->u.list.items[index], depth + 1);
        }
        break;
    case NODE_VARDECL:
        if (node->u.decl.init != NULL) {
            walk(out, node->u.decl.init, depth + 1);
        }
        break;
    case NODE_ASSIGN:
        walk(out, node->u.assign.value, depth + 1);
        break;
    case NODE_IF:
        walk(out, node->u.branch.cond, depth + 1);
        walk(out, node->u.branch.then_branch, depth + 1);
        if (node->u.branch.else_branch != NULL) {
            walk(out, node->u.branch.else_branch, depth + 1);
        }
        break;
    case NODE_WHILE:
        walk(out, node->u.loop.cond, depth + 1);
        walk(out, node->u.loop.body, depth + 1);
        break;
    case NODE_PRINT:
    case NODE_EXPRSTMT:
        walk(out, node->u.child, depth + 1);
        break;
    case NODE_BINARY:
        walk(out, node->u.binary.left, depth + 1);
        walk(out, node->u.binary.right, depth + 1);
        break;
    case NODE_UNARY:
        walk(out, node->u.unary.operand, depth + 1);
        break;
    default:
        break;
    }
}

void dump_ast(FILE *out, const Node *root)
{
    walk(out, root, 0);
}
