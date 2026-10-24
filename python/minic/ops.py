from .errors import RuntimeFault

INT_MIN = -(1 << 63)
INT_MAX = (1 << 63) - 1
MASK = (1 << 64) - 1

BINARY_OPS = frozenset({"+", "-", "*", "/", "%", "<", ">", "<=", ">=", "==", "!="})
COMPARISON_OPS = frozenset({"<", ">", "<=", ">=", "==", "!="})


def wrap(value):
    return ((value + (1 << 63)) & MASK) - (1 << 63)


def int_div(a, b):
    quotient = abs(a) // abs(b)
    if (a < 0) != (b < 0):
        quotient = -quotient
    return wrap(quotient)


def int_mod(a, b):
    remainder = abs(a) % abs(b)
    if a < 0:
        remainder = -remainder
    return remainder


def truthy(value):
    return value != 0


def negate(value):
    if isinstance(value, float):
        return -value
    return wrap(-value)


def logical_not(value):
    return 1 if value == 0 else 0


def binary(op, a, b):
    if op == "+":
        return a + b if isinstance(a, float) else wrap(a + b)
    if op == "-":
        return a - b if isinstance(a, float) else wrap(a - b)
    if op == "*":
        return a * b if isinstance(a, float) else wrap(a * b)
    if op == "/":
        if b == 0:
            raise RuntimeFault("division by zero")
        return a / b if isinstance(a, float) else int_div(a, b)
    if op == "%":
        if b == 0:
            raise RuntimeFault("division by zero")
        return int_mod(a, b)
    if op == "<":
        return 1 if a < b else 0
    if op == ">":
        return 1 if a > b else 0
    if op == "<=":
        return 1 if a <= b else 0
    if op == ">=":
        return 1 if a >= b else 0
    if op == "==":
        return 1 if a == b else 0
    if op == "!=":
        return 1 if a != b else 0
    raise ValueError(f"unknown operator {op}")


def format_float(value):
    if value != value:
        return "nan"
    text = "%.10g" % value
    if not any(ch in text for ch in ".en"):
        text += ".0"
    return text


def format_value(value):
    if isinstance(value, float):
        return format_float(value)
    return str(value)
