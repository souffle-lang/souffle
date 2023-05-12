/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AliasType.h
 *
 * Defines the subset type class
 *
 ***********************************************************************/

#pragma once

#include "ast/QualifiedName.h"
#include "ast/Type.h"
#include "parser/SrcLocation.h"
#include <iosfwd>

namespace souffle::ast {

/**
 * @class AliasType
 * @brief Defines alias type class
 *
 * Example:
 *    .type A = B
 */
class AliasType : public Type {
public:
    AliasType(QualifiedName name, QualifiedName aliasTypeName, SrcLocation loc = {});

    /** Return alias type */
    const QualifiedName& getAliasType() const {
        return aliasType;
    }

    /** Set alias type */
    void setAliasType(const QualifiedName& type) {
        aliasType = type;
    }

    static bool classof(const Node*);

protected:
    void print(std::ostream& os) const override;

private:
    bool equal(const Node& node) const override;

    AliasType* cloning() const override;

private:
    /** Base type */
    QualifiedName aliasType;
};

}  // namespace souffle::ast
