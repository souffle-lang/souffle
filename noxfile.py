import pathlib
import sys

import nox

# This is a hack to extract autoconf paths into python
_THIS_FILE = pathlib.Path(__file__).resolve()
_THIS_DIR = _THIS_FILE.parent
_SRC_DIR = _THIS_DIR / "src"

sys.path.append(str(_SRC_DIR))
from python_paths import POETRY_PATH

@nox.session()
def tests(session):
    session.run(POETRY_PATH, "install", external=True)
    session.run("pytest", *session.posargs, "tests/python")

# FIXME: Linting, doctest, mypy
