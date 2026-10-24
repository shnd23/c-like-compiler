import argparse
import contextlib
import io
import os
import pathlib
import random
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))

from minic.cli import main as python_main

MODES = [
    [],
    ["-O0"],
    ["--tac", "--no-run"],
    ["--tokens", "--ast", "--symbols", "--bytecode", "--no-run"],
]
MAX_STEPS = "20000"
BINARY_OPS = ["+", "-", "*", "/", "%", "<", ">", "<=", ">=", "==", "!=", "&&", "||"]
NOISE = list(";(){}+-*/%<>=!&|$#@ 0123456789abcxyz.\n\t") + ["é", "else", "int", "while"]


class Generator:
    def __init__(self, rng):
        self.rng = rng
        self.scopes = [{}]
        self.counter = 0

    def visible(self):
        merged = {}
        for scope in self.scopes:
            merged.update(scope)
        return merged

    def literal(self, want):
        if want == "any" and self.rng.random() < 0.4:
            return f"{self.rng.randint(0, 99)}.{self.rng.randint(0, 99)}", "float"
        if self.rng.random() < 0.04:
            return "9223372036854775807", "int"
        return str(self.rng.randint(0, 20)), "int"

    def leaf(self, want):
        names = [n for n, t in self.visible().items() if want == "any" or t == "int"]
        if names and self.rng.random() < 0.65:
            name = self.rng.choice(names)
            return name, self.visible()[name]
        return self.literal(want)

    def expr(self, depth, want="any"):
        roll = self.rng.random()
        if depth <= 0 or roll < 0.2:
            return self.leaf(want)
        if roll < 0.3:
            text, kind = self.expr(depth - 1, want)
            return f"-{text}", kind
        if roll < 0.36:
            text, _ = self.expr(depth - 1, "any")
            return f"!{text}", "int"
        if roll < 0.44:
            text, kind = self.expr(depth - 1, want)
            return f"({text})", kind
        if roll < 0.62:
            op = self.rng.choice(["<", ">", "<=", ">=", "==", "!=", "&&", "||"])
            left, _ = self.expr(depth - 1, "any")
            right, _ = self.expr(depth - 1, "any")
            return f"{left} {op} {right}", "int"
        if roll < 0.72:
            left, _ = self.expr(depth - 1, "int")
            right, _ = self.expr(depth - 1, "int")
            if self.rng.random() < 0.75:
                right = f"({right}) * ({right}) + 1"
            return f"(({left}) % ({right}))", "int"
        op = self.rng.choice(["+", "-", "*", "/"])
        left, lk = self.expr(depth - 1, want)
        right, rk = self.expr(depth - 1, want)
        if op == "/" and self.rng.random() < 0.75:
            right = f"({right}) * ({right}) + 1"
        return f"{left} {op} {right}", "float" if "float" in (lk, rk) else "int"

    def new_name(self):
        self.counter += 1
        return f"{self.rng.choice(['v', 'w', 'total', 'x'])}{self.counter}"

    def block(self, depth, count):
        self.scopes.append({})
        lines = [self.statement(depth - 1) for _ in range(count)]
        self.scopes.pop()
        return "{\n" + "\n".join(lines) + "\n}"

    def statement(self, depth):
        roll = self.rng.random()
        visible = self.visible()
        if roll < 0.3 or not visible:
            kind = self.rng.choice(["int", "float"])
            name = self.new_name()
            text = f"{kind} {name};"
            if self.rng.random() < 0.8:
                value, _ = self.expr(3, "int" if kind == "int" else "any")
                text = f"{kind} {name} = {value};"
            self.scopes[-1][name] = kind
            return text
        if roll < 0.5:
            name = self.rng.choice(list(visible))
            value, _ = self.expr(3, "int" if visible[name] == "int" else "any")
            return f"{name} = {value};"
        if roll < 0.65:
            value, _ = self.expr(3)
            return f"print({value});"
        if depth <= 0:
            value, _ = self.expr(2)
            return f"print({value});"
        if roll < 0.8:
            cond, _ = self.expr(2)
            text = f"if ({cond}) {self.block(depth, 2)}"
            if self.rng.random() < 0.5:
                text += f" else {self.block(depth, 2)}"
            return text
        if roll < 0.9:
            counter = f"i{self.counter}"
            self.counter += 1
            limit = self.rng.randint(0, 6)
            self.scopes[-1][counter] = "int"
            body = self.block(depth, 2)
            body = body[:-1] + f"{counter} = {counter} + 1;\n}}"
            return f"int {counter} = 0;\nwhile ({counter} < {limit}) {body}"
        if roll < 0.95:
            return self.block(depth, 2)
        value, _ = self.expr(3)
        return f"{value};"

    def program(self):
        return "\n".join(self.statement(3) for _ in range(self.rng.randint(2, 10))) + "\n"


def mutate(rng, source):
    text = list(source)
    for _ in range(rng.randint(1, 3)):
        if not text:
            break
        index = rng.randrange(len(text))
        action = rng.random()
        if action < 0.35:
            del text[index]
        elif action < 0.7:
            text.insert(index, rng.choice(NOISE))
        else:
            text[index] = rng.choice(NOISE)
    return "".join(text)


def run_python(workdir, flags):
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        try:
            code = python_main(flags + ["--max-steps", MAX_STEPS, "prog.mc"])
        except SystemExit as stop:
            code = stop.code
    return out.getvalue(), err.getvalue(), code


def run_c(binary, workdir, flags):
    proc = subprocess.run(
        [binary, *flags, "--max-steps", MAX_STEPS, "prog.mc"],
        cwd=workdir,
        capture_output=True,
        timeout=30,
    )
    return (
        proc.stdout.decode("utf-8", "replace"),
        proc.stderr.decode("utf-8", "replace"),
        proc.returncode,
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True)
    parser.add_argument("--count", type=int, default=500)
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()

    rng = random.Random(args.seed)
    binary = str(pathlib.Path(args.binary).resolve())
    mismatches = 0
    compiled = 0
    failed_compiles = 0
    runtime_faults = 0

    with tempfile.TemporaryDirectory() as workdir:
        os.chdir(workdir)
        for case in range(args.count):
            source = Generator(rng).program()
            if rng.random() < 0.2:
                source = mutate(rng, source)
            pathlib.Path("prog.mc").write_bytes(source.encode("utf-8"))
            optimized_result = None
            for flags in MODES:
                expected = run_python(workdir, flags)
                if flags == []:
                    optimized_result = expected
                elif (
                    flags == ["-O0"]
                    and expected != optimized_result
                    and "step limit" not in expected[1]
                    and "step limit" not in optimized_result[1]
                ):
                    mismatches += 1
                    print(f"OPTIMIZER CHANGED BEHAVIOUR case={case}")
                    print(source)
                    print(repr(optimized_result)[:600])
                    print(repr(expected)[:600])
                    if mismatches >= 3:
                        sys.exit(1)
                actual = run_c(binary, workdir, flags)
                if flags == []:
                    if expected[2] == 0:
                        compiled += 1
                    elif expected[2] == 1:
                        failed_compiles += 1
                    elif expected[2] == 3:
                        runtime_faults += 1
                if expected != actual:
                    mismatches += 1
                    print(f"MISMATCH case={case} flags={flags}")
                    print("--- source ---")
                    print(source)
                    print("--- python ---")
                    print(repr(expected)[:800])
                    print("--- c ---")
                    print(repr(actual)[:800])
                    if mismatches >= 3:
                        sys.exit(1)

    print(
        f"cases={args.count} modes={len(MODES)} ran_ok={compiled} "
        f"compile_errors={failed_compiles} runtime_faults={runtime_faults} mismatches={mismatches}"
    )
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()
