# MiniC

A complete compiler toolchain for a small C-like language, implemented twice from the same specification: once in **Python** and once in **C11**. Source code goes through a hand-written lexer, an error-recovering recursive-descent parser, scoped semantic analysis with type checking, a three-address-code optimizer, and a stack-based virtual machine.

The two implementations produce byte-identical tokens, syntax trees, symbol tables, optimized IR, bytecode, program output and error messages. This is enforced by shared golden tests and a differential fuzzer.

```c
int n = 27;
int steps = 0;

while (n != 1) {
    if (n % 2 == 0) {
        n = n / 2;
    } else {
        n = 3 * n + 1;
    }
    steps = steps + 1;
}

print(steps);
```

```
$ minic examples/collatz.mc
111
```

## Features

| Stage | What it does |
| --- | --- |
| Lexer | Hand-written scanner with line/column tracking, `//` and `/* */` comments, longest-match operators, literal range checks, collects every lexical error |
| Parser | Recursive descent with one function per precedence level, builds a typed AST, panic-mode error recovery so one run reports many syntax errors, nesting-depth guard |
| Semantic analysis | Scoped symbol table with shadowing, undeclared/redeclared detection, `int` to `float` promotion, rejection of lossy `float` to `int` assignment |
| Code generation | Three-address code with temporaries, labels, short-circuit `&&` / `\|\|`, explicit `itof` conversions |
| Optimizer | Constant folding and propagation, constant-branch elimination, unreachable-code removal, jump threading, copy merging, dead-temporary removal, run to a fixed point |
| Backend | Lowers IR to stack bytecode executed by a VM with 64-bit wraparound integers, IEEE doubles, runtime checks and an instruction budget |
| CLI | `--tokens --ast --symbols --tac --bytecode -O0 --no-run --stats --max-steps`, compiler-style diagnostics with source excerpt and caret |

Diagnostics look like a real compiler's:

```
multiple.mc:1:9: error: unexpected token ';'
    1 | int a = ;
      |         ^
multiple.mc:5:11: error: a declaration cannot be the body of 'while'; wrap it in a block
    5 | while (1) int z = 3;
      |           ^
compilation failed: 2 errors
```

## Quick start

Python (3.10+, no dependencies):

```
cd python
python -m minic ../examples/fibonacci.mc
pip install .          # optional: installs the `minic` command
```

C (any C11 compiler, tested with gcc and clang):

```
cd c
make
./build/minic ../examples/fibonacci.mc
```

Inspect what the compiler does:

```
$ minic examples/gcd.mc --tac --no-run
== tac ==
    a = 1071
    b = 462
L0:
    $t0 = b != 0
    ifz $t0 goto L1
    r = a % b
    a = b
    b = r
    goto L0
L1:
    print a

$ minic examples/gcd.mc --bytecode --no-run
== bytecode ==
0000  PUSH  1071
0001  STORE 0 a
0002  PUSH  462
0003  STORE 1 b
0004  LOAD  1 b
0005  PUSH  0
0006  NE
0007  STORE 3 $t0
0008  LOAD  3 $t0
0009  JZ    0019
...
```

The typed AST makes promotion visible:

```
$ minic prog.mc --ast --no-run          # int x = 1 + 2 * 3; print(x);
== ast ==
Program
  VarDecl int x
    Binary + : int
      Number 1 : int
      Binary * : int
        Number 2 : int
        Number 3 : int
  Print
    Var x : int
```

## The language

Two types (`int`, `float`), block scoping with shadowing, `if` / `else`, `while`, `print`, full arithmetic, comparison and logical operators, and comments. The full grammar, precedence table, type rules and error catalogue are in [docs/language.md](docs/language.md). Example programs live in [examples/](examples/): factorial with 64-bit wraparound, Fibonacci, GCD, primes, Collatz, Newton's method for the square root of 2, short-circuit evaluation and more.

## Architecture

```
source ──► Lexer ──► tokens ──► Parser ──► AST ──► Semantic analysis ──► typed AST + symbol table
                                                                              │
                                              Optimizer ◄── TAC ◄── Code generation
                                                  │
                                                  ▼
                                      optimized TAC ──► Bytecode lowering ──► Stack VM ──► output
```

| Stage | Python (`python/minic/`) | C (`c/src/`) |
| --- | --- | --- |
| Tokens, lexer | `tokens.py`, `lexer.py` | `token.c`, `lexer.c` |
| AST, parser | `ast_nodes.py`, `parser.py` | `ast.c`, `parser.c` |
| Symbols, semantics | `symbols.py`, `semantic.py` | `symbols.c`, `semantic.c` |
| IR, code generation | `ir.py`, `codegen.py` | `ir.c`, `codegen.c` |
| Optimizer | `optimizer.py` | `optimizer.c` |
| Bytecode, VM | `bytecode.py`, `vm.py` | `bytecode.c`, `vm.c` |
| Value semantics | `ops.py` | `ops.c` |
| Diagnostics | `errors.py` | `errors.c` |
| Driver | `pipeline.py`, `cli.py` | `pipeline.c`, `main.c` |

Design notes:

- **Recursive descent over a table.** The project began as an LL(1) table-driven recognizer. Recursive descent was chosen as the foundation because it builds an AST naturally, gives precise error positions, and recovers from errors.
- **Error recovery.** Each statement is parsed inside a recovery frame. On failure the parser records the error and skips to `;`, `}` or a statement keyword, so one run reports every independent mistake. In C this uses `setjmp`/`longjmp`; in Python, exceptions.
- **Explicit conversions.** Code generation inserts `itof` wherever an `int` meets a `float`, so the IR and VM never reason about mixed types.
- **Optimizer safety.** Constant propagation forgets everything at labels, so values never leak across loop back-edges. Divisions by a constant zero are never folded, preserving the runtime error. Passes repeat until nothing changes.
- **Memory in C.** All compiler data (tokens, AST, symbols, IR, bytecode) is allocated from one arena and released in a single call. The VM uses a fixed-size stack.
- **Portable arithmetic.** Integer wraparound, truncating division and `INT64_MIN / -1` are handled explicitly so results never depend on undefined behaviour or on the host language.

## Testing

| Layer | Where | What |
| --- | --- | --- |
| Python unit tests | `python/tests/` | 202 pytest cases: lexer, parser, semantic analysis, optimizer, VM, CLI |
| C unit tests | `c/tests/unit.c` | 143 checks covering the same layers |
| Golden tests | `tests/golden.sh`, `tests/golden/` | 102 cases run against **both** binaries: tokens, AST, symbols, TAC (optimized and not), bytecode, program output and error output for every example and error case |
| Differential fuzzer | `tools/diff_fuzz.py` | Generates random type-correct programs plus mutated invalid ones and compares stdout, stderr and exit code of Python vs C in four modes; also checks that optimized and unoptimized runs agree |
| Sanitizers | `make -C c asan` | The C unit and golden tests under AddressSanitizer and UBSan |

```
make test             # python tests, C unit + golden tests, 500-program fuzz run
make asan             # C tests under sanitizers
make golden-update    # regenerate golden files from the Python implementation
```

Before packaging, 3,800 random programs (about 1,400 of which ran to completion) were compared across the two implementations with zero mismatches; the C build is warning-free under `-Wall -Wextra -Wpedantic -Wshadow` with gcc and clang.

## Performance

The same compiled bytecode, a prime-counting loop executing 17.8 million VM instructions (measured once in the development sandbox; your numbers will differ):

| Implementation | Time |
| --- | --- |
| Python | about 3.0 s |
| C | about 0.06 s |

## Project layout

```
python/        Python package (minic/), tests, pyproject.toml
c/             C implementation: src/, include/, tests/, Makefile
examples/      Example MiniC programs
tests/         Shared golden runner, golden files, error and edge-case inputs
tools/         Differential fuzzer
docs/          Language reference
.github/       CI: pytest on 3.10–3.13, C on gcc and clang with sanitizers, cross-check fuzz
```

## Limitations and ideas

- No functions, arrays, strings or `for`/`break`; the IR and VM are structured so functions (call frames) are the natural next step.
- Variables live in numbered slots; there is no register allocation, and comparison results are materialized before branching (visible in the GCD listing above), so a fused compare-and-branch would be a worthwhile optimization.
- A second backend (emitting C or WebAssembly from the TAC) would reuse everything up to the optimizer.
