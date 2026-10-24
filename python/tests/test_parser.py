import pytest

from minic.ast_nodes import (
    Assign,
    Binary,
    Block,
    ExprStmt,
    If,
    Number,
    Print,
    Unary,
    Var,
    VarDecl,
    While,
    dump_ast,
)
from minic.lexer import Lexer
from minic.parser import Parser


def parse(source):
    tokens, lex_errors = Lexer(source).tokenize()
    assert lex_errors == []
    return Parser(tokens).parse()


def expr_of(source):
    program, errs = parse(source)
    assert errs == []
    return program.stmts[0].expr


def test_declaration_without_initializer():
    program, errs = parse("int x;")
    assert errs == []
    node = program.stmts[0]
    assert isinstance(node, VarDecl) and node.init is None and node.var_type == "int"


def test_declaration_with_initializer():
    program, _ = parse("float f = 1.5;")
    assert isinstance(program.stmts[0].init, Number)


def test_assignment_statement():
    program, _ = parse("x = 1;")
    assert isinstance(program.stmts[0], Assign)


def test_precedence_multiplication_over_addition():
    node = expr_of("1 + 2 * 3;")
    assert node.op == "+" and node.right.op == "*"


def test_left_associativity_of_subtraction():
    node = expr_of("8 - 3 - 2;")
    assert node.op == "-" and isinstance(node.left, Binary) and node.left.op == "-"


def test_parentheses_override_precedence():
    node = expr_of("(1 + 2) * 3;")
    assert node.op == "*" and node.left.op == "+"


def test_comparison_binds_looser_than_arithmetic():
    node = expr_of("1 + 2 < 3 * 4;")
    assert node.op == "<" and node.left.op == "+" and node.right.op == "*"


def test_equality_binds_looser_than_relational():
    node = expr_of("1 < 2 == 3 < 4;")
    assert node.op == "==" and node.left.op == "<"


def test_logical_precedence_and_over_or():
    node = expr_of("a || b && c;")
    assert node.op == "||" and node.right.op == "&&"


def test_unary_operators_nest():
    node = expr_of("- -x;")
    assert isinstance(node, Unary) and isinstance(node.operand, Unary)


def test_not_operator():
    node = expr_of("!x == 1;")
    assert node.op == "==" and isinstance(node.left, Unary) and node.left.op == "!"


def test_if_without_else():
    program, _ = parse("if (x) y = 1;")
    node = program.stmts[0]
    assert isinstance(node, If) and node.else_branch is None


def test_if_with_else_block():
    program, _ = parse("if (x) { y = 1; } else { y = 2; }")
    node = program.stmts[0]
    assert isinstance(node.then_branch, Block) and isinstance(node.else_branch, Block)


def test_else_binds_to_nearest_if():
    program, _ = parse("if (a) if (b) x = 1; else x = 2;")
    outer = program.stmts[0]
    assert outer.else_branch is None
    assert outer.then_branch.else_branch is not None


def test_while_statement():
    program, _ = parse("while (x < 3) x = x + 1;")
    assert isinstance(program.stmts[0], While)


def test_print_statement():
    program, _ = parse("print(1 + 2);")
    assert isinstance(program.stmts[0], Print)


def test_expression_statement():
    program, _ = parse("x + 1;")
    assert isinstance(program.stmts[0], ExprStmt)


def test_nested_blocks():
    program, errs = parse("{ { int x; } }")
    assert errs == []
    assert isinstance(program.stmts[0].stmts[0], Block)


def test_empty_program():
    program, errs = parse("")
    assert program.stmts == [] and errs == []


def test_dump_ast_shape():
    program, _ = parse("int x = 1 + 2;")
    assert dump_ast(program).split("\n")[:3] == ["Program", "  VarDecl int x", "    Binary +"]


def test_node_positions_point_at_operator():
    node = expr_of("1 +  2;")
    assert (node.line, node.col) == (1, 3)


@pytest.mark.parametrize(
    "source, fragment",
    [
        ("int x = 5", "expected ';'"),
        ("x = ;", "unexpected token ';'"),
        ("int = 3;", "expected identifier"),
        ("if x) y = 1;", "expected '('"),
        ("if (x y = 1;", "missing an operator"),
        ("(1 + 2;", "expected ')'"),
        ("print 1;", "expected '('"),
        ("x = x y;", "missing an operator"),
        ("else x = 1;", "without a matching 'if'"),
        ("while (1) int z;", "cannot be the body"),
    ],
)
def test_syntax_error_messages(source, fragment):
    _, errs = parse(source)
    assert errs and fragment in errs[0].message


def test_error_recovery_reports_multiple_errors():
    _, errs = parse("int a = ;\nint b = 1;\nint c = ;\n")
    assert [e.line for e in errs] == [1, 3]


def test_recovery_inside_block_does_not_escape_it():
    program, errs = parse("{ int a = ; int b = 2; } int c = 3;")
    assert len(errs) == 1
    assert len(program.stmts) == 2


def test_unclosed_block_reports_error():
    _, errs = parse("{ int x;")
    assert "expected '}'" in errs[0].message


def test_stray_closing_brace_terminates_without_hanging():
    _, errs = parse("}")
    assert errs


def test_excessive_nesting_is_rejected_not_crashed():
    source = "int x = " + "(" * 400 + "1" + ")" * 400 + ";"
    _, errs = parse(source)
    assert any("too deep" in e.message for e in errs)


def test_excessive_block_nesting_is_rejected():
    _, errs = parse("{" * 400 + "}" * 400)
    assert any("too deep" in e.message for e in errs)
