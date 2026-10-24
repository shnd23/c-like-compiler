from .ast_nodes import (
    Assign,
    Binary,
    Block,
    ExprStmt,
    If,
    Number,
    Print,
    Unary,
    Var,
    VarDecl,
    While,
)
from .errors import CompileError
from .symbols import SymbolTable

ERROR = "error"
COMPARISONS = frozenset({"<", ">", "<=", ">=", "==", "!="})
LOGICAL = frozenset({"&&", "||"})


class SemanticAnalyzer:
    def __init__(self):
        self.table = SymbolTable()
        self.errors = []

    def analyze(self, program):
        for stmt in program.stmts:
            self.visit_stmt(stmt)
        self.errors.sort(key=lambda err: (err.line, err.col))
        return self.table, self.errors

    def error(self, message, node):
        self.errors.append(CompileError(message, node.line, node.col))

    def visit_stmt(self, node):
        if isinstance(node, VarDecl):
            self.visit_decl(node)
        elif isinstance(node, Assign):
            self.visit_assign(node)
        elif isinstance(node, If):
            self.visit_expr(node.cond)
            self.visit_stmt(node.then_branch)
            if node.else_branch is not None:
                self.visit_stmt(node.else_branch)
        elif isinstance(node, While):
            self.visit_expr(node.cond)
            self.visit_stmt(node.body)
        elif isinstance(node, Print):
            self.visit_expr(node.value)
        elif isinstance(node, ExprStmt):
            self.visit_expr(node.expr)
        elif isinstance(node, Block):
            self.table.push_scope()
            for stmt in node.stmts:
                self.visit_stmt(stmt)
            self.table.pop_scope()

    def check_assignable(self, target_type, value_type, name, node):
        if value_type == ERROR:
            return
        if target_type == "int" and value_type == "float":
            self.error(f"cannot assign float to int variable '{name}'", node)

    def visit_decl(self, node):
        if node.init is not None:
            value_type = self.visit_expr(node.init)
            self.check_assignable(node.var_type, value_type, node.name, node)
        previous = self.table.lookup_current(node.name)
        if previous is not None:
            self.error(
                f"'{node.name}' is already declared in this scope (first declared at line {previous.line})",
                node,
            )
            return
        node.symbol = self.table.declare(node.name, node.var_type, node.line, node.col)

    def visit_assign(self, node):
        symbol = self.table.lookup(node.name)
        value_type = self.visit_expr(node.value)
        if symbol is None:
            self.error(f"undeclared variable '{node.name}'", node)
            return
        node.symbol = symbol
        self.check_assignable(symbol.type, value_type, node.name, node)

    def visit_expr(self, node):
        node.type = self.compute_type(node)
        return node.type

    def compute_type(self, node):
        if isinstance(node, Number):
            return "float" if node.is_float else "int"
        if isinstance(node, Var):
            symbol = self.table.lookup(node.name)
            if symbol is None:
                self.error(f"undeclared variable '{node.name}'", node)
                return ERROR
            node.symbol = symbol
            return symbol.type
        if isinstance(node, Unary):
            operand_type = self.visit_expr(node.operand)
            if operand_type == ERROR:
                return ERROR
            return "int" if node.op == "!" else operand_type
        if isinstance(node, Binary):
            left_type = self.visit_expr(node.left)
            right_type = self.visit_expr(node.right)
            if left_type == ERROR or right_type == ERROR:
                return ERROR
            if node.op in LOGICAL or node.op in COMPARISONS:
                return "int"
            if node.op == "%":
                if left_type == "float" or right_type == "float":
                    self.error("operator '%' requires integer operands", node)
                    return ERROR
                return "int"
            return "float" if "float" in (left_type, right_type) else "int"
        return ERROR
