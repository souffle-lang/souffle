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

#include "AstClause.h"
#include "RamNode.h"
#include "RamOperation.h"
#include "RamRelation.h"
#include "RamValue.h"
#include "Util.h"

#include <algorithm>
#include <memory>
#include <ostream>
#include <string>
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
 * RAM Statements with a relation
 */
class RamRelationStatement : public RamStatement {
protected:
    /** relation */
    RamRelation relation;

public:
    RamRelationStatement(RamNodeType type, const RamRelation& r) : RamStatement(type), relation(r) {}

    /** Get RAM relation */
    const RamRelation& getRelation() const {
        return relation;
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>();  // no child nodes
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        // TODO (#541): apply to relation after making relation an RamNode
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamRelationStatement*>(&node));
        const RamRelationStatement& other = static_cast<const RamRelationStatement&>(node);
        return relation == other.relation;
    }
};

/**
 * Create new RAM relation
 */
class RamCreate : public RamRelationStatement {
public:
    RamCreate(const RamRelation& relation) : RamRelationStatement(RN_Create, relation) {}

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
    };

    /** Create clone */
    RamCreate* clone() const override {
        RamCreate* res = new RamCreate(getRelation());
        return res;
    }
};

/**
 * Load data into a relation
 */
class RamLoad : public RamRelationStatement {
public:
    RamLoad(const RamRelation& relation) : RamRelationStatement(RN_Load, relation) {}

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "LOAD DATA FOR " << getRelation().getName() << " FROM {" << getRelation().getInputDirectives()
           << "}";
    };

    /** Create clone */
    RamLoad* clone() const override {
        RamLoad* res = new RamLoad(getRelation());
        return res;
    }
};

/**
 * Store data of a relation
 */
class RamStore : public RamRelationStatement {
public:
    RamStore(const RamRelation& relation) : RamRelationStatement(RN_Store, relation) {}

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        const auto& outputDirectives = getRelation().getOutputDirectives();
        os << std::string(tabpos, '\t');
        os << "STORE DATA FOR " << getRelation().getName() << " TO {";
        os << join(outputDirectives, "], [",
                [](std::ostream& out, const IODirectives& directives) { out << directives; });
        os << "}";
    };

    /** Create clone */
    RamStore* clone() const override {
        RamStore* res = new RamStore(getRelation());
        return res;
    }
};

/**
 * Delete tuples of a relation
 */
class RamClear : public RamRelationStatement {
public:
    RamClear(const RamRelation& rel) : RamRelationStatement(RN_Clear, rel) {}

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "CLEAR ";
        os << getRelation().getName();
    }

    /** Create clone */
    RamClear* clone() const override {
        RamClear* res = new RamClear(getRelation());
        return res;
    }
};

/**
 * Drop relation, i.e., delete it from memory
 */
class RamDrop : public RamRelationStatement {
public:
    RamDrop(const RamRelation& rel) : RamRelationStatement(RN_Drop, rel) {}

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "DROP " << getRelation().getName();
    }

    /** Create clone */
    RamDrop* clone() const override {
        RamDrop* res = new RamDrop(getRelation());
        return res;
    }
};

/**
 * Merge tuples from a source into target relation.
 * Note that semantically uniqueness of tuples is not checked.
 */
class RamMerge : public RamStatement {
protected:
    RamRelation target;
    RamRelation source;

public:
    RamMerge(const RamRelation& t, const RamRelation& s) : RamStatement(RN_Merge), target(t), source(s) {
        // TODO (#541): check not just for arity also for correct type!!
        assert(source.getArity() == target.getArity());
    }

    /** Get source relation */
    const RamRelation& getSourceRelation() const {
        return source;
    }

    /** Get target relation */
    const RamRelation& getTargetRelation() const {
        return target;
    }

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "MERGE " << target.getName() << " WITH " << source.getName();
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        // TODO (#541): apply to target/source relations after making relation an RamNode
        return std::vector<const RamNode*>();  // no child nodes
    }

    /** Create clone */
    RamMerge* clone() const override {
        RamMerge* res = new RamMerge(target, source);
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        // TODO (#541): apply to target/source relations after making relation an RamNode
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamMerge*>(&node));
        const RamMerge& other = static_cast<const RamMerge&>(node);
        return target == other.target && source == other.source;
    }
};

/**
 * Swap operation two relations
 */
class RamSwap : public RamStatement {
protected:
    /** first argument of swap statement */
    RamRelation first;

    /** second argument of swap statement */
    RamRelation second;

public:
    RamSwap(const RamRelation& f, const RamRelation& s) : RamStatement(RN_Swap), first(f), second(s) {
        // TODO (#541): check not just for arity also for correct type!!
        assert(first.getArity() == second.getArity());
    }

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "SWAP (" << first.getName() << ", " << second.getName() << ")";
    };

    /** Get first relation */
    const RamRelation& getFirstRelation() const {
        return first;
    }

    /** Get second relation */
    const RamRelation& getSecondRelation() const {
        return second;
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>();  // no child nodes
    }

    /** Create clone */
    RamSwap* clone() const override {
        RamSwap* res = new RamSwap(first, second);
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        // TODO (#541): apply to first/second relations after making relation an RamNode
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamSwap*>(&node));
        const RamSwap& other = static_cast<const RamSwap&>(node);
        return first == other.first && second == other.second;
    }
};

/**
 * Insert a fact into a relation
 */
class RamFact : public RamRelationStatement {
protected:
    /** Arguments of fact */
    // TODO (#541): check whether value_list is used somewhere else
    typedef std::vector<std::unique_ptr<RamValue>> value_list;
    value_list values;

public:
    RamFact(const RamRelation& rel, value_list&& v)
            : RamRelationStatement(RN_Fact, rel), values(std::move(v)) {}

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
        // TODO (#541): apply to relation after making relation an RamNode
        std::vector<const RamNode*> res;
        for (const auto& cur : values) {
            res.push_back(cur.get());
        }
        return res;
    }

    /** Create clone */
    RamFact* clone() const override {
        RamFact* res = new RamFact(getRelation(), {});
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
        assert(dynamic_cast<const RamFact*>(&node));
        const RamFact& other = static_cast<const RamFact&>(node);
        return RamRelationStatement::equal(other) && equal_targets(values, other.values);
    }
};

/**
 * A relational algebra query
 * TODO (#541): Rename RAM statement: it is used for projection and sub-routines.
 */
class RamInsert : public RamStatement {
protected:
    // TODO (#541): strong dependency to AST do we
    // need this dependency / better to encapsulate
    // information here
    std::unique_ptr<AstClause> clause;

    /** RAM operation */
    std::unique_ptr<RamOperation> operation;

public:
    RamInsert(const AstClause& c, std::unique_ptr<RamOperation> o)
            : RamStatement(RN_Insert), clause(std::unique_ptr<AstClause>(c.clone())),
              operation(std::move(o)) {}

    /** Get AST clause */
    // TODO (#541): remove dependency to AST
    const AstClause& getOrigin() const {
        ASSERT(clause);
        return *clause;
    }

    /** Get RAM operation */
    const RamOperation& getOperation() const {
        ASSERT(operation);
        return *operation;
    }

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "INSERT \n";
        operation->print(os, tabpos + 1);
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return toVector<const RamNode*>(operation.get());
    }

    /** Create clone */
    RamInsert* clone() const override {
        RamInsert* res = new RamInsert(*clause, std::unique_ptr<RamOperation>(operation->clone()));
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        operation = map(std::move(operation));
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamInsert*>(&node));
        const RamInsert& other = static_cast<const RamInsert&>(node);
        return *operation == *other.operation && clause == other.clause;
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
            ASSERT(cur);
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
        RamSequence* res = new RamSequence();
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
        assert(dynamic_cast<const RamSequence*>(&node));
        const RamSequence& other = static_cast<const RamSequence&>(node);

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
        RamParallel* res = new RamParallel();
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
        assert(dynamic_cast<const RamParallel*>(&node));
        const RamParallel& other = static_cast<const RamParallel&>(node);
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
            : RamStatement(RN_Loop), body(std::unique_ptr<RamStatement>(new RamSequence(
                                             std::move(f), std::move(s), std::move(rest)...))) {}

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
        assert(dynamic_cast<const RamLoop*>(&node));
        const RamLoop& other = static_cast<const RamLoop&>(node);
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
        ASSERT(condition);
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
        assert(dynamic_cast<const RamExit*>(&node));
        const RamExit& other = static_cast<const RamExit&>(node);
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

public:
    RamLogTimer(std::unique_ptr<RamStatement> stmt, const std::string& msg)
            : RamStatement(RN_LogTimer), statement(std::move(stmt)), message(msg) {
        ASSERT(statement);
    }

    /** get logging message */
    const std::string& getMessage() const {
        return message;
    }

    /** get logging statement */
    const RamStatement& getStatement() const {
        ASSERT(statement);
        return *statement;
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
        RamLogTimer* res = new RamLogTimer(std::unique_ptr<RamStatement>(statement->clone()), message);
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        statement = map(std::move(statement));
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamLogTimer*>(&node));
        const RamLogTimer& other = static_cast<const RamLogTimer&>(node);
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
    RamDebugInfo(std::unique_ptr<RamStatement> stmt, const std::string& msg)
            : RamStatement(RN_DebugInfo), statement(std::move(stmt)), message(msg) {
        ASSERT(statement);
    }

    /** Get debugging message */
    const std::string& getMessage() const {
        return message;
    }

    /** Get debugging statement */
    const RamStatement& getStatement() const {
        ASSERT(statement);
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
    virtual std::vector<const RamNode*> getChildNodes() const override {
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
        assert(dynamic_cast<const RamLogTimer*>(&node));
        const RamLogTimer& other = static_cast<const RamLogTimer&>(node);
        return getStatement() == other.getStatement() && getMessage() == other.getMessage();
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
    RamLogSize(const RamRelation& rel, const std::string& msg)
            : RamRelationStatement(RN_LogSize, rel), message(msg) {}

    /** Get logging message */
    const std::string& getMessage() const {
        return message;
    }

    /** Pretty print */
    void print(std::ostream& os, int tabpos) const override {
        os << std::string(tabpos, '\t');
        os << "LOGSIZE " << getRelation().getName() << " TEXT ";
        os << "\"" << stringify(message) << "\"";
    }

    /** Create clone */
    RamLogSize* clone() const override {
        RamLogSize* res = new RamLogSize(relation, message);
        return res;
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamLogSize*>(&node));
        const RamLogSize& other = static_cast<const RamLogSize&>(node);
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
    RamPrintSize(const RamRelation& rel)
            : RamRelationStatement(RN_PrintSize, rel), message(rel.getName() + "\t") {}

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
        RamPrintSize* res = new RamPrintSize(relation);
        return res;
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamPrintSize*>(&node));
        const RamPrintSize& other = static_cast<const RamPrintSize&>(node);
        RamRelationStatement::equal(other);
        return message == other.message;
    }
};

}  // end of namespace souffle
