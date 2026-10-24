from dataclasses import dataclass

KEYWORDS = frozenset({"int", "float", "if", "else", "while", "print"})
TYPE_KEYWORDS = frozenset({"int", "float"})


@dataclass(frozen=True)
class Token:
    kind: str
    text: str
    line: int
    col: int


def format_tokens(tokens):
    lines = []
    for tok in tokens:
        position = f"{tok.line}:{tok.col}"
        lines.append(f"{position:<8}{tok.kind:<12}{tok.text}")
    return "\n".join(lines)
