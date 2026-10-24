import io
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

import pytest

from minic.pipeline import compile_source
from minic.vm import VM


@pytest.fixture
def run():
    def runner(source, optimize=True, max_steps=1_000_000):
        result = compile_source(source, optimize_code=optimize)
        assert result.ok, [(e.line, e.col, e.message) for e in result.errors]
        buffer = io.StringIO()
        VM(result.bytecode, buffer, max_steps).run()
        return buffer.getvalue().split()

    return runner


@pytest.fixture
def errors():
    def collector(source):
        result = compile_source(source)
        return [(e.line, e.col, e.message) for e in result.errors]

    return collector
