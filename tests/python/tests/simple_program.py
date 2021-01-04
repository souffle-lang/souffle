import pytest

SIMPLE_PROGRAM = """
.decl A(a: number, b: symbol)
.decl B(x: number)
.decl C(y: number)

// input
.input A
A(1, "Hi").
A(2, "Bye").

// internal
B(x) :- A(x, _).

// output
C(y) :- B(y).

.output C
"""

def check_relation(relation, name, fields, long_types, short_types, tuples):
    arity = len(fields)
    full_types = tuple(f"{s}:{l}" for (s, l) in zip(short_types, long_types))
    signature = f"<{','.join(full_types)}>"
    assert relation.name == name
    assert relation.arity == arity
    assert relation.signature == signature
    assert len(relation) == len(tuples)
    assert repr(
        relation) == f"Relation {name}({signature}) with {len(tuples)} tuples"
    assert relation.attr_types == long_types
    assert relation.short_attr_types == short_types
    full_types = tuple(f"{s}:{l}" for (s, l) in zip(short_types, long_types))
    assert relation.full_attr_types == full_types
    assert list(relation.raw_tuples) == tuples

    assert issubclass(relation.named_tuple_type, tuple)
    assert relation.named_tuple_type._fields == fields
    for t1, t2 in zip(relation, tuples):
        # Check all the named value tuples
        for attr, val in zip(fields, t2):
            assert getattr(t1, attr) == val


def sp_check_A(simple_program, tuples):
    check_relation(simple_program.relations["A"], name="A", fields=("a", "b"),
                    long_types=("number", "symbol"), short_types=("i", "s"), tuples=tuples)


def sp_check_B(simple_program, tuples):
    check_relation(
        simple_program.relations["B"], name="B", fields=("x",), long_types=("number",), short_types=("i",), tuples=tuples)


def sp_check_C(simple_program, tuples):
    check_relation(
        simple_program.relations["C"], name="C", fields=("y",), long_types=("number",), short_types=("i",), tuples=tuples)
