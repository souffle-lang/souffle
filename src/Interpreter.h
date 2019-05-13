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
            Global::config().set("libraries", "functors");
        }
        if (!Global::config().has("library-dir")) {
            Global::config().set("library-dir", ".");
        }
        for (const std::string& library : splitString(Global::config().get("libraries"), ' ')) {
            // The library may be blank
            if (library.empty()) {
                continue;
            }
            auto paths = splitString(Global::config().get("library-dir"), ' ');
            // Set up our paths to have a library appended
            for (std::string& path : paths) {
                if (path.back() != '/') {
                    path += '/';
                }
            }

            if (library.find('/') != std::string::npos) {
                paths.clear();
            }

            paths.push_back("");

            void* tmp = nullptr;
            for (const std::string& path : paths) {
                std::string fullpath = path + "lib" + library + ".so";
                tmp = dlopen(fullpath.c_str(), RTLD_LAZY);
                if (tmp != nullptr) {
                    dll.push_back(tmp);
                    break;
                }
            }
        }

        return dll;
    }

    void* getMethodHandle(const std::string& method) {
        // load DLLs (if not done yet)
        for (void* libHandle : loadDLL()) {
            auto* methodHandle = dlsym(libHandle, method.c_str());
            if (methodHandle != nullptr) {
                return methodHandle;
            }
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
