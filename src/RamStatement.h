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

#include "RamNode.h"
#include "RamOperation.h"
#include "RamRelation.h"
#include "RamValue.h"
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
    RamStatement(RamNodeType type) : RamNode(type) {}

    /** Pretty print with indentation */
    virtual void print(std::ostream& os, int tabpos) const = 0;

    /** Print RAM statement */
    void print(std::ostream& os) const override {
        print(os, 0);
    }

    /** Create clone */
    RamStatement* clone() const override = 0;
};

/**
 * RAM Statements with a single relation
 */
class RamRelationStatement : public RamStatement {
protected:
    /** Relation */
    std::unique_ptr<RamRelationReference> relation;

public:
    RamRelationStatement(RamNodeType type, std::unique_ptr<RamRelationReference> r)
            : RamStatement(type), relation(std::move(r)) {}

    /** Get RAM relation */
    const RamRelationReference& getRelation() const {
        assert(relation);
        return *relation;
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>() = {relation.get()};  // no child nodes
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        relation = map(std::move(relation));
    }

protected:
    /** Check equality */
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
    RamCreate(std::unique_ptr<RamRelationReference> rel) : RamRelationStatement(RN_Create, std::move(rel)) {}

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        // TODO (#541): add type information for attributes
        os << std::string(tabpos, '\t');
        os << "CREATE " << getRelation().getName() << "(";
        os << getRelation().getArg(0);
        for (size_t i = 1; i < getRelation().getArity(); i++) {
            os << ",";
            os << getRelation().getArg(i);
        }
        os << ")";
        if (getRelation().isBTree()) os << " btree";
        if (getRelation().isBrie()) os << " brie";
        if (getRelation().isEqRel()) os << " eqrel";
    };

    /** Create clone */
    RamCreate* clone() const override {
        RamCreate* res = new RamCreate(std::unique_ptr<RamRelationReference>(relation->clone()));
        return res;
    }
};

/**
 * Load data into a relation
 */
class RamLoad : public RamRelationStatement {
public:
    RamLoad(std::unique_ptr<RamRelationReference> rel, std::vector<IODirectives> ioDirectives)
            : RamRelationStatement(RN_Load, std::move(rel)), ioDirectives(std::move(ioDirectives)) {}

    const std::vector<IODirectives>& getIODirectives() const {
        return ioDirectives;
    }

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "LOAD DATA FOR " << getRelation().getName() << " FROM {";
        os << join(ioDirectives, "], [",
                [](std::ostream& out, const IODirectives& directives) { out << directives; });
        os << ioDirectives << "}";
    };

    /** Create clone */
    RamLoad* clone() const override {
        RamLoad* res = new RamLoad(std::unique_ptr<RamRelationReference>(relation->clone()), ioDirectives);
        return res;
    }

private:
    const std::vector<IODirectives> ioDirectives;
};

/**
 * Store data of a relation
 */
class RamStore : public RamRelationStatement {
public:
    RamStore(std::unique_ptr<RamRelationReference> rel, std::vector<IODirectives> ioDirectives)
            : RamRelationStatement(RN_Store, std::move(rel)), ioDirectives(std::move(ioDirectives)) {}

    const std::vector<IODirectives>& getIODirectives() const {
        return ioDirectives;
    }

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "STORE DATA FOR " << getRelation().getName() << " TO {";
        os << join(ioDirectives, "], [",
                [](std::ostream& out, const IODirectives& directives) { out << directives; });
        os << "}";
    };

    /** Create clone */
    RamStore* clone() const override {
        RamStore* res = new RamStore(std::unique_ptr<RamRelationReference>(relation->clone()), ioDirectives);
        return res;
    }

private:
    const std::vector<IODirectives> ioDirectives;
};

/**
 * Delete tuples of a relation
 */
class RamClear : public RamRelationStatement {
public:
    RamClear(std::unique_ptr<RamRelationReference> rel) : RamRelationStatement(RN_Clear, std::move(rel)) {}

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "CLEAR ";
        os << getRelation().getName();
    }

    /** Create clone */
    RamClear* clone() const override {
        RamClear* res = new RamClear(std::unique_ptr<RamRelationReference>(relation->clone()));
        return res;
    }
};

/**
 * Drop relation, i.e., delete it from memory
 */
class RamDrop : public RamRelationStatement {
public:
    RamDrop(std::unique_ptr<RamRelationReference> rel) : RamRelationStatement(RN_Drop, std::move(rel)) {}

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "DROP " << getRelation().getName();
    }
    /** Create clone */
    RamDrop* clone() const override {
        RamDrop* res = new RamDrop(std::unique_ptr<RamRelationReference>(relation->clone()));
        return res;
    }
};

/**
 * Merge tuples from a source into target relation.
 * Note that semantically uniqueness of tuples is not checked.
 */
class RamMerge : public RamStatement {
protected:
    std::unique_ptr<RamRelationReference> target;
    std::unique_ptr<RamRelationReference> source;

public:
    RamMerge(std::unique_ptr<RamRelationReference> t, std::unique_ptr<RamRelationReference> s)
            : RamStatement(RN_Merge), target(std::move(t)), source(std::move(s)) {
        // TODO (#541): check not just for arity also for correct type!!
        // Introduce an equivalence type-check for two ram relations
        assert(source->getArity() == target->getArity());
    }

    /** Get source relation */
    const RamRelationReference& getSourceRelation() const {
        return *source;
    }

    /** Get target relation */
    const RamRelationReference& getTargetRelation() const {
        return *target;
    }

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "MERGE " << target->getName() << " WITH " << source->getName();
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>({source.get(), target.get()});
    }

    /** Create clone */
    RamMerge* clone() const override {
        RamMerge* res = new RamMerge(std::unique_ptr<RamRelationReference>(target->clone()),
                std::unique_ptr<RamRelationReference>(source->clone()));
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        source = map(std::move(source));
        target = map(std::move(target));
    }

protected:
    /** Check equality */
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
protected:
    /** first argument of swap statement */
    std::unique_ptr<RamRelationReference> first;

    /** second argument of swap statement */
    std::unique_ptr<RamRelationReference> second;

public:
    RamSwap(std::unique_ptr<RamRelationReference> f, std::unique_ptr<RamRelationReference> s)
            : RamStatement(RN_Swap), first(std::move(f)), second(std::move(s)) {
        // TODO (#541): check not just for arity also for correct type!!
        assert(first->getArity() == second->getArity());
    }

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "SWAP (" << first->getName() << ", " << second->getName() << ")";
    };

    /** Get first relation */
    const RamRelationReference& getFirstRelation() const {
        return *first;
    }

    /** Get second relation */
    const RamRelationReference& getSecondRelation() const {
        return *second;
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>({first.get(), second.get()});  // no child nodes
    }

    /** Create clone */
    RamSwap* clone() const override {
        RamSwap* res = new RamSwap(std::unique_ptr<RamRelationReference>(first->clone()),
                std::unique_ptr<RamRelationReference>(second->clone()));
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        first = map(std::move(first));
        second = map(std::move(second));
    }

protected:
    /** Check equality */
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
protected:
    /** Arguments of fact */
    // TODO (#541): Reoccuring type -> push to RamValue.h
    using value_list = std::vector<std::unique_ptr<RamValue>>;
    value_list values;

public:
    RamFact(std::unique_ptr<RamRelationReference> rel, value_list&& v)
            : RamRelationStatement(RN_Fact, std::move(rel)), values(std::move(v)) {}

    /** Get arguments of fact */
    std::vector<RamValue*> getValues() const {
        return toPtrVector(values);
    }

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "INSERT (" << join(values, ",", print_deref<std::unique_ptr<RamValue>>()) << ") INTO "
           << getRelation().getName();
    };

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res = RamRelationStatement::getChildNodes();
        for (const auto& cur : values) {
            res.push_back(cur.get());
        }
        return res;
    }

    /** Create clone */
    RamFact* clone() const override {
        RamFact* res = new RamFact(std::unique_ptr<RamRelationReference>(relation->clone()), {});
        for (auto& cur : values) {
            res->values.push_back(std::unique_ptr<RamValue>(cur->clone()));
        }
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        RamRelationStatement::apply(map);
        for (auto& val : values) {
            val = map(std::move(val));
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamFact*>(&node));
        const auto& other = static_cast<const RamFact&>(node);
        return RamRelationStatement::equal(other) && equal_targets(values, other.values);
    }
};

/**
 * A relational algebra query
 * TODO (#541): Rename RAM statement: it is used for projection and sub-routines.
 */
class RamInsert : public RamStatement {
protected:
    /** RAM operation */
    std::unique_ptr<RamOperation> operation;

    /** RAM condition */
    std::unique_ptr<RamCondition> condition;

public:
    RamInsert(std::unique_ptr<RamOperation> o, std::unique_ptr<RamCondition> c = nullptr)
            : RamStatement(RN_Insert), operation(std::move(o)), condition(std::move(c)) {}

    /** Get RAM operation */
    const RamOperation& getOperation() const {
        assert(operation);
        return *operation;
    }

    /** Get RAM condition */
    const RamCondition* getCondition() const {
        return condition.get();
    }

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "INSERT ";
        if (condition != nullptr) {
            os << "WHERE ";
            condition->print(os);
        }
        os << "\n";
        operation->print(os, tabpos + 1);
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>({operation.get(), condition.get()});
    }

    /** Create clone */
    RamInsert* clone() const override {
        RamInsert* res;
        if (condition != nullptr) {
            res = new RamInsert(std::unique_ptr<RamOperation>(operation->clone()));
        } else {
            res = new RamInsert(std::unique_ptr<RamOperation>(operation->clone()),
                    std::unique_ptr<RamCondition>(condition->clone()));
        }
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        operation = map(std::move(operation));
        if (condition != nullptr) {
            condition = map(std::move(condition));
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamInsert*>(&node));
        const auto& other = static_cast<const RamInsert&>(node);
        return getOperation() == other.getOperation() && getCondition() == other.getCondition();
    }
};

/**
 * Sequence of RAM statements
 *
 * Execute statement one by one from an ordered list of statements.
 */
class RamSequence : public RamStatement {
protected:
    /** ordered list of RAM statements */
    std::vector<std::unique_ptr<RamStatement>> statements;

public:
    RamSequence() : RamStatement(RN_Sequence) {}

    template <typename... Stmts>
    RamSequence(std::unique_ptr<Stmts>&&... stmts) : RamStatement(RN_Sequence) {
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

    /** TODO (#541): what's that for ?? */
    template <typename T>
    void moveSubprograms(std::vector<std::unique_ptr<T>>& destination) {
        movePtrVector(statements, destination);
    }

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << join(statements, ";\n", [&](std::ostream& os, const std::unique_ptr<RamStatement>& stmt) {
            stmt->print(os, tabpos);
        });
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        for (const auto& cur : statements) {
            res.push_back(cur.get());
        }
        return res;
    }

    /** Create clone */
    RamSequence* clone() const override {
        auto* res = new RamSequence();
        for (auto& cur : statements) {
            res->add(std::unique_ptr<RamStatement>(cur->clone()));
        }
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        for (auto& stmt : statements) {
            stmt = map(std::move(stmt));
        }
    }

protected:
    /** Check equality */
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
protected:
    /** list of statements executed in parallel */
    std::vector<std::unique_ptr<RamStatement>> statements;

public:
    RamParallel() : RamStatement(RN_Parallel) {}

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

    /* Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "PARALLEL\n";
        os << join(statements, ";\n", [&](std::ostream& os, const std::unique_ptr<RamStatement>& stmt) {
            stmt->print(os, tabpos + 1);
        });
        os << std::string(tabpos, '\t');
        os << "END PARALLEL";
    }

    /** Obtains a list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        for (const auto& cur : statements) {
            res.push_back(cur.get());
        }
        return res;
    }

    /** Create clone */
    RamParallel* clone() const override {
        auto* res = new RamParallel();
        for (auto& cur : statements) {
            res->add(std::unique_ptr<RamStatement>(cur->clone()));
        }
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        for (auto& stmt : statements) {
            stmt = map(std::move(stmt));
        }
    }

protected:
    /** Check equality */
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
protected:
    /** Body of loop */
    std::unique_ptr<RamStatement> body;

public:
    RamLoop(std::unique_ptr<RamStatement> b) : RamStatement(RN_Loop), body(std::move(b)) {}

    template <typename... Stmts>
    RamLoop(std::unique_ptr<RamStatement> f, std::unique_ptr<RamStatement> s, std::unique_ptr<Stmts>... rest)
            : RamStatement(RN_Loop),
              body(std::make_unique<RamSequence>(std::move(f), std::move(s), std::move(rest)...)) {}

    /** Get loop body */
    const RamStatement& getBody() const {
        return *body;
    }

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "LOOP\n";
        body->print(os, tabpos + 1);
        os << "\n";
        os << std::string(tabpos, '\t');
        os << "END LOOP";
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return toVector<const RamNode*>(body.get());
    }

    /** Create clone */
    RamLoop* clone() const override {
        RamLoop* res = new RamLoop(std::unique_ptr<RamStatement>(body->clone()));
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        body = map(std::move(body));
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamLoop*>(&node));
        const auto& other = static_cast<const RamLoop&>(node);
        return *body == *other.body;
    }
};

/**
 * Exit statement for a loop
 *
 * Exits a loop if exit condition holds.
 */
class RamExit : public RamStatement {
protected:
    /** exit condition */
    std::unique_ptr<RamCondition> condition;

public:
    RamExit(std::unique_ptr<RamCondition> c) : RamStatement(RN_Exit), condition(std::move(c)) {}

    /** Get exit condition */
    const RamCondition& getCondition() const {
        assert(condition);
        return *condition;
    }

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "EXIT ";
        condition->print(os);
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return toVector<const RamNode*>(condition.get());
    }

    /** Create clone */
    RamExit* clone() const override {
        RamExit* res = new RamExit(std::unique_ptr<RamCondition>(condition->clone()));
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        condition = map(std::move(condition));
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamExit*>(&node));
        const auto& other = static_cast<const RamExit&>(node);
        return *condition == *other.condition;
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
protected:
    /** logging statement */
    std::unique_ptr<RamStatement> statement;

    /** logging message */
    std::string message;

    /** Relation */
    std::unique_ptr<RamRelationReference> relation;

public:
    RamLogTimer(std::unique_ptr<RamStatement> stmt, std::string msg,
            std::unique_ptr<RamRelationReference> relation)
            : RamStatement(RN_LogTimer), statement(std::move(stmt)), message(std::move(msg)),
              relation(std::move(relation)) {
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

    /** get logged relation */
    const std::unique_ptr<RamRelationReference>& getRelation() const {
        return relation;
    }

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "START_TIMER \"" << stringify(message) << "\"\n";
        statement->print(os, tabpos + 1);
        os << "\n";
        os << std::string(tabpos, '\t');
        os << "END_TIMER";
    }

    /** Obtains a list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return toVector<const RamNode*>(statement.get());
    }

    /** Create clone */
    RamLogTimer* clone() const override {
        RamLogTimer* res = new RamLogTimer(std::unique_ptr<RamStatement>(statement->clone()), message,
                std::unique_ptr<RamRelationReference>(relation->clone()));
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        statement = map(std::move(statement));
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamLogTimer*>(&node));
        const auto& other = static_cast<const RamLogTimer&>(node);
        return *statement == *other.statement && message == other.message;
    }
};

/**
 * Debug statement
 */
class RamDebugInfo : public RamStatement {
protected:
    /** debugging statement */
    std::unique_ptr<RamStatement> statement;

    /** debugging message */
    std::string message;

public:
    RamDebugInfo(std::unique_ptr<RamStatement> stmt, std::string msg)
            : RamStatement(RN_DebugInfo), statement(std::move(stmt)), message(std::move(msg)) {
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

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "BEGIN_DEBUG \"" << stringify(message) << "\"\n";
        statement->print(os, tabpos + 1);
        os << "\n";
        os << std::string(tabpos, '\t');
        os << "END_DEBUG";
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return toVector<const RamNode*>(statement.get());
    }

    /** Create clone */
    RamDebugInfo* clone() const override {
        RamDebugInfo* res = new RamDebugInfo(std::unique_ptr<RamStatement>(statement->clone()), message);
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        statement = map(std::move(statement));
    }

protected:
    /** Check equality */
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
protected:
    /** Body of stratum */
    std::unique_ptr<RamStatement> body;
    const int index;

public:
    RamStratum(std::unique_ptr<RamStatement> b, const int i)
            : RamStatement(RN_Stratum), body(std::move(b)), index(i) {}

    /** Get stratum body */
    const RamStatement& getBody() const {
        return *body;
    }

    /** Get stratum index */
    const int getIndex() const {
        return index;
    }

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "BEGIN_STRATUM_" << index << "\n";
        body->print(os, tabpos + 1);
        os << "\n";
        os << std::string(tabpos, '\t');
        os << "END_STRATUM_" << index;
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return toVector<const RamNode*>(body.get());
    }

    /** Create clone */
    RamStratum* clone() const override {
        RamStratum* res = new RamStratum(std::unique_ptr<RamStatement>(body->clone()), index);
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        body = map(std::move(body));
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamStratum*>(&node));
        const auto& other = static_cast<const RamStratum&>(node);
        return *body == *other.body && index == other.index;
    }
};

/**
 *  Log relation size and a logging message.
 */
class RamLogSize : public RamRelationStatement {
protected:
    /** logging message */
    std::string message;

public:
    RamLogSize(std::unique_ptr<RamRelationReference> relation, std::string message)
            : RamRelationStatement(RN_LogSize, std::move(relation)), message(std::move(message)) {}

    /** Get logging message */
    const std::string& getMessage() const {
        return message;
    }

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "LOGSIZE " << getRelation().getName();
        os << " TEXT "
           << "\"" << stringify(message) << "\"";
    }

    /** Create clone */
    RamLogSize* clone() const override {
        RamLogSize* res = new RamLogSize(std::unique_ptr<RamRelationReference>(relation->clone()), message);
        return res;
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamLogSize*>(&node));
        const auto& other = static_cast<const RamLogSize&>(node);
        RamRelationStatement::equal(other);
        return getMessage() == other.getMessage();
    }
};

/**
 * Print relation size and a print message
 *
 * Print relation size for the printsize qualifier for relations.
 */
class RamPrintSize : public RamRelationStatement {
protected:
    /** print message */
    std::string message;

public:
    RamPrintSize(std::unique_ptr<RamRelationReference> rel)
            : RamRelationStatement(RN_PrintSize, std::move(rel)), message(relation->getName() + "\t") {}

    /** Get message */
    const std::string& getMessage() const {
        return message;
    }

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "PRINTSIZE " << getRelation().getName() << " TEXT ";
        os << "\"" << stringify(message) << "\"";
    }

    /** Create clone */
    RamPrintSize* clone() const override {
        RamPrintSize* res = new RamPrintSize(std::unique_ptr<RamRelationReference>(relation->clone()));
        return res;
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamPrintSize*>(&node));
        const auto& other = static_cast<const RamPrintSize&>(node);
        RamRelationStatement::equal(other);
        return message == other.message;
    }
};

#ifdef USE_MPI

class RamRecv : public RamRelationStatement {
private:
    const int sourceStratum;

public:
    RamRecv(std::unique_ptr<RamRelationReference> r, const int s)
            : RamRelationStatement(RN_Recv, std::move(r)), sourceStratum(s) {}

    const int getSourceStratum() const {
        return sourceStratum;
    }

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "RECV DATA FOR " << getRelation().getName() << " FROM STRATUM {" << sourceStratum << "}";
    };

    /** Create clone */
    RamRecv* clone() const override {
        RamRecv* res = new RamRecv(std::unique_ptr<RamRelationReference>(relation->clone()), sourceStratum);
        return res;
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamRecv*>(&node));
        const auto& other = static_cast<const RamRecv&>(node);
        RamRelationStatement::equal(other);
        return sourceStratum == other.sourceStratum;
    }
};

class RamSend : public RamRelationStatement {
private:
    const std::set<size_t> destinationStrata;

public:
    RamSend(std::unique_ptr<RamRelationReference> r, const std::set<size_t> s)
            : RamRelationStatement(RN_Send, std::move(r)), destinationStrata(s) {}

    const std::set<size_t> getDestinationStrata() const {
        return destinationStrata;
    }

    /** Pretty print */
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

    /** Create clone */
    RamSend* clone() const override {
        RamSend* res =
                new RamSend(std::unique_ptr<RamRelationReference>(relation->clone()), destinationStrata);
        return res;
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamSend*>(&node));
        const auto& other = static_cast<const RamSend&>(node);
        return destinationStrata == other.destinationStrata;
    }
};

class RamNotify : public RamStatement {
public:
    RamNotify() : RamStatement(RN_Notify) {}

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "NOTIFY";
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>(0);
    }

    /** Create clone */
    RamNotify* clone() const override {
        return new RamNotify();
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {}

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        return true;
    }
};

class RamWait : public RamStatement {
private:
    const size_t count;

public:
    RamWait(const size_t c) : RamStatement(RN_Wait), count(c) {}

    /** Get count of termination signals required. */
    const int getCount() const {
        return count;
    }

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "WAIT";
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>(0);
    }

    /** Create clone */
    RamWait* clone() const override {
        return new RamWait(count);
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {}

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamWait*>(&node));
        const auto& other = static_cast<const RamWait&>(node);
        return other.count == count;
    }
};

#endif

}  // end of namespace souffle
