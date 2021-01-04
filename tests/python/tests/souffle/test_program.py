import pytest

import souffle

from tests.simple_program import sp_check_A, sp_check_B, sp_check_C

# run_all
# print_all
# Invalid creation
# Different output dirs
# Multiple programs
# Multiple with the same name
# Flags (e.g. compile with -w)

def test_purge(simple_program):
    simple_program.run()

    # Reinsert, check again
    sp_check_A(simple_program, tuples=[(1, "Hi"), (2, "Bye")])
    sp_check_B(simple_program, tuples=[(1,), (2,)])
    sp_check_C(simple_program, tuples=[(1,), (2,)])

    # Purge only input relations
    simple_program.purge_input_relations()
    sp_check_A(simple_program, tuples=[])
    sp_check_B(simple_program, tuples=[(1,), (2,)])
    sp_check_C(simple_program, tuples=[(1,), (2,)])

    # Purge only internal relations
    rel_A = simple_program.relations["A"]
    rel_A.insert((1, "Again"))
    simple_program.purge_internal_relations()
    sp_check_A(simple_program, tuples=[(1, "Again")])
    sp_check_B(simple_program, tuples=[])
    sp_check_C(simple_program, tuples=[(1,), (2,)])

    # Purge only output relations
    rel_B = simple_program.relations["B"]
    rel_B.insert((3,))
    simple_program.purge_output_relations()
    sp_check_A(simple_program, tuples=[(1, "Again")])
    sp_check_B(simple_program, tuples=[(3,)])
    sp_check_C(simple_program, tuples=[])

    # Purge all relations
    rel_C = simple_program.relations["C"]
    rel_C.insert((4,))
    sp_check_A(simple_program, tuples=[(1, "Again")])
    sp_check_B(simple_program, tuples=[(3,)])
    sp_check_C(simple_program, tuples=[(4,)])

    simple_program.purge_all_relations()
    sp_check_A(simple_program, tuples=[])
    sp_check_B(simple_program, tuples=[])
    sp_check_C(simple_program, tuples=[])
