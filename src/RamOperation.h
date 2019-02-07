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
#include "RamConditionLevel.h"
#include "RamConstValue.h"
#include "RamNode.h"
#include "RamRelation.h"
#include "RamTypes.h"
#include "RamValue.h"
#include "RamValueLevel.h"
#include "Util.h"
#include <cassert>
#include <cstddef>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace souffle {

/**
 * Abstract class for a relational algebra operation
 */
class RamOperation : public RamNode {
public:
    RamOperation(RamNodeType type) : RamNode(type) {}

    /** Print */
    virtual void print(std::ostream& os, int tabpos) const = 0;

    /** Pretty print */
    void print(std::ostream& os) const override {
        print(os, 0);
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override = 0;

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override = 0;

    /** Create clone */
    RamOperation* clone() const override = 0;

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override = 0;
};

/**
 * Abstract class for nesting operations
 */
class RamNestedOperation : public RamOperation {
    /** Nested operation */
    std::unique_ptr<RamOperation> nestedOperation;

public:
    RamNestedOperation(RamNodeType type, std::unique_ptr<RamOperation> nested)
            : RamOperation(type), nestedOperation(std::move(nested)) {}

    /** Get nested operation */
    RamOperation& getOperation() const {
        assert(nestedOperation);
        return *nestedOperation;
    }

    /** Set nested operation */
    void setOperation(std::unique_ptr<RamOperation> nested) {
        nestedOperation = std::move(nested);
    }

    /** Print */
    void print(std::ostream& os, int tabpos) const override {
        nestedOperation->print(os, tabpos + 1);
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return {nestedOperation.get()};
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        nestedOperation = map(std::move(nestedOperation));
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamNestedOperation*>(&node));
        const auto& other = static_cast<const RamNestedOperation&>(node);
        return getOperation() == other.getOperation();
    }
};

/**
 * Abstract class for relation searches and lookups
 */
class RamSearch : public RamNestedOperation {
    /** Identifier for the tuple */
    size_t identifier;

    /** Profile text */
    const std::string profileText;

public:
    RamSearch(RamNodeType type, size_t ident, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamNestedOperation(type, std::move(nested)), identifier(ident),
              profileText(std::move(profileText)) {}

    /** Get identifier */
    std::size_t getIdentifier() const {
        return identifier;
    }

    /** Get profile text */
    const std::string& getProfileText() const {
        return profileText;
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamSearch*>(&node));
        const auto& other = static_cast<const RamSearch&>(node);
        return RamNestedOperation::equal(other) && getIdentifier() == other.getIdentifier() &&
               getProfileText() == other.getProfileText();
    }
};

/**
 * Abstract class for relation searches
 */
class RamRelationSearch : public RamSearch {
    /** Search relation */
    std::unique_ptr<RamRelationReference> relation;

    /**
     * Determines whether this search operation is merely verifying the existence
     * of a value (e.g. rel(_,_), rel(1,2), rel(1,_) or rel(X,Y) where X and Y are bound)
     * or actually contributing new variable bindings (X or Y are not bound).
     *
     * The exists-only can be check much more efficient than the other case.
     */
    bool pureExistenceCheck = false;

public:
    RamRelationSearch(RamNodeType type, std::unique_ptr<RamRelationReference> rel, size_t ident,
            std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : RamSearch(type, ident, std::move(nested), std::move(profileText)), relation(std::move(rel)) {}

    /** Get search relation */
    const RamRelationReference& getRelation() const {
        return *relation;
    }

    /** Check for pure existence check */
    // TODO (#541): rename pure existence check to complete/whole etc.
    bool isPureExistenceCheck() const {
        return pureExistenceCheck;
    }

    /** Set as pure existence check */
    void setIsPureExistenceCheck(const bool isExistCheck) {
        pureExistenceCheck = isExistCheck;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        RamSearch::apply(map);
        relation = map(std::move(relation));
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamRelationSearch*>(&node));
        const auto& other = static_cast<const RamRelationSearch&>(node);
        return RamSearch::equal(other) && getRelation() == other.getRelation() &&
               isPureExistenceCheck() == other.isPureExistenceCheck();
    }
};

/**
 * Relation Scan
 *
 * Iterate all tuples of a relation
 */
class RamScan : public RamRelationSearch {
public:
    RamScan(std::unique_ptr<RamRelationReference> rel, size_t ident, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamRelationSearch(RN_Scan, std::move(rel), ident, std::move(nested), std::move(profileText)) {}

    /** Print */
    void print(std::ostream& os, int tabpos) const override {
        os << times('\t', tabpos);
        if (isPureExistenceCheck()) {
            os << "if ∃ t" << getIdentifier() << " ∈ " << getRelation().getName();
        } else {
            os << "for t" << getIdentifier() << " in " << getRelation().getName();
        }
        os << " {\n";
        RamRelationSearch::print(os, tabpos + 1);
        os << times('\t', tabpos) << "}\n";
    }

    /** Create clone */
    RamScan* clone() const override {
        return new RamScan(std::unique_ptr<RamRelationReference>(getRelation().clone()), getIdentifier(),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
    }
};

/**
 * Relation Scan with Index
 *
 * Search for tuples of a relation matching a criteria
 */
class RamIndexScan : public RamRelationSearch {
protected:
    /** Values of index per column of table (if indexable) */
    std::vector<std::unique_ptr<RamValue>> queryPattern;

    /** Indexable columns for a range query */
    SearchColumns keys = 0;

public:
    RamIndexScan(std::unique_ptr<RamRelationReference> r, size_t ident,
            std::vector<std::unique_ptr<RamValue>> queryPattern, SearchColumns keys,
            std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : RamRelationSearch(RN_IndexScan, std::move(r), ident, std::move(nested), std::move(profileText)),
              queryPattern(std::move(queryPattern)), keys(keys) {}

    /** Get range pattern */
    std::vector<RamValue*> getRangePattern() const {
        return toPtrVector(queryPattern);
    }

    /** Get indexable columns of scan */
    const SearchColumns& getRangeQueryColumns() const {
        return keys;
    }

    /** Print */
    void print(std::ostream& os, int tabpos) const override {
        const RamRelationReference& rel = getRelation();
        os << times('\t', tabpos);
        if (isPureExistenceCheck()) {
            os << "if ∃ t" << getIdentifier() << " ∈ " << getRelation().getName() << " WITH ";
        } else {
            os << "SEARCH " << rel.getName() << " AS t" << getIdentifier() << " ON INDEX ";
        }
        bool first = true;
        for (unsigned int i = 0; i < rel.getArity(); ++i) {
            if (queryPattern[i] != nullptr) {
                if (first) {
                    first = false;
                } else {
                    os << " and ";
                }
                os << "t" << getIdentifier() << "." << rel.getArg(i) << "=";
                queryPattern[i]->print(os);
            }
        }
        os << '\n';
        RamRelationSearch::print(os, tabpos + 1);
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamRelationSearch::getChildNodes();
        for (auto& cur : queryPattern) {
            if (cur) {
                res.push_back(cur.get());
            }
        }
        return res;
    }

    /** Create clone */
    RamIndexScan* clone() const override {
        std::vector<std::unique_ptr<RamValue>> resQueryPattern;
        for (auto& cur : queryPattern) {
            if (cur) {
                resQueryPattern.push_back(std::unique_ptr<RamValue>(cur->clone()));
            }
        }
        RamIndexScan* res = new RamIndexScan(std::unique_ptr<RamRelationReference>(getRelation().clone()),
                getIdentifier(), std::move(resQueryPattern), keys,
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        RamRelationSearch::apply(map);
        for (auto& cur : queryPattern) {
            if (cur) {
                cur = map(std::move(cur));
            }
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamIndexScan*>(&node));
        const auto& other = static_cast<const RamIndexScan&>(node);
        return RamRelationSearch::equal(other) && equal_targets(queryPattern, other.queryPattern) &&
               keys == other.keys;
    }
};

/**
 * Record lookup
 */
class RamLookup : public RamSearch {
    /** Level of the tuple containing record reference */
    size_t refLevel;

    /** Position of the tuple containing record reference */
    size_t refPos;

    /** Arity of the unpacked tuple */
    size_t arity;

public:
    RamLookup(std::unique_ptr<RamOperation> nested, size_t ident, size_t ref_level, size_t ref_pos,
            size_t arity)
            : RamSearch(RN_Lookup, ident, std::move(nested)), refLevel(ref_level), refPos(ref_pos),
              arity(arity) {}

    /** Get reference level */
    std::size_t getReferenceLevel() const {
        return refLevel;
    }

    /** Get reference position */
    std::size_t getReferencePosition() const {
        return refPos;
    }

    /** Get arity */
    std::size_t getArity() const {
        return arity;
    }

    /** Print */
    void print(std::ostream& os, int tabpos) const override {
        os << times('\t', tabpos) << "UNPACK env(t" << refLevel << ", i" << refPos << ") INTO t"
           << getIdentifier() << " FOR \n";
        RamSearch::print(os, tabpos + 1);
    }

    /** Create clone */
    RamLookup* clone() const override {
        RamLookup* res = new RamLookup(std::unique_ptr<RamOperation>(getOperation().clone()), getIdentifier(),
                refLevel, refPos, arity);
        return res;
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamLookup*>(&node));
        const auto& other = static_cast<const RamLookup&>(node);
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
    /**
     * Condition that is checked for each obtained tuple
     *
     * If condition is a nullptr, then no condition applies
     */
    std::unique_ptr<RamCondition> condition;

    /** Aggregation function */
    Function function;

    /** Aggregation value */
    // TODO (#541): rename to target expression
    std::unique_ptr<RamValue> value;

    /** Aggregation relation */
    std::unique_ptr<RamRelationReference> relation;

    /** Pattern for filtering tuples */
    std::vector<std::unique_ptr<RamValue>> pattern;

    /** Columns to be matched when using a range query */
    SearchColumns keys = 0;

public:
    RamAggregate(std::unique_ptr<RamOperation> nested, Function fun, std::unique_ptr<RamValue> value,
            std::unique_ptr<RamRelationReference> rel, size_t ident)
            : RamSearch(RN_Aggregate, ident, std::move(nested)), function(fun), value(std::move(value)),
              relation(std::move(rel)), pattern(relation->getArity()) {}

    /** Get condition */
    RamCondition* getCondition() const {
        return condition.get();
    }

    /** Get aggregation function */
    Function getFunction() const {
        return function;
    }

    /** Get target expression */
    // TODO (#541): rename to getExpression
    const RamValue* getTargetExpression() const {
        assert(value);
        return value.get();
    }

    /** Get relation */
    const RamRelationReference& getRelation() const {
        return *relation;
    }

    /** Get pattern */
    std::vector<RamValue*> getPattern() const {
        return toPtrVector(pattern);
    }

    /** Get range query columns */
    SearchColumns getRangeQueryColumns() const {
        return keys;
    }

    /** Get indexable element */
    std::unique_ptr<RamValue> getIndexElement(RamCondition* c, size_t& element, size_t level);

    /** Add condition */
    void addCondition(std::unique_ptr<RamCondition> newCondition);

    /** Print */
    void print(std::ostream& os, int tabpos) const override {
        os << times('\t', tabpos);

        switch (function) {
            case MIN:
                os << "MIN ";
                break;
            case MAX:
                os << "MAX ";
                break;
            case COUNT:
                os << "COUNT ";
                break;
            case SUM:
                os << "SUM ";
                break;
        }

        if (function != COUNT) {
            os << *value << " ";
        }

        os << "AS t" << getIdentifier() << ".0 IN t" << getIdentifier() << " ∈ " << relation->getName();
        os << "(" << join(pattern, ",", [&](std::ostream& out, const std::unique_ptr<RamValue>& value) {
            if (!value) {
                out << "_";
            } else {
                out << *value;
            }
        }) << ")";

        if (auto condition = getCondition()) {
            os << " WHERE ";
            condition->print(os);
        }

        os << " FOR \n";
        RamSearch::print(os, tabpos + 1);
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamSearch::getChildNodes();
        if (condition) {
            res.push_back(condition.get());
        }
        return res;
    }

    /** Create clone */
    RamAggregate* clone() const override {
        RamAggregate* res = new RamAggregate(std::unique_ptr<RamOperation>(getOperation().clone()), function,
                value == nullptr ? nullptr : std::unique_ptr<RamValue>(value->clone()),
                std::unique_ptr<RamRelationReference>(relation->clone()), getIdentifier());
        res->keys = keys;
        for (size_t i = 0; i < pattern.size(); ++i) {
            if (pattern[i] != nullptr) {
                res->pattern[i] = std::unique_ptr<RamValue>(pattern[i]->clone());
            }
        }
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        RamSearch::apply(map);
        if (condition != nullptr) {
            condition = map(std::move(condition));
        }
        relation = map(std::move(relation));
        if (value != nullptr) {
            value = map(std::move(value));
        }
        for (auto& cur : pattern) {
            if (cur) {
                cur = map(std::move(cur));
            }
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamAggregate*>(&node));
        const auto& other = static_cast<const RamAggregate&>(node);
        if (getCondition() != nullptr && other.getCondition() != nullptr &&
                *getCondition() != *other.getCondition()) {
            return false;
        }
        return RamSearch::equal(other) && getCondition() == other.getCondition() &&
               getFunction() == other.getFunction() && getRelation() == other.getRelation() &&
               getTargetExpression() == other.getTargetExpression() &&
               equal_targets(pattern, other.pattern) &&
               getRangeQueryColumns() == other.getRangeQueryColumns();
    }
};

/**
 * Filter statement
 */
class RamFilter : public RamNestedOperation {
protected:
    /**
     * Condition that is checked for each obtained tuple
     *
     * If condition is a nullptr, then no condition applies
     */
    std::unique_ptr<RamCondition> condition;

public:
    RamFilter(std::unique_ptr<RamCondition> cond, std::unique_ptr<RamOperation> nested)
            : RamNestedOperation(RN_Filter, std::move(nested)), condition(std::move(cond)) {}

    /** Get condition */
    const RamCondition& getCondition() const {
        return *condition;
    }

    /** Print */
    void print(std::ostream& os, int tabpos) const override {
        os << times('\t', tabpos) << "IF ";
        getCondition().print(os);
        os << " {\n";
        RamNestedOperation::print(os, tabpos + 1);
        os << times('\t', tabpos) << "}\n";
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return {condition.get(), &getOperation()};
    }

    /** Create clone */
    RamFilter* clone() const override {
        return new RamFilter(std::unique_ptr<RamCondition>(condition->clone()),
                std::unique_ptr<RamOperation>(getOperation().clone()));
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        RamNestedOperation::apply(map);
        condition = map(std::move(condition));
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamFilter*>(&node));
        const auto& other = static_cast<const RamFilter&>(node);
        return getCondition() == other.getCondition() && RamNestedOperation::equal(node);
    }
};

/** Projection */
class RamProject : public RamOperation {
protected:
    /** Relation */
    std::unique_ptr<RamRelationReference> relation;

    /** Relation to check whether does not exist */
    // TODO (#541): rename
    std::unique_ptr<RamRelationReference> filter;

    /* Values for projection */
    std::vector<std::unique_ptr<RamValue>> values;

public:
    RamProject(std::unique_ptr<RamRelationReference> rel)
            : RamOperation(RN_Project), relation(std::move(rel)), filter(nullptr) {}

    RamProject(std::unique_ptr<RamRelationReference> rel, const RamRelationReference& filter)
            : RamOperation(RN_Project), relation(std::move(rel)),
              filter(std::make_unique<RamRelationReference>(filter)) {}

    /** Add value for a column */
    void addArg(std::unique_ptr<RamValue> v) {
        values.push_back(std::move(v));
    }

    /** Get relation */
    const RamRelationReference& getRelation() const {
        return *relation;
    }

    /** Check filter */
    bool hasFilter() const {
        return (bool)filter;
    }

    /** Get filter */
    const RamRelationReference& getFilter() const {
        assert(hasFilter());
        return *filter;
    }

    /** Get values */
    std::vector<RamValue*> getValues() const {
        return toPtrVector(values);
    }

    /** Print */
    void print(std::ostream& os, int tabpos) const override {
        const std::string tabs(tabpos, '\t');

        os << tabs << "PROJECT (" << join(values, ", ", print_deref<std::unique_ptr<RamValue>>()) << ") INTO "
           << relation->getName();
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        res.push_back(relation.get());
        for (const auto& cur : values) {
            res.push_back(cur.get());
        }
        return res;
    }

    /** Create clone */
    RamProject* clone() const override {
        RamProject* res = new RamProject(std::unique_ptr<RamRelationReference>(relation->clone()));
        if (filter != nullptr) {
            res->filter = std::make_unique<RamRelationReference>(*filter);
        }
        for (auto& cur : values) {
            res->values.push_back(std::unique_ptr<RamValue>(cur->clone()));
        }
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        relation = map(std::move(relation));
        for (auto& cur : values) {
            cur = map(std::move(cur));
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamProject*>(&node));
        const auto& other = static_cast<const RamProject&>(node);
        bool isFilterEqual = false;
        if (filter == nullptr && other.filter == nullptr) {
            isFilterEqual = true;
        } else if (filter != nullptr && other.filter != nullptr) {
            isFilterEqual = (*filter == *other.filter);
        }
        return getRelation() == other.getRelation() && equal_targets(values, other.values) && isFilterEqual;
    }
};

/** A statement for returning from a ram subroutine */
class RamReturn : public RamOperation {
protected:
    std::vector<std::unique_ptr<RamValue>> values;

public:
    RamReturn() : RamOperation(RN_Return) {}

    void print(std::ostream& os, int tabpos) const override {
        const std::string tabs(tabpos, '\t');

        // return
        os << tabs << "RETURN (";

        for (auto val : getValues()) {
            if (val == nullptr) {
                os << "_";
            } else {
                val->print(os);
            }

            if (val != *(getValues().end() - 1)) {
                os << ", ";
            }
        }

        os << ")";
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

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        for (const auto& cur : values) {
            res.push_back(cur.get());
        }
        return res;
    }

    /** Create clone */
    RamReturn* clone() const override {
        auto* res = new RamReturn();
        for (auto& cur : values) {
            res->values.push_back(std::unique_ptr<RamValue>(cur->clone()));
        }
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        for (auto& cur : values) {
            cur = map(std::move(cur));
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamReturn*>(&node));
        const auto& other = static_cast<const RamReturn&>(node);
        return equal_targets(values, other.values);
    }
};

}  // namespace souffle
