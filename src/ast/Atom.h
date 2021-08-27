/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Atom.h
 *
 * Defines the atom class
 *
 ***********************************************************************/

#pragma once

#include "ast/Argument.h"
#include "ast/Literal.h"
#include "ast/QualifiedName.h"
#include "parser/SrcLocation.h"
#include "souffle/utility/Types.h"
#include <cstddef>
#include <iosfwd>
#include <vector>

namespace souffle::ast {

/**
 * @class Atom
 * @brief An atom class
 *
 * An atom representing the use of a relation
 * either in the head or in the body of a clause,
 * e.g., parent(x,y), !parent(x,y), ...
 */
class Atom : public Literal {
public:
    Atom(QualifiedName name = {}, VecOwn<Argument> args = {}, SrcLocation loc = {});

    /** Return qualified name */
    const QualifiedName& getQualifiedName() const {
        return name;
    }

    /** Return arity of the atom */
    std::size_t getArity() const {
        return arguments.size();
    }

    /** Set qualified name */
    void setQualifiedName(QualifiedName n);

    /** Add argument to the atom */
    void addArgument(Own<Argument> arg);

    /** Return arguments */
    std::vector<Argument*> getArguments() const;

    void apply(const NodeMapper& map) override;

protected:
    void print(std::ostream& os) const override;

    NodeVec getChildren() const override;

private:
    bool equal(const Node& node) const override;

    Atom* cloning() const override;

private:
    /** Name of atom */
    QualifiedName name;

    /** Arguments of atom */
    VecOwn<Argument> arguments;
};

}  // namespace souffle::ast
