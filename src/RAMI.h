/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RAMI.h
 *
 * Declares the RAMI (RamInterpreter) class for executing RAM programs.
 *
 ***********************************************************************/

#pragma once

#include "Interpreter.h"
#include "InterpreterContext.h"
#include "InterpreterRelation.h"
#include "RamCondition.h"
#include "RamRelation.h"
#include "RamStatement.h"
#include "RamTranslationUnit.h"
#include "RamTypes.h"
#include "RelationRepresentation.h"

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <dlfcn.h>

namespace souffle {

class InterpreterProgInterface;
class RamOperation;
class RamExpression;
class SymbolTable;

/**
 * Interpreter executing a RAM translation unit
 */

class RAMI : public Interpreter {
public:
    RAMI(RamTranslationUnit& tUnit) : Interpreter(tUnit) {}
    ~RAMI() override = default;

    /** Execute main program */
    void executeMain() override;

    /* Execute subroutine */
    void executeSubroutine(const std::string& name, const std::vector<RamDomain>& arguments,
            std::vector<RamDomain>& returnValues, std::vector<bool>& returnErrors) override;

protected:
    /** Evaluate value */
    RamDomain evalExpr(const RamExpression& value, const InterpreterContext& ctxt = InterpreterContext());

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

    void createRelation(const RamRelation& id) {
        InterpreterRelation* res = nullptr;
        assert(environment.find(id.getName()) == environment.end());
        if (id.getRepresentation() == RelationRepresentation::EQREL) {
            res = new InterpreterEqRelation(id.getArity());
        } else {
            res = new InterpreterRelation(id.getArity());
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
    inline InterpreterRelation& getRelation(const RamRelation& id) {
        return getRelation(id.getName());
    }

    /** Drop relation */
    void dropRelation(const RamRelation& id) {
        InterpreterRelation& rel = getRelation(id);
        environment.erase(id.getName());
        delete &rel;
    }

    /** Swap relation */
    void swapRelation(const RamRelation& ramRel1, const RamRelation& ramRel2) {
        InterpreterRelation* rel1 = &getRelation(ramRel1);
        InterpreterRelation* rel2 = &getRelation(ramRel2);
        environment[ramRel1.getName()] = rel2;
        environment[ramRel2.getName()] = rel1;
    }

    /** Load dll */
    const std::vector<void*>& loadDLL() {
        if (!dll.empty()) {
            return dll;
        }

        if (!Global::config().has("libraries")) {
            Global::config().set("libraries", SOUFFLE_DLL);
        }
        for (const std::string& library : splitString(Global::config().get("libraries"), ',')) {
            dll.push_back(dlopen(library.c_str(), RTLD_LAZY));
            if (dll.back() == nullptr) {
                std::cerr << "Cannot find '" << library << "' DLL" << std::endl;
                exit(1);
            }
        }

        return dll;
    }

    void* getMethodHandle(const std::string& method) {
        // load DLLs (if not done yet)
        for (void* libHandle : loadDLL()) {
            return dlsym(libHandle, method.c_str());
        }
        return nullptr;
    }

private:
    friend InterpreterProgInterface;

    /** counters for atom profiling */
    std::map<std::string, std::map<size_t, size_t>> frequencies;

    /** counters for non-existence checks */
    std::map<std::string, std::atomic<size_t>> reads;

    /** counter for $ operator */
    int counter = 0;

    /** iteration number (in a fix-point calculation) */
    size_t iteration = 0;
};

}  // end of namespace souffle
