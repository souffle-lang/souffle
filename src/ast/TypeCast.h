/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TypeCast.h
 *
 * Defines the type cast class
 *
 ***********************************************************************/

#pragma once

#include "ast/Argument.h"
#include "ast/QualifiedName.h"
#include "parser/SrcLocation.h"
#include <iosfwd>

namespace souffle::ast {

/**
 * @class TypeCast
 * @brief Defines a type cast class for expressions
 */

class TypeCast : public Argument {
public:
    TypeCast(Own<Argument> value, QualifiedName type, SrcLocation loc = {});

    /** Return value */
    Argument* getValue() const {
        return value.get();
    }

    /** Return cast type */
    const QualifiedName& getType() const {
        return type;
    }

    /** Set cast type */
    void setType(QualifiedName type);

    void apply(const NodeMapper& map) override;

protected:
    void print(std::ostream& os) const override;

    NodeVec getChildren() const override;

private:
    bool equal(const Node& node) const override;

    TypeCast* cloning() const override;

private:
    /** Casted value */
    Own<Argument> value;

    /** Cast type */
    QualifiedName type;
};

}  // namespace souffle::ast
