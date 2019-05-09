/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
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
 * Interpreter Interface
 */
class Interpreter {
public:
    Interpreter(RamTranslationUnit& tUnit) : translationUnit(tUnit) {}

    virtual ~Interpreter() {
        for (auto& x : environment) {
            delete x.second;
        }
    }

    /** Get translation unit */
    RamTranslationUnit& getTranslationUnit() {
        return translationUnit;
    }

    /** Interface for executing the main program */
    virtual void executeMain() = 0;

    /** Execute the subroutine */
    virtual void executeSubroutine(const std::string& name, const std::vector<RamDomain>& arguments,
            std::vector<RamDomain>& returnValues, std::vector<bool>& returnErrors) = 0;

protected:
    /** Load dll */
    const std::vector<void*>& loadDLL() {
        if (!dll.empty()) {
            return dll;
        }

        if (!Global::config().has("libraries")) {
            Global::config().set("libraries", SOUFFLE_DLL);
        }
        std::cout << "libraries: <" << Global::config().get("libraries") << ">" << std::endl;
        for (const std::string& library : splitString(Global::config().get("libraries"), ',')) {
            auto tmp = dlopen(library.c_str(), RTLD_LAZY);
            std::cout << tmp << std::endl;
            dll.push_back(tmp);
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

    friend InterpreterProgInterface;

    /** relation environment type */
    using relation_map = std::map<std::string, InterpreterRelation*>;

    /** Get symbol table */
    SymbolTable& getSymbolTable() {
        return translationUnit.getSymbolTable();
    }

    /** Get relation map */
    relation_map& getRelationMap() const {
        return const_cast<relation_map&>(environment);
    }

    /** RAM translation Unit */
    RamTranslationUnit& translationUnit;

    /** Relation Environment */
    relation_map environment;

    /** Dynamic library for user-defined functors */
    std::vector<void*> dll;
};

}  // end of namespace souffle
