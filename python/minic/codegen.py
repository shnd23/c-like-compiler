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
from .ir import Const, Instr, Temp, VarRef
from .ops import COMPARISON_OPS


class CodeGen:
    def __init__(self):
        self.instrs = []
        self.temp_count = 0
        self.label_count = 0

    def emit(self, instr):
        self.instrs.append(instr)

    def new_temp(self, type_name):
        temp = Temp(self.temp_count, type_name)
        self.temp_count += 1
        return temp

    def new_label(self):
        label = f"L{self.label_count}"
        self.label_count += 1
        return label

    def generate(self, program):
        for stmt in program.stmts:
            self.stmt(stmt)
        return self.instrs

    def coerce(self, operand, target):
        if operand.type == target:
            return operand
        temp = self.new_temp(target)
        self.emit(Instr("itof", dst=temp, a=operand))
        return temp

    def stmt(self, node):
        if isinstance(node, VarDecl):
            dst = VarRef(node.symbol)
            if node.init is not None:
                value = self.coerce(self.expr(node.init), node.symbol.type)
            elif node.symbol.type == "float":
                value = Const("float", 0.0)
            else:
                value = Const("int", 0)
            self.emit(Instr("copy", dst=dst, a=value))
        elif isinstance(node, Assign):
            value = self.coerce(self.expr(node.value), node.symbol.type)
            self.emit(Instr("copy", dst=VarRef(node.symbol), a=value))
        elif isinstance(node, If):
            cond = self.expr(node.cond)
            else_label = self.new_label()
            self.emit(Instr("jz", a=cond, target=else_label))
            self.stmt(node.then_branch)
            if node.else_branch is not None:
                end_label = self.new_label()
                self.emit(Instr("jmp", target=end_label))
                self.emit(Instr("label", target=else_label))
                self.stmt(node.else_branch)
                self.emit(Instr("label", target=end_label))
            else:
                self.emit(Instr("label", target=else_label))
        elif isinstance(node, While):
            start = self.new_label()
            end = self.new_label()
            self.emit(Instr("label", target=start))
            cond = self.expr(node.cond)
            self.emit(Instr("jz", a=cond, target=end))
            self.stmt(node.body)
            self.emit(Instr("jmp", target=start))
            self.emit(Instr("label", target=end))
        elif isinstance(node, Print):
            self.emit(Instr("print", a=self.expr(node.value)))
        elif isinstance(node, ExprStmt):
            self.expr(node.expr)
        elif isinstance(node, Block):
            for inner in node.stmts:
                self.stmt(inner)

    def expr(self, node):
        if isinstance(node, Number):
            if node.is_float:
                return Const("float", float(node.text))
            return Const("int", int(node.text))
        if isinstance(node, Var):
            return VarRef(node.symbol)
        if isinstance(node, Unary):
            operand = self.expr(node.operand)
            if node.op == "-":
                result = self.new_temp(operand.type)
                self.emit(Instr("neg", dst=result, a=operand))
            else:
                result = self.new_temp("int")
                self.emit(Instr("not", dst=result, a=operand))
            return result
        if isinstance(node, Binary):
            if node.op in ("&&", "||"):
                return self.logical(node)
            left = self.expr(node.left)
            right = self.expr(node.right)
            operand_type = "float" if "float" in (left.type, right.type) else "int"
            left = self.coerce(left, operand_type)
            right = self.coerce(right, operand_type)
            result_type = "int" if node.op in COMPARISON_OPS else operand_type
            result = self.new_temp(result_type)
            self.emit(Instr(node.op, dst=result, a=left, b=right))
            return result
        raise TypeError(f"cannot generate code for {node!r}")

    def logical(self, node):
        is_and = node.op == "&&"
        jump_op = "jz" if is_and else "jnz"
        result = self.new_temp("int")
        short = self.new_label()
        end = self.new_label()
        left = self.expr(node.left)
        self.emit(Instr(jump_op, a=left, target=short))
        right = self.expr(node.right)
        self.emit(Instr(jump_op, a=right, target=short))
        self.emit(Instr("copy", dst=result, a=Const("int", 1 if is_and else 0)))
        self.emit(Instr("jmp", target=end))
        self.emit(Instr("label", target=short))
        self.emit(Instr("copy", dst=result, a=Const("int", 0 if is_and else 1)))
        self.emit(Instr("label", target=end))
        return result


def generate(program):
    return CodeGen().generate(program)
