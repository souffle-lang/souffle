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
 * Defines class Clause that represents rules including facts, predicates, and
 * queries in a Datalog program.
 *
 ***********************************************************************/

#pragma once

#include "AstArgument.h"
#include "AstLiteral.h"
#include "AstRelationIdentifier.h"
#include "Util.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

namespace souffle {

class AstProgram;

/**
 * An execution order for atoms within a clause.
 */
class AstExecutionOrder : public AstNode {
public:
    typedef typename std::vector<unsigned int>::const_iterator const_iterator;

private:
    /** The actual order, starting with 1 (!) */
    std::vector<unsigned int> order;

public:
    /** The length of this order */
    std::size_t size() const {
        return order.size();
    }

    /** Appends another atom to this order */
    void appendAtomIndex(int index) {
        order.push_back(index);
    }

    /** Obtains a reference to the underlying vector */
    const std::vector<unsigned int>& getOrder() const {
        return order;
    }

    /** Provides access to individual elements of this order */
    int operator[](unsigned index) const {
        assert(index < order.size());
        return order[index];
    }

    /** Verifies that this order is complete */
    bool isComplete() const {
        for (unsigned i = 1; i <= order.size(); i++) {
            if (!contains(order, i)) {
                return false;
            }
        }
        return true;
    }

    const_iterator begin() const {
        return order.begin();
    }

    const_iterator end() const {
        return order.end();
    }

    AstExecutionOrder* clone() const override {
        auto res = new AstExecutionOrder();
        res->setSrcLoc(getSrcLoc());
        res->order = order;
        return res;
    }

    void apply(const AstNodeMapper& /*mapper*/) override {}

    std::vector<const AstNode*> getChildNodes() const override {
        return {};
    }

    void print(std::ostream& out) const override {
        out << "(" << order[0];
        for (size_t i = 1; i < order.size(); i++) {
            out << "," << order[i];
        }
        out << ")";
    }

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(dynamic_cast<const AstExecutionOrder*>(&node));
        const AstExecutionOrder& other = static_cast<const AstExecutionOrder&>(node);
        return order == other.order;
    }
};

/**
 * The class utilized to model user-defined execution plans for various
 * versions of clauses.
 */
class AstExecutionPlan : public AstNode {
public:
    typedef typename std::map<int, AstExecutionOrder>::const_iterator const_iterator;

private:
    /** Mapping versions of clauses to execution plans */
    std::map<int, std::unique_ptr<AstExecutionOrder>> plans;

    /** remember maximal version number */
    int maxVersion;

public:
    AstExecutionPlan() : maxVersion(-1) {}

    /** Updates the execution order for a special version of a rule */
    void setOrderFor(int version, std::unique_ptr<AstExecutionOrder> plan) {
        plans[version] = std::move(plan);
        if (version > maxVersion) {
            maxVersion = version;
        }
    }

    /** Determines whether for the given version a plan has been specified */
    bool hasOrderFor(int version) const {
        return plans.find(version) != plans.end();
    }

    /** get maximal version number */
    const int getMaxVersion() const {
        return maxVersion;
    }

    /** Obtains the order defined for the given version */
    const AstExecutionOrder& getOrderFor(int version) const {
        assert(hasOrderFor(version));
        return *plans.find(version)->second;
    }

    /** Tests whether there has any order been defined */
    bool empty() const {
        return plans.empty();
    }

    std::map<int, const AstExecutionOrder*> getOrders() const {
        std::map<int, const AstExecutionOrder*> result;
        for (auto& plan : plans) {
            result.insert(std::make_pair(plan.first, plan.second.get()));
        }
        return result;
    }

    void print(std::ostream& out) const override {
        if (!plans.empty()) {
            out << "\n\n   .plan ";
            bool first = true;
            for (auto it = plans.begin(); it != plans.end(); ++it) {
                if (first) {
                    first = false;
                } else {
                    out << ",";
                }
                out << (*it).first << ":";
                (*it).second->print(out);
            }
        }
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

    /** Obtains a list of all embedded child nodes */
    std::vector<const AstNode*> getChildNodes() const override {
        std::vector<const AstNode*> childNodes;
        for (auto& plan : plans) {
            childNodes.push_back(plan.second.get());
        }
        return childNodes;
    }

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(dynamic_cast<const AstExecutionPlan*>(&node));
        const AstExecutionPlan& other = static_cast<const AstExecutionPlan&>(node);
        if (maxVersion != other.maxVersion) {
            return false;
        }
        if (plans.size() != other.plans.size()) {
            return false;
        }
        auto iter = plans.begin();
        auto otherIter = other.plans.begin();
        for (; iter != plans.end(); ++iter, ++otherIter) {
            if (iter->first != otherIter->first) {
                return false;
            }
            if (*iter->second != *otherIter->second) {
                return false;
            }
        }
        return true;
    }
};

/**
 * Intermediate representation of a datalog clause.
 *
 *  A clause can either be:
 *      - a fact  - a clause with no body (e.g., X(a,b))
 *      - a rule  - a clause with a head and a body (e.g., Y(a,b) -: X(a,b))
 *
 * TODO: Currently Clause object is used to represent 2 different types of datalog
 *       clauses, such as rules, queries and facts. This solution was to quickly
 *       overcome issues related to bottom-up construction of IR. In future,
 *       Clause should be  made abstract and have 2 subclasses: Rule and Fact.
 */
class AstClause : public AstNode {
protected:
    /** The head of the clause */
    std::unique_ptr<AstAtom> head;

    /** The atoms in the body of this clause */
    std::vector<std::unique_ptr<AstAtom>> atoms;

    /** The negations in the body of this clause */
    std::vector<std::unique_ptr<AstNegation>> negations;

    /** The constraints in the body of this clause */
    std::vector<std::unique_ptr<AstConstraint>> constraints;

    /** Determines whether the given execution order should be enforced */
    bool fixedPlan;

    /** The user defined execution plan -- if any */
    std::unique_ptr<AstExecutionPlan> plan;

    /** Determines whether this is an internally generated clause resulting from resolving syntactic sugar */
    bool generated;

public:
    /** Construct an empty clause with empty list of literals and
        its head set to NULL */
    AstClause() : head(nullptr), fixedPlan(false), plan(nullptr), generated(false) {}

    ~AstClause() override = default;

    /** Add a Literal to the body of the clause */
    void addToBody(std::unique_ptr<AstLiteral> l);

    /** Set the head of clause to @p h */
    void setHead(std::unique_ptr<AstAtom> h);

    /** Return the atom that represents the head of the clause */
    AstAtom* getHead() const {
        return head.get();
    }

    /** Return the number of elements in the body of the Clause */
    size_t getBodySize() const {
        return atoms.size() + negations.size() + constraints.size();
    }

    /** Return the i-th Literal in body of the clause */
    AstLiteral* getBodyLiteral(size_t idx) const;

    /** Obtains a copy of the internally maintained body literals */
    std::vector<AstLiteral*> getBodyLiterals() const;

    /** Re-orders atoms to be in the given order. */
    void reorderAtoms(const std::vector<unsigned int>& newOrder);

    /** Obtains a list of contained body-atoms. */
    std::vector<AstAtom*> getAtoms() const {
        return toPtrVector(atoms);
    }

    /** Obtains a list of contained negations. */
    std::vector<AstNegation*> getNegations() const {
        return toPtrVector(negations);
    }

    /** Obtains a list of constraints */
    std::vector<AstConstraint*> getConstraints() const {
        return toPtrVector(constraints);
    }

    /** Return @p true if the clause is a rule */
    bool isRule() const {
        return head && !isFact();
    }

    /** Return @p true if the clause is a fact */
    bool isFact() const;

    /** Updates the fixed execution order flag */
    void setFixedExecutionPlan(bool value = true) {
        fixedPlan = value;
    }

    /** Determines whether the execution order plan is fixed */
    bool hasFixedExecutionPlan() const {
        return fixedPlan;
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

    /** Determines whether this is a internally generated clause */
    bool isGenerated() const {
        return generated;
    }

    /** Updates the generated flag */
    void setGenerated(bool value = true) {
        generated = value;
    }

    /** Print this clause to a given stream */
    void print(std::ostream& os) const override;

    /** Creates a clone if this AST sub-structure */
    AstClause* clone() const override {
        auto res = new AstClause();
        res->setSrcLoc(getSrcLoc());
        if (getExecutionPlan()) {
            res->setExecutionPlan(std::unique_ptr<AstExecutionPlan>(plan->clone()));
        }
        res->head = (head) ? std::unique_ptr<AstAtom>(head->clone()) : nullptr;
        for (const auto& cur : atoms) {
            res->atoms.push_back(std::unique_ptr<AstAtom>(cur->clone()));
        }
        for (const auto& cur : negations) {
            res->negations.push_back(std::unique_ptr<AstNegation>(cur->clone()));
        }
        for (const auto& cur : constraints) {
            res->constraints.push_back(std::unique_ptr<AstConstraint>(cur->clone()));
        }
        res->fixedPlan = fixedPlan;
        res->generated = generated;
        return res;
    }

    /** Mutates this node */
    void apply(const AstNodeMapper& map) override {
        head = map(std::move(head));
        for (auto& lit : atoms) {
            lit = map(std::move(lit));
        }
        for (auto& lit : negations) {
            lit = map(std::move(lit));
        }
        for (auto& lit : constraints) {
            lit = map(std::move(lit));
        }
    }

    /** clone head generates a new clause with the same head but empty body */
    AstClause* cloneHead() const {
        AstClause* clone = new AstClause();
        clone->setSrcLoc(getSrcLoc());
        clone->setHead(std::unique_ptr<AstAtom>(getHead()->clone()));
        if (getExecutionPlan()) {
            clone->setExecutionPlan(std::unique_ptr<AstExecutionPlan>(getExecutionPlan()->clone()));
        }
        clone->setFixedExecutionPlan(hasFixedExecutionPlan());
        return clone;
    }

    /** Obtains a list of all embedded child nodes */
    std::vector<const AstNode*> getChildNodes() const override {
        std::vector<const AstNode*> res = {head.get()};
        for (auto& cur : atoms) {
            res.push_back(cur.get());
        }
        for (auto& cur : negations) {
            res.push_back(cur.get());
        }
        for (auto& cur : constraints) {
            res.push_back(cur.get());
        }
        return res;
    }

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(dynamic_cast<const AstClause*>(&node));
        const AstClause& other = static_cast<const AstClause&>(node);
        return *head == *other.head && equal_targets(atoms, other.atoms) &&
               equal_targets(negations, other.negations) && equal_targets(constraints, other.constraints);
    }
};

}  // end of namespace souffle
