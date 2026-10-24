import pytest

from minic.pipeline import compile_source


def analyze(source):
    return compile_source(source)


def typed(source):
    result = analyze(source)
    assert result.ok, [e.message for e in result.errors]
    return result.program.stmts[-1].value.type


def test_valid_program_has_no_errors(errors):
    assert errors("int x = 1; float y = x + 2.5; print(y);") == []


def test_undeclared_variable_use(errors):
    assert errors("print(x);") == [(1, 7, "undeclared variable 'x'")]


def test_undeclared_variable_assignment(errors):
    assert errors("x = 1;") == [(1, 1, "undeclared variable 'x'")]


def test_redeclaration_in_same_scope(errors):
    found = errors("int x;\nint x;")
    assert len(found) == 1
    assert "already declared" in found[0][2] and "line 1" in found[0][2]


def test_shadowing_in_inner_scope_is_allowed(errors):
    assert errors("int x = 1; { int x = 2; print(x); }") == []


def test_variable_not_visible_after_block(errors):
    assert errors("{ int y = 1; } print(y);") == [(1, 22, "undeclared variable 'y'")]


def test_variable_used_in_own_initializer_is_rejected(errors):
    assert errors("int x = x + 1;") == [(1, 9, "undeclared variable 'x'")]


def test_inner_initializer_sees_outer_variable(errors):
    assert errors("int x = 1; { int x = x + 1; }") == []


def test_float_to_int_declaration_rejected(errors):
    assert errors("int n = 2.5;")[0][2] == "cannot assign float to int variable 'n'"


def test_float_to_int_assignment_rejected(errors):
    assert "cannot assign float" in errors("int n; n = 1 + 0.5;")[0][2]


def test_int_to_float_is_allowed(errors):
    assert errors("float f = 3; f = 4;") == []


def test_modulo_with_float_rejected(errors):
    assert "requires integer operands" in errors("float f = 1.0; print(f % 2);")[0][2]


def test_modulo_with_ints_allowed(errors):
    assert errors("print(7 % 3);") == []


def test_errors_sorted_by_position(errors):
    found = errors("print(b);\nprint(a);")
    assert [e[0] for e in found] == [1, 2]


def test_error_in_operand_does_not_cascade(errors):
    assert len(errors("int x = (y + 1) * 2;")) == 1


def test_if_condition_and_branches_are_checked(errors):
    assert len(errors("if (a) { b = 1; } else { c = 2; }")) == 3


def test_while_body_is_checked(errors):
    assert len(errors("while (1) { z = 1; }")) == 1


def test_integer_arithmetic_type():
    assert typed("print(1 + 2);") == "int"


def test_float_promotion_type():
    assert typed("print(1 + 2.0);") == "float"


def test_comparison_yields_int():
    assert typed("print(1.5 < 2.5);") == "int"


def test_logical_yields_int():
    assert typed("print(1.5 && 2.5);") == "int"


def test_not_yields_int():
    assert typed("print(!2.5);") == "int"


def test_negation_keeps_type():
    assert typed("print(-2.5);") == "float"


def test_symbol_ids_are_unique_across_shadowing():
    result = analyze("int x; { int x; { float x; } }")
    assert [s.ir_name for s in result.table.symbols] == ["x", "x.1", "x.2"]
    assert [s.depth for s in result.table.symbols] == [0, 1, 2]


def test_lexical_errors_stop_pipeline_before_parsing():
    result = analyze("int x = 1 $ 2;")
    assert result.program is None and len(result.errors) == 1


def test_syntax_errors_stop_pipeline_before_semantic_analysis():
    result = analyze("int x = ; y = 1;")
    assert result.table is None
