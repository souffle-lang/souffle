/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SwigInterface.i
 *
 * SWIG interface file that transforms SwigInterface.h to the necessary language and creates a wrapper file
 * for it
 *
 ***********************************************************************/

%module SwigInterface
%include "std_string.i"
%include "std_map.i"
%include<std_vector.i>
namespace std {
    %template(map_string_string) map<string, string>;
}

%{
#include "SwigInterface.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>
using namespace std;
souffle::SouffleProgram* prog;
souffle::Relation* rel;
souffle::Relation* rel_out;
%}

// Memory management directives - tell SWIG these methods return new objects
%newobject newInstance;
%newobject SWIGSouffleProgram::getRelation;
%newobject SWIGRelation::createTuple;
%newobject SWIGRelation::iterator;
%newobject SWIGRelationIterator::next;

%include "SwigInterface.h"

SWIGSouffleProgram* newInstance(const std::string& name);

// Python-specific extensions for iteration support
#ifdef SWIGPYTHON
%extend SWIGRelation {
%pythoncode %{
    def __iter__(self):
        """Iterate over all tuples in this relation"""
        it = self.iterator()
        while True:
            t = it.next()
            if t is None:
                break
            yield t

    def __len__(self):
        """Return the number of tuples in this relation"""
        return self.size()
%}
}

%extend SWIGTuple {
%pythoncode %{
    def __len__(self):
        """Return the number of elements in this tuple"""
        return self.size()
%}
}
#endif
