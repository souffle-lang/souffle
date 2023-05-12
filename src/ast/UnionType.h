/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file UnionType.h
 *
 * Defines the union type class
 *
 ***********************************************************************/

#pragma once

#include "ast/QualifiedName.h"
#include "ast/Type.h"
#include "parser/SrcLocation.h"
#include <cstddef>
#include <iosfwd>
#include <vector>

namespace souffle::ast {

/**
 * @class UnionType
 * @brief The union type class
 *
 * Example:
 *  .type A = B1 | B2 | ... | Bk
 *
 * A union type combines multiple types into a new super type.
 * Each of the enumerated types become a sub-type of the new
 * union type.
 */
class UnionType : public Type {
public:
    UnionType(QualifiedName name, std::vector<QualifiedName> types, SrcLocation loc = {});

    /** Return list of unioned types */
    const std::vector<QualifiedName>& getTypes() const {
        return types;
    }

    std::vector<QualifiedName>& getTypes() {
        return types;
    }

    /** Add another unioned type */
    void add(QualifiedName type);

    /** Set type */
    void setType(std::size_t idx, QualifiedName type);

    static bool classof(const Node*);

protected:
    void print(std::ostream& os) const override;

private:
    bool equal(const Node& node) const override;

    UnionType* cloning() const override;

private:
    /** List of unioned types */
    std::vector<QualifiedName> types;
};

}  // namespace souffle::ast
