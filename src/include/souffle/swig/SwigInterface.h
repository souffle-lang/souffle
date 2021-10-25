/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SwigInterface.h
 *
 * Header file for SWIG to invoke functions in souffle::SouffleProgram
 *
 ***********************************************************************/

#pragma once

#include "souffle/RamTypes.h"
#include "souffle/SouffleInterface.h"
#include <iostream>
#include <string>

#if RAM_DOMAIN_SIZE == 64
using S_int = long long;
using S_uint = unsigned long long;
using S_float = double;
#else
using S_int = long;
using S_uint = unsigned long;
using S_float = float;
#endif

class SWIGSouffleTuple {
    souffle::tuple* t;
    souffle::Relation* relation;

public:
    SWIGSouffleTuple(souffle::tuple* t, souffle::Relation* r) : t(t), relation(r) {}

    std::string getString(unsigned int i) {
        (*t).rewind();
        std::string result;
        for (unsigned int j = 0; j <= i; j++) {
            (*t) >> result;
        }
        return result;
    }

    void putString(std::string s) {
        (*t) << s;
    }

    S_int getInteger(unsigned int i) {
        (*t).rewind();
        souffle::RamDomain result;
        for (unsigned int j = 0; j <= i; j++) {
            (*t) >> result;
        }
        return (S_int)result;
    }

    void putInteger(S_int i) {
        (*t) << (souffle::RamSigned)i;
    }

    S_uint getUnsigned(unsigned int i) {
        (*t).rewind();
        souffle::RamUnsigned result;
        for (unsigned int j = 0; j <= i; j++) {
            (*t) >> result;
        }
        return (S_uint)result;
    }

    void putUnsigned(S_uint i) {
        (*t) << (souffle::RamUnsigned)i;
    }

    S_float getFloat(unsigned int i) {
        (*t).rewind();
        souffle::RamFloat result;
        for (unsigned int j = 0; j <= i; j++) {
            (*t) >> result;
        }
        return (double)result;
    }

    void putFloat(S_float i) {
        (*t) << (souffle::RamFloat)i;
    }

    void insert() {
        relation->insert(*t);
    }
};

class SWIGSouffleRelation {
private:
    souffle::Relation* relation;
    souffle::Relation::iterator iter;
    souffle::Relation::iterator end;

public:
    SWIGSouffleRelation(souffle::Relation* relation) : relation(relation) {
        iter = relation->begin();
        end = relation->end();
    }

    SWIGSouffleTuple* newTuple() {
        return new SWIGSouffleTuple(new souffle::tuple(relation), relation);
    }

    SWIGSouffleTuple* next() {
        SWIGSouffleTuple* t = new SWIGSouffleTuple(&(*iter), relation);
        if (iter != end) {
            iter++;
        }
        return t;
    }

    void purge() {
        relation->purge();
    }

    std::string getSignature() {
        return relation->getSignature();
    }

    unsigned int size() {
        return relation->size();
    }

    unsigned int getArity() {
        return relation->getArity();
    }
};

/**
 * Abstract base class for generated Datalog programs
 */
class SWIGSouffleProgram {
    /**
     * pointer to SouffleProgram to invoke functions from SouffleInterface.h
     */
    souffle::SouffleProgram* program;

public:
    SWIGSouffleProgram(souffle::SouffleProgram* program) : program(program) {}

    virtual ~SWIGSouffleProgram() {
        delete program;
    }

    /**
     * Calls the corresponding method souffle::SouffleProgram::run in
     * SouffleInterface.h
     */
    void run() {
        program->run();
    }

    /**
     * Calls the corresponding method souffle::SouffleProgram::runAll in
     * SouffleInterface.h
     */
    void runAll(const std::string& inputDirectory, const std::string& outputDirectory) {
        program->runAll(inputDirectory, outputDirectory);
    }

    /**
     * Calls the corresponding method souffle::SouffleProgram::loadAll in
     * SouffleInterface.h
     */
    void loadAll(const std::string& inputDirectory) {
        program->loadAll(inputDirectory);
    }

    /**
     * Calls the corresponding method souffle::SouffleProgram::printAll in
     * SouffleInterface.h
     */
    void printAll(const std::string& outputDirectory) {
        program->printAll(outputDirectory);
    }

    /**
     * Calls the corresponding method souffle::SouffleProgram::dumpInputs in
     * SouffleInterface.h
     */
    void dumpInputs() {
        program->dumpInputs();
    }

    /**
     * Calls the corresponding method souffle::SouffleProgram::dumpOutputs in
     * SouffleInterface.h
     */
    void dumpOutputs() {
        program->dumpOutputs();
    }

    /**
     * Set the number of threads to be used
     */
    void setNumThreads(std::size_t numThreadsValue) {
        program->setNumThreads(numThreadsValue);
    }

    /**
     * Get the number of threads to be used
     */
    std::size_t getNumThreads() {
        return program->getNumThreads();
    }

    SWIGSouffleRelation* getRelation(const std::string& relationName) {
        auto* rel = program->getRelation(relationName);
        return new SWIGSouffleRelation(rel);
    }
};

/**
 * Creates an instance of a SWIG souffle::SouffleProgram that can be called
 * within a program of a supported language for the SWIG option specified in
 * main.cpp. This enables the program to use this instance and call the
 * supported souffle::SouffleProgram methods.
 * @param name Name of the datalog file/ instance to be created
 */
SWIGSouffleProgram* newInstance(const std::string& name) {
    auto* prog = souffle::ProgramFactory::newInstance(name);
    if (!prog) {
        return NULL;
    }
    return new SWIGSouffleProgram(prog);
}
