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

std::optional<std::size_t> LevelAnalysis::getLevel(const Node* node) const {
    // visitor
    using maybe_level = std::optional<std::size_t>;
    class ValueLevelVisitor : public Visitor<maybe_level> {
        using Visitor<maybe_level>::visit_;

        static maybe_level max(const maybe_level& a, const maybe_level& b) {
            if (a.has_value() && b.has_value()) {
                return std::max(*a, *b);
            } else if (a.has_value()) {
                return a;
            } else if (b.has_value()) {
                return b;
            } else {
                return std::nullopt;
            }
        }

    public:
        // string constant
        maybe_level visit_(type_identity<StringConstant>, const StringConstant&) override {
            return std::nullopt;
        }

        // number constant
        maybe_level visit_(type_identity<NumericConstant>, const NumericConstant&) override {
            return std::nullopt;
        }

        // true
        maybe_level visit_(type_identity<True>, const True&) override {
            return std::nullopt;
        }

        // false
        maybe_level visit_(type_identity<False>, const False&) override {
            return std::nullopt;
        }

        // tuple element access
        maybe_level visit_(type_identity<TupleElement>, const TupleElement& elem) override {
            return elem.getTupleId();
        }

        // scan
        maybe_level visit_(type_identity<Scan>, const Scan&) override {
            return std::nullopt;
        }

        // index scan
        maybe_level visit_(type_identity<IndexScan>, const IndexScan& indexScan) override {
            maybe_level level = std::nullopt;
            for (auto& index : indexScan.getRangePattern().first) {
                level = max(level, dispatch(*index));
            }
            for (auto& index : indexScan.getRangePattern().second) {
                level = max(level, dispatch(*index));
            }
            return level;
        }

        // choice
        maybe_level visit_(type_identity<IfExists>, const IfExists& choice) override {
            return max(-1, dispatch(choice.getCondition()));
        }

        // index choice
        maybe_level visit_(type_identity<IndexIfExists>, const IndexIfExists& indexIfExists) override {
            maybe_level level = std::nullopt;
            for (auto& index : indexIfExists.getRangePattern().first) {
                level = max(level, dispatch(*index));
            }
            for (auto& index : indexIfExists.getRangePattern().second) {
                level = max(level, dispatch(*index));
            }
            return max(level, dispatch(indexIfExists.getCondition()));
        }

        // aggregate
        maybe_level visit_(type_identity<Aggregate>, const Aggregate& aggregate) override {
            return max(dispatch(aggregate.getExpression()), dispatch(aggregate.getCondition()));
        }

        // index aggregate
        maybe_level visit_(type_identity<IndexAggregate>, const IndexAggregate& indexAggregate) override {
            maybe_level level = std::nullopt;
            for (auto& index : indexAggregate.getRangePattern().first) {
                level = max(level, dispatch(*index));
            }
            for (auto& index : indexAggregate.getRangePattern().second) {
                level = max(level, dispatch(*index));
            }
            level = max(dispatch(indexAggregate.getExpression()), level);
            return max(level, dispatch(indexAggregate.getCondition()));
        }

        // unpack record
        maybe_level visit_(type_identity<UnpackRecord>, const UnpackRecord& unpack) override {
            return dispatch(unpack.getExpression());
        }

        // filter
        maybe_level visit_(type_identity<Filter>, const Filter& filter) override {
            return dispatch(filter.getCondition());
        }

        // break
        maybe_level visit_(type_identity<Break>, const Break& b) override {
            return dispatch(b.getCondition());
        }

        // guarded insert
        maybe_level visit_(type_identity<GuardedInsert>, const GuardedInsert& guardedInsert) override {
            maybe_level level = std::nullopt;
            for (auto& exp : guardedInsert.getValues()) {
                level = max(level, dispatch(*exp));
            }
            level = max(level, dispatch(*guardedInsert.getCondition()));
            return level;
        }

        // insert
        maybe_level visit_(type_identity<Insert>, const Insert& insert) override {
            maybe_level level = std::nullopt;
            for (auto& exp : insert.getValues()) {
                level = max(level, dispatch(*exp));
            }
            return level;
        }

        // return
        maybe_level visit_(type_identity<SubroutineReturn>, const SubroutineReturn& ret) override {
            maybe_level level = std::nullopt;
            for (auto& exp : ret.getValues()) {
                level = max(level, dispatch(*exp));
            }
            return level;
        }

        // auto increment
        maybe_level visit_(type_identity<AutoIncrement>, const AutoIncrement&) override {
            return std::nullopt;
        }

        // undef value
        maybe_level visit_(type_identity<UndefValue>, const UndefValue&) override {
            return std::nullopt;
        }

        // intrinsic functors
        maybe_level visit_(type_identity<IntrinsicOperator>, const IntrinsicOperator& op) override {
            maybe_level level = std::nullopt;
            for (const auto& arg : op.getArguments()) {
                level = max(level, dispatch(*arg));
            }
            return level;
        }

        // pack operator
        maybe_level visit_(type_identity<PackRecord>, const PackRecord& pack) override {
            maybe_level level = std::nullopt;
            for (const auto& arg : pack.getArguments()) {
                level = max(level, dispatch(*arg));
            }
            return level;
        }

        // argument
        maybe_level visit_(type_identity<SubroutineArgument>, const SubroutineArgument&) override {
            return std::nullopt;
        }

        // user defined operator
        maybe_level visit_(type_identity<UserDefinedOperator>, const UserDefinedOperator& op) override {
            maybe_level level = std::nullopt;
            for (const auto& arg : op.getArguments()) {
                level = max(level, dispatch(*arg));
            }
            return level;
        }

        // conjunction
        maybe_level visit_(type_identity<Conjunction>, const Conjunction& conj) override {
            return max(dispatch(conj.getLHS()), dispatch(conj.getRHS()));
        }

        // negation
        maybe_level visit_(type_identity<Negation>, const Negation& neg) override {
            return dispatch(neg.getOperand());
        }

        // constraint
        maybe_level visit_(type_identity<Constraint>, const Constraint& binRel) override {
            return max(dispatch(binRel.getLHS()), dispatch(binRel.getRHS()));
        }

        // existence check
        maybe_level visit_(type_identity<ExistenceCheck>, const ExistenceCheck& exists) override {
            maybe_level level = std::nullopt;
            for (const auto& cur : exists.getValues()) {
                level = max(level, dispatch(*cur));
            }
            return level;
        }

        // provenance existence check
        maybe_level visit_(type_identity<ProvenanceExistenceCheck>,
                const ProvenanceExistenceCheck& provExists) override {
            maybe_level level = std::nullopt;
            for (const auto& cur : provExists.getValues()) {
                level = max(level, dispatch(*cur));
            }
            return level;
        }

        // emptiness check
        maybe_level visit_(type_identity<EmptinessCheck>, const EmptinessCheck&) override {
            return std::nullopt;  // can be in the top level
        }

        // default rule
        maybe_level visit_(type_identity<Node>, const Node&) override {
            fatal("Node not implemented!");
        }
    };

    assert((isA<Expression>(node) || isA<Condition>(node) || isA<Operation>(node)) &&
            "not an expression/condition/operation");
    auto res = ValueLevelVisitor().dispatch(*node);
    return res;
}

bool LevelAnalysis::hasLevel(const Node* value) const {
    return getLevel(value).has_value();
}

}  // namespace souffle::ram::analysis
