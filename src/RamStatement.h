/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamStatement.h
 *
 * Defines abstract class Statement and sub-classes for implementing the
 * Relational Algebra Machine (RAM), which is an abstract machine.
 *
 ***********************************************************************/

#pragma once

#include "RamExpression.h"
#include "RamNode.h"
#include "RamOperation.h"
#include "RamRelation.h"
#include "Util.h"

#include <algorithm>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 * Abstract class for RAM statements
 */
class RamStatement : public RamNode {
public:
    /** Pretty print with indentation */
    virtual void print(std::ostream& os, int tabpos) const = 0;

    void print(std::ostream& os) const override {
        print(os, 0);
    }

    RamStatement* clone() const override = 0;
};

/**
 * RAM Statements with a single relation
 */
class RamRelationStatement : public RamStatement {
public:
    RamRelationStatement(std::unique_ptr<RamRelationReference> relRef) : relationRef(std::move(relRef)) {}

    /** Get RAM relation */
    const RamRelation& getRelation() const {
        assert(relationRef != nullptr && "Relation reference is a null-pointer");
        return *relationRef->get();
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return {relationRef.get()};
    }

    void apply(const RamNodeMapper& map) override {
        relationRef = map(std::move(relationRef));
    }

protected:
    /** Relation */
    std::unique_ptr<RamRelationReference> relationRef;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamRelationStatement*>(&node));
        const auto& other = static_cast<const RamRelationStatement&>(node);
        return getRelation() == other.getRelation();
    }
};

/**
 * Create new RAM relation
 */
class RamCreate : public RamRelationStatement {
public:
    RamCreate(std::unique_ptr<RamRelationReference> relRef) : RamRelationStatement(std::move(relRef)) {}

    void print(std::ostream& os, int tabpos) const override {
        const RamRelation& rel = getRelation();
        os << times(" ", tabpos);
        os << "CREATE " << rel.getName() << " " << rel.getRepresentation() << std::endl;
    };

    RamCreate* clone() const override {
        return new RamCreate(std::unique_ptr<RamRelationReference>(relationRef->clone()));
    }
};

/**
 * Load data into a relation
 *
 * TODO (b-scholz): introduce an abstract class for load/store
 */
class RamLoad : public RamRelationStatement {
public:
    RamLoad(std::unique_ptr<RamRelationReference> relRef, std::vector<IODirectives> ioDirectives)
            : RamRelationStatement(std::move(relRef)), ioDirectives(std::move(ioDirectives)) {}

    /** Get load directives */
    const std::vector<IODirectives>& getIODirectives() const {
        return ioDirectives;
    }

    void print(std::ostream& os, int tabpos) const override {
        const RamRelation& rel = getRelation();
        os << times(" ", tabpos);
        os << "LOAD DATA FOR " << rel.getName() << " FROM {";
        os << join(ioDirectives, "], [",
                [](std::ostream& out, const IODirectives& directives) { out << directives; });
        os << ioDirectives << "}";
        os << std::endl;
    };

    RamLoad* clone() const override {
        return new RamLoad(std::unique_ptr<RamRelationReference>(relationRef->clone()), ioDirectives);
    }

protected:
    /** load directives of a relation */
    const std::vector<IODirectives> ioDirectives;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamLoad*>(&node));
        const auto& other = static_cast<const RamLoad&>(node);
        return RamRelationStatement::equal(other) && getIODirectives() == other.getIODirectives();
    }
};

/**
 * Store data of a relation
 */
class RamStore : public RamRelationStatement {
public:
    RamStore(std::unique_ptr<RamRelationReference> relRef, std::vector<IODirectives> ioDirectives)
            : RamRelationStatement(std::move(relRef)), ioDirectives(std::move(ioDirectives)) {}

    /** Get store directives */
    const std::vector<IODirectives>& getIODirectives() const {
        return ioDirectives;
    }

    void print(std::ostream& os, int tabpos) const override {
        const RamRelation& rel = getRelation();
        os << times(" ", tabpos);
        os << "STORE DATA FOR " << rel.getName() << " TO {";
        os << join(ioDirectives, "], [",
                [](std::ostream& out, const IODirectives& directives) { out << directives; });
        os << "}";
        os << std::endl;
    };

    RamStore* clone() const override {
        return new RamStore(std::unique_ptr<RamRelationReference>(relationRef->clone()), ioDirectives);
    }

protected:
    /** store directives of a relation */
    const std::vector<IODirectives> ioDirectives;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamStore*>(&node));
        const auto& other = static_cast<const RamStore&>(node);
        return RamRelationStatement::equal(other) && getIODirectives() == other.getIODirectives();
    }
};

/**
 * Delete tuples of a relation
 */
class RamClear : public RamRelationStatement {
public:
    RamClear(std::unique_ptr<RamRelationReference> relRef) : RamRelationStatement(std::move(relRef)) {}

    void print(std::ostream& os, int tabpos) const override {
        const RamRelation& rel = getRelation();
        os << times(" ", tabpos);
        os << "CLEAR ";
        os << rel.getName();
        os << std::endl;
    }

    RamClear* clone() const override {
        return new RamClear(std::unique_ptr<RamRelationReference>(relationRef->clone()));
    }
};

/**
 * Drop relation, i.e., delete it from memory
 */
class RamDrop : public RamRelationStatement {
public:
    RamDrop(std::unique_ptr<RamRelationReference> relRef) : RamRelationStatement(std::move(relRef)) {}

    void print(std::ostream& os, int tabpos) const override {
        const RamRelation& rel = getRelation();
        os << times(" ", tabpos);
        os << "DROP " << rel.getName();
        os << std::endl;
    }

    RamDrop* clone() const override {
        return new RamDrop(std::unique_ptr<RamRelationReference>(relationRef->clone()));
    }
};

/**
 * Merge tuples from a source into target relation.
 * Note that semantically uniqueness of tuples is not checked.
 *
 * TODO (b-scholz): introduce an abstract class for Merge/Swap
 */
class RamMerge : public RamStatement {
public:
    RamMerge(std::unique_ptr<RamRelationReference> tRef, std::unique_ptr<RamRelationReference> sRef)
            : targetRef(std::move(tRef)), sourceRef(std::move(sRef)) {
        const RamRelation* source = sourceRef->get();
        const RamRelation* target = targetRef->get();
        assert(source->getArity() == target->getArity() && "mismatching relations");
        for (size_t i = 0; i < source->getArity(); i++) {
            assert(source->getArgTypeQualifier(i) == target->getArgTypeQualifier(i) && "mismatching type");
        }
    }

    /** Get source relation */
    const RamRelation& getSourceRelation() const {
        assert(sourceRef != nullptr && "Source relation is a null-pointer");
        return *sourceRef->get();
    }

    /** Get target relation */
    const RamRelation& getTargetRelation() const {
        assert(targetRef != nullptr && "Target relation is a null-pointer");
        return *targetRef->get();
    }

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "MERGE " << getTargetRelation().getName() << " WITH " << getSourceRelation().getName();
        os << std::endl;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return {sourceRef.get(), targetRef.get()};
    }

    RamMerge* clone() const override {
        auto* res = new RamMerge(std::unique_ptr<RamRelationReference>(targetRef->clone()),
                std::unique_ptr<RamRelationReference>(sourceRef->clone()));
        return res;
    }

    void apply(const RamNodeMapper& map) override {
        sourceRef = map(std::move(sourceRef));
        targetRef = map(std::move(targetRef));
    }

protected:
    /** source relation reference of merge statement */
    std::unique_ptr<RamRelationReference> targetRef;

    /** target relation reference of merge statement */
    std::unique_ptr<RamRelationReference> sourceRef;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamMerge*>(&node));
        const auto& other = static_cast<const RamMerge&>(node);
        return getTargetRelation() == other.getTargetRelation() &&
               getSourceRelation() == other.getSourceRelation();
    }
};

/**
 * Swap operation two relations
 */
class RamSwap : public RamStatement {
public:
    RamSwap(std::unique_ptr<RamRelationReference> f, std::unique_ptr<RamRelationReference> s)
            : first(std::move(f)), second(std::move(s)) {
        // check for type equivalence
        assert(first->get()->getArity() == second->get()->getArity() && "mismatching relations");
        for (size_t i = 0; i < first->get()->getArity(); i++) {
            assert(first->get()->getArgTypeQualifier(i) == second->get()->getArgTypeQualifier(i) &&
                    "mismatching type");
        }
    }

    /** Get first relation */
    const RamRelation& getFirstRelation() const {
        assert(first != nullptr && "Relation is a null-pointer");
        return *first->get();
    }

    /** Get second relation */
    const RamRelation& getSecondRelation() const {
        assert(second != nullptr && "Relation is a null-pointer");
        return *second->get();
    }

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "SWAP (" << getFirstRelation().getName() << ", " << getSecondRelation().getName() << ")";
        os << std::endl;
    };

    std::vector<const RamNode*> getChildNodes() const override {
        return {first.get(), second.get()};
    }

    RamSwap* clone() const override {
        return new RamSwap(std::unique_ptr<RamRelationReference>(first->clone()),
                std::unique_ptr<RamRelationReference>(second->clone()));
    }

    void apply(const RamNodeMapper& map) override {
        first = map(std::move(first));
        second = map(std::move(second));
    }

protected:
    /** first argument of swap statement */
    std::unique_ptr<RamRelationReference> first;

    /** second argument of swap statement */
    std::unique_ptr<RamRelationReference> second;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamSwap*>(&node));
        const auto& other = static_cast<const RamSwap&>(node);
        return getFirstRelation() == other.getFirstRelation() &&
               getSecondRelation() == other.getSecondRelation();
    }
};

/**
 * Insert a fact into a relation
 */
class RamFact : public RamRelationStatement {
public:
    RamFact(std::unique_ptr<RamRelationReference> relRef, std::vector<std::unique_ptr<RamExpression>>&& v)
            : RamRelationStatement(std::move(relRef)), values(std::move(v)) {}

    /** Get arguments of fact */
    std::vector<RamExpression*> getValues() const {
        return toPtrVector(values);
    }

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "INSERT (" << join(values, ",", print_deref<std::unique_ptr<RamExpression>>()) << ") INTO "
           << getRelation().getName();
        os << std::endl;
    };

    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res = RamRelationStatement::getChildNodes();
        for (const auto& cur : values) {
            res.push_back(cur.get());
        }
        return res;
    }

    RamFact* clone() const override {
        auto* res = new RamFact(std::unique_ptr<RamRelationReference>(relationRef->clone()), {});
        for (auto& cur : values) {
            res->values.emplace_back(cur->clone());
        }
        return res;
    }

    void apply(const RamNodeMapper& map) override {
        RamRelationStatement::apply(map);
        for (auto& val : values) {
            val = map(std::move(val));
        }
    }

protected:
    /** Arguments of fact */
    std::vector<std::unique_ptr<RamExpression>> values;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamFact*>(&node));
        const auto& other = static_cast<const RamFact&>(node);
        return RamRelationStatement::equal(other) && equal_targets(values, other.values);
    }
};

/**
 * A relational algebra query
 */
class RamQuery : public RamStatement {
public:
    RamQuery(std::unique_ptr<RamOperation> o) : operation(std::move(o)) {}

    /** Get RAM operation */
    const RamOperation& getOperation() const {
        assert(operation);
        return *operation;
    }

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "QUERY" << std::endl;
        operation->print(os, tabpos + 1);
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return {operation.get()};
    }

    RamQuery* clone() const override {
        return new RamQuery(std::unique_ptr<RamOperation>(operation->clone()));
    }

    void apply(const RamNodeMapper& map) override {
        operation = map(std::move(operation));
    }

protected:
    /** RAM operation */
    std::unique_ptr<RamOperation> operation;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamQuery*>(&node));
        const auto& other = static_cast<const RamQuery&>(node);
        return getOperation() == other.getOperation();
    }
};

/**
 * Sequence of RAM statements
 *
 * Execute statement one by one from an ordered list of statements.
 *
 * TODO (b-scholz): introduce an abstract class for sequence/loop
 *
 */
class RamSequence : public RamStatement {
public:
    RamSequence() : RamStatement() {}

    template <typename... Stmts>
    RamSequence(std::unique_ptr<Stmts>&&... stmts) : RamStatement() {
        // move all the given statements into the vector (not so simple)
        std::unique_ptr<RamStatement> tmp[] = {std::move(stmts)...};
        for (auto& cur : tmp) {
            statements.emplace_back(std::move(cur));
        }
        for (const auto& cur : statements) {
            (void)cur;
            assert(cur);
        }
    }

    /** Add new statement to the end of ordered list */
    void add(std::unique_ptr<RamStatement> stmt) {
        if (stmt) {
            statements.push_back(std::move(stmt));
        }
    }

    /** Get RAM statements from ordered list */
    std::vector<RamStatement*> getStatements() const {
        return toPtrVector(statements);
    }

    void print(std::ostream& os, int tabpos) const override {
        for (const auto& stmt : statements) {
            stmt->print(os, tabpos);
        }
    }

    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        for (const auto& cur : statements) {
            res.push_back(cur.get());
        }
        return res;
    }

    RamSequence* clone() const override {
        auto* res = new RamSequence();
        for (auto& cur : statements) {
            res->add(std::unique_ptr<RamStatement>(cur->clone()));
        }
        return res;
    }

    void apply(const RamNodeMapper& map) override {
        for (auto& stmt : statements) {
            stmt = map(std::move(stmt));
        }
    }

protected:
    /** ordered list of RAM statements */
    std::vector<std::unique_ptr<RamStatement>> statements;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamSequence*>(&node));
        const auto& other = static_cast<const RamSequence&>(node);

        return equal_targets(statements, other.statements);
    }
};

/**
 * Parallel block
 *
 * Execute statements in parallel and wait until all statements have
 * completed their execution before completing the execution of the
 * parallel block.
 */
class RamParallel : public RamStatement {
public:
    RamParallel() : RamStatement() {}

    /** Add new statement to parallel block */
    void add(std::unique_ptr<RamStatement> stmt) {
        if (stmt) {
            statements.push_back(std::move(stmt));
        }
    }

    /** Get statements of parallel block */
    std::vector<RamStatement*> getStatements() const {
        return toPtrVector(statements);
    }

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "PARALLEL" << std::endl;
        for (auto const& stmt : statements) {
            stmt->print(os, tabpos + 1);
        }
        os << times(" ", tabpos) << "END PARALLEL" << std::endl;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        for (const auto& cur : statements) {
            res.push_back(cur.get());
        }
        return res;
    }

    RamParallel* clone() const override {
        auto* res = new RamParallel();
        for (auto& cur : statements) {
            res->add(std::unique_ptr<RamStatement>(cur->clone()));
        }
        return res;
    }

    void apply(const RamNodeMapper& map) override {
        for (auto& stmt : statements) {
            stmt = map(std::move(stmt));
        }
    }

protected:
    /** list of statements executed in parallel */
    std::vector<std::unique_ptr<RamStatement>> statements;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamParallel*>(&node));
        const auto& other = static_cast<const RamParallel&>(node);
        return equal_targets(statements, other.statements);
    }
};

/**
 * Statement loop
 *
 * Execute the statement repeatedly until statement terminates loop via an exit statement
 */
class RamLoop : public RamStatement {
public:
    RamLoop(std::unique_ptr<RamStatement> b) : body(std::move(b)) {}

    template <typename... Stmts>
    RamLoop(std::unique_ptr<RamStatement> f, std::unique_ptr<RamStatement> s, std::unique_ptr<Stmts>... rest)
            : body(std::make_unique<RamSequence>(std::move(f), std::move(s), std::move(rest)...)) {}

    /** Get loop body */
    const RamStatement& getBody() const {
        assert(body != nullptr && "Loop body is a null-pointer");
        return *body;
    }

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "LOOP" << std::endl;
        body->print(os, tabpos + 1);
        os << times(" ", tabpos) << "END LOOP" << std::endl;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return {body.get()};
    }

    RamLoop* clone() const override {
        return new RamLoop(std::unique_ptr<RamStatement>(body->clone()));
    }

    void apply(const RamNodeMapper& map) override {
        body = map(std::move(body));
    }

protected:
    /** Body of loop */
    std::unique_ptr<RamStatement> body;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamLoop*>(&node));
        const auto& other = static_cast<const RamLoop&>(node);
        return getBody() == other.getBody();
    }
};

/**
 * Exit statement for a loop
 *
 * Exits a loop if exit condition holds.
 */
class RamExit : public RamStatement {
public:
    RamExit(std::unique_ptr<RamCondition> c) : condition(std::move(c)) {}

    /** Get exit condition */
    const RamCondition& getCondition() const {
        assert(condition);
        return *condition;
    }

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "EXIT " << getCondition() << std::endl;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return {condition.get()};
    }

    RamExit* clone() const override {
        return new RamExit(std::unique_ptr<RamCondition>(condition->clone()));
    }

    void apply(const RamNodeMapper& map) override {
        condition = map(std::move(condition));
    }

protected:
    /** exit condition */
    std::unique_ptr<RamCondition> condition;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamExit*>(&node));
        const auto& other = static_cast<const RamExit&>(node);
        return getCondition() == other.getCondition();
    }
};

/**
 * Execution time logger for a statement
 *
 * Logs the execution time of a statement. Before and after
 * the execution of the logging statement the wall-clock time
 * is taken to compute the time duration for the statement.
 * Duration and logging message is printed after the execution
 * of the statement.
 */
class RamLogRelationTimer : public RamRelationStatement {
public:
    RamLogRelationTimer(
            std::unique_ptr<RamStatement> stmt, std::string msg, std::unique_ptr<RamRelationReference> relRef)
            : RamRelationStatement(std::move(relRef)), statement(std::move(stmt)), message(std::move(msg)) {}

    /** get logging message */
    const std::string& getMessage() const {
        return message;
    }

    /** get logging statement */
    const RamStatement& getStatement() const {
        assert(statement);
        return *statement;
    }

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "START_TIMER ON " << getRelation().getName() << " \""
           << stringify(message) << "\"" << std::endl;
        statement->print(os, tabpos + 1);
        os << times(" ", tabpos) << "END_TIMER" << std::endl;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res = RamRelationStatement::getChildNodes();
        res.push_back(statement.get());
        return res;
    }

    RamLogRelationTimer* clone() const override {
        return new RamLogRelationTimer(std::unique_ptr<RamStatement>(statement->clone()), message,
                std::unique_ptr<RamRelationReference>(relationRef->clone()));
    }

    void apply(const RamNodeMapper& map) override {
        RamRelationStatement::apply(map);
        statement = map(std::move(statement));
    }

protected:
    /** logging statement */
    std::unique_ptr<RamStatement> statement;

    /** logging message */
    std::string message;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamLogRelationTimer*>(&node));
        const auto& other = static_cast<const RamLogRelationTimer&>(node);
        return RamRelationStatement::equal(other) && getStatement() == other.getStatement() &&
               getMessage() == other.getMessage();
    }
};

/**
 * Execution time logger for a statement
 *
 * Logs the execution time of a statement. Before and after
 * the execution of the logging statement the wall-clock time
 * is taken to compute the time duration for the statement.
 * Duration and logging message is printed after the execution
 * of the statement.
 */
class RamLogTimer : public RamStatement {
public:
    RamLogTimer(std::unique_ptr<RamStatement> stmt, std::string msg)
            : statement(std::move(stmt)), message(std::move(msg)) {
        assert(statement);
    }

    /** get logging message */
    const std::string& getMessage() const {
        return message;
    }

    /** get logging statement */
    const RamStatement& getStatement() const {
        assert(statement);
        return *statement;
    }

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "START_TIMER \"" << stringify(message) << "\"" << std::endl;
        statement->print(os, tabpos + 1);
        os << times(" ", tabpos) << "END_TIMER" << std::endl;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return {statement.get()};
    }

    RamLogTimer* clone() const override {
        return new RamLogTimer(std::unique_ptr<RamStatement>(statement->clone()), message);
    }

    void apply(const RamNodeMapper& map) override {
        statement = map(std::move(statement));
    }

protected:
    /** logging statement */
    std::unique_ptr<RamStatement> statement;

    /** logging message */
    std::string message;

    /** Relation */
    std::unique_ptr<RamRelationReference> relationRef;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamLogTimer*>(&node));
        const auto& other = static_cast<const RamLogTimer&>(node);
        return getStatement() == other.getStatement() && getMessage() == other.getMessage();
    }
};

/**
 * Debug statement
 */
class RamDebugInfo : public RamStatement {
public:
    RamDebugInfo(std::unique_ptr<RamStatement> stmt, std::string msg)
            : statement(std::move(stmt)), message(std::move(msg)) {
        assert(statement);
    }

    /** Get debugging message */
    const std::string& getMessage() const {
        return message;
    }

    /** Get debugging statement */
    const RamStatement& getStatement() const {
        assert(statement);
        return *statement;
    }

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "BEGIN_DEBUG \"" << stringify(message) << "\"" << std::endl;
        statement->print(os, tabpos + 1);
        os << times(" ", tabpos) << "END_DEBUG" << std::endl;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return {statement.get()};
    }

    RamDebugInfo* clone() const override {
        return new RamDebugInfo(std::unique_ptr<RamStatement>(statement->clone()), message);
    }

    void apply(const RamNodeMapper& map) override {
        statement = map(std::move(statement));
    }

protected:
    /** debugging statement */
    std::unique_ptr<RamStatement> statement;

    /** debugging message */
    std::string message;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamLogTimer*>(&node));
        const auto& other = static_cast<const RamLogTimer&>(node);
        return getStatement() == other.getStatement() && getMessage() == other.getMessage();
    }
};

/**
 * Stratum statement
 *
 * Wrap strata of program
 */
class RamStratum : public RamStatement {
public:
    RamStratum(std::unique_ptr<RamStatement> b, const int i) : body(std::move(b)), index(i) {}

    /** Get stratum body */
    const RamStatement& getBody() const {
        assert(body != nullptr && "Body of stratum is a null-pointer");
        return *body;
    }

    /** Get stratum index */
    const int getIndex() const {
        return index;
    }

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "BEGIN_STRATUM " << index << std::endl;
        body->print(os, tabpos + 1);
        os << times(" ", tabpos);
        os << "END_STRATUM " << index << std::endl;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return {body.get()};
    }

    RamStratum* clone() const override {
        return new RamStratum(std::unique_ptr<RamStatement>(body->clone()), index);
    }

    void apply(const RamNodeMapper& map) override {
        body = map(std::move(body));
    }

protected:
    /** Body of stratum */
    std::unique_ptr<RamStatement> body;

    /** Stratum number */
    const int index;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamStratum*>(&node));
        const auto& other = static_cast<const RamStratum&>(node);
        return getBody() == other.getBody() && getIndex() == other.getIndex();
    }
};

/**
 *  Log relation size and a logging message.
 */
class RamLogSize : public RamRelationStatement {
public:
    RamLogSize(std::unique_ptr<RamRelationReference> relRef, std::string message)
            : RamRelationStatement(std::move(relRef)), message(std::move(message)) {}

    /** Get logging message */
    const std::string& getMessage() const {
        return message;
    }

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "LOGSIZE " << getRelation().getName();
        os << " TEXT "
           << "\"" << stringify(message) << "\"";
        os << std::endl;
    }

    RamLogSize* clone() const override {
        return new RamLogSize(std::unique_ptr<RamRelationReference>(relationRef->clone()), message);
    }

protected:
    /** logging message */
    std::string message;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamLogSize*>(&node));
        const auto& other = static_cast<const RamLogSize&>(node);
        return RamRelationStatement::equal(other) && getMessage() == other.getMessage();
    }
};

#ifdef USE_MPI

class RamRecv : public RamRelationStatement {
public:
    RamRecv(std::unique_ptr<RamRelationReference> r, const int s)
            : RamRelationStatement(std::move(r)), sourceStratum(s) {}

    const int getSourceStratum() const {
        return sourceStratum;
    }

    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "RECV DATA FOR " << getRelation().getName() << " FROM STRATUM {" << sourceStratum << "}";
    };

    RamRecv* clone() const override {
        RamRecv* res =
                new RamRecv(std::unique_ptr<RamRelationReference>(relationRef->clone()), sourceStratum);
        return res;
    }

protected:
    /** source stratum */
    const int sourceStratum;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamRecv*>(&node));
        const auto& other = static_cast<const RamRecv&>(node);
        RamRelationStatement::equal(other);
        return sourceStratum == other.sourceStratum;
    }
};

class RamSend : public RamRelationStatement {
public:
    RamSend(std::unique_ptr<RamRelationReference> r, const std::set<size_t> s)
            : RamRelationStatement(std::move(r)), destinationStrata(s) {}

    const std::set<size_t> getDestinationStrata() const {
        return destinationStrata;
    }

    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "SEND DATA FOR " << getRelation().getName() << " TO STRATUM {";
        auto it = destinationStrata.begin();
        os << *it;
        ++it;
        while (it != destinationStrata.end()) {
            os << ", " << *it;
            ++it;
        }
        os << "}";
    };

    RamSend* clone() const override {
        RamSend* res =
                new RamSend(std::unique_ptr<RamRelationReference>(relationRef->clone()), destinationStrata);
        return res;
    }

protected:
    /** destination stratum */
    const std::set<size_t> destinationStrata;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamSend*>(&node));
        const auto& other = static_cast<const RamSend&>(node);
        return destinationStrata == other.destinationStrata;
    }
};

class RamNotify : public RamStatement {
public:
    RamNotify() : RamStatement() {}

    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "NOTIFY";
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return {};
    }

    RamNotify* clone() const override {
        return new RamNotify();
    }

    void apply(const RamNodeMapper& map) override {}

protected:
    bool equal(const RamNode& node) const override {
        return true;
    }
};

class RamWait : public RamStatement {
public:
    RamWait(const size_t c) : count(c) {}

    /** Get count of termination signals required. */
    const int getCount() const {
        return count;
    }

    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "WAIT";
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return {};
    }

    RamWait* clone() const override {
        return new RamWait(count);
    }

    void apply(const RamNodeMapper& map) override {}

protected:
    /** counter */
    const size_t count;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamWait*>(&node));
        const auto& other = static_cast<const RamWait&>(node);
        return other.count == count;
    }
};

#endif

}  // end of namespace souffle
