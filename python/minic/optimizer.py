from dataclasses import replace

from .ir import DEFINING_OPS, PURE_OPS, Const, Temp
from .ops import BINARY_OPS, binary, logical_not, negate, truthy


def resolve(operand, known):
    if operand is None or isinstance(operand, Const):
        return operand
    return known.get(operand, operand)


def propagate(instrs):
    known = {}
    changed = False
    for instr in instrs:
        op = instr.op
        if op == "label":
            known.clear()
            continue
        if op == "jmp":
            continue
        new_a = resolve(instr.a, known)
        new_b = resolve(instr.b, known)
        if new_a is not instr.a or new_b is not instr.b:
            instr.a = new_a
            instr.b = new_b
            changed = True
        if op in BINARY_OPS and isinstance(instr.a, Const) and isinstance(instr.b, Const):
            if not (op in ("/", "%") and instr.b.value == 0):
                value = binary(op, instr.a.value, instr.b.value)
                instr.op = "copy"
                instr.a = Const(instr.dst.type, value)
                instr.b = None
                changed = True
        elif op in ("neg", "not", "itof") and isinstance(instr.a, Const):
            if op == "neg":
                value = negate(instr.a.value)
            elif op == "not":
                value = logical_not(instr.a.value)
            else:
                value = float(instr.a.value)
            instr.op = "copy"
            instr.a = Const(instr.dst.type, value)
            changed = True
        elif op in ("jz", "jnz") and isinstance(instr.a, Const):
            take = (not truthy(instr.a.value)) if op == "jz" else truthy(instr.a.value)
            if take:
                instr.op = "jmp"
                instr.a = None
            else:
                instr.op = "nop"
            changed = True
        if instr.op == "copy":
            if isinstance(instr.a, Const):
                known[instr.dst] = instr.a
            else:
                known.pop(instr.dst, None)
        elif instr.op in DEFINING_OPS:
            known.pop(instr.dst, None)
    instrs[:] = [instr for instr in instrs if instr.op != "nop"]
    return changed


def remove_unreachable(instrs):
    kept = []
    dead = False
    for instr in instrs:
        if instr.op == "label":
            dead = False
            kept.append(instr)
            continue
        if dead:
            continue
        kept.append(instr)
        if instr.op == "jmp":
            dead = True
    changed = len(kept) != len(instrs)
    instrs[:] = kept
    return changed


def remove_trivial_jumps(instrs):
    changed = False
    for index, instr in enumerate(instrs):
        if instr.op != "jmp":
            continue
        probe = index + 1
        while probe < len(instrs) and instrs[probe].op == "label":
            if instrs[probe].target == instr.target:
                instr.op = "nop"
                changed = True
                break
            probe += 1
    instrs[:] = [instr for instr in instrs if instr.op != "nop"]
    return changed


def count_uses(instrs):
    uses = {}
    for instr in instrs:
        for operand in (instr.a, instr.b):
            if isinstance(operand, Temp):
                uses[operand] = uses.get(operand, 0) + 1
    return uses


def merge_copies(instrs):
    uses = count_uses(instrs)
    changed = False
    for index in range(len(instrs) - 1):
        first = instrs[index]
        second = instrs[index + 1]
        if first.op not in DEFINING_OPS or not isinstance(first.dst, Temp):
            continue
        if second.op != "copy" or second.a != first.dst:
            continue
        if uses.get(first.dst, 0) != 1 or first.dst.type != second.dst.type:
            continue
        first.dst = second.dst
        second.op = "nop"
        changed = True
    instrs[:] = [instr for instr in instrs if instr.op != "nop"]
    return changed


def remove_dead_temps(instrs):
    uses = count_uses(instrs)
    kept = [
        instr
        for instr in instrs
        if not (
            instr.op in PURE_OPS and isinstance(instr.dst, Temp) and uses.get(instr.dst, 0) == 0
        )
    ]
    changed = len(kept) != len(instrs)
    instrs[:] = kept
    return changed


def remove_unused_labels(instrs):
    targets = {instr.target for instr in instrs if instr.op in ("jmp", "jz", "jnz")}
    kept = [instr for instr in instrs if not (instr.op == "label" and instr.target not in targets)]
    changed = len(kept) != len(instrs)
    instrs[:] = kept
    return changed


PASSES = (
    propagate,
    remove_unreachable,
    remove_trivial_jumps,
    merge_copies,
    remove_dead_temps,
    remove_unused_labels,
)


def optimize(instrs):
    work = [replace(instr) for instr in instrs]
    while True:
        changed = False
        for step in PASSES:
            if step(work):
                changed = True
        if not changed:
            return work
