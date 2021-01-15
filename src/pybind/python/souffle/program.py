import ctypes
import os
import pathlib
import subprocess
import sys
import tempfile
import typing as ty

import souffle.libsouffle_py as _py  # type: ignore
import souffle.relation as rel

_PATH_TYPE = ty.Union[pathlib.Path, str]


def _get_plat_extension() -> str:
    plat = sys.platform
    os_name = os.name

    if os_name == "posix":
        if sys.platform.startswith("darwin"):
            return ".dylib"

        else:
            return ".so"

    elif plat.startswith("win32"):
        return ".dll"

    else:
        raise RuntimeError("Unable to determine native platform extension")


def _get_plat_dll():
    os_name = os.name
    if os_name == "posix":
        return ctypes.cdll

    elif os_name == "nt":
        return ctypes.windll

    else:
        raise RuntimeError("Unable to determine native platform loader")


def _make_relations(rels: ty.Iterable[_py.Relation]) -> ty.Dict[str, rel.Relation]:
    return {r.get_name(): rel.Relation(r) for r in rels}


class Program:
    __loaded: ty.Set = set()
    __loaded_paths: ty.List[pathlib.Path] = []

    _name: str
    _path: pathlib.Path
    _program: _py.Program
    _output_relations: ty.Optional[ty.Dict[str, rel.Relation]]
    _input_relations: ty.Optional[ty.Dict[str, rel.Relation]]
    _internal_relations: ty.Optional[ty.Dict[str, rel.Relation]]
    _relations: ty.Optional[ty.Dict[str, rel.Relation]]

    def __init__(self, name, path: _PATH_TYPE = None):
        if not name:
            raise ValueError("Program name must be non-empty")

        if name in self.__loaded:
            raise ValueError(f"Program with the name '{name}' is already loaded")

        if not path:
            path = pathlib.Path(name)

        else:
            path = pathlib.Path(path)

        if not path.suffix:
            path = path.with_suffix(_get_plat_extension())

        self._path = self.load_program(path)
        self._name = name
        self._program = _py.Program(name)
        self.__loaded.add(name)
        self._output_relations = None
        self._input_relations = None
        self._internal_relations = None
        self._relations = None

    @staticmethod
    def compile_str(program: str, name: str, work_dir: ty.Optional[_PATH_TYPE] = None, flags: ty.Sequence[str] = (), souffle: _PATH_TYPE = "souffle") -> "Program":
        if not work_dir:
            # This will get cleaned up later automatically
            temp_dir = tempfile.TemporaryDirectory()
            work_dir = temp_dir.name

        work_dir = pathlib.Path(work_dir)

        if not work_dir.is_dir():
            raise RuntimeError(
                f"Ouput directory '{work_dir}' is not a directory")

        # Will get cleaned up later
        temp_file = work_dir / f"{name}.dl"
        temp_file.write_text(program)
        return Program.compile(temp_file, output_name=temp_file.with_suffix(""),
                               name=name, flags=flags, souffle=souffle)

    @staticmethod
    def compile(dl_file: _PATH_TYPE, name: ty.Optional[str] = None, flags: ty.Sequence[str] = (), output_name: ty.Optional[_PATH_TYPE] = None, souffle: _PATH_TYPE = "souffle") -> "Program":
        dl_file = pathlib.Path(dl_file)

        name = name or str(dl_file.with_suffix("").name)
        output_name = output_name or name

        output_flags = ["--dl-program", str(output_name)]
        command_line = ([str(souffle), "-s", "pybind"] +
                        output_flags + list(flags) + [str(dl_file)])

        proc = subprocess.run(command_line,
                              stderr=subprocess.PIPE, stdout=subprocess.PIPE)

        if proc.returncode or len(proc.stderr):
            raise RuntimeError(
                f"Compilation failed: Command line '{command_line}' resulted in\n{proc.stderr.decode()}")

        return Program(name, output_name)

    @ property
    def name(self) -> str:
        return self._name

    @ property
    def path(self) -> pathlib.Path:
        return self._path

    def load_program(self, path: _PATH_TYPE) -> pathlib.Path:
        abs_path=pathlib.Path(path).resolve()

        def same_file(path) -> bool:
            return path.exists() and os.path.samefile(abs_path, path)

        same=map(same_file, self.__loaded_paths)
        if not any(same):
            loader=_get_plat_dll()
            loader.LoadLibrary(abs_path)
            self.__loaded_paths.append(abs_path)

        return abs_path

    def run(self) -> None:
        self._program.run()

    def run_all(self, input_dir: _PATH_TYPE="", output_dir: _PATH_TYPE="") -> None:
        input_dir="" if input_dir is None else str(input_dir)
        output_dir="" if output_dir is None else str(output_dir)
        self._program.run_all(input_dir, output_dir)

    @ property
    def output_relations(self) -> ty.Dict[str, rel.Relation]:
        if self._output_relations is None:
            self._output_relations=_make_relations(
                self._program.get_output_relations())

        return self._output_relations

    @ property
    def input_relations(self) -> ty.Dict[str, rel.Relation]:
        if self._input_relations is None:
            self._input_relations=_make_relations(
                self._program.get_input_relations())

        return self._input_relations

    @ property
    def internal_relations(self) -> ty.Dict[str, rel.Relation]:
        if self._internal_relations is None:
            self._internal_relations=_make_relations(
                self._program.get_internal_relations())

        return self._internal_relations

    @ property
    def relations(self) -> ty.Dict[str, rel.Relation]:
        if self._relations is None:
            self._relations=_make_relations(
                self._program.get_all_relations())

        return self._relations

    def __repr__(self):
        return f"Souffle program '{self.name}' at '{self.path}'"

    def purge_input_relations(self):
        self._program.purge_input_relations()

    def purge_output_relations(self):
        self._program.purge_output_relations()

    def purge_internal_relations(self):
        self._program.purge_internal_relations()

    def purge_all_relations(self):
        # Not implemented in C++!
        for rr in self.relations.values():
            rr.purge()

    def print_all(self, output_dir: _PATH_TYPE=None):
        # TODO: Is this still the best name for this?  I would prefer
        # dump_outputs but that means something else in the C++ code.
        # RFC?
        output_dir="" if output_dir is None else str(output_dir)
        self._program.print_all(output_dir)
