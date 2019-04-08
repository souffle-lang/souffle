/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018 The Souffle Developers. All Rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstFunctorDeclaration.h
 *
 * Defines external functors.
 *
 ***********************************************************************/

#pragma once

#include "AstNode.h"
#include "Util.h"

#include <algorithm>
#include <string>
#include <vector>

namespace souffle {

/**
 * AstFunctorDeclaration
 */

class AstFunctorDeclaration : public AstNode {
private:
    /** name of functor */
    const std::string name;

    /** name of type */
    const std::string type;

public:
    AstFunctorDeclaration(const std::string& name, const std::string& type) : name(name), type(type) {
        assert(name.length() > 0 && "functor name is empty");
        assert(type.length() > 0 && "type is empty");
    }

    const std::string& getName() const {
        return name;
    }

    const std::string& getType() const {
        return type;
    }

    /** get number of arguments */
    size_t getArgCount() const {
        assert(type.length() > 0 && "wrong type declaration for user-defined functor");
        return type.length() - 1;
    }

    /** is return type a symbolic value */
    bool isSymbolic() const {
        assert(type.length() > 0 && "wrong type declaration for user-defined functor");
        return (type[type.length() - 1] == 'S');
    }

    /** is return type a number value */
    bool isNumerical() const {
        assert(type.length() > 0 && "wrong type declaration for user-defined functor");
        return (type[type.length() - 1] == 'N');
    }

    /** accepts the i-th argument as a symbolic value */
    bool acceptsSymbols(size_t idx) const {
        assert(idx <= getArgCount() && "argument index out of bound");
        return (type[idx] == 'S');
    }

    /** accepts the i-th argument as a number value */
    bool acceptsNumbers(size_t idx) const {
        assert(idx <= getArgCount() && "argument index out of bound");
        return (type[idx] == 'N');
    }

    /** print */
    void print(std::ostream& out) const override {
        auto convert = [&](char type) {
            switch (type) {
                case 'N':
                    return "number";
                case 'S':
                    return "symbol";
                default:
                    abort();
            }
        };
        out << ".declfun " << name << "(";
        std::vector<std::string> args;
        for (size_t i = 0; i < type.length() - 1; i++) {
            args.push_back(convert(type[i]));
        }
        out << join(args, ",");
        out << "):" << convert(type[type.length() - 1]) << std::endl;
    }

    /** get children */
    std::vector<const AstNode*> getChildNodes() const override {
        std::vector<const AstNode*> res;
        return res;  // no child nodes
    }

    /** apply */
    void apply(const AstNodeMapper& /*mapper*/) override {
        // nothing to do
    }

    /** clone */
    AstFunctorDeclaration* clone() const override {
        auto* res = new AstFunctorDeclaration(name, type);
        res->setSrcLoc(getSrcLoc());
        return res;
    }

protected:
    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstFunctorDeclaration*>(&node));
        const auto& other = static_cast<const AstFunctorDeclaration&>(node);
        return name == other.name && type == other.type;
    }
};

}  // end of namespace souffle
