/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstAbstract.h
 *
 * Abstract class definitions for AST nodes
 *
 ***********************************************************************/

#pragma once

#include "ast/AstNode.h"

namespace souffle {

/**
 * Literal
 * e.g. atoms, binary relations, and negated atoms
 * in the body and head of a clause.
 */
class AstLiteral : public AstNode {
public:
    using AstNode::AstNode;

    AstLiteral* clone() const override = 0;
};

/**
 * Logical constraint
 */
class AstConstraint : public AstLiteral {
public:
    using AstLiteral::AstLiteral;

    AstConstraint* clone() const override = 0;
};

/**
 * Argument
 */
class AstArgument : public AstNode {
public:
    using AstNode::AstNode;

    /** Create clone */
    AstArgument* clone() const override = 0;
};

}  // end of namespace souffle
