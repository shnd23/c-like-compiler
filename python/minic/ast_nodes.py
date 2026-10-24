from dataclasses import dataclass, field
from typing import Any, List, Optional


@dataclass
class Node:
    line: int
    col: int


@dataclass
class Expr(Node):
    type: Optional[str] = field(default=None, kw_only=True)


@dataclass
class Number(Expr):
    text: str
    is_float: bool


@dataclass
class Var(Expr):
    name: str
    symbol: Any = field(default=None, kw_only=True)


@dataclass
class Unary(Expr):
    op: str
    operand: Expr


@dataclass
class Binary(Expr):
    op: str
    left: Expr
    right: Expr


@dataclass
class VarDecl(Node):
    var_type: str
    name: str
    init: Optional[Expr]
    symbol: Any = field(default=None, kw_only=True)


@dataclass
class Assign(Node):
    name: str
    value: Expr
    symbol: Any = field(default=None, kw_only=True)


@dataclass
class If(Node):
    cond: Expr
    then_branch: Node
    else_branch: Optional[Node]


@dataclass
class While(Node):
    cond: Expr
    body: Node


@dataclass
class Print(Node):
    value: Expr


@dataclass
class ExprStmt(Node):
    expr: Expr


@dataclass
class Block(Node):
    stmts: List[Node]


@dataclass
class Program(Node):
    stmts: List[Node]


def _typed(node):
    return f" : {node.type}" if getattr(node, "type", None) else ""


def describe(node):
    if isinstance(node, Program):
        return "Program"
    if isinstance(node, Block):
        return "Block"
    if isinstance(node, VarDecl):
        return f"VarDecl {node.var_type} {node.name}"
    if isinstance(node, Assign):
        return f"Assign {node.name}"
    if isinstance(node, If):
        return "If"
    if isinstance(node, While):
        return "While"
    if isinstance(node, Print):
        return "Print"
    if isinstance(node, ExprStmt):
        return "ExprStmt"
    if isinstance(node, Binary):
        return f"Binary {node.op}{_typed(node)}"
    if isinstance(node, Unary):
        return f"Unary {node.op}{_typed(node)}"
    if isinstance(node, Number):
        return f"Number {node.text}{_typed(node)}"
    if isinstance(node, Var):
        return f"Var {node.name}{_typed(node)}"
    raise TypeError(f"unknown node {node!r}")


def children(node):
    if isinstance(node, (Program, Block)):
        return list(node.stmts)
    if isinstance(node, VarDecl):
        return [node.init] if node.init is not None else []
    if isinstance(node, Assign):
        return [node.value]
    if isinstance(node, If):
        kids = [node.cond, node.then_branch]
        if node.else_branch is not None:
            kids.append(node.else_branch)
        return kids
    if isinstance(node, While):
        return [node.cond, node.body]
    if isinstance(node, Print):
        return [node.value]
    if isinstance(node, ExprStmt):
        return [node.expr]
    if isinstance(node, Binary):
        return [node.left, node.right]
    if isinstance(node, Unary):
        return [node.operand]
    return []


def dump_ast(root):
    lines = []

    def walk(node, depth):
        lines.append("  " * depth + describe(node))
        for child in children(node):
            walk(child, depth + 1)

    walk(root, 0)
    return "\n".join(lines)
