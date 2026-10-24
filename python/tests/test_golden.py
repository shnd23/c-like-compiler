import pathlib
import shutil
import subprocess
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
EXAMPLES = sorted((ROOT / "examples").glob("*.mc"))


@pytest.mark.skipif(shutil.which("sh") is None, reason="needs a POSIX shell")
def test_all_golden_cases_match():
    proc = subprocess.run(
        ["sh", str(ROOT / "tests" / "golden.sh"), "check", sys.executable, "-m", "minic"],
        capture_output=True,
        text=True,
        env={"PYTHONPATH": str(ROOT / "python"), "PATH": "/usr/bin:/bin"},
    )
    assert proc.returncode == 0, proc.stdout + proc.stderr


@pytest.mark.parametrize("path", EXAMPLES, ids=lambda p: p.stem)
def test_optimizer_preserves_behaviour(path, run):
    source = path.read_text()
    assert run(source, optimize=True) == run(source, optimize=False)


def test_examples_exist():
    assert len(EXAMPLES) >= 10
