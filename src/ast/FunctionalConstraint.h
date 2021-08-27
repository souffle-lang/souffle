/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file FunctionalConstraint.h
 *
 * Defines the functional constraint class
 *
 ***********************************************************************/

#pragma once

#include "ast/Constraint.h"
#include "ast/Variable.h"
#include "parser/SrcLocation.h"
#include <cstddef>
#include <iosfwd>
#include <vector>

namespace souffle::ast {

/**
 * @class FunctionalConstraint
 * @brief Functional constraint (choice construct) class
 *
 * Representing a functional dependency
 * Example:
 * .decl rel(x:symbol, y:symbol, z:number) choice-domain x
 * The functional constraint "x" makes sure every x in the rel uniquely defines an element.
 *
 * .decl rel(x:symbol, y:symbol, z:number) choice-domain x, y
 * A relation can have more then one key, each key uniquely defines an element in the relation.
 *
 * .decl rel(x:symbol, y:symbol, z:number) choice-domain (x, y)
 * Multiple attributes can serve as a single key, the pair (x,y) uniquely defines an element in the relation.
 */
class FunctionalConstraint : public Constraint {
public:
    FunctionalConstraint(VecOwn<Variable> keys, SrcLocation loc = {});

    FunctionalConstraint(Own<Variable> key, SrcLocation loc = {});

    /** get keys */
    std::vector<Variable*> getKeys() const;

    /** get arity of the keys (i.e. number of source nodes: (x,y)->z has an arity of 2) */
    std::size_t getArity() const {
        return keys.size();
    }

    bool equivalentConstraint(const FunctionalConstraint& other) const;

protected:
    void print(std::ostream& os) const override;

    NodeVec getChildren() const override;

private:
    bool equal(const Node& node) const override;

    FunctionalConstraint* cloning() const override;

private:
    /* Functional constraint */
    VecOwn<Variable> keys;
};

}  // namespace souffle::ast
