/*
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All Rights reserved
 * 
 * The Universal Permissive License (UPL), Version 1.0
 * 
 * Subject to the condition set forth below, permission is hereby granted to any person obtaining a copy of this software,
 * associated documentation and/or data (collectively the "Software"), free of charge and under any and all copyright rights in the 
 * Software, and any and all patent rights owned or freely licensable by each licensor hereunder covering either (i) the unmodified 
 * Software as contributed to or provided by such licensor, or (ii) the Larger Works (as defined below), to deal in both
 * 
 * (a) the Software, and
 * (b) any piece of software and/or hardware listed in the lrgrwrks.txt file if one is included with the Software (each a “Larger
 * Work” to which the Software is contributed by such licensors),
 * 
 * without restriction, including without limitation the rights to copy, create derivative works of, display, perform, and 
 * distribute the Software and make, use, sell, offer for sale, import, export, have made, and have sold the Software and the 
 * Larger Work(s), and to sublicense the foregoing rights on either these or other terms.
 * 
 * This license is subject to the following condition:
 * The above copyright notice and either this complete permission notice or at a minimum a reference to the UPL must be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/************************************************************************
 *
 * @file RamNode.h
 *
 * Top level syntactic element of intermediate representation,
 * i.e., a node of the RAM machine code.
 *
 ***********************************************************************/

#pragma once

#include <vector>
#include <iostream>

namespace souffle {

enum RamNodeType {

    // values
    RN_ElementAccess,
    RN_Number,
    RN_BinaryOperator,
    RN_AutoIncrement,
    RN_Ord,
    RN_Negation,
    RN_Complement,
    RN_Not,
    RN_Pack,

    // conditions
    RN_NotExists,
    RN_Empty,
    RN_And,
    RN_BinaryRelation,

    // operations
    RN_Project,
    RN_Lookup,
    RN_Scan,
    RN_Aggregate,

    // statements
    RN_Create,
    RN_Fact,
    RN_Load,
    RN_Store,
    RN_Insert,
    RN_Clear,
    RN_Drop,
    RN_PrintSize,
    RN_LogSize,

    RN_Merge,

    RN_Sequence,
    RN_Loop,
    RN_Parallel,
    RN_Exit,
    RN_LogTimer
};


/**
 *  @class RamNode
 *  @brief RamNode is a superclass for all elements of the RAM IR.
 */
class RamNode {

    const RamNodeType type;

public:

    RamNode(RamNodeType type) : type(type) {}

    /** A virtual destructor for RAM nodes */
    virtual ~RamNode() {}

    RamNodeType getNodeType() const {
        return type;
    }

    /** Obtains a list of all embedded child nodes */
    virtual std::vector<const RamNode*> getChildNodes() const =0;

    /** Requires all RAM nodes to be printable */
    virtual void print(std::ostream& out = std::cout) const = 0;

    /** Add support for printing nodes */
    friend std::ostream& operator<<(std::ostream& out, const RamNode& node) {
        node.print(out);
        return out;
    }

};

} // end of namespace souffle

