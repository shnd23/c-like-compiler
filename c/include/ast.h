#ifndef MINIC_AST_H
#define MINIC_AST_H

#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "ops.h"

typedef enum {
    NODE_PROGRAM,
    NODE_BLOCK,
    NODE_VARDECL,
    NODE_ASSIGN,
    NODE_IF,
    NODE_WHILE,
    NODE_PRINT,
    NODE_EXPRSTMT,
    NODE_BINARY,
    NODE_UNARY,
    NODE_NUMBER,
    NODE_VAR
} NodeKind;

typedef struct Node Node;

struct Node {
    NodeKind kind;
    int line;
    int col;
    TypeKind type;
    union {
        struct {
            char *text;
            int is_float;
        } number;
        struct {
            char *name;
            int symbol;
        } var;
        struct {
            char op[3];
            Node *operand;
        } unary;
        struct {
            char op[3];
            Node *left;
            Node *right;
        } binary;
        struct {
            TypeKind var_type;
            char *name;
            Node *init;
            int symbol;
        } decl;
        struct {
            char *name;
            Node *value;
            int symbol;
        } assign;
        struct {
            Node *cond;
            Node *then_branch;
            Node *else_branch;
        } branch;
        struct {
            Node *cond;
            Node *body;
        } loop;
        Node *child;
        VEC(Node *) list;
    } u;
};

Node *node_new(Arena *arena, NodeKind kind, int line, int col);
void dump_ast(FILE *out, const Node *root);

#endif
