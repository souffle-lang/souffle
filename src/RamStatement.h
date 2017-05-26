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

#include <map>
#include <set>

#include <pthread.h>

namespace souffle {

/** abstract class for statements */
class RamStatement : public RamNode {
public:
    RamStatement(RamNodeType type) : RamNode(type) {}

    /** Pretty print statement */
    virtual void print(std::ostream& os, int tabpos) const = 0;

    /** Pretty print node */
    void print(std::ostream& os) const override {
        print(os, 0);
    }
};

// ------------------------------------------------------------------
//                          Table Operations
// ------------------------------------------------------------------

class RamRelationStatement : public RamStatement {
    /** The referenced ram relation */
    RamRelationIdentifier relation;

public:
    /** Creates a new statement targeting the given table */
    RamRelationStatement(RamNodeType type, const RamRelationIdentifier& r)
            : RamStatement(type), relation(r) {}

    /** Obtains a reference on the targeted relation */
    const RamRelationIdentifier& getRelation() const {
        return relation;
    }

    /** Obtains a list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>();  // no child nodes
    }
};

/** Creates a new relation */
class RamCreate : public RamRelationStatement {
public:
    RamCreate(const RamRelationIdentifier& relation) : RamRelationStatement(RN_Create, relation) {}

    /** Pretty print statement */
    void print(std::ostream& os, int tabpos) const override {
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "CREATE " << getRelation().getName() << "(";
        os << getRelation().getArg(0);
        for (size_t i = 1; i < getRelation().getArity(); i++) {
            os << ",";
            os << getRelation().getArg(i);
        }
        os << ")";
    };
};

/** Adds a fact to a given relation */
class RamFact : public RamRelationStatement {
protected:
    typedef std::vector<std::unique_ptr<const RamValue>> value_list;
    value_list values;

public:
    RamFact(const RamRelationIdentifier& rel, value_list&& values)
            : RamRelationStatement(RN_Fact, rel), values(std::move(values)) {}

    ~RamFact() override = default;

    std::vector<const RamValue*> getValues() const {
        return toPtrVector(values);
    }

    /** Pretty print statement */
    void print(std::ostream& os, int tabpos) const override {
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "INSERT (" << join(values, ",", print_deref<std::unique_ptr<const RamValue>>()) << ") INTO "
           << getRelation().getName();
    };

    /** Obtains a list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        for (const auto& cur : values) {
            res.push_back(cur.get());
        }
        return res;
    }
};

/** Loads data from a file into a relation */
class RamLoad : public RamRelationStatement {
public:
    RamLoad(const RamRelationIdentifier& relation) : RamRelationStatement(RN_Load, relation) {}

    /** Pretty print statement */
    void print(std::ostream& os, int tabpos) const override {
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "LOAD DATA FOR " << getRelation().getName();
    };
};

/** Dumps all data from a relation into file */
class RamStore : public RamRelationStatement {
public:
    RamStore(const RamRelationIdentifier& relation) : RamRelationStatement(RN_Store, relation) {}

    /** Pretty print statement */
    void print(std::ostream& os, int tabpos) const override {
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "STORE DATA FOR " << getRelation().getName();
    };
};

/** Removes all tuples form a relation */
class RamClear : public RamRelationStatement {
public:
    RamClear(const RamRelationIdentifier& rel) : RamRelationStatement(RN_Clear, rel) {}

    void print(std::ostream& os, int tabpos) const override {
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "CLEAR ";
        os << getRelation().getName();
    }
};

/** drop table */
class RamDrop : public RamRelationStatement {
public:
    RamDrop(const RamRelationIdentifier& rel) : RamRelationStatement(RN_Drop, rel) {}

    void print(std::ostream& os, int tabpos) const override {
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "DROP " << getRelation().getName();
    }
};

/** log table size */
class RamLogSize : public RamRelationStatement {
    std::string txt;

public:
    RamLogSize(const RamRelationIdentifier& rel, const std::string& s)
            : RamRelationStatement(RN_LogSize, rel), txt(s) {}

    const std::string& getLabel() const {
        return txt;
    }

    void print(std::ostream& os, int tabpos) const override {
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "LOGSIZE " << getRelation().getName() << " TEXT ";
        os << "\"" << txt << "\"";
    }
};

/** print table size */
class RamPrintSize : public RamRelationStatement {
    std::string txt;

public:
    RamPrintSize(const RamRelationIdentifier& rel)
            : RamRelationStatement(RN_PrintSize, rel), txt(rel.getName() + "\t") {}

    const std::string& getLabel() const {
        return txt;
    }

    void print(std::ostream& os, int tabpos) const override {
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "PRINTSIZE " << getRelation().getName() << " TEXT ";
        os << "\"" << txt << "\"";
    }
};

/** A relational algebra query */
class RamInsert : public RamStatement {
    std::unique_ptr<const AstClause> clause;

    std::unique_ptr<RamOperation> operation;

public:
    RamInsert(const AstClause& clause, std::unique_ptr<RamOperation> o)
            : RamStatement(RN_Insert), clause(std::unique_ptr<const AstClause>(clause.clone())),
              operation(std::move(o)) {}

    ~RamInsert() override = default;

    const AstClause& getOrigin() const {
        return *clause;
    }

    const RamOperation& getOperation() const {
        return *operation;
    }

    void print(std::ostream& os, int tabpos) const override {
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "INSERT \n";
        operation->print(os, tabpos + 1);
    }

    /** Obtains a list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return toVector<const RamNode*>(operation.get());
    }
};

/** copy tuples from a source table to a destination table. Uniquness is not checked */
class RamMerge : public RamStatement {
    RamRelationIdentifier src;
    RamRelationIdentifier dest;

public:
    RamMerge(const RamRelationIdentifier& d, const RamRelationIdentifier& s)
            : RamStatement(RN_Merge), src(s), dest(d) {
        assert(src.getArity() == dest.getArity());
    }

    const RamRelationIdentifier& getSourceRelation() const {
        return src;
    }
    const RamRelationIdentifier& getTargetRelation() const {
        return dest;
    }

    void print(std::ostream& os, int tabpos) const override {
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "MERGE ";
        os << src.getName() << " INTO " << dest.getName();
    }

    /** Obtains a list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>();  // no child nodes
    }
};

// ------------------------------------------------------------------
//                          Control Flow
// ------------------------------------------------------------------

/** sequential execution of statements */
class RamSequence : public RamStatement {
    std::vector<std::unique_ptr<RamStatement>> stmts;

public:
    template <typename... Stmts>
    RamSequence(std::unique_ptr<Stmts>&&... stmts) : RamStatement(RN_Sequence) {
        // move all the given statements into the vector (not so simple)
        std::unique_ptr<RamStatement> tmp[] = {std::move(stmts)...};
        for (auto& cur : tmp) {
            this->stmts.emplace_back(std::move(cur));
        }

        for (const auto& cur : this->stmts) {
            ASSERT(cur);
        }
    }

    ~RamSequence() override = default;

    /* add new statement to parallel construct */
    void add(std::unique_ptr<RamStatement> s) {
        if (s) {
            stmts.push_back(std::move(s));
        }
    }

    std::vector<RamStatement*> getStatements() const {
        return toPtrVector(stmts);
    }

    void print(std::ostream& os, int tabpos) const override {
        os << join(stmts, ";\n", [&](std::ostream& os, const std::unique_ptr<RamStatement>& stmt) {
            stmt->print(os, tabpos);
        });
    }

    /** Obtains a list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        for (const auto& cur : stmts) {
            res.push_back(cur.get());
        }
        return res;
    }
};

/** Parallel execution of statements */
class RamParallel : public RamStatement {
    std::vector<std::unique_ptr<RamStatement>> stmts;

public:
    RamParallel() : RamStatement(RN_Parallel) {}

    ~RamParallel() override = default;

    /* add new statement to parallel construct */
    void add(std::unique_ptr<RamStatement> s) {
        if (s) {
            stmts.push_back(std::move(s));
        }
    }

    std::vector<RamStatement*> getStatements() const {
        return toPtrVector(stmts);
    }

    /* print parallel statement */
    void print(std::ostream& os, int tabpos) const override {
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "PARALLEL\n";
        for (uint32_t i = 0; i < stmts.size(); i++) {
            stmts[i]->print(os, tabpos + 1);
            if (i < stmts.size() - 1) {
                os << "\n";
                for (int i = 0; i < tabpos; ++i) {
                    os << '\t';
                }
                os << " ||";
            }
            os << "\n";
        }
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "END PARALLEL";
    }

    /** Obtains a list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        for (const auto& cur : stmts) {
            res.push_back(cur.get());
        }
        return res;
    }
};

/** An endless loop until a statement inside the loop returns false */
class RamLoop : public RamStatement {
    std::unique_ptr<RamStatement> body;

public:
    RamLoop(std::unique_ptr<RamStatement> b) : RamStatement(RN_Loop), body(std::move(b)) {}

    template <typename... Stmts>
    RamLoop(std::unique_ptr<RamStatement> f, std::unique_ptr<RamStatement> s, std::unique_ptr<Stmts>... rest)
            : RamStatement(RN_Loop), body(std::unique_ptr<RamStatement>(new RamSequence(
                                             std::move(f), std::move(s), std::move(rest)...))) {}

    ~RamLoop() override = default;

    const RamStatement& getBody() const {
        return *body;
    }

    void print(std::ostream& os, int tabpos) const override {
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "LOOP\n";
        body->print(os, tabpos + 1);
        os << "\n";
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "END LOOP";
    }

    /** Obtains a list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return toVector<const RamNode*>(body.get());
    }
};

/** Swap operation for temporary relations. */
class RamSwap : public RamStatement {
    RamRelationIdentifier first;
    RamRelationIdentifier second;

public:
    RamSwap(const RamRelationIdentifier& f, const RamRelationIdentifier& s)
            : RamStatement(RN_Swap), first(f), second(s) {
        assert(first.getArity() == second.getArity());
    }

    ~RamSwap() override = default;

    void print(std::ostream& os, int tabpos) const override {
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "SWAP (" << first.getName() << ", " << second.getName() << ")";
    };

    const RamRelationIdentifier& getFirstRelation() const {
        return first;
    }
    const RamRelationIdentifier& getSecondRelation() const {
        return second;
    }

    /** Obtains a list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>();  // no child nodes
    }
};

/** exit the body if condition holds */
class RamExit : public RamStatement {
    std::unique_ptr<RamCondition> condition;

public:
    RamExit(std::unique_ptr<RamCondition> c) : RamStatement(RN_Exit), condition(std::move(c)) {}
    ~RamExit() override = default;

    const RamCondition& getCondition() const {
        return *condition;
    }

    void print(std::ostream& os, int tabpos) const override {
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "EXIT ";
        condition->print(os);
    }

    /** Obtains a list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return toVector<const RamNode*>(condition.get());
    }
};

/** A statement logging the execution time of a statement */
class RamLogTimer : public RamStatement {
    std::unique_ptr<const RamStatement> nested;
    std::string label;

public:
    RamLogTimer(std::unique_ptr<const RamStatement> stmt, const std::string& label)
            : RamStatement(RN_LogTimer), nested(std::move(stmt)), label(label) {
        ASSERT(nested);
    }

    const std::string& getLabel() const {
        return label;
    }

    const RamStatement& getNested() const {
        return *nested;
    }

    void print(std::ostream& os, int tabpos) const override {
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "START_TIMER \"" << label << "\"\n";
        nested->print(os, tabpos + 1);
        os << "\n";
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "END_TIMER \"" << label << "\"";
    }

    /** Obtains a list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return toVector<const RamNode*>(nested.get());
    }
};

/** A statement logging the execution time of a statement */
class RamDebugInfo : public RamStatement {
    std::unique_ptr<const RamStatement> nested;
    std::string label;

public:
    RamDebugInfo(std::unique_ptr<const RamStatement> stmt, const std::string& label)
            : RamStatement(RN_DebugInfo), nested(std::move(stmt)), label(label) {
        ASSERT(nested);
    }

    const std::string& getLabel() const {
        return label;
    }

    const RamStatement& getNested() const {
        return *nested;
    }

    virtual void print(std::ostream& os, int tabpos) const {
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "BEGIN_DEBUG \"" << label << "\"\n";
        nested->print(os, tabpos + 1);
        os << "\n";
        for (int i = 0; i < tabpos; ++i) {
            os << '\t';
        }
        os << "END_DEBUG \"" << label << "\"";
    }

    /** Obtains a list of child nodes */
    virtual std::vector<const RamNode*> getChildNodes() const {
        return toVector<const RamNode*>(nested.get());
    }
};

}  // end of namespace souffle
