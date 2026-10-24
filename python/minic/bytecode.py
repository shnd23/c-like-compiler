from dataclasses import dataclass
from typing import List

from .ir import Const, Temp, VarRef
from .ops import format_value

BINARY_MNEMONICS = {
    "+": "ADD",
    "-": "SUB",
    "*": "MUL",
    "/": "DIV",
    "%": "MOD",
    "<": "LT",
    ">": "GT",
    "<=": "LE",
    ">=": "GE",
    "==": "EQ",
    "!=": "NE",
}
UNARY_MNEMONICS = {"neg": "NEG", "not": "NOT", "itof": "ITOF"}
MNEMONIC_TO_BINARY = {name: op for op, name in BINARY_MNEMONICS.items()}


@dataclass
class Bytecode:
    code: List[tuple]
    slot_names: List[str]
    var_count: int


def lower(instrs, table):
    var_count = len(table.symbols)
    temp_count = 0
    for instr in instrs:
        for operand in (instr.dst, instr.a, instr.b):
            if isinstance(operand, Temp):
                temp_count = max(temp_count, operand.id + 1)
    slot_names = [symbol.ir_name for symbol in table.symbols]
    slot_names += [f"$t{index}" for index in range(temp_count)]

    code = []
    labels = {}
    fixups = []

    def slot(operand):
        if isinstance(operand, VarRef):
            return operand.symbol.uid
        return var_count + operand.id

    def load(operand):
        if isinstance(operand, Const):
            code.append(("PUSH", operand.value))
        else:
            code.append(("LOAD", slot(operand)))

    for instr in instrs:
        op = instr.op
        if op == "label":
            labels[instr.target] = len(code)
        elif op == "copy":
            load(instr.a)
            code.append(("STORE", slot(instr.dst)))
        elif op in BINARY_MNEMONICS:
            load(instr.a)
            load(instr.b)
            code.append((BINARY_MNEMONICS[op], None))
            code.append(("STORE", slot(instr.dst)))
        elif op in UNARY_MNEMONICS:
            load(instr.a)
            code.append((UNARY_MNEMONICS[op], None))
            code.append(("STORE", slot(instr.dst)))
        elif op == "jmp":
            fixups.append(len(code))
            code.append(("JMP", instr.target))
        elif op in ("jz", "jnz"):
            load(instr.a)
            fixups.append(len(code))
            code.append(("JZ" if op == "jz" else "JNZ", instr.target))
        elif op == "print":
            load(instr.a)
            code.append(("PRINT", None))
        else:
            raise ValueError(f"cannot lower {op}")
    code.append(("HALT", None))
    for index in fixups:
        name, target = code[index]
        code[index] = (name, labels[target])
    return Bytecode(code, slot_names, var_count)


def format_bytecode(program):
    lines = []
    for address, (name, arg) in enumerate(program.code):
        if name == "PUSH":
            text = format_value(arg)
        elif name in ("LOAD", "STORE"):
            text = f"{arg} {program.slot_names[arg]}"
        elif name in ("JMP", "JZ", "JNZ"):
            text = f"{arg:04d}"
        else:
            text = ""
        if text:
            lines.append(f"{address:04d}  {name:<6}{text}")
        else:
            lines.append(f"{address:04d}  {name}")
    return "\n".join(lines)
