import pathlib
import sys

import nox

# This is a hack to extract autoconf paths into python
sys.path.append("src")
from python_paths import POETRY_PATH

_THIS_FILE = pathlib.Path(__file__).resolve()
_SOUFFLE_PATH = _THIS_FILE.parent / "src/souffle"

@nox.session()
def tests(session):
    session.run(POETRY_PATH, "install", external=True)
    session.run("pytest", "--cov", *session.posargs, f"--souffle-path={_SOUFFLE_PATH}", "tests/python")

# FIXME: Linting, doctest, mypy
