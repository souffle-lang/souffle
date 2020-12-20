from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext

setup(
    # Information
    name = "souffle",
    # version = "1.0.0",
    url = "https://github.com/souffle-lang/souffle",
    # license = "...",
    # keywords = "...",
    packages=["souffle"],
    package_dir={"": "src/pybind/python"},
    include_package_data=True
)
