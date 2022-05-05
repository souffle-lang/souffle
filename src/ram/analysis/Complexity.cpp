/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Complexity.cpp
 *
 * Implementation of RAM Complexity Analysis
 *
 ***********************************************************************/

#include "ram/analysis/Complexity.h"
#include "ram/Condition.h"
#include "ram/Conjunction.h"
#include "ram/EmptinessCheck.h"
#include "ram/ExistenceCheck.h"
#include "ram/Expression.h"
#include "ram/Negation.h"
#include "ram/Node.h"
#include "ram/ProvenanceExistenceCheck.h"
#include "ram/AggregateExistenceCheck.h"
#include "ram/Relation.h"
#include "ram/utility/Visitor.h"
#include <cassert>

namespace souffle::ram::analysis {

int ComplexityAnalysis::getComplexity(const Node* node) const {
    // visitor
    class ValueComplexityVisitor : public Visitor<int> {
        using Visitor<int>::visit_;

    public:
        ValueComplexityVisitor(RelationAnalysis* relAnalysis) : ra(relAnalysis) {}

        // conjunction
        int visit_(type_identity<Conjunction>, const Conjunction& conj) override {
            return dispatch(conj.getLHS()) + dispatch(conj.getRHS());
        }

        // negation
        int visit_(type_identity<Negation>, const Negation& neg) override {
            return dispatch(neg.getOperand());
        }

        // existence check
        int visit_(type_identity<ExistenceCheck>, const ExistenceCheck&) override {
            return 2;
        }

        // provenance existence check
        int visit_(type_identity<ProvenanceExistenceCheck>, const ProvenanceExistenceCheck&) override {
            return 2;
        }

        // aggregate existence check
        int visit_(type_identity<AggregateExistenceCheck>, const AggregateExistenceCheck&) override {
            return 2;
        }

        int visit_(type_identity<Constraint>, const Constraint& c) override {
            return dispatch(c.getLHS()) + dispatch(c.getRHS());
        }

        int visit_(type_identity<UserDefinedOperator>, const UserDefinedOperator&) override {
            return 10;
        }

        // emptiness check
        int visit_(type_identity<EmptinessCheck>, const EmptinessCheck& emptiness) override {
            // emptiness check for nullary relations is for free; others have weight one
            return (ra->lookup(emptiness.getRelation()).getArity() > 0) ? 1 : 0;
        }

        // default rule
        int visit_(type_identity<Node>, const Node&) override {
            return 0;
        }

    protected:
        RelationAnalysis* ra{nullptr};
    };

    assert((isA<Expression>(node) || isA<Condition>(node)) && "not an expression/condition/operation");
    return ValueComplexityVisitor(ra).dispatch(*node);
}

}  // namespace souffle::ram::analysis
