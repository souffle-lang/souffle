"""
Souffle - A Datalog Compiler
Copyright (c) 2019, The Souffle Developers. All rights reserved
Licensed under the Universal Permissive License v 1.0 as shown at:
- https://opensource.org/licenses/UPL
- <souffle root>/licenses/SOUFFLE-UPL.txt

Test for different attribute types via Python SWIG bindings.
Tests: symbol (string), number (signed int), unsigned, and float types.
"""

import SwigInterface

# Create program instance
p = SwigInterface.newInstance('test_relation_types')

# Get the data relation
data = p.getRelation('data')

# Print type info
print(f"Relation: {data.getName()}")
print(f"Arity: {data.getArity()}")
for i in range(data.getArity()):
    print(f"  Attr {i}: {data.getAttrName(i)} ({data.getAttrType(i)})")

# Insert tuples with different types
test_data = [
    ("Alice", -10, 100, 3.14),
    ("Bob", 0, 200, 2.71),
    ("Charlie", 42, 300, 1.41),
]

for name, num, value, score in test_data:
    t = data.createTuple()
    t.putString(name)
    t.putInt(num)
    t.putUInt(value)
    t.putFloat(score)
    data.insert(t)

# Run the program (identity - no rules, just output what was inserted)
p.run()

# Read back and verify
print(f"\nStored {data.size()} tuples:")
results = []
for t in data:
    name = t.getString()
    num = t.getInt()
    value = t.getUInt()
    score = t.getFloat()
    results.append((name, num, value, score))

results.sort()
for name, num, value, score in results:
    print(f"  {name}: num={num}, value={value}, score={score:.2f}")

# Also output to CSV for test comparison
p.printAll('.')

# Cleanup
p.thisown = 1
del p
