/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamOperation.h
 *
 * Defines the Operation of a relational algebra query.
 *
 ***********************************************************************/

#pragma once

#include "RamCondition.h"
#include "RamNode.h"
#include "RamRelation.h"

#include <memory>
#include <set>

namespace souffle {

/**
 * Abstract class for a relational algebra operation
 */
class RamOperation : public RamNode {
protected:
    /** the nesting level of this operation */
    size_t level;

    /**
     * condition that is checked for each obtained tuple
     *
     * If condition is a nullptr, then no condition applies
     */
    std::unique_ptr<RamCondition> condition;

public:
    RamOperation(RamNodeType type, size_t l) : RamNode(type), level(l), condition(nullptr) {}

    /** Get level */
    // TODO (#541): move to analysis
    size_t getLevel() const {
        return level;
    }

    /** Get depth */
    // TODO (#541): move to analysis
    virtual size_t getDepth() const = 0;

    /** Print */
    virtual void print(std::ostream& os, int tabpos) const = 0;

    /** Pretty print */
    void print(std::ostream& os) const override {
        print(os, 0);
    }

    /** Add condition */
    virtual void addCondition(std::unique_ptr<RamCondition> c, RamOperation* root);

    /** Add condition */
    void addCondition(std::unique_ptr<RamCondition> c) {
        addCondition(std::move(c), this);
    }

    /** Get condition */
    RamCondition* getCondition() const {
        return condition.get();
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        if (!condition) {
            return toVector<const RamNode*>();
        }
        return {condition.get()};
    }

    /** Create clone */
    RamOperation* clone() const override = 0;

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        if (condition) {
            condition = map(std::move(condition));
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamOperation*>(&node));
        const RamOperation& other = static_cast<const RamOperation&>(node);
        if (getCondition() != nullptr && other.getCondition() != nullptr) {
            return *getCondition() == *other.getCondition();
        } else if (getCondition() == nullptr && other.getCondition() == nullptr) {
            return true;
        } else {
            return false;
        }
    }
};

/**
 * Abstract class for relation scans and lookups
 */
class RamSearch : public RamOperation {
    /** Nested operation */
    std::unique_ptr<RamOperation> nestedOperation;

public:
    RamSearch(RamNodeType type, std::unique_ptr<RamOperation> nested)
            : RamOperation(type, nested->getLevel() - 1), nestedOperation(std::move(nested)) {}

    /** get nested operation */
    RamOperation* getNestedOperation() const {
        return nestedOperation.get();
    }
    /** get nested operation */
    const RamOperation& getOperation() const {
        ASSERT(nestedOperation);
        return *nestedOperation;
    }

    /** Add condition */
    void addCondition(std::unique_ptr<RamCondition> c, RamOperation* root) override;

    /** Get depth of query */
    size_t getDepth() const override {
        return 1 + nestedOperation->getDepth();
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamOperation::getChildNodes();
        res.push_back(nestedOperation.get());
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        RamOperation::apply(map);
        nestedOperation = map(std::move(nestedOperation));
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamSearch*>(&node));
        const RamSearch& other = static_cast<const RamSearch&>(node);
        return RamOperation::equal(other) && getOperation() == other.getOperation();
    }
};

/**
 * Relation Scan
 *
 * Iterate all tuples of a relation and filter them via a condition
 */
class RamScan : public RamSearch {
protected:
    /** Search relation */
    RamRelation relation;

    /** Values of index per column of table (if indexable) */
    std::vector<std::unique_ptr<RamValue>> queryPattern;

    /** Indexable columns for a range query */
    SearchColumns keys;

    /**
     * Determines whether this scan operation is merely verifying the existence
     * of a value (e.g. rel(_,_), rel(1,2), rel(1,_) or rel(X,Y) where X and Y are bound)
     * or actually contributing new variable bindings (X or Y are not bound).
     *
     * The exists-only can be check much more efficient than the other case.
     */
    bool pureExistenceCheck;

public:
    RamScan(const RamRelation& r, std::unique_ptr<RamOperation> nested, bool pureExistenceCheck)
            : RamSearch(RN_Scan, std::move(nested)), relation(r), queryPattern(r.getArity()), keys(0),
              pureExistenceCheck(pureExistenceCheck) {}

    /** Get search relation */
    const RamRelation& getRelation() const {
        return relation;
    }

    /** Get indexable columns of scan */
    const SearchColumns& getRangeQueryColumns() const {
        return keys;
    }

    /** Get range pattern */
    std::vector<RamValue*> getRangePattern() const {
        return toPtrVector(queryPattern);
    }

    /** Check for pure existence check */
    // TODO (#541): rename pure existence check to complete/whole etc.
    bool isPureExistenceCheck() const {
        return pureExistenceCheck;
    }

    /** Print */
    void print(std::ostream& os, int tabpos) const override;

    /** Add condition */
    void addCondition(std::unique_ptr<RamCondition> c, RamOperation* root) override;

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamSearch::getChildNodes();
        for (auto& cur : queryPattern) {
            if (cur) {
                res.push_back(cur.get());
            }
        }
        return res;
    }

    /** Create clone */
    RamScan* clone() const override {
        RamScan* res = new RamScan(getRelation(),
                std::unique_ptr<RamOperation>(getNestedOperation()->clone()), pureExistenceCheck);
        res->condition = std::unique_ptr<RamCondition>(condition->clone());
        res->keys = keys;
        for (auto& cur : queryPattern) {
            if (cur) {
                res->queryPattern.push_back(std::unique_ptr<RamValue>(cur->clone()));
            }
        }
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        RamSearch::apply(map);
        for (auto& cur : queryPattern) {
            if (cur) {
                cur = map(std::move(cur));
            }
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamScan*>(&node));
        const RamScan& other = static_cast<const RamScan&>(node);
        return RamSearch::equal(other) && getRelation() == other.getRelation() &&
               equal_targets(queryPattern, other.queryPattern) && keys == other.keys &&
               pureExistenceCheck == other.pureExistenceCheck;
    }
};

/**
 * Record lookup
 */
// TODO (#541): wrong class hierarchy, no condition in RAMOperation necessary
class RamLookup : public RamSearch {
    /** Level of the tuple containing record reference */
    std::size_t refLevel;

    /** Position of the tuple containing record reference */
    std::size_t refPos;

    /** Arity of the unpacked tuple */
    std::size_t arity;

public:
    RamLookup(std::unique_ptr<RamOperation> nested, std::size_t ref_level, std::size_t ref_pos,
            std::size_t arity)
            : RamSearch(RN_Lookup, std::move(nested)), refLevel(ref_level), refPos(ref_pos), arity(arity) {}

    /** Get reference level */
    std::size_t getReferenceLevel() const {
        return refLevel;
    }

    /** Get reference position */
    std::size_t getReferencePosition() const {
        return refPos;
    }

    /** Get Arity */
    std::size_t getArity() const {
        return arity;
    }

    /** Print */
    void print(std::ostream& os, int tabpos) const override;

    /** Create clone */
    RamLookup* clone() const override {
        RamLookup* res = new RamLookup(
                std::unique_ptr<RamOperation>(getNestedOperation()->clone()), refLevel, refPos, arity);
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        RamSearch::apply(map);
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamLookup*>(&node));
        const RamLookup& other = static_cast<const RamLookup&>(node);
        return RamSearch::equal(other) && getReferencePosition() == other.getReferencePosition() &&
               getReferenceLevel() == other.getReferenceLevel() && getArity() == other.getArity();
    }
};

/**
 * Aggregation
 */
class RamAggregate : public RamSearch {
public:
    /** Types of aggregation functions */
    enum Function { MAX, MIN, COUNT, SUM };

private:
    /** Aggregation function */
    Function fun;

    /** Aggregation value */
    // TODO (#541): rename to target expression
    std::unique_ptr<RamValue> value;

    /** Aggregation relation */
    RamRelation relation;

    /** Pattern for filtering tuples */
    std::vector<std::unique_ptr<RamValue>> pattern;

    /** Columns to be matched when using a range query */
    SearchColumns keys;

public:
    RamAggregate(std::unique_ptr<RamOperation> nested, Function fun, std::unique_ptr<RamValue> value,
            const RamRelation& relation)
            : RamSearch(RN_Aggregate, std::move(nested)), fun(fun), value(std::move(value)),
              relation(relation), pattern(relation.getArity()), keys(0) {}

    /** Get aggregation function */
    Function getFunction() const {
        return fun;
    }

    /** Get target expression */
    // TODO (#541): rename to getExpression
    const RamValue* getTargetExpression() const {
        ASSERT(value);
        return value.get();
    }

    /** Get relation */
    const RamRelation& getRelation() const {
        return relation;
    }

    /** Get pattern */
    std::vector<RamValue*> getPattern() const {
        return toPtrVector(pattern);
    }

    /** Add condition */
    void addCondition(std::unique_ptr<RamCondition> c, RamOperation* root) override;

    /** Get range query columns */
    SearchColumns getRangeQueryColumns() const {
        return keys;
    }

    /** Print */
    void print(std::ostream& os, int tabpos) const override;

    /** Create clone */
    RamAggregate* clone() const override {
        RamAggregate* res = new RamAggregate(std::unique_ptr<RamOperation>(getNestedOperation()->clone()),
                fun, std::unique_ptr<RamValue>(value->clone()), relation);
        res->keys = keys;
        for (auto& cur : pattern) {
            if (cur) {
                res->pattern.push_back(std::unique_ptr<RamValue>(cur->clone()));
            }
        }
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        RamSearch::apply(map);
        value = map(std::move(value));
        for (auto& cur : pattern) {
            if (cur) {
                cur = map(std::move(cur));
            }
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamAggregate*>(&node));
        const RamAggregate& other = static_cast<const RamAggregate&>(node);
        return RamSearch::equal(other) && getRelation() == other.getRelation() &&
               equal_targets(pattern, other.pattern) && keys == other.keys && fun == other.fun &&
               getTargetExpression() == other.getTargetExpression();
    }
};

/** Projection */
class RamProject : public RamOperation {
protected:
    /** Relation */
    RamRelation relation;

    /** Relation to check whether does not exist */
    // TODO (#541): rename
    std::unique_ptr<RamRelation> filter;

    /* Values for projection */
    std::vector<std::unique_ptr<RamValue>> values;

public:
    RamProject(const RamRelation& relation, size_t level)
            : RamOperation(RN_Project, level), relation(relation), filter(nullptr) {}

    RamProject(const RamRelation& relation, const RamRelation& filter, size_t level)
            : RamOperation(RN_Project, level), relation(relation),
              filter(std::unique_ptr<RamRelation>(new RamRelation(filter))) {}

    /** Add value for a column */
    void addArg(std::unique_ptr<RamValue> v) {
        values.push_back(std::move(v));
    }

    /** Add condition to project, needed for different level check */
    void addCondition(std::unique_ptr<RamCondition> c, RamOperation* root) override;

    /** Get relation */
    const RamRelation& getRelation() const {
        return relation;
    }

    /** Check filter */
    bool hasFilter() const {
        return (bool)filter;
    }

    /** Get filter */
    const RamRelation& getFilter() const {
        assert(hasFilter());
        return *filter;
    }

    /** Get values */
    std::vector<RamValue*> getValues() const {
        return toPtrVector(values);
    }

    /** Get depth */
    size_t getDepth() const override {
        return 1;
    }

    /** Print */
    void print(std::ostream& os, int tabpos) const override;

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamOperation::getChildNodes();
        for (const auto& cur : values) {
            res.push_back(cur.get());
        }
        return res;
    }

    /** Create clone */
    RamProject* clone() const override {
        RamProject* res = new RamProject(getRelation(), level);
        if (filter != nullptr) {
            res->filter = std::unique_ptr<RamRelation>(new RamRelation(*filter));
        }
        for (auto& cur : values) {
            res->values.push_back(std::unique_ptr<RamValue>(cur->clone()));
        }
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        RamOperation::apply(map);
        for (auto& cur : values) {
            cur = map(std::move(cur));
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamProject*>(&node));
        const RamProject& other = static_cast<const RamProject&>(node);
        bool isFilterEqual = false;
        if (filter == nullptr && other.filter == nullptr) {
            isFilterEqual = true;
        } else if (filter != nullptr && other.filter != nullptr) {
            isFilterEqual = (*filter == *other.filter);
        }
        return RamOperation::equal(other) && getRelation() == other.getRelation() &&
               equal_targets(values, other.values) && isFilterEqual;
    }
};

/** A statement for returning from a ram subroutine */
class RamReturn : public RamOperation {
protected:
    std::vector<std::unique_ptr<RamValue>> values;

public:
    RamReturn(size_t level) : RamOperation(RN_Return, level) {}

    void print(std::ostream& out, int tabpos) const override;

    size_t getDepth() const override {
        return 1;
    }

    void addValue(std::unique_ptr<RamValue> val) {
        values.push_back(std::move(val));
    }

    std::vector<RamValue*> getValues() const {
        return toPtrVector(values);
    }

    /** Get value */
    RamValue& getValue(size_t i) const {
        assert(i < values.size() && "value index out of range");
        return *values[i];
    }

    /** Create clone */
    RamReturn* clone() const override {
        RamReturn* res = new RamReturn(level);
        for (auto& cur : values) {
            res->values.push_back(std::unique_ptr<RamValue>(cur->clone()));
        }
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        RamOperation::apply(map);
        for (auto& cur : values) {
            cur = map(std::move(cur));
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamReturn*>(&node));
        const RamReturn& other = static_cast<const RamReturn&>(node);
        return RamOperation::equal(other) && equal_targets(values, other.values);
    }
};

}  // namespace souffle
