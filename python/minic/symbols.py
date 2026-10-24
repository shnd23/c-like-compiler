from dataclasses import dataclass


@dataclass(frozen=True)
class Symbol:
    name: str
    type: str
    line: int
    col: int
    depth: int
    uid: int
    ir_name: str


class SymbolTable:
    def __init__(self):
        self.scopes = [{}]
        self.symbols = []
        self.counts = {}

    @property
    def depth(self):
        return len(self.scopes) - 1

    def push_scope(self):
        self.scopes.append({})

    def pop_scope(self):
        self.scopes.pop()

    def lookup(self, name):
        for scope in reversed(self.scopes):
            if name in scope:
                return scope[name]
        return None

    def lookup_current(self, name):
        return self.scopes[-1].get(name)

    def declare(self, name, type_name, line, col):
        if name in self.scopes[-1]:
            return None
        count = self.counts.get(name, 0)
        self.counts[name] = count + 1
        ir_name = name if count == 0 else f"{name}.{count}"
        symbol = Symbol(name, type_name, line, col, self.depth, len(self.symbols), ir_name)
        self.scopes[-1][name] = symbol
        self.symbols.append(symbol)
        return symbol


def format_symbols(table):
    lines = [f"  {'ID':<4}{'NAME':<12}{'TYPE':<7}{'DEPTH':<7}LINE"]
    for sym in table.symbols:
        lines.append(f"  {sym.uid:<4}{sym.ir_name:<12}{sym.type:<7}{sym.depth:<7}{sym.line}")
    return "\n".join(lines)
