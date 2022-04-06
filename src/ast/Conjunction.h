/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Conjunction.h
 *
 * Define conjunction class
 *
 ***********************************************************************/

#pragma once

#include "ast/Literal.h"
#include "parser/SrcLocation.h"
#include <iosfwd>

namespace souffle::ast {

/**
 * @class Conjunction
 * @brief Conjunction of two literals
 *
 * Example:
 *     A /\ B.
 *
 * where A and B are literals.
 */
class Conjunction : public Literal {
public:
    Conjunction(Own<Literal> lhs, Own<Literal> rhs, SrcLocation loc = {}) :
       Literal(std::move(loc)), lhs(std::move(lhs)), rhs(std::move(rhs)) { 
       assert(this->lhs != nullptr && "Left-hand side cannot be a null-pointer!");
       assert(this->rhs != nullptr && "Righ-hand side cannot be a null-pointer!");
    } 
         

    /** Get left-hand side */
    Literal* getLHS() const {
        return lhs.get();
    }

    /** Get right-hand side */ 
    Literal* getRHS() const {
         return rhs.get();
    } 

    void apply(const NodeMapper& map) override {
       lhs = map(std::move(lhs));
       rhs = map(std::move(rhs));
    } 

protected:
    void print(std::ostream& os) const override {
      os << "(" << *lhs << ")/\\(" << *rhs << ")"; 
    }

    NodeVec getChildren() const override {
       return {lhs.get(), rhs.get()};
    } 

private:

    bool equal(const Node& node) const override { 
       const auto& other = asAssert<Conjunction>(node);
       return equal_ptr(lhs, other.lhs) && equal_ptr(rhs,other.rhs);
    } 

    Conjunction* cloning() const override {
        return new Conjunction(clone(lhs), clone(rhs), getSrcLoc());
    } 

private:
    /** left-hand and right-hand side */
    Own<Literal> lhs,rhs; 
};

}  // namespace souffle::ast
