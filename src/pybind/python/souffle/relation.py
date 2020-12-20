import collections
import typing as ty

import souffle._souffle_py as _py


class Relation:
    def __init__(self, rel: _py.Relation):
        self._rel = rel
        self._tt = None

    @property
    def name(self) -> str:
        return self._rel.get_name()

    @property
    def arity(self) -> int:
        return self._rel.get_arity()

    @property
    def signature(self) -> str:
        return self._rel.get_signature()

    def __len__(self):
        return len(self._rel)

    def __repr__(self):
        return f"Relation {self.name}({self.signature}) with {len(self)} tuples"

    def __iter__(self):
        return self.tuples

    @property
    def raw_tuples(self) -> ty.Iterator[tuple]:
        return iter(self._rel)

    @property
    def named_tuple_type(self) -> ty.Type:
        if self._tt is None:
            self._tt = collections.namedtuple(
                f"{self.name}_tuple", self.attr_names)

        return self._tt

    @property
    def tuples(self) -> ty.Iterator[ty.NamedTuple]:
        tt = self.named_tuple_type
        return map(lambda x: tt(*x), self.raw_tuples)

    @property
    def attr_names(self) -> ty.Tuple[str]:
        return self._rel.get_attr_names()

    @property
    def attr_types(self) -> ty.Tuple[str]:
        return self.named_tuple_type(*(t for (_, t) in self._split_attr_types()))

    @property
    def short_attr_types(self) -> ty.Tuple[str]:
        return self.named_tuple_type(*(t for (t, _) in self._split_attr_types()))

    @property
    def full_attr_types(self) -> ty.Tuple[str]:
        return self.named_tuple_type(*self._rel.get_attr_types())

    def _split_attr_types(self) -> ty.Iterator[ty.Tuple[str, str]]:
        return map(lambda v: tuple(v.split(":")), self._rel.get_attr_types())

    def contains(self, tup: ty.Tuple) -> bool:
        return self._rel.contains(tup)

    def insert(self, elem: ty.Union[ty.Tuple, ty.Iterable[ty.Tuple]]) -> None:
        if isinstance(elem, tuple):
            elem = [elem]

        else:
            elem = list(elem)

        self._rel.insert(elem)

    def purge(self):
        self._rel.purge()
