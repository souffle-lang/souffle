/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file LVM.h
 *
 * Declares the bytecode interpreter class
 *
 ***********************************************************************/

#pragma once

#include "Interpreter.h"
#include "InterpreterContext.h"
#include "InterpreterRelation.h"
#include "LVMCode.h"
#include "LVMGenerator.h"
#include "Logger.h"
#include "RamTranslationUnit.h"
#include "RamTypes.h"
#include "RelationRepresentation.h"

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <map>
#include <stack>
#include <string>
#include <utility>
#include <vector>
#include <dlfcn.h>

#define SOUFFLE_DLL "libfunctors.so"

namespace souffle {

class InterpreterProgInterface;

/**
 * Bytecode Interpreter executing a RAM translation unit.
 *
 * RAM is transferred into an equivalent Bytecode Representation (LVMCode) by LVMGenerator.
 * LVM then directly execute the program by interpreting the LVMCode.
 * The LVMCode will be cached in the memory to save time when repeatedly execution is needed.
 */
class LVM : public Interpreter {
public:
    LVM(RamTranslationUnit& tUnit) : Interpreter(tUnit) {}

    virtual ~LVM() {
        for (auto* timer : timers) {
            delete timer;
        }
    }

    /** Execute the main program */
    void executeMain() override;

    /** Clean the cache of main Program */
    void resetMainProgram() {
        mainProgram.reset();
    }

    /** Clean the cache of subroutine */
    void resetSubroutine(const std::string& name) {
        subroutines.erase(name);
    }

    /** Execute the subroutine */
    void executeSubroutine(const std::string& name, const std::vector<RamDomain>& arguments,
            std::vector<RamDomain>& returnValues, std::vector<bool>& returnErrors) override {
        InterpreterContext ctxt;
        ctxt.setReturnValues(returnValues);
        ctxt.setReturnErrors(returnErrors);
        ctxt.setArguments(arguments);

        if (subroutines.find(name) != subroutines.cend()) {
            execute(subroutines.at(name), ctxt);
        } else {
            // Parse and cache the program
            LVMGenerator generator(
                    translationUnit.getSymbolTable(), translationUnit.getProgram()->getSubroutine(name));
            subroutines.emplace(std::make_pair(name, generator.getCodeStream()));
            execute(subroutines.at(name), ctxt);
        }
    }

    /** Print out the instruction stream */
    void printMain() {
        if (mainProgram.get() == nullptr) {
            LVMGenerator generator(
                    translationUnit.getSymbolTable(), *translationUnit.getProgram()->getMain());
            mainProgram = generator.getCodeStream();
        }
        mainProgram->print();
    }

protected:
    using index_set = btree_multiset<const RamDomain*, InterpreterIndex::comparator,
            std::allocator<const RamDomain*>, 512>;

    /** Insert Logger */
    void insertTimerAt(size_t index, Logger* timer) {
        if (index >= timers.size()) {
            timers.resize((index + 1), nullptr);
        }
        timers[index] = timer;
    }

    /** Stop and destroy logger */
    void stopTimerAt(size_t index) {
        assert(index < timers.size());
        delete timers[index];
        timers[index] = nullptr;
    }

    /** Get symbol table */
    SymbolTable& getSymbolTable() {
        return translationUnit.getSymbolTable();
    }

    /** Get Counter */
    int getCounter() {
        return counter;
    }

    /** Increment counter */
    int incCounter() {
        return counter++;
    }

    /** Increment iteration number */
    void incIterationNumber() {
        iteration++;
    }

    /** Get Iteration Number */
    size_t getIterationNumber() const {
        return iteration;
    }
    /** Reset iteration number */
    void resetIterationNumber() {
        iteration = 0;
    }

    /** Get relation */
    InterpreterRelation& getRelation(const std::string& name) {
        // look up relation
        auto pos = environment.find(name);
        assert(pos != environment.end());
        return *pos->second;
    }

    /** Drop relation */
    void dropRelation(const std::string& relName) {
        InterpreterRelation& rel = getRelation(relName);
        environment.erase(relName);
        delete &rel;
    }

    /** Swap relation */
    void swapRelation(const std::string& ramRel1, const std::string& ramRel2) {
        InterpreterRelation* rel1 = &getRelation(ramRel1);
        InterpreterRelation* rel2 = &getRelation(ramRel2);
        environment[ramRel1] = rel2;
        environment[ramRel2] = rel1;
    }

    /** Lookup IndexScan iterator, resize the iterator pool if necessary */
    std::pair<index_set::iterator, index_set::iterator>& lookUpIndexScanIterator(size_t idx) {
        if (idx >= indexScanIteratorPool.size()) {
            indexScanIteratorPool.resize(idx + 1);
        }
        return indexScanIteratorPool[idx];
    }

    /** Lookup Scan iterator, resize the iterator pool if necessary */
    std::pair<InterpreterRelation::iterator, InterpreterRelation::iterator>& lookUpScanIterator(size_t idx) {
        if (idx >= scanIteratorPool.size()) {
            scanIteratorPool.resize(idx + 1);
        }
        return scanIteratorPool[idx];
    }

    /** Lookup Choice iterator, resize the iterator pool if necessary */
    std::pair<InterpreterRelation::iterator, InterpreterRelation::iterator>& lookUpChoiceIterator(
            size_t idx) {
        if (idx >= choiceIteratorPool.size()) {
            choiceIteratorPool.resize(idx + 1);
        }
        return choiceIteratorPool[idx];
    }

    /** Lookup IndexChoice iterator, resize the iterator pool if necessary */
    std::pair<index_set::iterator, index_set::iterator>& lookUpIndexChoiceIterator(size_t idx) {
        if (idx >= indexChoiceIteratorPool.size()) {
            indexChoiceIteratorPool.resize(idx + 1);
        }
        return indexChoiceIteratorPool[idx];
    }

    /** Obtain the search columns */
    SearchSignature getSearchSignature(const std::string& patterns, size_t arity) {
        SearchSignature res = 0;
        for (size_t i = 0; i < arity; ++i) {
            if (patterns[i] == 'V') {
                res |= (1 << i);
            }
        }
        return res;
    }

private:
    friend InterpreterProgInterface;

    /** Execute given program
     *
     * @param ip the instruction pointer start position, default is 0.
     * */
    void execute(std::unique_ptr<LVMCode>& codeStream, InterpreterContext& ctxt, size_t ip = 0);

    /** subroutines */
    std::map<std::string, std::unique_ptr<LVMCode>> subroutines;

    /** Main program */
    std::unique_ptr<LVMCode> mainProgram = nullptr;

    /** counters for atom profiling */
    std::map<std::string, std::map<size_t, size_t>> frequencies;

    /** counters for non-existence check */
    std::map<std::string, std::atomic<size_t>> reads;

    /** List of iters for indexScan operation */
    std::vector<std::pair<index_set::iterator, index_set::iterator>> indexScanIteratorPool;

    /** List of iters for Scan operation */
    std::vector<std::pair<InterpreterRelation::iterator, InterpreterRelation::iterator>> scanIteratorPool;

    /** List of iters for indexChoice operation */
    std::vector<std::pair<index_set::iterator, index_set::iterator>> indexChoiceIteratorPool;

    /** List of iters for Choice operation */
    std::vector<std::pair<InterpreterRelation::iterator, InterpreterRelation::iterator>> choiceIteratorPool;

    /** stratum */
    size_t level = 0;

    /** List of loggers for logtimer */
    std::vector<Logger*> timers;

    /** counter for $ operator */
    int counter = 0;

    /** iteration number (in a fix-point calculation) */
    size_t iteration = 0;

    /** Dynamic library for user-defined functors */
    void* dll = nullptr;
};

}  // end of namespace souffle
