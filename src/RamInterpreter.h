/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamInterpreter.h
 *
 * Declares the interpreter class for executing RAM programs.
 *
 ***********************************************************************/

#pragma once

#include "RamExecutor.h"

#include <functional>
#include <vector>

namespace souffle {

/**
 * A class representing the order of predicates in the body of a rule
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

/** Defines the type of execution strategies for interpreter */
typedef std::function<ExecutionSummary(const RamInsert&, RamEnvironment& env, std::ostream*)>
        QueryExecutionStrategy;

/** With this strategy queries will be processed without profiling */
extern const QueryExecutionStrategy DirectExecution;

/** With this strategy queries will be dynamically with profiling */
extern const QueryExecutionStrategy ScheduledExecution;

/**
 * An interpreter based implementation of a RAM executor. The RAM program will
 * be processed within the callers process. Before every query operation, an
 * optional scheduling step will be conducted.
 */
class RamInterpreter : public RamExecutor {
    /** The executor processing a query */
    QueryExecutionStrategy queryStrategy;

public:
    /** A constructor accepting a query executor strategy */
    RamInterpreter(const QueryExecutionStrategy& queryStrategy) : queryStrategy(queryStrategy) {}

    /**
     * The implementation of the interpreter applying the given program
     * on the given environment.
     */
    void applyOn(const RamProgram& prog, RamEnvironment& env, RamData* data) const override;

    /**
     * Runs a subroutine of a RamProgram
     */
    virtual void executeSubroutine(RamEnvironment& env, const RamStatement& stmt,
            const std::vector<RamDomain>& arguments, std::vector<RamDomain>& returnValues,
            std::vector<bool>& returnErrors) const override;
};

}  // end of namespace souffle
