/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file PackRecord.h
 *
 * Defines a class for evaluating values in the Relational Algebra Machine
 *
 ************************************************************************/

#pragma once

#include "ram/Expression.h"
#include "ram/Node.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class PackRecord
 * @brief Packs a record's arguments into a reference
 */
class PackRecord : public Expression {
public:
    PackRecord(VecOwn<Expression> args) : arguments(std::move(args)) {
        assert(allValidPtrs(arguments));
    }

    /** @brief Get record arguments */
    std::vector<Expression*> getArguments() const {
        return toPtrVector(arguments);
    }

    PackRecord* cloning() const override {
        auto* res = new PackRecord({});
        for (auto& cur : arguments) {
            res->arguments.emplace_back(cur->cloning());
        }
        return res;
    }

    void apply(const NodeMapper& map) override {
        for (auto& arg : arguments) {
            arg = map(std::move(arg));
        }
    }

protected:
    void print(std::ostream& os) const override {
        os << "PACK("
           << join(arguments, ",", [](std::ostream& out, const Own<Expression>& arg) { out << *arg; }) << ")";
    }

    void print_sexpr(std::ostream& os) const override  {
        os << "(PACK "
           << join(arguments, " ", [](std::ostream& out, const Own<Expression>& arg) { out << *arg; }) << ")";
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<PackRecord>(node);
        return equal_targets(arguments, other.arguments);
    }

    NodeVec getChildren() const override {
        return toPtrVector<Node const>(arguments);
    }

    /** Arguments */
    VecOwn<Expression> arguments;
};

}  // namespace souffle::ram
