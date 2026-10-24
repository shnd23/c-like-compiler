import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]


def cli(tmp_path, source, *flags):
    path = tmp_path / "prog.mc"
    path.write_text(source)
    return subprocess.run(
        [sys.executable, "-m", "minic", *flags, "prog.mc"],
        cwd=tmp_path,
        capture_output=True,
        text=True,
        env={"PYTHONPATH": str(ROOT), "PATH": ""},
    )


def test_successful_run(tmp_path):
    proc = cli(tmp_path, "print(6 * 7);")
    assert proc.returncode == 0 and proc.stdout == "42\n"


def test_compile_error_exit_code_and_format(tmp_path):
    proc = cli(tmp_path, "int x = ;")
    assert proc.returncode == 1
    assert "prog.mc:1:9: error:" in proc.stderr
    assert "^" in proc.stderr
    assert "compilation failed: 1 error" in proc.stderr


def test_runtime_error_exit_code(tmp_path):
    proc = cli(tmp_path, "int z = 0; print(1 / z);")
    assert proc.returncode == 3 and "division by zero" in proc.stderr


def test_missing_file_exit_code(tmp_path):
    proc = subprocess.run(
        [sys.executable, "-m", "minic", "nope.mc"],
        cwd=tmp_path,
        capture_output=True,
        text=True,
        env={"PYTHONPATH": str(ROOT), "PATH": ""},
    )
    assert proc.returncode == 2


def test_tac_flag(tmp_path):
    proc = cli(tmp_path, "int x = 1; print(x);", "--tac", "--no-run")
    assert "== tac ==" in proc.stdout and "print" in proc.stdout


def test_bytecode_flag(tmp_path):
    proc = cli(tmp_path, "print(1);", "--bytecode", "--no-run")
    assert "HALT" in proc.stdout


def test_tokens_ast_symbols_flags(tmp_path):
    proc = cli(tmp_path, "int x = 1;", "--tokens", "--ast", "--symbols", "--no-run")
    for section in ("== tokens ==", "== ast ==", "== symbols =="):
        assert section in proc.stdout


def test_no_run_suppresses_output(tmp_path):
    proc = cli(tmp_path, "print(1);", "--no-run")
    assert proc.stdout == ""


def test_stats_flag(tmp_path):
    proc = cli(tmp_path, "int x = 1 + 2; print(x);", "--stats")
    assert "tac instructions" in proc.stderr and "vm steps" in proc.stderr


def test_max_steps_flag(tmp_path):
    proc = cli(tmp_path, "while (1) { }", "--max-steps", "500")
    assert proc.returncode == 3 and "step limit" in proc.stderr


def test_output_flushed_before_runtime_error(tmp_path):
    proc = cli(tmp_path, "int z = 0; print(1); print(1 / z);")
    assert proc.stdout == "1\n" and proc.returncode == 3


def test_error_snippet_preserves_tabs(tmp_path):
    proc = cli(tmp_path, "\tint x = ;")
    assert "\t" in proc.stderr.split("\n")[2]
