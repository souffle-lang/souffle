/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2022, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file UserDefinedAggregator.h
 *
 ***********************************************************************/

#pragma once

#include "AggregateOp.h"
#include "Aggregator.h"
#include "ram/Expression.h"
#include "souffle/utility/DynamicCasting.h"

namespace souffle::ram {

/**
 * @class IntrinsicAggregate
 * @brief Class for intrinsic aggregate functions
 *
 */
class UserDefinedAggregator : public Aggregator {
public:
    UserDefinedAggregator(const std::string name, Own<Expression> init, std::vector<TypeAttribute> argsTypes,
            TypeAttribute returnType, bool stateful)
            : name(name), initValue(std::move(init)), argsTypes(std::move(argsTypes)), returnType(returnType),
              stateful(stateful) {}

    ~UserDefinedAggregator() override = default;

    /** @brief Get aggregation function */
    std::string getName() const {
        return name;
    }

    const Expression* getInitValue() const {
        return initValue.get();
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

    std::vector<const Node*> getChildren() const override {
        return {initValue.get()};
    }

    UserDefinedAggregator* cloning() const override {
        auto res = new UserDefinedAggregator(name, clone(initValue), argsTypes, returnType, stateful);
        return res;
    }

    bool equal(const Aggregator& agg) const override {
        const auto& other = asAssert<UserDefinedAggregator>(agg);
        return name == other.name;
    }

    void print(std::ostream& os, int /* tabpos */) const override {
        os << name << " INIT " << *initValue << " ";
    }

    void print_sexpr(std::ostream& os, int /* tabpos */) const override {
        os << "(USER_AGGREGATOR " << name << " INIT " << *initValue << ") ";
    }

protected:
    /** Aggregation function */
    const std::string name;

    Own<Expression> initValue;

    /** Argument types */
    const std::vector<TypeAttribute> argsTypes;

    /** Return type */
    const TypeAttribute returnType;

    /** Stateful */
    const bool stateful;
};
}  // namespace souffle::ram