/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamExecutor.h
 *
 * Declares entities capable of executing a RAM program.
 *
 ***********************************************************************/

#pragma once

#include "RamData.h"
#include "RamRelation.h"
#include "SymbolTable.h"
#include "Util.h"

#include <functional>
//#include <map>
#include <ostream>
#include <string>
#include <vector>

namespace souffle {

/** forward declaration */
class RamStatement;
class RamInsert;

/**
 * An abstract base class for entities capable of processing a RAM program.
 */
class RamExecutor {
protected:
    /** An optional stream to print logging information to */
    std::ostream* report;

public:
    RamExecutor() : report(nullptr) {}

    /** A virtual destructor to support safe inheritance */
    virtual ~RamExecutor() = default;

    /**
     * Updates the target this executor is reporting to.
     */
    void setReportTarget(std::ostream& report) {
        this->report = &report;
    }

    /**
     * Disables the reporting. No more report messages will be printed.
     */
    void disableReporting() {
        report = nullptr;
    }

    /**
     * Runs the given RAM statement on an empty environment and returns
     * this environment after the completion of the execution.
     */
    RamEnvironment execute(SymbolTable& table, const RamStatement& stmt) const {
        RamEnvironment env(table);
        applyOn(stmt, env, nullptr);
        return env;
    }

    /**
     * Runs the given RAM statement on an empty environment and input data and returns
     * this environment after the completion of the execution.
     */
    RamEnvironment* execute(SymbolTable& table, const RamStatement& stmt, RamData* data) const {
        // Ram env managed by the interface
        RamEnvironment* env = new RamEnvironment(table);
        applyOn(stmt, *env, data);
        return env;
    }

    /**
     * Runs the given statement on the given environment.
     */
    virtual void applyOn(const RamStatement& stmt, RamEnvironment& env, RamData* data) const = 0;
};

/**
 * A class representing the order of elements.
 */
class Order {
    /** The covered order */
    std::vector<unsigned> order;

public:
    static Order getIdentity(unsigned size) {
        Order res;
        for (unsigned i = 0; i < size; i++) {
            res.append(i);
        }
        return res;
    }

    void append(unsigned pos) {
        order.push_back(pos);
    }

    unsigned operator[](unsigned index) const {
        return order[index];
    }

    std::size_t size() const {
        return order.size();
    }

    bool isComplete() const {
        for (size_t i = 0; i < order.size(); i++) {
            if (!contains(order, i)) {
                return false;
            }
        }
        return true;
    }

    const std::vector<unsigned>& getOrder() const {
        return order;
    }

    void print(std::ostream& out) const {
        out << order;
    }

    friend std::ostream& operator<<(std::ostream& out, const Order& order) {
        order.print(out);
        return out;
    }
};

/**
 * The summary to be returned from a statement executor.
 */
struct ExecutionSummary {
    Order order;
    long time;
};

/** Defines the type of execution strategies */
typedef std::function<ExecutionSummary(const RamInsert&, RamEnvironment& env, std::ostream*)>
        QueryExecutionStrategy;

// -- example strategies --

/** With this strategy queries will be processed as they are stated by the user */
extern const QueryExecutionStrategy DirectExecution;

/** With this strategy queries will be dynamically rescheduled before each execution */
extern const QueryExecutionStrategy ScheduledExecution;

/**
 * An interpreter based implementation of a RAM executor. The RAM program will
 * be processed within the callers process. Before every query operation, an
 * optional scheduling step will be conducted.
 */
class RamGuidedInterpreter : public RamExecutor {
    /** The executor processing a query */
    QueryExecutionStrategy queryStrategy;

public:
    /** A constructor accepting a query executor strategy */
    RamGuidedInterpreter(const QueryExecutionStrategy& queryStrategy = ScheduledExecution)
            : queryStrategy(queryStrategy) {}

    /**
     * The implementation of the interpreter applying the given program
     * on the given environment.
     */
    void applyOn(const RamStatement& stmt, RamEnvironment& env, RamData* data) const override;
};

/**
 * An interpreter based implementation of a RAM executor. The RAM program will
 * be processed within the callers process. In this version, no scheduling
 * will be conducted.
 */
struct RamInterpreter : public RamGuidedInterpreter {
    /** A constructor setting the query policy for the base class */
    RamInterpreter() : RamGuidedInterpreter(DirectExecution){};
};

/**
 * A RAM executor based on the creation and compilation of an executable conducting
 * the actual computation.
 */
class RamCompiler : public RamExecutor {
private:
    std::string compileCmd;

public:
    /** A simple constructor */
    RamCompiler(const std::string& compileCmd) : compileCmd(compileCmd) {}

    /**
     * Generates the code for the given ram statement.The target file
     * name is either set by the corresponding member field or will
     * be determined randomly. The chosen file-name will be returned.
     */
    std::string generateCode(
            const SymbolTable& symTable, const RamStatement& stmt, const std::string& filename = "") const;

    /**
     * Generates the code for the given ram statement.The target file
     * name is either set by the corresponding member field or will
     * be determined randomly. The chosen file-name will be returned.
     */
    std::string compileToLibrary(const SymbolTable& symTable, const RamStatement& stmt,
            const std::string& filename = "default") const;

    /**
     * Compiles the given statement to a binary file. The target file
     * name is either set by the corresponding member field or will
     * be determined randomly. The chosen file-name will be returned.
     */
    std::string compileToBinary(const SymbolTable& symTable, const RamStatement& stmt) const;

    /**
     * The actual implementation of this executor encoding the given
     * program into a source file, compiling and executing it.
     */
    void applyOn(const RamStatement& stmt, RamEnvironment& env, RamData* data) const override;

private:
    /**
     * Obtains a file name for the resulting source and executable file.
     */
    std::string resolveFileName() const;
};

/**
 * A singleton which provides a mapping from strings to unique valid CPP identifiers.
 */
class CPPIdentifierMap {
public:
    /**
     * Obtains the singleton instance.
     */
    static CPPIdentifierMap& getInstance() {
        if (instance == nullptr) {
            instance = new CPPIdentifierMap();
        }
        return *instance;
    }

    /**
     * Given a string, returns its corresponding unique valid identifier;
     */
    static std::string getIdentifier(const std::string& name) {
        return getInstance().identifier(name);
    }

    ~CPPIdentifierMap() = default;

private:
    CPPIdentifierMap() {}

    static CPPIdentifierMap* instance;

    /**
     * Instance method for getIdentifier above.
     */
    const std::string identifier(const std::string& name) {
        auto it = identifiers.find(name);
        if (it != identifiers.end()) {
            return it->second;
        }
        // strip leading numbers
        unsigned int i;
        for (i = 0; i < name.length(); ++i) {
            if (isalnum(name.at(i)) || name.at(i) == '_') {
                break;
            }
        }
        std::string id;
        for (auto ch : std::to_string(identifiers.size() + 1) + '_' + name.substr(i)) {
            // alphanumeric characters are allowed
            if (isalnum(ch)) {
                id += ch;
            }
            // all other characters are replaced by an underscore, except when
            // the previous character was an underscore as double underscores
            // in identifiers are reserved by the standard
            else if (id.size() == 0 || id.back() != '_') {
                id += '_';
            }
        }
        // most compilers have a limit of 2048 characters (if they have a limit at all) for
        // identifiers; we use half of that for safety
        id = id.substr(0, 1024);
        identifiers.insert(std::make_pair(name, id));
        return id;
    }

    // The map of identifiers.
    std::map<const std::string, const std::string> identifiers;
};

}  // end of namespace souffle
