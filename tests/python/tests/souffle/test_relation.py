import re

import pytest

from tests.simple_program import sp_check_A, sp_check_B, sp_check_C


def test_empty(simple_program):
    simple_program.purge_all_relations()

    assert len(simple_program.input_relations) == 1
    assert len(simple_program.internal_relations) == 1
    assert len(simple_program.output_relations) == 1
    assert len(simple_program.relations) == 3

    assert list(simple_program.input_relations.keys()) == ["A"]
    assert list(simple_program.internal_relations.keys()) == ["B"]
    assert list(simple_program.output_relations.keys()) == ["C"]
    assert set("A B C".split()) == set(simple_program.relations.keys())

    sp_check_A(simple_program, tuples=[])
    sp_check_B(simple_program, tuples=[])
    sp_check_C(simple_program, tuples=[])


def test_run(simple_program):
    simple_program.run()


def test_relation_values(simple_program):
    sp_check_A(simple_program, tuples=[(1, "Hi"), (2, "Bye")])
    sp_check_B(simple_program, tuples=[(1,), (2,)])
    sp_check_C(simple_program, tuples=[(1,), (2,)])


def test_relation_contains(simple_program):
    rel_A = simple_program.relations["A"]
    ttA = rel_A.named_tuple_type

    assert rel_A.contains((1, "Hi"))
    assert rel_A.contains(ttA(a=1, b="Hi"))

    assert not rel_A.contains((1, "Bye"))
    assert not rel_A.contains(ttA(a=1, b="Bye"))

    with pytest.raises(RuntimeError) as exc:
        rel_A.contains((1, "Bye", "what?"))
    assert re.search("tuple of arity 3 but relation A has arity 2", str(exc))

    with pytest.raises(RuntimeError) as exc:
        rel_A.contains(("Bye", 1))
    assert re.search("Type conversion for tuple element 0", str(exc))


def test_insert(simple_program):
    rel_A = simple_program.relations["A"]
    ttA = rel_A.named_tuple_type

    # Insert a single element
    new_tuple = (9, "Adios")
    rel_A.insert(new_tuple)

    # Insert multiple at the same time, this time as
    # typed tuples
    new_elems = [ttA(10, "Hello"), ttA(11, "World")]
    rel_A.insert(new_elems)

    sp_check_A(simple_program, tuples=[
               (1, "Hi"), (2, "Bye")] + [new_tuple] + new_elems)


def test_purge(simple_program):
    simple_program.run()
    rel_A = simple_program.relations["A"]

    # Purge a single relation
    rel_A.purge()
    sp_check_A(simple_program, tuples=[])
