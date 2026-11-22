"""
Souffle - A Datalog Compiler
Copyright (c) 2019, The Souffle Developers. All rights reserved
Licensed under the Universal Permissive License v 1.0 as shown at:
- https://opensource.org/licenses/UPL
- <souffle root>/licenses/SOUFFLE-UPL.txt

Test for programmatic tuple insertion via Python SWIG bindings.
This test demonstrates inserting tuples directly from Python instead of loading from files.
"""

import SwigInterface

# Create program instance
p = SwigInterface.newInstance('test_relation_insert')

# Get the edge relation
edge = p.getRelation('edge')

# Insert edges programmatically (no file I/O)
edges = [("A", "B"), ("B", "C"), ("C", "D"), ("D", "E"), ("E", "F"), ("F", "A")]
for src, dst in edges:
    t = edge.createTuple()
    t.putString(src)
    t.putString(dst)
    edge.insert(t)

# Run the program to compute paths
p.run()

# Get the path relation and iterate over results
path = p.getRelation('path')

# Collect and sort results for deterministic output
results = []
for t in path:
    src = t.getString()
    dst = t.getString()
    results.append((src, dst))

results.sort()
for src, dst in results:
    print(f"{src}\t{dst}")

# Also output to CSV for test comparison
p.printAll('.')

# Cleanup
p.thisown = 1
del p
