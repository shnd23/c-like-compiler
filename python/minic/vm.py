import sys

from .bytecode import MNEMONIC_TO_BINARY
from .errors import RuntimeFault
from .ops import binary, format_value, logical_not, negate

DEFAULT_MAX_STEPS = 10_000_000


class VM:
    def __init__(self, program, out=None, max_steps=DEFAULT_MAX_STEPS):
        self.program = program
        self.out = out if out is not None else sys.stdout
        self.max_steps = max_steps
        self.steps = 0

    def run(self):
        code = self.program.code
        slots = [0] * len(self.program.slot_names)
        stack = []
        pc = 0
        steps = 0
        limit = self.max_steps
        try:
            while True:
                if steps >= limit:
                    raise RuntimeFault("step limit exceeded (possible infinite loop)")
                steps += 1
                name, arg = code[pc]
                pc += 1
                if name == "PUSH":
                    stack.append(arg)
                elif name == "LOAD":
                    stack.append(slots[arg])
                elif name == "STORE":
                    slots[arg] = stack.pop()
                elif name in MNEMONIC_TO_BINARY:
                    right = stack.pop()
                    left = stack.pop()
                    stack.append(binary(MNEMONIC_TO_BINARY[name], left, right))
                elif name == "NEG":
                    stack.append(negate(stack.pop()))
                elif name == "NOT":
                    stack.append(logical_not(stack.pop()))
                elif name == "ITOF":
                    stack.append(float(stack.pop()))
                elif name == "JMP":
                    pc = arg
                elif name == "JZ":
                    if stack.pop() == 0:
                        pc = arg
                elif name == "JNZ":
                    if stack.pop() != 0:
                        pc = arg
                elif name == "PRINT":
                    self.out.write(format_value(stack.pop()) + "\n")
                elif name == "HALT":
                    return steps
                else:
                    raise RuntimeFault(f"invalid instruction {name}")
        finally:
            self.steps = steps
