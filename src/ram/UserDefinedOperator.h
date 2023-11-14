/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file UserDefinedOperator.h
 *
 * Defines a class for evaluating values in the Relational Algebra Machine
 *
 ************************************************************************/

#pragma once

#include "ram/AbstractOperator.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "souffle/TypeAttribute.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class UserDefinedOperator
 * @brief Operator that represents an extrinsic (user-defined) functor
 */
class UserDefinedOperator : public AbstractOperator {
public:
    UserDefinedOperator(std::string n, std::vector<TypeAttribute> argsTypes, TypeAttribute returnType,
            bool stateful, VecOwn<Expression> args)
            : AbstractOperator(std::move(args)), name(std::move(n)), argsTypes(std::move(argsTypes)),
              returnType(returnType), stateful(stateful) {
        assert(argsTypes.size() == args.size());
    }

    /** @brief Get operator name */
    const std::string& getName() const {
        return name;
    }

    /** @brief Get types of arguments */
    const std::vector<TypeAttribute>& getArgsTypes() const {
        return argsTypes;
    }

    /** @brief Get return type */
    TypeAttribute getReturnType() const {
        return returnType;
    }

    /** @brief Is functor stateful? */
    bool isStateful() const {
        return stateful;
    }

    UserDefinedOperator* cloning() const override {
        auto* res = new UserDefinedOperator(name, argsTypes, returnType, stateful, {});
        for (auto& cur : arguments) {
            Expression* arg = cur->cloning();
            res->arguments.emplace_back(arg);
        }
        return res;
    }

protected:
    void print(std::ostream& os) const override {
        os << "@" << name << "_" << argsTypes;
        os << "_" << returnType;
        if (stateful) {
            os << "_stateful";
        }
        os << "(" << join(arguments, ",", [](std::ostream& out, const Own<Expression>& arg) { out << *arg; })
           << ")";
    }

    void print_sexpr(std::ostream& os) const override {
        os << "(USER_FUNCTOR " << name << "_" << argsTypes;
        os << "_" << returnType;
        if (stateful) {
            os << " TRUE ";
        } else {
            os << " FALSE ";
        }
        os << " (" << join(arguments, " ", [](std::ostream& out, const Own<Expression>& arg) { out << *arg; })
           << "))";
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<UserDefinedOperator>(node);
        return AbstractOperator::equal(node) && name == other.name && argsTypes == other.argsTypes &&
               returnType == other.returnType && stateful == other.stateful;
    }

    /** Name of user-defined operator */
    const std::string name;

    /** Argument types */
    const std::vector<TypeAttribute> argsTypes;

    /** Return type */
    const TypeAttribute returnType;

    /** Stateful */
    const bool stateful;
};
}  // namespace souffle::ram
