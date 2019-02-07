/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamOperation.cpp
 *
 * Implements the operation of a relational algebra query consisting of
 * Search/Scan and a Project operation. The Search/Scan operation traverses
 * a table and/or check for condition of each tuple and/or uses an index.
 *
 ***********************************************************************/

#include "RamOperation.h"
#include "BinaryConstraintOps.h"
#include "RamCondition.h"
#include "RamRelation.h"
#include "RamValue.h"
#include "RamVisitor.h"
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>

namespace souffle {

/** Determine whether a RAM value is a constant */
// TODO (#541): Remove or replace by RamConstValueAnalysis
bool isConstant(const RamValue* value) {
    // visitor
    class ConstValueVisitor : public RamVisitor<bool> {
    public:
        // number
        bool visitNumber(const RamNumber& num) override {
            return true;
        }

        // tuple element access
        bool visitElementAccess(const RamElementAccess& elem) override {
            return false;
        }

        // auto increment
        bool visitAutoIncrement(const RamAutoIncrement& increment) override {
            return false;
        }

        // intrinsic functors
        bool visitIntrinsicOperator(const RamIntrinsicOperator& op) override {
            const auto& args = op.getArguments();
            bool isConst = true;
            for (const auto arg : args) {
                isConst = isConst && visit(arg);
            }
            return isConst;
        }

        // pack operator
        bool visitPack(const RamPack& pack) override {
            return false;
        }

        // argument
        bool visitArgument(const RamArgument& arg) override {
            return false;
        }

        // user defined operator
        bool visitUserDefinedOperator(const RamUserDefinedOperator& op) override {
            const auto& args = op.getArguments();
            bool isConst = true;
            for (const auto arg : args) {
                isConst = isConst && visit(arg);
            }
            return isConst;
        }
    };
    return ConstValueVisitor().visit(value);
}

/** Get level of value (which for-loop of a query) */
// TODO (#541): Remove or replace by RamValueLevelAnalysis
size_t getLevel(const RamValue* value) {
    // visitor
    class ValueLevelVisitor : public RamVisitor<size_t> {
    public:
        // number
        size_t visitNumber(const RamNumber& num) override {
            return 0;
        }

        // tuple element access
        size_t visitElementAccess(const RamElementAccess& elem) override {
            return elem.getLevel();
        }

        // auto increment
        size_t visitAutoIncrement(const RamAutoIncrement& increment) override {
            return 0;
        }

        // intrinsic functors
        size_t visitIntrinsicOperator(const RamIntrinsicOperator& op) override {
            size_t level = 0;
            for (const auto& arg : op.getArguments()) {
                if (arg != nullptr) {
                    level = std::max(level, visit(arg));
                }
            }
            return level;
        }

        // pack operator
        size_t visitPack(const RamPack& pack) override {
            size_t level = 0;
            for (const auto& arg : pack.getArguments()) {
                if (arg != nullptr) {
                    level = std::max(level, visit(arg));
                }
            }
            return level;
        }

        // argument
        size_t visitArgument(const RamArgument& arg) override {
            return 0;
        }

        // user defined operator
        size_t visitUserDefinedOperator(const RamUserDefinedOperator& op) override {
            size_t level = 0;
            for (const auto& arg : op.getArguments()) {
                if (arg != nullptr) {
                    level = std::max(level, visit(arg));
                }
            }
            return level;
        }
    };
    return ValueLevelVisitor().visit(value);
}

/** Get level of condition (which for-loop of a query) */
// TODO (#541): Remove or replace by RamConditionLevelAnalysis
size_t getLevel(const RamCondition* condition) {
    // visitor
    class ConditionLevelVisitor : public RamVisitor<size_t> {
    public:
        // conjunction
        size_t visitAnd(const RamAnd& conj) override {
            return std::max(visit(conj.getLHS()), visit(conj.getRHS()));
        }

        // binary constraint
        size_t visitBinaryRelation(const RamBinaryRelation& binRel) override {
            return std::max(getLevel(binRel.getLHS()), getLevel(binRel.getRHS()));
        }

        // not exists check
        size_t visitNotExists(const RamNotExists& notExists) override {
            size_t level = 0;
            for (const auto& cur : notExists.getValues()) {
                if (cur != nullptr) {
                    level = std::max(level, getLevel(cur));
                }
            }
            return level;
        }

        // not exists check for a provenance existence check
        size_t visitProvenanceNotExists(const RamProvenanceNotExists& provNotExists) override {
            size_t level = 0;
            for (const auto& cur : provNotExists.getValues()) {
                if (cur != nullptr) {
                    level = std::max(level, getLevel(cur));
                }
            }
            return level;
        }

        // emptiness check
        size_t visitEmpty(const RamEmpty& emptiness) override {
            return 0;  // can be in the top level
        }
    };
    return ConditionLevelVisitor().visit(condition);
}

/** get indexable element */
std::unique_ptr<RamValue> RamAggregate::getIndexElement(RamCondition* c, size_t& element, size_t level) {
    if (auto* binRelOp = dynamic_cast<RamBinaryRelation*>(c)) {
        if (binRelOp->getOperator() == BinaryConstraintOp::EQ) {
            if (auto* lhs = dynamic_cast<RamElementAccess*>(binRelOp->getLHS())) {
                RamValue* rhs = binRelOp->getRHS();
                if (lhs->getLevel() == level && (isConstant(rhs) || getLevel(rhs) < level)) {
                    element = lhs->getElement();
                    return binRelOp->takeRHS();
                }
            }
            if (auto* rhs = dynamic_cast<RamElementAccess*>(binRelOp->getRHS())) {
                RamValue* lhs = binRelOp->getLHS();
                if (rhs->getLevel() == level && (isConstant(lhs) || getLevel(lhs) < level)) {
                    element = rhs->getElement();
                    return binRelOp->takeLHS();
                }
            }
        }
    }
    return std::unique_ptr<RamValue>(nullptr);
}

/** add condition */
void RamAggregate::addCondition(std::unique_ptr<RamCondition> newCondition) {
    assert(getLevel(newCondition.get()) == getIdentifier());

    // use condition to narrow scan if possible
    size_t element = 0;
    if (std::unique_ptr<RamValue> value = getIndexElement(newCondition.get(), element, getIdentifier())) {
        if (element > 0 || relation->getName().find("__agg") == std::string::npos) {
            keys |= (1 << element);
            if (pattern[element] == nullptr) {
                pattern[element] = std::move(value);
            } else {
                std::unique_ptr<RamValue> field(new RamElementAccess(getIdentifier(), element));

                auto addCondition = [&](std::unique_ptr<RamCondition> c) {
                    assert(getLevel(c.get()) == getIdentifier());
                    if (condition != nullptr) {
                        condition = std::make_unique<RamAnd>(std::move(condition), std::move(c));
                    } else {
                        condition = std::move(c);
                    }
                };

                addCondition(std::make_unique<RamBinaryRelation>(
                        BinaryConstraintOp::EQ, std::move(field), std::move(value)));
            }
        } else {
            std::unique_ptr<RamValue> field(new RamElementAccess(getIdentifier(), element));
            std::unique_ptr<RamCondition> eq(
                    new RamBinaryRelation(BinaryConstraintOp::EQ, std::move(field), std::move(value)));
            if (condition != nullptr) {
                condition = std::make_unique<RamAnd>(std::move(condition), std::move(eq));
            } else {
                condition.swap(eq);
            }
        }
    }
}

}  // end of namespace souffle
