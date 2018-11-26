/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Interpreter.h
 *
 * Declares the interpreter class for executing RAM programs.
 *
 ***********************************************************************/

#pragma once

#include "InterpreterContext.h"
#include "InterpreterRelation.h"
#include "RamCondition.h"
#include "RamRelation.h"
#include "RamStatement.h"
#include "RamTranslationUnit.h"
#include "RamTypes.h"

#include <cassert>
#include <map>
#include <string>
#include <vector>
#include <dlfcn.h>

#define SOUFFLE_DLL "libfunctors.so"

namespace souffle {

class InterpreterProgInterface;
class RamOperation;
class RamValue;
class SymbolTable;

/**
 * Interpreter executing a RAM translation unit
 */

class Interpreter {
    friend InterpreterProgInterface;

private:
    /** RAM translation Unit */
    RamTranslationUnit& translationUnit;

    /** relation environment type */
    using relation_map = std::map<std::string, InterpreterRelation*>;

    /** relation environment */
    relation_map environment;

    /** counters for atom profiling */
    std::map<std::string, std::map<size_t, size_t>> frequencies;

    /** counters for non-existence checks */
    std::map<std::string, std::atomic<size_t>> reads;

    /** counter for $ operator */
    int counter;

    /** iteration number (in a fix-point calculation) */
    size_t iteration;

    /** Dynamic library for user-defined functors */
    void* dll;

protected:
    /** Evaluate value */
    RamDomain evalVal(const RamValue& value, const InterpreterContext& ctxt = InterpreterContext());

    /** Evaluate operation */
    void evalOp(const RamOperation& op, const InterpreterContext& args = InterpreterContext());

    /** Evaluate conditions */
    bool evalCond(const RamCondition& cond, const InterpreterContext& ctxt = InterpreterContext());

    /** Evaluate statement */
    void evalStmt(const RamStatement& stmt);

    /** Get symbol table */
    SymbolTable& getSymbolTable() {
        return translationUnit.getSymbolTable();
    }

    /** Get counter */
    int getCounter() const {
        return counter;
    }

    /** Get Iteration Number */
    size_t getIterationNumber() const {
        return iteration;
    }

    /** Increment counter */
    int incCounter() {
        return counter++;
    }

    /** Increment iteration number */
    void incIterationNumber() {
        iteration++;
    }

    /** Reset iteration number */
    void resetIterationNumber() {
        iteration = 0;
    }

    /** Create relation */
    void createRelation(const RamRelationReference& id) {
        InterpreterRelation* res = nullptr;
        assert(environment.find(id.getName()) == environment.end());
        if (!id.isEqRel()) {
            res = new InterpreterRelation(id.getArity());
        } else {
            res = new InterpreterEqRelation(id.getArity());
        }
        environment[id.getName()] = res;
    }

    /** Get relation */
    InterpreterRelation& getRelation(const std::string& name) {
        // look up relation
        auto pos = environment.find(name);
        assert(pos != environment.end());
        return *pos->second;
    }

    /** Get relation */
    inline InterpreterRelation& getRelation(const RamRelationReference& id) {
        return getRelation(id.getName());
    }

    /** Get relation map */
    relation_map& getRelationMap() const {
        return const_cast<relation_map&>(environment);
    }

    /** Drop relation */
    void dropRelation(const RamRelationReference& id) {
        InterpreterRelation& rel = getRelation(id);
        environment.erase(id.getName());
        delete &rel;
    }

    /** Swap relation */
    void swapRelation(const RamRelationReference& ramRel1, const RamRelationReference& ramRel2) {
        InterpreterRelation* rel1 = &getRelation(ramRel1);
        InterpreterRelation* rel2 = &getRelation(ramRel2);
        environment[ramRel1.getName()] = rel2;
        environment[ramRel2.getName()] = rel1;
    }

    /** Load dll */
    void* loadDLL() {
        if (dll == nullptr) {
            // check environment variable
            std::string fname = SOUFFLE_DLL;
            dll = dlopen(SOUFFLE_DLL, RTLD_LAZY);
            if (dll == nullptr) {
                std::cerr << "Cannot find Souffle's DLL" << std::endl;
                exit(1);
            }
        }
        return dll;
    }

public:
    Interpreter(RamTranslationUnit& tUnit) : translationUnit(tUnit), counter(0), iteration(0), dll(nullptr) {}
    virtual ~Interpreter() {
        for (auto& x : environment) {
            delete x.second;
        }
    }

    /** Get translation unit */
    RamTranslationUnit& getTranslationUnit() {
        return translationUnit;
    }

    /** Execute main program */
    void executeMain();

    /* Execute subroutine */
    void executeSubroutine(const RamStatement& stmt, const std::vector<RamDomain>& arguments,
            std::vector<RamDomain>& returnValues, std::vector<bool>& returnErrors);
};

}  // end of namespace souffle
