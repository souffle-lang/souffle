import collections
import typing as ty

import souffle.libsouffle_py as _py  # type: ignore


class ProvenanceTuple(ty.NamedTuple):
    rule: int
    height: int


class RelationTuple(ty.NamedTuple):
    data: ty.Tuple
    provenance: ty.Tuple


class Relation:
    _rel: _py.Relation
    _tt: ty.Optional[ty.Type[ty.Tuple]]

    def __init__(self, rel: _py.Relation):
        self._rel = rel
        self._tt = None

    @property
    def name(self) -> str:
        return self._rel.get_name()

    @property
    def arity(self) -> int:
        return self.get_arity()

    def get_arity(self, data_only: bool = True) -> int:
        if data_only:
            return self._rel.get_primary_arity()

        else:
            return self._rel.get_arity()

    @property
    def aux_arity(self) -> int:
        return self._rel.get_auxiliary_arity()

    @property
    def has_provenance(self) -> bool:
        return self.aux_arity == 0

    @property
    def signature(self) -> str:
        return self.get_signature()

    def get_signature(self, data_only: bool = True) -> str:
        full_types = self._rel.get_attr_types()
        return f"<{','.join(full_types)}>"

    def __len__(self):
        return len(self._rel)

    def __repr__(self):
        return f"Relation {self.name}({self.signature}) with {len(self)} tuples"

    def __iter__(self):
        return self.tuples

    @property
    def raw_tuples(self) -> ty.Iterator[tuple]:
        return self.get_raw_tuples()

    def get_raw_tuples(self, data_only: bool = True, split: bool = True) -> ty.Iterator[tuple]:
        return self._rel.iter(data_only, split)

    @property
    def named_tuple_type(self, data_only=True) -> ty.Type:
        if self._tt is None:
            self._tt = collections.namedtuple(  # type: ignore
                f"{self.name}_tuple",
                self.attr_names)

        return self._tt

    @property
    def tuples(self) -> ty.Iterator[ty.Tuple]:
        return self.get_tuples()

    def get_tuples(self, data_only: bool = True) -> ty.Iterator[ty.Tuple]:
        tt = self.named_tuple_type
        if data_only:
            return map(lambda x: tt(*x), self.raw_tuples)

        else:
            # At the moment, only arity 2 provenance is supported
            tups = self.get_raw_tuples(data_only=False)
            if self.aux_arity == 0:
                # Prov will be an empty tuple, just reuse it
                return (RelationTuple(tt(*vals), prov) for (vals, prov) in tups)

            elif self.aux_arity == 2:
                return (RelationTuple(tt(*vals), ProvenanceTuple(*prov)) for (vals, prov) in tups)

            else:
                assert self.aux_arity in (0, 2), "Unexpected auxiliary arity"

    @property
    def attr_names(self) -> ty.Tuple[str]:
        return self.get_attr_names()

    def get_attr_names(self, data_only: bool = True) -> ty.Tuple[str]:
        names = self._rel.get_attr_names()
        return names[:self.get_arity(data_only)]

    @property
    def attr_types(self) -> ty.Tuple[str]:
        return self.get_attr_types()

    def get_attr_types(self, data_only: bool = True) -> ty.Tuple[str]:
        return self.named_tuple_type(*(t for (_, t) in self._split_attr_types(data_only)))

    @property
    def short_attr_types(self) -> ty.Tuple[str]:
        return self.get_short_attr_types()

    def get_short_attr_types(self, data_only: bool = True) -> ty.Tuple[str]:
        return self.named_tuple_type(*(t for (t, _) in self._split_attr_types(data_only)))

    @property
    def full_attr_types(self) -> ty.Tuple[str]:
        return self.get_full_attr_types()

    def get_full_attr_types(self, data_only: bool = True) -> ty.Tuple[str]:
        return self.named_tuple_type(*self._rel.get_attr_types()[:self.get_arity(data_only)])

    def _split_attr_types(self, data_only: bool) -> ty.Iterator[ty.Tuple[str, str]]:
        def split_type(v: str) -> ty.Tuple[str, str]:
            s, l = v.split(":")
            return (s, l)

        return map(split_type, self._rel.get_attr_types()[:self.get_arity(data_only)])

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
