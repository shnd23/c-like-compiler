from dataclasses import dataclass
from typing import Any, Optional

from .ops import BINARY_OPS, format_value
from .symbols import Symbol


@dataclass(frozen=True)
class Const:
    type: str
    value: Any

    def __str__(self):
        return format_value(self.value)


@dataclass(frozen=True)
class VarRef:
    symbol: Symbol

    @property
    def type(self):
        return self.symbol.type

    def __str__(self):
        return self.symbol.ir_name


@dataclass(frozen=True)
class Temp:
    id: int
    type: str

    def __str__(self):
        return f"$t{self.id}"


@dataclass
class Instr:
    op: str
    dst: Any = None
    a: Any = None
    b: Any = None
    target: Optional[str] = None


DEFINING_OPS = BINARY_OPS | {"copy", "neg", "not", "itof"}
PURE_OPS = (BINARY_OPS - {"/", "%"}) | {"copy", "neg", "not", "itof"}


def format_instr(instr):
    op = instr.op
    if op == "label":
        return f"{instr.target}:"
    if op == "copy":
        body = f"{instr.dst} = {instr.a}"
    elif op in BINARY_OPS:
        body = f"{instr.dst} = {instr.a} {op} {instr.b}"
    elif op == "neg":
        body = f"{instr.dst} = -{instr.a}"
    elif op == "not":
        body = f"{instr.dst} = !{instr.a}"
    elif op == "itof":
        body = f"{instr.dst} = float({instr.a})"
    elif op == "jz":
        body = f"ifz {instr.a} goto {instr.target}"
    elif op == "jnz":
        body = f"ifnz {instr.a} goto {instr.target}"
    elif op == "jmp":
        body = f"goto {instr.target}"
    elif op == "print":
        body = f"print {instr.a}"
    else:
        raise ValueError(f"unknown instruction {op}")
    return "    " + body


def format_tac(instrs):
    return "\n".join(format_instr(instr) for instr in instrs)
