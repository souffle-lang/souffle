/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018 The Souffle Developers. All Rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file FunctorDeclaration.h
 *
 * Defines the external functor class
 *
 ***********************************************************************/

#pragma once

#include "ast/Node.h"
#include "parser/SrcLocation.h"
#include "parser/TypeAttribute.h"
#include "utility/ContainerUtil.h"
#include "utility/MiscUtil.h"
#include "utility/StreamUtil.h"
#include "utility/tinyformat.h"
#include <cassert>
#include <cstdlib>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 * @class AstFunctorDeclaration
 * @brief User-defined functor declaration
 *
 * Example:
 *    .declfun foo(x:number, y:number):number
 */

class AstFunctorDeclaration : public AstNode {
public:
    AstFunctorDeclaration(std::string name, std::vector<TypeAttribute> argsTypes, TypeAttribute returnType,
            SrcLocation loc = {})
            : AstNode(std::move(loc)), name(std::move(name)), argsTypes(std::move(argsTypes)),
              returnType(returnType) {
        assert(this->name.length() > 0 && "functor name is empty");
    }

    /** Return name */
    const std::string& getName() const {
        return name;
    }

    /** Return type */
    const std::vector<TypeAttribute>& getArgsTypes() const {
        return argsTypes;
    }

    /** Get return type */
    TypeAttribute getReturnType() const {
        return returnType;
    }

    /** Return number of arguments */
    size_t getArity() const {
        return argsTypes.size();
    }

    AstFunctorDeclaration* clone() const override {
        return new AstFunctorDeclaration(name, argsTypes, returnType, getSrcLoc());
    }

protected:
    void print(std::ostream& out) const override {
        auto convert = [&](TypeAttribute type) {
            switch (type) {
                case TypeAttribute::Signed: return "number";
                case TypeAttribute::Symbol: return "symbol";
                case TypeAttribute::Float: return "float";
                case TypeAttribute::Unsigned: return "unsigned";
                case TypeAttribute::Record: fatal("unhandled `TypeAttribute`");
            }

            UNREACHABLE_BAD_CASE_ANALYSIS
        };

        tfm::format(
                out, ".declfun %s(%s): %s\n", name, join(map(argsTypes, convert), ","), convert(returnType));
    }

    bool equal(const AstNode& node) const override {
        const auto& other = static_cast<const AstFunctorDeclaration&>(node);
        return name == other.name && argsTypes == other.argsTypes && returnType == other.returnType;
    }

    /** Name of functor */
    const std::string name;

    /** Types of arguments */
    const std::vector<TypeAttribute> argsTypes;

    /** Type of the return value */
    const TypeAttribute returnType;
};

}  // end of namespace souffle
