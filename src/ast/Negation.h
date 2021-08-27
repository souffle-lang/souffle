/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Negation.h
 *
 * Define the negated atom class
 *
 ***********************************************************************/

#pragma once

#include "ast/Atom.h"
#include "ast/Literal.h"
#include "parser/SrcLocation.h"
#include <iosfwd>

namespace souffle::ast {

/**
 * @class Negation
 * @brief Negation of an atom negated atom
 *
 * Example:
 *     !parent(x,y).
 *
 * A negated atom can only occur in the body of a clause.
 */
class Negation : public Literal {
public:
    Negation(Own<Atom> atom, SrcLocation loc = {});

    /** Get negated atom */
    Atom* getAtom() const {
        return atom.get();
    }

    void apply(const NodeMapper& map) override;

protected:
    void print(std::ostream& os) const override;

    NodeVec getChildren() const override;

private:
    bool equal(const Node& node) const override;

    Negation* cloning() const override;

private:
    /** Negated atom */
    Own<Atom> atom;
};

}  // namespace souffle::ast
