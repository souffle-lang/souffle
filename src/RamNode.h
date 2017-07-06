/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
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

#include <iostream>
#include <vector>

namespace souffle {

enum RamNodeType {

    // values
    RN_ElementAccess,
    RN_Number,
    RN_UnaryOperator,
    RN_BinaryOperator,
    RN_TernaryOperator,
    RN_AutoIncrement,
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
    RN_Swap,

    // control flow
    RN_Sequence,
    RN_Loop,
    RN_Parallel,
    RN_Exit,
    RN_LogTimer,
    RN_DebugInfo
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
    virtual ~RamNode() = default;

    RamNodeType getNodeType() const {
        return type;
    }

    /** Obtains a list of all embedded child nodes */
    virtual std::vector<const RamNode*> getChildNodes() const = 0;

    /** Requires all RAM nodes to be printable */
    virtual void print(std::ostream& out = std::cout) const = 0;

    /** Add support for printing nodes */
    friend std::ostream& operator<<(std::ostream& out, const RamNode& node) {
        node.print(out);
        return out;
    }
};

}  // end of namespace souffle
