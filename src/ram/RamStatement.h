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

#include "ram/RamCondition.h"
#include "ram/RamNode.h"
#include "ram/RamOperation.h"
#include "ram/RamRelation.h"
#include "utility/ContainerUtil.h"
#include "utility/StreamUtil.h"
#include "utility/StringUtil.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 * @class RamStatement
 * @brief Abstract class for RAM statements
 */
class RamStatement : public RamNode {
public:
    RamStatement* clone() const override = 0;

protected:
    void print(std::ostream& os) const override {
        print(os, 0);
    }
    /** @brief Pretty print with indentation */
    virtual void print(std::ostream& os, int tabpos) const = 0;

    /** @brief Pretty print jump-bed */
    static void print(const RamStatement* statement, std::ostream& os, int tabpos) {
        assert(statement != nullptr && "statement is a null-pointer");
        statement->print(os, tabpos);
    }

    friend class RamProgram;
};

/**
 * @class RamRelationStatement
 * @brief RAM Statements with a single relation
 */
class RamRelationStatement : public RamStatement {
public:
    RamRelationStatement(std::unique_ptr<RamRelationReference> relRef) : relationRef(std::move(relRef)) {
        assert(relationRef != nullptr && "Relation reference is a null-pointer");
    }

    /** @brief Get RAM relation */
    const RamRelation& getRelation() const {
        return *relationRef->get();
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return {relationRef.get()};
    }

    void apply(const RamNodeMapper& map) override {
        relationRef = map(std::move(relationRef));
    }

protected:
    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamRelationStatement&>(node);
        return equal_ptr(relationRef, other.relationRef);
    }

protected:
    /** Relation reference */
    std::unique_ptr<RamRelationReference> relationRef;
};

/**
 * @class RamIO
 * @brief I/O statement for a relation
 *
 * I/O operation for a relation, e.g., input/output/printsize
 */
class RamIO : public RamRelationStatement {
public:
    RamIO(std::unique_ptr<RamRelationReference> relRef, std::map<std::string, std::string> directives)
            : RamRelationStatement(std::move(relRef)), directives(std::move(directives)) {}

    /** @brief get I/O directives */
    const std::map<std::string, std::string>& getDirectives() const {
        return directives;
    }

    /** @get value of I/O directive */
    const std::string get(const std::string& key) const {
        return directives.at(key);
    }

    RamIO* clone() const override {
        return new RamIO(souffle::clone(relationRef), directives);
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        const RamRelation& rel = getRelation();
        os << times(" ", tabpos);
        os << "IO " << rel.getName() << " (";
        os << join(directives, ",", [](std::ostream& out, const auto& arg) {
            out << arg.first << "=\"" << escape(arg.second) << "\"";
        });
        os << ")" << std::endl;
    };

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamIO&>(node);
        return RamRelationStatement::equal(other) && directives == other.directives;
    }

    /** IO directives */
    std::map<std::string, std::string> directives;
};

/**
 * @class RamClear
 * @brief Delete tuples of a relation
 *
 * This retains the target relation, but cleans its content
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * CLEAR A
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamClear : public RamRelationStatement {
public:
    RamClear(std::unique_ptr<RamRelationReference> relRef) : RamRelationStatement(std::move(relRef)) {}

    RamClear* clone() const override {
        return new RamClear(souffle::clone(relationRef));
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        const RamRelation& rel = getRelation();
        os << times(" ", tabpos);
        os << "CLEAR ";
        os << rel.getName();
        os << std::endl;
    }
};

/**
 * @class RamBinRelationStatement
 * @brief Abstract class for a binary relation
 *
 * Comprises two RamRelations
 */
class RamBinRelationStatement : public RamStatement {
public:
    RamBinRelationStatement(std::unique_ptr<RamRelationReference> f, std::unique_ptr<RamRelationReference> s)
            : first(std::move(f)), second(std::move(s)) {
        assert(first->get()->getArity() == second->get()->getArity() && "mismatching arities");

        assert(first != nullptr && "First relation is a null-pointer");
        assert(second != nullptr && "Second relation is a null-pointer");
        const auto& type1 = first->get()->getAttributeTypes();
        const auto& type2 = first->get()->getAttributeTypes();
        for (size_t i = 0; i < first->get()->getArity(); i++) {
            assert(type1[i] == type2[i] && "mismatching type");
        }
    }

    /** @brief Get first relation */
    const RamRelation& getFirstRelation() const {
        return *first->get();
    }

    /** @brief Get second relation */
    const RamRelation& getSecondRelation() const {
        return *second->get();
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return {first.get(), second.get()};
    }

    void apply(const RamNodeMapper& map) override {
        first = map(std::move(first));
        second = map(std::move(second));
    }

protected:
    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamBinRelationStatement&>(node);
        return equal_ptr(first, other.first) && equal_ptr(second, other.second);
    }

protected:
    /** first argument of binary statement */
    std::unique_ptr<RamRelationReference> first;

    /** second argument of binary statement */
    std::unique_ptr<RamRelationReference> second;
};

/**
 * @class RamExtend
 * @brief Extend equivalence relation.
 *
 * The following example merges A into B:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * EXTEND B WITH A
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamExtend : public RamBinRelationStatement {
public:
    RamExtend(std::unique_ptr<RamRelationReference> tRef, std::unique_ptr<RamRelationReference> sRef)
            : RamBinRelationStatement(std::move(sRef), std::move(tRef)) {}

    /** @brief Get source relation */
    const RamRelation& getSourceRelation() const {
        return getFirstRelation();
    }

    /** @brief Get target relation */
    const RamRelation& getTargetRelation() const {
        return getSecondRelation();
    }

    RamExtend* clone() const override {
        auto* res = new RamExtend(souffle::clone(second), souffle::clone(first));
        return res;
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "EXTEND " << getTargetRelation().getName() << " WITH " << getSourceRelation().getName();
        os << std::endl;
    }
};

/**
 * @class RamSwap
 * @brief Swap operation with respect to two relations
 *
 * Swaps the contents of the two relations
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * SWAP(A, B)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamSwap : public RamBinRelationStatement {
public:
    RamSwap(std::unique_ptr<RamRelationReference> f, std::unique_ptr<RamRelationReference> s)
            : RamBinRelationStatement(std::move(f), std::move(s)) {}

    RamSwap* clone() const override {
        return new RamSwap(souffle::clone(first), souffle::clone(second));
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "SWAP (" << getFirstRelation().getName() << ", " << getSecondRelation().getName() << ")";
        os << std::endl;
    }
};

/**
 * @class RamQuery
 * @brief A relational algebra query
 *
 * Corresponds to the core machinery of semi-naive evaluation
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * QUERY
 *   FOR t0 in A
 *     FOR t1 in B
 *       ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamQuery : public RamStatement {
public:
    RamQuery(std::unique_ptr<RamOperation> o) : operation(std::move(o)) {
        assert(operation && "operation is a nullptr");
    }

    /** @brief Get RAM operation */
    const RamOperation& getOperation() const {
        return *operation;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return {operation.get()};
    }

    RamQuery* clone() const override {
        return new RamQuery(souffle::clone(operation));
    }

    void apply(const RamNodeMapper& map) override {
        operation = map(std::move(operation));
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "QUERY" << std::endl;
        operation->print(os, tabpos + 1);
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamQuery&>(node);
        return equal_ptr(operation, other.operation);
    }

    /** RAM operation */
    std::unique_ptr<RamOperation> operation;
};

/**
 * @class RamListStatement
 * @brief Abstract class for a list of RAM statements
 */
class RamListStatement : public RamStatement {
public:
    RamListStatement() = default;
    RamListStatement(std::vector<std::unique_ptr<RamStatement>> statements)
            : statements(std::move(statements)) {}

    template <typename... Stmts>
    RamListStatement(std::unique_ptr<Stmts>&&... stmts) {
        std::unique_ptr<RamStatement> tmp[] = {std::move(stmts)...};
        for (auto& cur : tmp) {
            assert(cur.get() != nullptr && "statement is a null-pointer");
            statements.emplace_back(std::move(cur));
        }
    }

    /** @brief Get statements */
    std::vector<RamStatement*> getStatements() const {
        return toPtrVector(statements);
    }

    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        for (const auto& cur : statements) {
            res.push_back(cur.get());
        }
        return res;
    }

    void apply(const RamNodeMapper& map) override {
        for (auto& stmt : statements) {
            stmt = map(std::move(stmt));
        }
    }

protected:
    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamListStatement&>(node);
        return equal_targets(statements, other.statements);
    }

protected:
    /** ordered list of RAM statements */
    std::vector<std::unique_ptr<RamStatement>> statements;
};

/**
 * @class RamSequence
 * @brief Sequence of RAM statements
 *
 * Execute statement one by one from an ordered list of statements.
 */
class RamSequence : public RamListStatement {
public:
    RamSequence(std::vector<std::unique_ptr<RamStatement>> statements)
            : RamListStatement(std::move(statements)) {}
    RamSequence() : RamListStatement() {}
    template <typename... Stmts>
    RamSequence(std::unique_ptr<RamStatement> first, std::unique_ptr<Stmts>... rest)
            : RamListStatement(std::move(first), std::move(rest)...) {}

    RamSequence* clone() const override {
        auto* res = new RamSequence();
        for (auto& cur : statements) {
            res->statements.push_back(souffle::clone(cur));
        }
        return res;
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        for (const auto& stmt : statements) {
            RamStatement::print(stmt.get(), os, tabpos);
        }
    }
};

/**
 * @class RamParallel
 * @brief Parallel block of statements
 *
 * Execute statements in parallel and wait until all statements have
 * completed their execution before completing the execution of the
 * parallel block.
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * PARALLEL
 *   BEGIN DEBUG...
 *     QUERY
 *       ...
 * END PARALLEL
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamParallel : public RamListStatement {
public:
    RamParallel(std::vector<std::unique_ptr<RamStatement>> statements)
            : RamListStatement(std::move(statements)) {}
    RamParallel() : RamListStatement() {}
    template <typename... Stmts>
    RamParallel(std::unique_ptr<RamStatement> first, std::unique_ptr<Stmts>... rest)
            : RamListStatement(std::move(first), std::move(rest)...) {}

    RamParallel* clone() const override {
        auto* res = new RamParallel();
        for (auto& cur : statements) {
            res->statements.push_back(souffle::clone(cur));
        }
        return res;
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "PARALLEL" << std::endl;
        for (auto const& stmt : statements) {
            RamStatement::print(stmt.get(), os, tabpos + 1);
        }
        os << times(" ", tabpos) << "END PARALLEL" << std::endl;
    }
};

/**
 * @class RamLoop
 * @brief Execute statement until statement terminates loop via an exit statement
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * LOOP
 *   PARALLEL
 *     ...
 *   END PARALLEL
 * END LOOP
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamLoop : public RamStatement {
public:
    RamLoop(std::unique_ptr<RamStatement> b) : body(std::move(b)) {
        assert(body != nullptr && "Loop body is a null-pointer");
    }

    /** @brief Get loop body */
    const RamStatement& getBody() const {
        return *body;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return {body.get()};
    }

    RamLoop* clone() const override {
        return new RamLoop(souffle::clone(body));
    }

    void apply(const RamNodeMapper& map) override {
        body = map(std::move(body));
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "LOOP" << std::endl;
        RamStatement::print(body.get(), os, tabpos + 1);
        os << times(" ", tabpos) << "END LOOP" << std::endl;
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamLoop&>(node);
        return equal_ptr(body, other.body);
    }

    /** loop body */
    std::unique_ptr<RamStatement> body;
};

/**
 * @class RamExit
 * @brief Exit statement for a loop
 *
 * Exits a loop if exit condition holds.
 *
 * The following example will exit the loop given
 * that A is the empty set:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * EXIT (A = ∅)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamExit : public RamStatement {
public:
    RamExit(std::unique_ptr<RamCondition> c) : condition(std::move(c)) {
        assert(condition && "condition is a nullptr");
    }

    /** @brief Get exit condition */
    const RamCondition& getCondition() const {
        return *condition;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return {condition.get()};
    }

    RamExit* clone() const override {
        return new RamExit(souffle::clone(condition));
    }

    void apply(const RamNodeMapper& map) override {
        condition = map(std::move(condition));
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "EXIT " << getCondition() << std::endl;
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamExit&>(node);
        return equal_ptr(condition, other.condition);
    }

    /** exit condition */
    std::unique_ptr<RamCondition> condition;
};

/**
 * @class RamAbstractLog
 * @brief Abstract class for logging
 *
 * Comprises a RamStatement and the message (string) to be logged
 */
class RamAbstractLog {
public:
    RamAbstractLog(std::unique_ptr<RamStatement> stmt, std::string msg)
            : statement(std::move(stmt)), message(std::move(msg)) {
        assert(statement && "log statement is a nullptr");
    }

    std::vector<const RamNode*> getChildNodes() const {
        return {statement.get()};
    }

    /** @brief Get logging message */
    const std::string& getMessage() const {
        return message;
    }

    /** @brief Get logging statement */
    const RamStatement& getStatement() const {
        return *statement;
    }

    void apply(const RamNodeMapper& map) {
        statement = map(std::move(statement));
    }

protected:
    bool equal(const RamNode& node) const {
        const auto& other = dynamic_cast<const RamAbstractLog&>(node);
        return equal_ptr(statement, other.statement) && message == other.message;
    }

protected:
    /** logging statement */
    std::unique_ptr<RamStatement> statement;

    /** logging message */
    std::string message;
};

/**
 * @class RamLogRelationTimer
 * @brief Execution time logger for a statement
 *
 * Logs the execution time of a statement. Before and after
 * the execution of the logging statement the wall-clock time
 * is taken to compute the time duration for the statement.
 * Duration and logging message is printed after the execution
 * of the statement.
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * START_TIMER ON A "file.dl [8:1-8:8]\;"
 *   ...
 * END_TIMER
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamLogRelationTimer : public RamRelationStatement, public RamAbstractLog {
public:
    RamLogRelationTimer(
            std::unique_ptr<RamStatement> stmt, std::string msg, std::unique_ptr<RamRelationReference> relRef)
            : RamRelationStatement(std::move(relRef)), RamAbstractLog(std::move(stmt), std::move(msg)) {}

    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res = RamRelationStatement::getChildNodes();
        res.push_back(RamAbstractLog::getChildNodes().at(0));
        return res;
    }

    RamLogRelationTimer* clone() const override {
        return new RamLogRelationTimer(souffle::clone(statement), message, souffle::clone(relationRef));
    }

    void apply(const RamNodeMapper& map) override {
        RamRelationStatement::apply(map);
        RamAbstractLog::apply(map);
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "START_TIMER ON " << getRelation().getName() << " \""
           << stringify(message) << "\"" << std::endl;
        RamStatement::print(statement.get(), os, tabpos + 1);
        os << times(" ", tabpos) << "END_TIMER" << std::endl;
    }
};

/**
 * @class RamLogTimer
 * @brief Execution time logger for a statement
 *
 * Logs the execution time of a statement. Before and after
 * the execution of the logging statement the wall-clock time
 * is taken to compute the time duration for the statement.
 * Duration and logging message is printed after the execution
 * of the statement.
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * START_TIMER "@runtime\;"
 *   BEGIN_STRATUM 0
 *     ...
 *   ...
 * END_TIMER
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamLogTimer : public RamStatement, public RamAbstractLog {
public:
    RamLogTimer(std::unique_ptr<RamStatement> stmt, std::string msg)
            : RamAbstractLog(std::move(stmt), std::move(msg)) {}

    std::vector<const RamNode*> getChildNodes() const override {
        return RamAbstractLog::getChildNodes();
    }

    RamLogTimer* clone() const override {
        return new RamLogTimer(souffle::clone(statement), message);
    }

    void apply(const RamNodeMapper& map) override {
        RamAbstractLog::apply(map);
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "START_TIMER \"" << stringify(message) << "\"" << std::endl;
        RamStatement::print(statement.get(), os, tabpos + 1);
        os << times(" ", tabpos) << "END_TIMER" << std::endl;
    }
};

/**
 * @class RamDebugInfo
 * @brief Debug statement
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * BEGIN_DEBUG "gen(1) \nin file /file.dl [7:7-7:10]\;"
 *   ...
 * END_DEBUG
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamDebugInfo : public RamStatement, public RamAbstractLog {
public:
    RamDebugInfo(std::unique_ptr<RamStatement> stmt, std::string msg)
            : RamAbstractLog(std::move(stmt), std::move(msg)) {}

    std::vector<const RamNode*> getChildNodes() const override {
        return RamAbstractLog::getChildNodes();
    }

    RamDebugInfo* clone() const override {
        return new RamDebugInfo(souffle::clone(statement), message);
    }

    void apply(const RamNodeMapper& map) override {
        RamAbstractLog::apply(map);
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "BEGIN_DEBUG \"" << stringify(message) << "\"" << std::endl;
        RamStatement::print(statement.get(), os, tabpos + 1);
        os << times(" ", tabpos) << "END_DEBUG" << std::endl;
    }
};

/**
 * @class RamLogSize
 * @brief Log relation size and a logging message.
 */
class RamLogSize : public RamRelationStatement {
public:
    RamLogSize(std::unique_ptr<RamRelationReference> relRef, std::string message)
            : RamRelationStatement(std::move(relRef)), message(std::move(message)) {}

    /** @brief Get logging message */
    const std::string& getMessage() const {
        return message;
    }

    RamLogSize* clone() const override {
        return new RamLogSize(souffle::clone(relationRef), message);
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "LOGSIZE " << getRelation().getName();
        os << " TEXT "
           << "\"" << stringify(message) << "\"";
        os << std::endl;
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamLogSize&>(node);
        return RamRelationStatement::equal(other) && message == other.message;
    }

    /** logging message */
    std::string message;
};

/**
 * @class RamCall
 * @brief Call a subroutine
 *
 * Calls a subroutine
 *
 * The following example shows how subroutine A is invoked
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * CALL A
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

class RamCall : public RamStatement {
public:
    RamCall(std::string name) : name(std::move(name)) {}

    /** @brief Get call name */
    const std::string& getName() const {
        return name;
    }

    RamCall* clone() const override {
        return new RamCall(name);
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "CALL " << name << std::endl;
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamCall&>(node);
        return name == other.name;
    }

    /** name of subroutine */
    std::string name;
};

}  // end of namespace souffle
