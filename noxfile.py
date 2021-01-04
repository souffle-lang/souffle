import nox

@nox.session()
def tests(session):
    session.run("poetry", "install", external=True)
    session.run("pytest", "--cov", *session.posargs, "tests/python")

# FIXME: Linting, doctest, mypy
