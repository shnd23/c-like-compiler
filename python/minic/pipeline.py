from dataclasses import dataclass, field
from typing import Any, List, Optional

from .bytecode import lower
from .codegen import generate
from .lexer import Lexer
from .optimizer import optimize
from .parser import Parser
from .semantic import SemanticAnalyzer


@dataclass
class Compilation:
    tokens: List[Any] = field(default_factory=list)
    program: Optional[Any] = None
    table: Optional[Any] = None
    raw_tac: Optional[List[Any]] = None
    tac: Optional[List[Any]] = None
    bytecode: Optional[Any] = None
    errors: List[Any] = field(default_factory=list)

    @property
    def ok(self):
        return not self.errors


def compile_source(source, optimize_code=True):
    tokens, errors = Lexer(source).tokenize()
    result = Compilation(tokens=tokens)
    if errors:
        result.errors = errors
        return result

    program, errors = Parser(tokens).parse()
    result.program = program
    if errors:
        result.errors = errors
        return result

    table, errors = SemanticAnalyzer().analyze(program)
    result.table = table
    if errors:
        result.errors = errors
        return result

    result.raw_tac = generate(program)
    result.tac = optimize(result.raw_tac) if optimize_code else list(result.raw_tac)
    result.bytecode = lower(result.tac, table)
    return result
