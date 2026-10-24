import argparse
import sys

from . import __version__
from .ast_nodes import dump_ast
from .bytecode import format_bytecode
from .errors import RuntimeFault, format_error
from .ir import format_tac
from .pipeline import compile_source
from .symbols import format_symbols
from .tokens import format_tokens
from .vm import DEFAULT_MAX_STEPS, VM

EXIT_OK = 0
EXIT_COMPILE_ERROR = 1
EXIT_USAGE = 2
EXIT_RUNTIME_ERROR = 3


def build_parser():
    parser = argparse.ArgumentParser(
        prog="minic", description="MiniC compiler and virtual machine"
    )
    parser.add_argument("file", help="source file to compile and run")
    parser.add_argument("--tokens", action="store_true", help="print the token stream")
    parser.add_argument("--ast", action="store_true", help="print the typed syntax tree")
    parser.add_argument("--symbols", action="store_true", help="print the symbol table")
    parser.add_argument("--tac", action="store_true", help="print the three-address code")
    parser.add_argument("--bytecode", action="store_true", help="print the stack VM bytecode")
    parser.add_argument("-O0", dest="optimize", action="store_false", help="disable optimizer")
    parser.add_argument("--no-run", action="store_true", help="compile without executing")
    parser.add_argument("--stats", action="store_true", help="print compilation statistics")
    parser.add_argument(
        "--max-steps",
        type=int,
        default=DEFAULT_MAX_STEPS,
        help="abort execution after this many VM steps",
    )
    parser.add_argument("--version", action="version", version=f"minic {__version__}")
    return parser


def emit_section(title, body):
    sys.stdout.write(f"== {title} ==\n")
    if body:
        sys.stdout.write(body + "\n")


def main(argv=None):
    args = build_parser().parse_args(argv)
    try:
        with open(args.file, "r", encoding="utf-8", errors="replace", newline="") as handle:
            source = handle.read()
    except OSError:
        sys.stderr.write(f"minic: cannot open '{args.file}'\n")
        return EXIT_USAGE

    result = compile_source(source, optimize_code=args.optimize)
    show_dumps = args.tokens or args.ast or args.symbols or args.tac or args.bytecode

    if args.tokens:
        emit_section("tokens", format_tokens(result.tokens))

    if result.errors:
        for error in result.errors:
            sys.stderr.write(format_error(args.file, source, error) + "\n")
        count = len(result.errors)
        sys.stderr.write(f"compilation failed: {count} error{'s' if count != 1 else ''}\n")
        return EXIT_COMPILE_ERROR

    if args.ast:
        emit_section("ast", dump_ast(result.program))
    if args.symbols:
        emit_section("symbols", format_symbols(result.table))
    if args.tac:
        emit_section("tac", format_tac(result.tac))
    if args.bytecode:
        emit_section("bytecode", format_bytecode(result.bytecode))

    if args.stats:
        sys.stderr.write(f"tac instructions : {len(result.raw_tac)} -> {len(result.tac)}\n")
        sys.stderr.write(f"bytecode size    : {len(result.bytecode.code)}\n")

    if args.no_run:
        return EXIT_OK

    if show_dumps:
        emit_section("output", "")

    machine = VM(result.bytecode, sys.stdout, args.max_steps)
    try:
        machine.run()
    except RuntimeFault as fault:
        sys.stdout.flush()
        sys.stderr.write(f"runtime error: {fault}\n")
        return EXIT_RUNTIME_ERROR

    if args.stats:
        sys.stderr.write(f"vm steps         : {machine.steps}\n")
    return EXIT_OK
