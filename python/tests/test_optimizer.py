from minic.ir import format_tac
from minic.pipeline import compile_source


def tac(source, optimize=True):
    result = compile_source(source, optimize_code=optimize)
    assert result.ok
    return [line.strip() for line in format_tac(result.tac).split("\n") if line.strip()]


def test_constant_expression_is_folded():
    assert tac("int x = 2 + 3 * 4; print(x);") == ["x = 14", "print 14"]


def test_unoptimized_keeps_temporaries():
    lines = tac("int x = 2 + 3;", optimize=False)
    assert len(lines) >= 2 and any("$t" in line for line in lines)


def test_float_folding():
    assert tac("print(1.5 + 2);")[-1] == "print 3.5"


def test_comparison_folding():
    assert tac("print(3 < 4);") == ["print 1"]


def test_division_by_zero_is_not_folded():
    lines = tac("print(1 / 0);")
    assert any("/" in line for line in lines)


def test_constant_true_branch_removes_else():
    lines = tac("int r; if (1) r = 1; else r = 2; print(r);")
    assert "r = 2" not in lines and "print 1" in lines


def test_constant_false_branch_removes_then():
    lines = tac("int r; if (0) r = 1; else r = 2; print(r);")
    assert "r = 1" not in lines


def test_while_false_is_eliminated():
    lines = tac("int i = 0; while (0) { i = i + 1; } print(i);")
    assert not any(line.endswith(":") or line.startswith("goto") for line in lines)


def test_loop_variables_are_not_propagated_across_back_edge():
    lines = tac("int i = 0; while (i < 3) { i = i + 1; } print(i);")
    assert any("i < 3" in line for line in lines)
    assert "print i" in lines


def test_dead_temporaries_removed():
    lines = tac("int x = 1; x + 2;")
    assert not any("$t" in line for line in lines)


def test_short_circuit_constant_folds():
    assert tac("int z = 0; print(z != 0 && 10 / z > 1);")[-1] == "print 0"


def test_unreachable_code_after_goto_removed():
    lines = tac("int i = 1; while (i) { i = 0; }")
    assert len(lines) < 12


def test_optimized_and_unoptimized_agree(run):
    source = "int i = 0; int s = 0; while (i < 10) { s = s + i * i; i = i + 1; } print(s);"
    assert run(source, optimize=True) == run(source, optimize=False) == ["285"]


def test_optimizer_is_idempotent():
    from minic.optimizer import optimize

    result = compile_source("int a = 3; int b = 0; while (b < a) { b = b + 1; } print(b);")
    once = format_tac(result.tac)
    again = format_tac(optimize(result.tac))
    assert once == again


def test_optimizer_does_not_mutate_raw_tac():
    result = compile_source("int x = 1 + 2; print(x);")
    assert len(result.raw_tac) > len(result.tac)
    assert any("+" in line for line in format_tac(result.raw_tac).split("\n"))
