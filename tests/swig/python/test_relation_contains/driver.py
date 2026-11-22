"""
Souffle - A Datalog Compiler
Copyright (c) 2019, The Souffle Developers. All rights reserved
Licensed under the Universal Permissive License v 1.0 as shown at:
- https://opensource.org/licenses/UPL
- <souffle root>/licenses/SOUFFLE-UPL.txt

Test for tuple existence checking via Python SWIG bindings.
"""

import SwigInterface

# Create program instance
p = SwigInterface.newInstance('test_relation_contains')

# Get the edge relation and insert data
edge = p.getRelation('edge')

edges = [("A", "B"), ("B", "C"), ("C", "D")]
for src, dst in edges:
    t = edge.createTuple()
    t.putString(src)
    t.putString(dst)
    edge.insert(t)

# Run the program
p.run()

# Get the path relation
path = p.getRelation('path')

# Test contains for tuples that should exist
def check_contains(rel, src, dst):
    t = rel.createTuple()
    t.putString(src)
    t.putString(dst)
    return rel.contains(t)

# Direct edges should exist as paths
print(f"path contains (A, B): {check_contains(path, 'A', 'B')}")
print(f"path contains (B, C): {check_contains(path, 'B', 'C')}")
print(f"path contains (C, D): {check_contains(path, 'C', 'D')}")

# Transitive paths should exist
print(f"path contains (A, C): {check_contains(path, 'A', 'C')}")
print(f"path contains (A, D): {check_contains(path, 'A', 'D')}")
print(f"path contains (B, D): {check_contains(path, 'B', 'D')}")

# Non-existent paths
print(f"path contains (D, A): {check_contains(path, 'D', 'A')}")
print(f"path contains (X, Y): {check_contains(path, 'X', 'Y')}")

# Also output to CSV for test comparison
p.printAll('.')

# Cleanup
p.thisown = 1
del p
