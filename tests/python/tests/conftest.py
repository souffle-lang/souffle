import pytest

import souffle
import tests.simple_program as sp

def pytest_addoption(parser):
    parser.addoption("--souffle-path", action="store", default="souffle")

@pytest.fixture(scope="session")
def simple_program(pytestconfig):
    souffle_path = pytestconfig.getoption("--souffle-path")
    return souffle.Program.compile_str(sp.SIMPLE_PROGRAM, name="simple_program", souffle=pytestconfig.getoption("--souffle-path"))
