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

#include "AstAbstract.h"
#include "AstArgument.h"
#include "AstLiteral.h"
#include "AstNode.h"
#include "Util.h"
#include <cassert>
#include <cstddef>
#include <map>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

namespace souffle {

/**
 * An execution order for atoms within a clause.
 */
class AstExecutionOrder : public AstNode {
public:
    /** appends index of an atom */
    void appendAtomIndex(int index) {
        order.push_back(index);
    }

    /** get order */
    const std::vector<unsigned int>& getOrder() const {
        return order;
    }

    AstExecutionOrder* clone() const override {
        auto res = new AstExecutionOrder();
        res->setSrcLoc(getSrcLoc());
        res->order = order;
        return res;
    }

protected:
    void print(std::ostream& out) const override {
        out << "(" << join(order) << ")";
    }

    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstExecutionOrder*>(&node));
        const auto& other = static_cast<const AstExecutionOrder&>(node);
        return order == other.order;
    }

private:
    /** literal order of body (starting from 1) */
    std::vector<unsigned int> order;
};

/**
 * The class utilized to model user-defined execution plans for various
 * versions of clauses.
 */
class AstExecutionPlan : public AstNode {
public:
    /** updates execution order for rule version */
    void setOrderFor(int version, std::unique_ptr<AstExecutionOrder> plan) {
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
            res->setOrderFor(plan.first, std::unique_ptr<AstExecutionOrder>(plan.second->clone()));
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
        assert(nullptr != dynamic_cast<const AstExecutionPlan*>(&node));
        const auto& other = static_cast<const AstExecutionPlan&>(node);
        return equal_targets(plans, other.plans);
    }

private:
    /** mapping versions of clauses to execution plans */
    std::map<int, std::unique_ptr<AstExecutionOrder>> plans;
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
    /** Add a Literal to the body of the clause */
    void addToBody(std::unique_ptr<AstLiteral> literal) {
        bodyLiterals.push_back(std::move(literal));
    }

    /** Set the head of clause to @p h */
    void setHead(std::unique_ptr<AstAtom> h) {
        assert(!head && "Head is already set");
        head = std::move(h);
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
    void setExecutionPlan(std::unique_ptr<AstExecutionPlan> plan) {
        this->plan = std::move(plan);
    }

    /** Resets the execution plan */
    void clearExecutionPlan() {
        plan = nullptr;
    }

    AstClause* clone() const override {
        auto res = new AstClause();
        res->setSrcLoc(getSrcLoc());
        if (getExecutionPlan() != nullptr) {
            res->setExecutionPlan(std::unique_ptr<AstExecutionPlan>(plan->clone()));
        }
        res->head = (head) ? std::unique_ptr<AstAtom>(head->clone()) : nullptr;
        for (const auto& lit : bodyLiterals) {
            res->bodyLiterals.emplace_back(lit->clone());
        }
        return res;
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
            os << " :- \n   ";
            os << join(getBodyLiterals(), ",\n   ", print_deref<AstLiteral*>());
        }
        os << ".";
        if (plan != nullptr) {
            os << *plan;
        }
    }

    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstClause*>(&node));
        const auto& other = static_cast<const AstClause&>(node);
        return equal_ptr(head, other.head) && equal_targets(bodyLiterals, other.bodyLiterals) &&
               equal_ptr(plan, other.plan);
    }

    /** head of the clause */
    std::unique_ptr<AstAtom> head;

    /** body literals of clause */
    std::vector<std::unique_ptr<AstLiteral>> bodyLiterals;

    /** user defined execution plan (if not defined, plan is null) */
    std::unique_ptr<AstExecutionPlan> plan;
};

}  // end of namespace souffle
