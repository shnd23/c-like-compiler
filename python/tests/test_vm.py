import io

import pytest

from minic.errors import RuntimeFault
from minic.pipeline import compile_source
from minic.vm import VM


def execute(source, max_steps=1_000_000, optimize=True):
    result = compile_source(source, optimize_code=optimize)
    assert result.ok
    buffer = io.StringIO()
    machine = VM(result.bytecode, buffer, max_steps)
    machine.run()
    return buffer.getvalue().split(), machine


@pytest.mark.parametrize("optimize", [True, False])
@pytest.mark.parametrize(
    "source, expected",
    [
        ("print(1 + 2 * 3);", ["7"]),
        ("print((1 + 2) * 3);", ["9"]),
        ("print(7 / 2);", ["3"]),
        ("print(-7 / 2);", ["-3"]),
        ("print(7 / -2);", ["-3"]),
        ("print(-7 % 3);", ["-1"]),
        ("print(7 % -3);", ["1"]),
        ("print(7.0 / 2);", ["3.5"]),
        ("print(1 / 3.0);", ["0.3333333333"]),
        ("print(2 < 3);", ["1"]),
        ("print(3 <= 2);", ["0"]),
        ("print(1 == 1.0);", ["1"]),
        ("print(1 != 1);", ["0"]),
        ("print(!0);", ["1"]),
        ("print(!7);", ["0"]),
        ("print(-(-4));", ["4"]),
        ("print(1 && 2);", ["1"]),
        ("print(0 || 0);", ["0"]),
        ("print(0 || 5);", ["1"]),
        ("float f = 3; print(f);", ["3.0"]),
        ("int x; print(x);", ["0"]),
        ("float f; print(f);", ["0.0"]),
    ],
)
def test_expression_results(source, expected, optimize):
    assert execute(source, optimize=optimize)[0] == expected


def test_integer_overflow_wraps():
    out, _ = execute("int m = 9223372036854775807; print(m + 1); print(-m - 2);")
    assert out == ["-9223372036854775808", "9223372036854775807"]


def test_min_int_division_wraps():
    out, _ = execute("int m = -9223372036854775807 - 1; print(m / -1);")
    assert out == ["-9223372036854775808"]


def test_short_circuit_and_skips_right_side():
    out, _ = execute("int z = 0; print(z != 0 && 1 / z > 0);", optimize=False)
    assert out == ["0"]


def test_short_circuit_or_skips_right_side():
    out, _ = execute("int z = 0; print(z == 0 || 1 / z > 0);", optimize=False)
    assert out == ["1"]


def test_division_by_zero_raises_at_runtime():
    with pytest.raises(RuntimeFault, match="division by zero"):
        execute("int z = 0; print(1 / z);")


def test_modulo_by_zero_raises_at_runtime():
    with pytest.raises(RuntimeFault, match="division by zero"):
        execute("int z = 0; print(1 % z);")


def test_float_division_by_zero_raises():
    with pytest.raises(RuntimeFault):
        execute("float z = 0.0; print(1.0 / z);")


def test_step_limit_stops_infinite_loop():
    with pytest.raises(RuntimeFault, match="step limit"):
        execute("while (1) { }", max_steps=1000)


def test_step_count_is_reported():
    _, machine = execute("print(1);")
    assert machine.steps >= 2


def test_while_loop_accumulates():
    out, _ = execute("int i = 0; int s = 0; while (i < 5) { s = s + i; i = i + 1; } print(s);")
    assert out == ["10"]


def test_nested_loops():
    source = """
    int i = 0; int count = 0;
    while (i < 4) { int j = 0; while (j < 3) { count = count + 1; j = j + 1; } i = i + 1; }
    print(count);
    """
    assert execute(source)[0] == ["12"]


def test_scoping_runtime_values():
    source = "int x = 1; { int x = 2; { int x = 3; print(x); } print(x); } print(x);"
    assert execute(source)[0] == ["3", "2", "1"]


def test_block_variable_reinitialized_each_iteration():
    source = "int i = 0; while (i < 3) { int fresh; print(fresh); fresh = 9; i = i + 1; }"
    assert execute(source)[0] == ["0", "0", "0"]


def test_assignment_converts_int_to_float():
    assert execute("float f; f = 5; print(f / 2);")[0] == ["2.5"]


def test_mixed_expression_promotes_to_float():
    assert execute("int a = 3; print(a * 1.5);")[0] == ["4.5"]


def test_large_float_formatting():
    assert execute("print(1.0 * 1000000 * 1000000 * 1000000);")[0] == ["1e+18"]


def test_comment_syntax_in_source():
    source = "int x = 1; // trailing\n/* block\n comment */ print(x + 1);"
    assert execute(source)[0] == ["2"]


def test_if_else_chain():
    source = """
    int v = 7;
    if (v < 5) print(1); else if (v < 10) print(2); else print(3);
    """
    assert execute(source)[0] == ["2"]
