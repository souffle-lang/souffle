/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Type.h
 *
 * Defines the type class
 *
 ***********************************************************************/

#pragma once

#include "ast/Node.h"
#include "ast/QualifiedName.h"
#include "parser/SrcLocation.h"

namespace souffle::ast {

/**
 *  @class Type
 *  @brief An abstract base class for types
 */
class Type : public Node {
public:
    Type(NodeKind kind, QualifiedName name = {}, SrcLocation loc = {});

    /** Return type name */
    const QualifiedName& getQualifiedName() const {
        return name;
    }

    /** Set type name */
    void setQualifiedName(QualifiedName name);

    static bool classof(const Node*);

private:
    /** type name */
    QualifiedName name;
};

}  // namespace souffle::ast
