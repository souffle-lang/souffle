"""
Souffle - A Datalog Compiler
Copyright (c) 2019, The Souffle Developers. All rights reserved
Licensed under the Universal Permissive License v 1.0 as shown at:
- https://opensource.org/licenses/UPL
- <souffle root>/licenses/SOUFFLE-UPL.txt

Test for relation iteration and metadata access via Python SWIG bindings.
"""

import SwigInterface
import sys

# Create program instance
p = SwigInterface.newInstance('test_relation_iterate')

# Load input from facts directory
p.loadAll(sys.argv[1])

# Run the program
p.run()

# Get the path relation
path = p.getRelation('path')

# Test metadata access
print(f"Name: {path.getName()}")
print(f"Arity: {path.getArity()}")
print(f"Size: {path.size()}")
print(f"Signature: {path.getSignature()}")

# Print attribute info
for i in range(path.getArity()):
    print(f"Attr {i}: name={path.getAttrName(i)}, type={path.getAttrType(i)}")

# Test Python len() support
print(f"len(path): {len(path)}")

# Test iteration and collect results
print("Tuples:")
results = []
for t in path:
    src = t.getString()
    dst = t.getString()
    results.append((src, dst))

results.sort()
for src, dst in results:
    print(f"  {src} -> {dst}")

# Also output to CSV for test comparison
p.printAll('.')

# Cleanup
p.thisown = 1
del p
