/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstClause.h
 *
 * Defines AST Clauses
 *
 ***********************************************************************/

#pragma once

#include "SrcLocation.h"
#include "ast/AstAbstract.h"
#include "ast/AstLiteral.h"
#include "ast/AstNode.h"
#include "utility/ContainerUtil.h"
#include "utility/MiscUtil.h"
#include "utility/StreamUtil.h"
#include <algorithm>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 * An execution order for atoms within a clause.
 */
class AstExecutionOrder : public AstNode {
public:
    using ExecOrder = std::vector<unsigned int>;

    AstExecutionOrder(ExecOrder order = {}, SrcLocation loc = {}) : order(std::move(order)) {
        setSrcLoc(std::move(loc));
    }

    /** get order */
    const ExecOrder& getOrder() const {
        return order;
    }

    AstExecutionOrder* clone() const override {
        return new AstExecutionOrder(order, getSrcLoc());
    }

protected:
    void print(std::ostream& out) const override {
        out << "(" << join(order) << ")";
    }

    bool equal(const AstNode& node) const override {
        const auto& other = static_cast<const AstExecutionOrder&>(node);
        return order == other.order;
    }

private:
    /** literal order of body (starting from 1) */
    ExecOrder order;
};

/**
 * The class utilized to model user-defined execution plans for various
 * versions of clauses.
 */
class AstExecutionPlan : public AstNode {
public:
    /** updates execution order for rule version */
    void setOrderFor(int version, Own<AstExecutionOrder> plan) {
        plans[version] = std::move(plan);
    }

    /** get orders */
    std::map<int, const AstExecutionOrder*> getOrders() const {
        std::map<int, const AstExecutionOrder*> result;
        for (auto& plan : plans) {
            result.insert(std::make_pair(plan.first, plan.second.get()));
        }
        return result;
    }

    AstExecutionPlan* clone() const override {
        auto res = new AstExecutionPlan();
        res->setSrcLoc(getSrcLoc());
        for (auto& plan : plans) {
            res->setOrderFor(plan.first, Own<AstExecutionOrder>(plan.second->clone()));
        }
        return res;
    }

    void apply(const AstNodeMapper& map) override {
        for (auto& plan : plans) {
            plan.second = map(std::move(plan.second));
        }
    }

    std::vector<const AstNode*> getChildNodes() const override {
        std::vector<const AstNode*> childNodes;
        for (auto& plan : plans) {
            childNodes.push_back(plan.second.get());
        }
        return childNodes;
    }

protected:
    void print(std::ostream& out) const override {
        if (!plans.empty()) {
            out << " .plan ";
            out << join(plans, ", ",
                    [](std::ostream& os, const auto& arg) { os << arg.first << ":" << *arg.second; });
        }
    }

    bool equal(const AstNode& node) const override {
        const auto& other = static_cast<const AstExecutionPlan&>(node);
        return equal_targets(plans, other.plans);
    }

private:
    /** mapping versions of clauses to execution plans */
    std::map<int, Own<AstExecutionOrder>> plans;
};

/**
 * Intermediate representation of a datalog clause.
 *
 *  A clause can either be:
 *      - a fact  - a clause with no body (e.g., X(a,b))
 *      - a rule  - a clause with a head and a body (e.g., Y(a,b) -: X(a,b))
 *
 * TODO (azreika): make clause abstract and split into two subclasses: Rule and Fact
 */
class AstClause : public AstNode {
public:
    AstClause(Own<AstAtom> head = {}, VecOwn<AstLiteral> bodyLiterals = {}, Own<AstExecutionPlan> plan = {},
            SrcLocation loc = {})
            : AstNode(std::move(loc)), head(std::move(head)), bodyLiterals(std::move(bodyLiterals)),
              plan(std::move(plan)) {}

    /** Add a Literal to the body of the clause */
    void addToBody(Own<AstLiteral> literal) {
        bodyLiterals.push_back(std::move(literal));
    }

    /** Set the head of clause to @p h */
    void setHead(Own<AstAtom> h) {
        head = std::move(h);
    }

    /** Set the bodyLiterals of clause to @p body */
    void setBodyLiterals(VecOwn<AstLiteral> body) {
        bodyLiterals = std::move(body);
    }

    /** Return the atom that represents the head of the clause */
    AstAtom* getHead() const {
        return head.get();
    }

    /** Obtains a copy of the internally maintained body literals */
    std::vector<AstLiteral*> getBodyLiterals() const {
        return toPtrVector(bodyLiterals);
    }

    /** Obtains the execution plan associated to this clause or null if there is none */
    const AstExecutionPlan* getExecutionPlan() const {
        return plan.get();
    }

    /** Updates the execution plan associated to this clause */
    void setExecutionPlan(Own<AstExecutionPlan> plan) {
        this->plan = std::move(plan);
    }

    /** Resets the execution plan */
    void clearExecutionPlan() {
        plan = nullptr;
    }

    AstClause* clone() const override {
        return new AstClause(
                souffle::clone(head), souffle::clone(bodyLiterals), souffle::clone(plan), getSrcLoc());
    }

    void apply(const AstNodeMapper& map) override {
        head = map(std::move(head));
        for (auto& lit : bodyLiterals) {
            lit = map(std::move(lit));
        }
    }

    std::vector<const AstNode*> getChildNodes() const override {
        std::vector<const AstNode*> res = {head.get()};
        for (auto& cur : bodyLiterals) {
            res.push_back(cur.get());
        }
        return res;
    }

protected:
    void print(std::ostream& os) const override {
        if (head != nullptr) {
            os << *head;
        }
        if (!bodyLiterals.empty()) {
            os << " :- \n   " << join(bodyLiterals, ",\n   ");
        }
        os << ".";
        if (plan != nullptr) {
            os << *plan;
        }
    }

    bool equal(const AstNode& node) const override {
        const auto& other = static_cast<const AstClause&>(node);
        return equal_ptr(head, other.head) && equal_targets(bodyLiterals, other.bodyLiterals) &&
               equal_ptr(plan, other.plan);
    }

    /** head of the clause */
    Own<AstAtom> head;

    /** body literals of clause */
    VecOwn<AstLiteral> bodyLiterals;

    /** user defined execution plan (if not defined, plan is null) */
    Own<AstExecutionPlan> plan;
};

}  // end of namespace souffle
