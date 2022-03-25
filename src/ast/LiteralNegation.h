/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file LiteralNegation.h
 *
 * Define literal negation class
 *
 ***********************************************************************/

#pragma once

#include "ast/Literal.h"
#include "parser/SrcLocation.h"
#include <iosfwd>

namespace souffle::ast {

/**
 * @class LiteralNegation
 * @brief literal negation of a literal
 *
 * Example:
 *     ! A 
 *
 * where A is a literal
 */
class LiteralNegation : public Literal {
public:
    LiteralNegation(Own<Literal> arg, SrcLocation loc = {}) :
       Literal(std::move(loc)), arg(std::move(arg)) { 
       assert(arg != nullptr && "Argument cannot be a null-pointer!");
    } 
         

    /** Get left-hand side */
    Literal* getArg() const {
        return arg.get();
    }

    void apply(const NodeMapper& map) override {
       arg = map(std::move(arg));
    } 

protected:
    void print(std::ostream& os) const override {
      os << "!(" << *arg << ")"; 
    }

    NodeVec getChildren() const override {
       return {arg.get()};
    } 

private:

    bool equal(const Node& node) const override { 
       const auto& other = asAssert<LiteralNegation>(node);
       return equal_ptr(arg, other.arg);
    } 

    LiteralNegation* cloning() const override {
        return new LiteralNegation(clone(arg), getSrcLoc());
    } 

private:
    /** argument */
    Own<Literal> arg;
};

}  // namespace souffle::ast
