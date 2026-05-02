"""
Souffle - A Datalog Compiler
Copyright (c) 2025, The Souffle Developers. All rights reserved
Licensed under the Universal Permissive License v 1.0 as shown at:
- https://opensource.org/licenses/UPL
- <souffle root>/licenses/SOUFFLE-UPL.txt

Test case for issue #2533: SWIG python bindings SEGFAULT if name of .dl file contains -

The bug: when a .dl file contains a hyphen in its name (e.g., "hyphen-name.dl"),
the identifier() function converts it to a valid C++ identifier by replacing
hyphens with underscores ("hyphen_name"). However, users calling newInstance()
expect to use the original filename with the hyphen.

This test verifies that newInstance("hyphen-name") works correctly.
"""

import SwigInterface
import sys

# This should work with the original filename containing a hyphen
# Before the fix, this would cause a SEGFAULT because:
# 1. The program is registered in the factory as "hyphen_name" (with underscore)
# 2. But newInstance("hyphen-name") looks for "hyphen-name" (with hyphen)
# 3. The factory returns nullptr
# 4. SWIGSouffleProgram tries to dereference nullptr -> SEGFAULT
p = SwigInterface.newInstance('hyphen-name')
p.loadAll(sys.argv[1])
p.run()
p.printAll('.')
p.thisown = 1
del p
