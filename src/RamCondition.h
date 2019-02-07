/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamCondition.h
 *
 * Defines a class for evaluating conditions in the Relational Algebra
 * Machine.
 *
 ***********************************************************************/

#pragma once

#include "BinaryConstraintOps.h"
#include "RamNode.h"
#include "RamRelation.h"
#include "RamValue.h"
#include "SymbolTable.h"

#include <algorithm>
#include <sstream>
#include <string>

#include <cstdlib>

namespace souffle {

/**
 * Abstract Class for RAM condition
 */
class RamCondition : public RamNode {
public:
    RamCondition(RamNodeType type) : RamNode(type) {}

    /** Create clone */
    RamCondition* clone() const override = 0;
};

/**
 * Conjunction
 */
// TODO (#541): rename to RAMConjunction
class RamAnd : public RamCondition {
protected:
    /** Left-hand side of conjunction */
    std::unique_ptr<RamCondition> lhs;

    /** Right-hand side of conjunction */
    std::unique_ptr<RamCondition> rhs;

public:
    RamAnd(std::unique_ptr<RamCondition> l, std::unique_ptr<RamCondition> r)
            : RamCondition(RN_And), lhs(std::move(l)), rhs(std::move(r)) {}

    /** Get left-hand side of conjunction */
    const RamCondition& getLHS() const {
        assert(lhs);
        return *lhs;
    }

    /** Get right-hand side of conjunction */
    const RamCondition& getRHS() const {
        assert(rhs);
        return *rhs;
    }

    /** Print */
    void print(std::ostream& os) const override {
        lhs->print(os);
        os << " and ";
        rhs->print(os);
    }

    /** Obtains list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return {lhs.get(), rhs.get()};
    }

    /** Create clone */
    RamAnd* clone() const override {
        RamAnd* res = new RamAnd(
                std::unique_ptr<RamCondition>(lhs->clone()), std::unique_ptr<RamCondition>(rhs->clone()));
        return res;
    }

    /** Apply */
    void apply(const RamNodeMapper& map) override {
        lhs = map(std::move(lhs));
        rhs = map(std::move(rhs));
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamAnd*>(&node));
        const auto& other = static_cast<const RamAnd&>(node);
        return getLHS() == other.getLHS() && getRHS() == other.getRHS();
    }
};

/**
 * Binary constraint
 */
// TODO (#541): rename to RamConstraint
class RamBinaryRelation : public RamCondition {
private:
    /** Operator */
    BinaryConstraintOp op;

    /** Left-hand side of constraint*/
    std::unique_ptr<RamValue> lhs;

    /** Right-hand side of constraint */
    std::unique_ptr<RamValue> rhs;

public:
    RamBinaryRelation(BinaryConstraintOp op, std::unique_ptr<RamValue> l, std::unique_ptr<RamValue> r)
            : RamCondition(RN_BinaryRelation), op(op), lhs(std::move(l)), rhs(std::move(r)) {}

    /** Print */
    void print(std::ostream& os) const override {
        lhs->print(os);
        os << " " << toBinaryConstraintSymbol(op) << " ";
        rhs->print(os);
    }

    /** Get left-hand side */
    RamValue* getLHS() const {
        return lhs.get();
    }
    /** Get right-hand side */
    RamValue* getRHS() const {
        return rhs.get();
    }

    /** Take left-hand side */
    std::unique_ptr<RamValue> takeLHS() {
        return std::move(lhs);
    }

    /** Take right-hand side */
    std::unique_ptr<RamValue> takeRHS() {
        return std::move(rhs);
    }

    /** Set left-hand side */
    void setLHS(std::unique_ptr<RamValue> l) {
        lhs.swap(l);
    }
    /** Set right-hand side */
    void setRHS(std::unique_ptr<RamValue> r) {
        rhs.swap(r);
    }

    /** Get operator symbol */
    BinaryConstraintOp getOperator() const {
        return op;
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return {lhs.get(), rhs.get()};
    }

    /** Create clone */
    RamBinaryRelation* clone() const override {
        RamBinaryRelation* res = new RamBinaryRelation(
                op, std::unique_ptr<RamValue>(lhs->clone()), std::unique_ptr<RamValue>(rhs->clone()));
        return res;
    }

    /** Apply */
    void apply(const RamNodeMapper& map) override {
        lhs = map(std::move(lhs));
        rhs = map(std::move(rhs));
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamBinaryRelation*>(&node));
        const auto& other = static_cast<const RamBinaryRelation&>(node);
        return getOperator() == other.getOperator() && getLHS() == other.getLHS() &&
               getRHS() == other.getRHS();
    }
};

/** Not existence check for a relation */
class RamNotExists : public RamCondition {
protected:
    /* Relation */
    std::unique_ptr<RamRelationReference> relation;

    /** Pattern -- nullptr if undefined */
    // TODO (#541): rename to argument
    std::vector<std::unique_ptr<RamValue>> values;

public:
    RamNotExists(std::unique_ptr<RamRelationReference> rel)
            : RamCondition(RN_NotExists), relation(std::move(rel)) {}

    /** Get relation */
    const RamRelationReference& getRelation() const {
        return *relation;
    }

    /** Get arguments */
    std::vector<RamValue*> getValues() const {
        return toPtrVector(values);
    }

    /** Add argument */
    void addArg(std::unique_ptr<RamValue> v) {
        values.push_back(std::move(v));
    }

    /** Print */
    void print(std::ostream& os) const override {
        os << "("
           << join(values, ",",
                      [](std::ostream& out, const std::unique_ptr<RamValue>& value) {
                          if (!value) {
                              out << "_";
                          } else {
                              out << *value;
                          }
                      })
           << ") ∉ " << relation->getName();
    }

    /** Get key */
    SearchColumns getKey() const {
        SearchColumns res = 0;
        for (unsigned i = 0; i < values.size(); i++) {
            if (values[i]) {
                res |= (1 << i);
            }
        }
        return res;
    }

    /** Is key total */
    bool isTotal() const {
        for (const auto& cur : values) {
            if (!cur) {
                return false;
            }
        }
        return true;
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res = {relation.get()};
        for (const auto& cur : values) {
            res.push_back(cur.get());
        }
        return res;
    }

    /** Create clone */
    RamNotExists* clone() const override {
        RamNotExists* res = new RamNotExists(std::unique_ptr<RamRelationReference>(relation->clone()));
        for (auto& cur : values) {
            RamValue* val = nullptr;
            if (cur != nullptr) {
                val = cur->clone();
            }
            res->values.push_back(std::unique_ptr<RamValue>(val));
        }
        return res;
    }

    /** Apply */
    void apply(const RamNodeMapper& map) override {
        relation = map(std::move(relation));
        for (auto& val : values) {
            if (val != nullptr) {
                val = map(std::move(val));
            }
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamNotExists*>(&node));
        const auto& other = static_cast<const RamNotExists&>(node);
        return getRelation() == other.getRelation() && equal_targets(values, other.values);
    }
};

/** Not existence check for a relation for provenance existence check */
class RamProvenanceNotExists : public RamCondition {
protected:
    /* Relation */
    std::unique_ptr<RamRelationReference> relation;

    /** Pattern -- nullptr if undefined */
    // TODO (#541): rename to argument
    std::vector<std::unique_ptr<RamValue>> values;

public:
    RamProvenanceNotExists(std::unique_ptr<RamRelationReference> rel)
            : RamCondition(RN_ProvenanceNotExists), relation(std::move(rel)) {}

    /** Get relation */
    const RamRelationReference& getRelation() const {
        return *relation;
    }

    /** Get arguments */
    std::vector<RamValue*> getValues() const {
        return toPtrVector(values);
    }

    /** Add argument */
    void addArg(std::unique_ptr<RamValue> v) {
        values.push_back(std::move(v));
    }

    /** Print */
    void print(std::ostream& os) const override {
        os << "("
           << join(values, ",",
                      [](std::ostream& out, const std::unique_ptr<RamValue>& value) {
                          if (!value) {
                              out << "_";
                          } else {
                              out << *value;
                          }
                      })
           << ") prov∉ " << relation->getName();
    }

    /** Get key */
    SearchColumns getKey() const {
        SearchColumns res = 0;
        // values.size() - 1 because we discard the height annotation
        for (unsigned i = 0; i < values.size() - 1; i++) {
            if (values[i]) {
                res |= (1 << i);
            }
        }
        return res;
    }

    /** Is key total */
    bool isTotal() const {
        for (const auto& cur : values) {
            if (!cur) {
                return false;
            }
        }
        return true;
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res = {relation.get()};
        for (const auto& cur : values) {
            res.push_back(cur.get());
        }
        return res;
    }

    /** Create clone */
    RamProvenanceNotExists* clone() const override {
        RamProvenanceNotExists* res =
                new RamProvenanceNotExists(std::unique_ptr<RamRelationReference>(relation->clone()));
        for (auto& cur : values) {
            RamValue* val = nullptr;
            if (cur != nullptr) {
                val = cur->clone();
            }
            res->values.push_back(std::unique_ptr<RamValue>(val));
        }
        return res;
    }

    /** Apply */
    void apply(const RamNodeMapper& map) override {
        relation = map(std::move(relation));
        for (auto& val : values) {
            if (val != nullptr) {
                val = map(std::move(val));
            }
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamProvenanceNotExists*>(&node));
        const auto& other = static_cast<const RamProvenanceNotExists&>(node);
        return getRelation() == other.getRelation() && equal_targets(values, other.values);
    }
};

/**
 * Emptiness check for a relation
 */
// TODO (#541): Rename to RamEmptyCheck
class RamEmpty : public RamCondition {
    /** Relation */
    std::unique_ptr<RamRelationReference> relation;

public:
    RamEmpty(std::unique_ptr<RamRelationReference> rel) : RamCondition(RN_Empty), relation(std::move(rel)) {}

    /** Get relation */
    const RamRelationReference& getRelation() const {
        return *relation;
    }

    /** Print */
    void print(std::ostream& os) const override {
        os << relation->getName() << " = ∅";
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>() = {relation.get()};
    }

    /** Create clone */
    RamEmpty* clone() const override {
        RamEmpty* res = new RamEmpty(std::unique_ptr<RamRelationReference>(relation->clone()));
        return res;
    }

    /** Apply */
    void apply(const RamNodeMapper& map) override {
        relation = map(std::move(relation));
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamEmpty*>(&node));
        const auto& other = static_cast<const RamEmpty&>(node);
        return getRelation() == other.getRelation();
    }
};

}  // end of namespace souffle
