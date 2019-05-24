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

#include "RamLevelAnalysis.h"
#include "RamVisitor.h"
#include <algorithm>

namespace souffle {

int RamLevelAnalysis::getLevel(const RamNode* node) const {
    // visitor
    class ValueLevelVisitor : public RamVisitor<int> {
    public:
        // number
        int visitNumber(const RamNumber& num) override {
            return -1;
        }

        // undefined value
        int visitUndefValue(const RamUndefValue& undef) override {
            return -1;
        }

        // tuple element access
        int visitElementAccess(const RamElementAccess& elem) override {
            return elem.getTupleId();
        }

        // auto increment
        int visitAutoIncrement(const RamAutoIncrement& increment) override {
            return -1;
        }

        // intrinsic functors
        int visitIntrinsicOperator(const RamIntrinsicOperator& op) override {
            int level = -1;
            for (const auto& arg : op.getArguments()) {
                if (arg != nullptr) {
                    level = std::max(level, visit(arg));
                }
            }
            return level;
        }

        // pack operator
        int visitPackRecord(const RamPackRecord& pack) override {
            int level = -1;
            for (const auto& arg : pack.getArguments()) {
                if (arg != nullptr) {
                    level = std::max(level, visit(arg));
                }
            }
            return level;
        }

        // argument
        int visitArgument(const RamArgument& arg) override {
            return -1;
        }

        // user defined operator
        int visitUserDefinedOperator(const RamUserDefinedOperator& op) override {
            int level = -1;
            for (const auto& arg : op.getArguments()) {
                if (arg != nullptr) {
                    level = std::max(level, visit(arg));
                }
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
                if (cur != nullptr) {
                    level = std::max(level, visit(cur));
                }
            }
            return level;
        }

        // provenance existence check
        int visitProvenanceExistenceCheck(const RamProvenanceExistenceCheck& provExists) override {
            int level = -1;
            for (const auto& cur : provExists.getValues()) {
                if (cur != nullptr) {
                    level = std::max(level, visit(cur));
                }
            }
            return level;
        }

        // emptiness check
        int visitEmptinessCheck(const RamEmptinessCheck& emptiness) override {
            return -1;  // can be in the top level
        }

        // default rule
        int visitNode(const RamNode& node) override {
            assert(false && "RamNode not implemented!");
            return -1;
        }
    };

    assert((dynamic_cast<const RamExpression*>(node) != nullptr ||
                   dynamic_cast<const RamCondition*>(node) != nullptr) &&
            "not an expression/condition");
    return ValueLevelVisitor().visit(node);
}

}  // end of namespace souffle
