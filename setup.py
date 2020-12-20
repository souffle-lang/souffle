from distutils.core import setup

setup(
    # Information
    name="souffle",
    # version = "1.0.0",
    url="https://github.com/souffle-lang/souffle",
    # license = "...",
    # keywords = "...",
    packages=["souffle"],
    package_dir={"": "src/pybind/python"},
    package_data={"souffle": ["_souffle_py.*"]},
)
