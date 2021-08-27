/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AlgebraicDataType.h
 *
 * Defines a node corresponding to an ast declaration of Algebraic Data Type
 *
 ***********************************************************************/

#pragma once

#include "ast/BranchType.h"
#include "ast/QualifiedName.h"
#include "ast/Type.h"
#include "parser/SrcLocation.h"
#include <iosfwd>
#include <vector>

namespace souffle::ast {

/**
 * @class AlgebraicDataType
 * @brief Combination of types using sums and products.
 *
 * ADT combines a simpler types using product types and sum types.
 *
 * Example:
 * .type Nat = S {n : Nat} | Zero {}
 *
 * The type Nat has two branches, S which takes element of type Nat and Zero which doesn't take any
 * arguments.
 *
 */
class AlgebraicDataType : public Type {
public:
    AlgebraicDataType(QualifiedName name, VecOwn<BranchType> branches, SrcLocation loc = {});

    std::vector<BranchType*> getBranches() const;

protected:
    void print(std::ostream& os) const override;

    NodeVec getChildren() const override;

private:
    bool equal(const Node& node) const override;

    AlgebraicDataType* cloning() const override;

private:
    /** The list of branches for this sum type. */
    VecOwn<BranchType> branches;
};

}  // namespace souffle::ast
