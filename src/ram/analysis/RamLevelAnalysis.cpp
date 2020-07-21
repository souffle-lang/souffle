/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamLevelAnalysis.cpp
 *
 * Implementation of RAM Level Analysis
 *
 ***********************************************************************/

#include "ram/analysis/RamLevelAnalysis.h"
#include "ram/RamCondition.h"
#include "ram/RamExpression.h"
#include "ram/RamNode.h"
#include "ram/RamOperation.h"
#include "ram/RamVisitor.h"
#include "utility/MiscUtil.h"
#include <algorithm>
#include <cassert>
#include <utility>
#include <vector>

namespace souffle {

int RamLevelAnalysis::getLevel(const RamNode* node) const {
    // visitor
    class ValueLevelVisitor : public RamVisitor<int> {
    public:
        // number
        int visitConstant(const RamConstant&) override {
            return -1;
        }

        // true
        int visitTrue(const RamTrue&) override {
            return -1;
        }

        // false
        int visitFalse(const RamFalse&) override {
            return -1;
        }

        // tuple element access
        int visitTupleElement(const RamTupleElement& elem) override {
            return elem.getTupleId();
        }

        // scan
        int visitScan(const RamScan&) override {
            return -1;
        }

        // index scan
        int visitIndexScan(const RamIndexScan& indexScan) override {
            int level = -1;
            for (auto& index : indexScan.getRangePattern().first) {
                level = std::max(level, visit(index));
            }
            for (auto& index : indexScan.getRangePattern().second) {
                level = std::max(level, visit(index));
            }
            return level;
        }

        // choice
        int visitChoice(const RamChoice& choice) override {
            return std::max(-1, visit(choice.getCondition()));
        }

        // index choice
        int visitIndexChoice(const RamIndexChoice& indexChoice) override {
            int level = -1;
            for (auto& index : indexChoice.getRangePattern().first) {
                level = std::max(level, visit(index));
            }
            for (auto& index : indexChoice.getRangePattern().second) {
                level = std::max(level, visit(index));
            }
            return std::max(level, visit(indexChoice.getCondition()));
        }

        // aggregate
        int visitAggregate(const RamAggregate& aggregate) override {
            return std::max(visit(aggregate.getExpression()), visit(aggregate.getCondition()));
        }

        // index aggregate
        int visitIndexAggregate(const RamIndexAggregate& indexAggregate) override {
            int level = -1;
            for (auto& index : indexAggregate.getRangePattern().first) {
                level = std::max(level, visit(index));
            }
            for (auto& index : indexAggregate.getRangePattern().second) {
                level = std::max(level, visit(index));
            }
            level = std::max(visit(indexAggregate.getExpression()), level);
            return std::max(level, visit(indexAggregate.getCondition()));
        }

        // unpack record
        int visitUnpackRecord(const RamUnpackRecord& unpack) override {
            return visit(unpack.getExpression());
        }

        // filter
        int visitFilter(const RamFilter& filter) override {
            return visit(filter.getCondition());
        }

        // break
        int visitBreak(const RamBreak& b) override {
            return visit(b.getCondition());
        }

        // project
        int visitProject(const RamProject& project) override {
            int level = -1;
            for (auto& exp : project.getValues()) {
                level = std::max(level, visit(exp));
            }
            return level;
        }

        // return
        int visitSubroutineReturn(const RamSubroutineReturn& ret) override {
            int level = -1;
            for (auto& exp : ret.getValues()) {
                level = std::max(level, visit(exp));
            }
            return level;
        }

        // auto increment
        int visitAutoIncrement(const RamAutoIncrement&) override {
            return -1;
        }

        // undef value
        int visitUndefValue(const RamUndefValue&) override {
            return -1;
        }

        // intrinsic functors
        int visitIntrinsicOperator(const RamIntrinsicOperator& op) override {
            int level = -1;
            for (const auto& arg : op.getArguments()) {
                level = std::max(level, visit(arg));
            }
            return level;
        }

        // pack operator
        int visitPackRecord(const RamPackRecord& pack) override {
            int level = -1;
            for (const auto& arg : pack.getArguments()) {
                level = std::max(level, visit(arg));
            }
            return level;
        }

        // argument
        int visitSubroutineArgument(const RamSubroutineArgument&) override {
            return -1;
        }

        // user defined operator
        int visitUserDefinedOperator(const RamUserDefinedOperator& op) override {
            int level = -1;
            for (const auto& arg : op.getArguments()) {
                level = std::max(level, visit(arg));
            }
            return level;
        }

        // conjunction
        int visitConjunction(const RamConjunction& conj) override {
            return std::max(visit(conj.getLHS()), visit(conj.getRHS()));
        }

        // negation
        int visitNegation(const RamNegation& neg) override {
            return visit(neg.getOperand());
        }

        // constraint
        int visitConstraint(const RamConstraint& binRel) override {
            return std::max(visit(binRel.getLHS()), visit(binRel.getRHS()));
        }

        // existence check
        int visitExistenceCheck(const RamExistenceCheck& exists) override {
            int level = -1;
            for (const auto& cur : exists.getValues()) {
                level = std::max(level, visit(cur));
            }
            return level;
        }

        // provenance existence check
        int visitProvenanceExistenceCheck(const RamProvenanceExistenceCheck& provExists) override {
            int level = -1;
            for (const auto& cur : provExists.getValues()) {
                level = std::max(level, visit(cur));
            }
            return level;
        }

        // emptiness check
        int visitEmptinessCheck(const RamEmptinessCheck&) override {
            return -1;  // can be in the top level
        }

        // default rule
        int visitNode(const RamNode&) override {
            fatal("RamNode not implemented!");
        }
    };

    assert((dynamic_cast<const RamExpression*>(node) != nullptr ||
                   dynamic_cast<const RamCondition*>(node) != nullptr ||
                   dynamic_cast<const RamOperation*>(node) != nullptr) &&
            "not an expression/condition/operation");
    return ValueLevelVisitor().visit(node);
}

}  // end of namespace souffle
