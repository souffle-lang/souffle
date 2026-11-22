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

#include "souffle/SouffleInterface.h"
#include <cstdint>
#include <iostream>
#include <string>

// Forward declarations
class SWIGRelation;
class SWIGTuple;
class SWIGRelationIterator;

/**
 * Wrapper class for souffle::tuple
 * Provides simple methods instead of C++ operator overloading
 */
class SWIGTuple {
    souffle::tuple* tuple;
    souffle::Relation* relation;
    bool owning;  // Whether we own the tuple memory

public:
    /**
     * Constructor for creating a new tuple for insertion
     */
    SWIGTuple(souffle::Relation* rel) : relation(rel), owning(true) {
        tuple = new souffle::tuple(rel);
    }

    /**
     * Constructor for wrapping an existing tuple (from iteration)
     */
    SWIGTuple(souffle::tuple* t, souffle::Relation* rel) : tuple(t), relation(rel), owning(true) {}

    ~SWIGTuple() {
        if (owning && tuple) {
            delete tuple;
        }
    }

    /**
     * Push a string value into the tuple
     */
    void putString(const std::string& val) {
        *tuple << val;
    }

    /**
     * Push a signed integer value into the tuple
     */
    void putInt(long val) {
        *tuple << static_cast<souffle::RamSigned>(val);
    }

    /**
     * Push an unsigned integer value into the tuple
     */
    void putUInt(unsigned long val) {
        *tuple << static_cast<souffle::RamUnsigned>(val);
    }

    /**
     * Push a float value into the tuple
     */
    void putFloat(double val) {
        *tuple << static_cast<souffle::RamFloat>(val);
    }

    /**
     * Get the next string value from the tuple
     */
    std::string getString() {
        std::string val;
        *tuple >> val;
        return val;
    }

    /**
     * Get the next signed integer value from the tuple
     */
    long getInt() {
        souffle::RamSigned val;
        *tuple >> val;
        return static_cast<long>(val);
    }

    /**
     * Get the next unsigned integer value from the tuple
     */
    unsigned long getUInt() {
        souffle::RamUnsigned val;
        *tuple >> val;
        return static_cast<unsigned long>(val);
    }

    /**
     * Get the next float value from the tuple
     */
    double getFloat() {
        souffle::RamFloat val;
        *tuple >> val;
        return static_cast<double>(val);
    }

    /**
     * Reset the read/write position to the beginning
     */
    void rewind() {
        tuple->rewind();
    }

    /**
     * Get the number of elements in the tuple
     */
    size_t size() {
        return tuple->size();
    }

    /**
     * Get the internal tuple pointer (for use by other wrapper classes)
     */
    souffle::tuple* getInternalTuple() {
        return tuple;
    }
};

/**
 * Iterator wrapper for iterating over relation tuples
 */
class SWIGRelationIterator {
    souffle::Relation::iterator current;
    souffle::Relation::iterator end;
    souffle::Relation* relation;

public:
    SWIGRelationIterator(souffle::Relation* rel)
            : current(rel->begin()), end(rel->end()), relation(rel) {}

    /**
     * Get the next tuple, or nullptr if iteration is complete
     */
    SWIGTuple* next() {
        if (current == end) {
            return nullptr;
        }
        // Create a copy of the current tuple
        souffle::tuple* t = new souffle::tuple(*current);
        ++current;
        return new SWIGTuple(t, relation);
    }

    /**
     * Check if there are more tuples to iterate
     */
    bool hasNext() {
        return current != end;
    }
};

/**
 * Wrapper class for souffle::Relation
 */
class SWIGRelation {
    souffle::Relation* relation;

public:
    SWIGRelation(souffle::Relation* rel) : relation(rel) {}

    /**
     * Create a new empty tuple for this relation
     */
    SWIGTuple* createTuple() {
        return new SWIGTuple(relation);
    }

    /**
     * Insert a tuple into the relation
     */
    void insert(SWIGTuple* tuple) {
        relation->insert(*tuple->getInternalTuple());
    }

    /**
     * Check if the relation contains the given tuple
     */
    bool contains(SWIGTuple* tuple) {
        return relation->contains(*tuple->getInternalTuple());
    }

    /**
     * Get an iterator for this relation
     */
    SWIGRelationIterator* iterator() {
        return new SWIGRelationIterator(relation);
    }

    /**
     * Get the name of the relation
     */
    std::string getName() {
        return relation->getName();
    }

    /**
     * Get the number of tuples in the relation
     */
    size_t size() {
        return relation->size();
    }

    /**
     * Get the arity (number of columns) of the relation
     */
    size_t getArity() {
        return relation->getArity();
    }

    /**
     * Get the type of an attribute at the given index
     * Returns: "s" (symbol), "i" (signed), "u" (unsigned), "f" (float), "r" (record), "+" (ADT)
     */
    std::string getAttrType(size_t index) {
        return std::string(relation->getAttrType(index));
    }

    /**
     * Get the name of an attribute at the given index
     */
    std::string getAttrName(size_t index) {
        return std::string(relation->getAttrName(index));
    }

    /**
     * Get the signature of the relation (e.g., "<s:Node,s:Node>")
     */
    std::string getSignature() {
        return relation->getSignature();
    }

    /**
     * Remove all tuples from the relation
     */
    void purge() {
        relation->purge();
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
     * Calls the corresponding method souffle::SouffleProgram::run in SouffleInterface.h
     */
    void run() {
        program->run();
    }

    /**
     * Calls the corresponding method souffle::SouffleProgram::runAll in SouffleInterface.h
     */
    void runAll(const std::string& inputDirectory, const std::string& outputDirectory) {
        program->runAll(inputDirectory, outputDirectory);
    }

    /**
     * Calls the corresponding method souffle::SouffleProgram::loadAll in SouffleInterface.h
     */
    void loadAll(const std::string& inputDirectory) {
        program->loadAll(inputDirectory);
    }

    /**
     * Calls the corresponding method souffle::SouffleProgram::printAll in SouffleInterface.h
     */
    void printAll(const std::string& outputDirectory) {
        program->printAll(outputDirectory);
    }

    /**
     * Calls the corresponding method souffle::SouffleProgram::dumpInputs in SouffleInterface.h
     */
    void dumpInputs() {
        program->dumpInputs();
    }

    /**
     * Calls the corresponding method souffle::SouffleProgram::dumpOutputs in SouffleInterface.h
     */
    void dumpOutputs() {
        program->dumpOutputs();
    }

    /**
     * Get a relation by name
     * @param name The name of the relation
     * @return A SWIGRelation wrapper, or nullptr if not found
     */
    SWIGRelation* getRelation(const std::string& name) {
        souffle::Relation* rel = program->getRelation(name);
        if (rel == nullptr) {
            return nullptr;
        }
        return new SWIGRelation(rel);
    }

    /**
     * Set the number of threads to use for parallel execution
     */
    void setNumThreads(size_t num) {
        program->setNumThreads(num);
    }

    /**
     * Get the number of threads used for parallel execution
     */
    size_t getNumThreads() {
        return program->getNumThreads();
    }
};

/**
 * Creates an instance of a SWIG souffle::SouffleProgram that can be called within a program of a supported
 * language for the SWIG option specified in main.cpp. This enables the program to use this instance and call
 * the supported souffle::SouffleProgram methods.
 * @param name Name of the datalog file/ instance to be created
 */
SWIGSouffleProgram* newInstance(const std::string& name) {
    auto* prog = souffle::ProgramFactory::newInstance(name);
    return new SWIGSouffleProgram(prog);
}
