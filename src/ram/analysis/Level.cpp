/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Level.cpp
 *
 * Implementation of RAM Level Analysis
 *
 ***********************************************************************/

#include "ram/analysis/Level.h"
#include "ram/Aggregate.h"
#include "ram/AutoIncrement.h"
#include "ram/Break.h"
#include "ram/Condition.h"
#include "ram/Conjunction.h"
#include "ram/Constraint.h"
#include "ram/EmptinessCheck.h"
#include "ram/ExistenceCheck.h"
#include "ram/Expression.h"
#include "ram/False.h"
#include "ram/Filter.h"
#include "ram/IfExists.h"
#include "ram/IndexAggregate.h"
#include "ram/IndexIfExists.h"
#include "ram/IndexScan.h"
#include "ram/Insert.h"
#include "ram/IntrinsicOperator.h"
#include "ram/Negation.h"
#include "ram/Node.h"
#include "ram/NumericConstant.h"
#include "ram/Operation.h"
#include "ram/PackRecord.h"
#include "ram/ProvenanceExistenceCheck.h"
#include "ram/Scan.h"
#include "ram/StringConstant.h"
#include "ram/SubroutineArgument.h"
#include "ram/SubroutineReturn.h"
#include "ram/True.h"
#include "ram/TupleElement.h"
#include "ram/UndefValue.h"
#include "ram/UnpackRecord.h"
#include "ram/UserDefinedOperator.h"
#include "ram/utility/Visitor.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <cassert>
#include <utility>
#include <vector>

namespace souffle::ram::analysis {

int LevelAnalysis::getLevel(const Node* node) const {
    // visitor
    class ValueLevelVisitor : public Visitor<int> {
        using Visitor<int>::visit_;

    public:
        // string constant
        int visit_(type_identity<StringConstant>, const StringConstant&) override {
            return -1;
        }

        // number constant
        int visit_(type_identity<NumericConstant>, const NumericConstant&) override {
            return -1;
        }

        // true
        int visit_(type_identity<True>, const True&) override {
            return -1;
        }

        // false
        int visit_(type_identity<False>, const False&) override {
            return -1;
        }

        // tuple element access
        int visit_(type_identity<TupleElement>, const TupleElement& elem) override {
            return elem.getTupleId();
        }

        // scan
        int visit_(type_identity<Scan>, const Scan&) override {
            return -1;
        }

        // index scan
        int visit_(type_identity<IndexScan>, const IndexScan& indexScan) override {
            int level = -1;
            for (auto& index : indexScan.getRangePattern().first) {
                level = std::max(level, dispatch(*index));
            }
            for (auto& index : indexScan.getRangePattern().second) {
                level = std::max(level, dispatch(*index));
            }
            return level;
        }

        // choice
        int visit_(type_identity<IfExists>, const IfExists& choice) override {
            return std::max(-1, dispatch(choice.getCondition()));
        }

        // index choice
        int visit_(type_identity<IndexIfExists>, const IndexIfExists& indexIfExists) override {
            int level = -1;
            for (auto& index : indexIfExists.getRangePattern().first) {
                level = std::max(level, dispatch(*index));
            }
            for (auto& index : indexIfExists.getRangePattern().second) {
                level = std::max(level, dispatch(*index));
            }
            return std::max(level, dispatch(indexIfExists.getCondition()));
        }

        // aggregate
        int visit_(type_identity<Aggregate>, const Aggregate& aggregate) override {
            return std::max(dispatch(aggregate.getExpression()), dispatch(aggregate.getCondition()));
        }

        // index aggregate
        int visit_(type_identity<IndexAggregate>, const IndexAggregate& indexAggregate) override {
            int level = -1;
            for (auto& index : indexAggregate.getRangePattern().first) {
                level = std::max(level, dispatch(*index));
            }
            for (auto& index : indexAggregate.getRangePattern().second) {
                level = std::max(level, dispatch(*index));
            }
            level = std::max(dispatch(indexAggregate.getExpression()), level);
            return std::max(level, dispatch(indexAggregate.getCondition()));
        }

        // unpack record
        int visit_(type_identity<UnpackRecord>, const UnpackRecord& unpack) override {
            return dispatch(unpack.getExpression());
        }

        // filter
        int visit_(type_identity<Filter>, const Filter& filter) override {
            return dispatch(filter.getCondition());
        }

        // break
        int visit_(type_identity<Break>, const Break& b) override {
            return dispatch(b.getCondition());
        }

        // guarded insert
        int visit_(type_identity<GuardedInsert>, const GuardedInsert& guardedInsert) override {
            int level = -1;
            for (auto& exp : guardedInsert.getValues()) {
                level = std::max(level, dispatch(*exp));
            }
            level = std::max(level, dispatch(*guardedInsert.getCondition()));
            return level;
        }

        // insert
        int visit_(type_identity<Insert>, const Insert& insert) override {
            int level = -1;
            for (auto& exp : insert.getValues()) {
                level = std::max(level, dispatch(*exp));
            }
            return level;
        }

        // return
        int visit_(type_identity<SubroutineReturn>, const SubroutineReturn& ret) override {
            int level = -1;
            for (auto& exp : ret.getValues()) {
                level = std::max(level, dispatch(*exp));
            }
            return level;
        }

        // auto increment
        int visit_(type_identity<AutoIncrement>, const AutoIncrement&) override {
            return -1;
        }

        // auto increment
        int visit_(type_identity<IterationNumber>, const IterationNumber&) override {
            return -1;
        }

        // undef value
        int visit_(type_identity<UndefValue>, const UndefValue&) override {
            return -1;
        }

        // intrinsic functors
        int visit_(type_identity<IntrinsicOperator>, const IntrinsicOperator& op) override {
            int level = -1;
            for (const auto& arg : op.getArguments()) {
                level = std::max(level, dispatch(*arg));
            }
            return level;
        }

        // pack operator
        int visit_(type_identity<PackRecord>, const PackRecord& pack) override {
            int level = -1;
            for (const auto& arg : pack.getArguments()) {
                level = std::max(level, dispatch(*arg));
            }
            return level;
        }

        // argument
        int visit_(type_identity<SubroutineArgument>, const SubroutineArgument&) override {
            return -1;
        }

        // user defined operator
        int visit_(type_identity<UserDefinedOperator>, const UserDefinedOperator& op) override {
            int level = -1;
            for (const auto& arg : op.getArguments()) {
                level = std::max(level, dispatch(*arg));
            }
            return level;
        }

        // conjunction
        int visit_(type_identity<Conjunction>, const Conjunction& conj) override {
            return std::max(dispatch(conj.getLHS()), dispatch(conj.getRHS()));
        }

        // negation
        int visit_(type_identity<Negation>, const Negation& neg) override {
            return dispatch(neg.getOperand());
        }

        // constraint
        int visit_(type_identity<Constraint>, const Constraint& binRel) override {
            return std::max(dispatch(binRel.getLHS()), dispatch(binRel.getRHS()));
        }

        // existence check
        int visit_(type_identity<ExistenceCheck>, const ExistenceCheck& exists) override {
            int level = -1;
            for (const auto& cur : exists.getValues()) {
                level = std::max(level, dispatch(*cur));
            }
            return level;
        }

        // provenance existence check
        int visit_(type_identity<ProvenanceExistenceCheck>,
                const ProvenanceExistenceCheck& provExists) override {
            int level = -1;
            for (const auto& cur : provExists.getValues()) {
                level = std::max(level, dispatch(*cur));
            }
            return level;
        }

        // emptiness check
        int visit_(type_identity<EmptinessCheck>, const EmptinessCheck&) override {
            return -1;  // can be in the top level
        }

        // default rule
        int visit_(type_identity<Node>, const Node&) override {
            fatal("Node not implemented!");
        }
    };

    assert((isA<Expression>(node) || isA<Condition>(node) || isA<Operation>(node)) &&
            "not an expression/condition/operation");
    return ValueLevelVisitor().dispatch(*node);
}

}  // namespace souffle::ram::analysis
