/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file BranchInit.h
 *
 * Defines an argument covering the branch initialization of ADTs.
 *
 ***********************************************************************/

#pragma once

#include "ast/Argument.h"
#include "ast/QualifiedName.h"
#include "ast/Term.h"
#include "parser/SrcLocation.h"
#include <iosfwd>
#include <string>

namespace souffle::ast {

/**
 * @class BranchInit
 * @brief Initialization of ADT instance.
 *
 * @param name An entity used to create a variant type. Can be though of as a name of the branch.
 *
 * Initializes one of the branches of ADT. The syntax for branches initialization is
 * $Constructor(args...)
 * In case of the branch with no arguments it is simplified to $Constructor.
 */
class BranchInit : public Term {
public:
    BranchInit(QualifiedName name, VecOwn<Argument> args, SrcLocation loc = {});

    /** get ADT branch name */
    const QualifiedName& getBranchName() const {
        return name;
    }

    /** set ADT branch name */
    void setBranchName(QualifiedName name) {
        this->name = name;
    }

protected:
    void print(std::ostream& os) const override;

private:
    /** Implements the node comparison for this node type */
    bool equal(const Node& node) const override;

    BranchInit* cloning() const override;

private:
    /** Name of ADT Branch */
    QualifiedName name;
};

}  // namespace souffle::ast
