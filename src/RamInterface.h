/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamInterface.h
 *
 * Defines classes that implement the SouffleInterface abstract class
 *
 ***********************************************************************/

#pragma once

#include "RamRelation.h"
#include "SouffleInterface.h"

#include <array>

namespace souffle {

class RamRelationInterface : public Relation {
private:
    RamRelation& ramRelation;
    SymbolTable& symTable;
    std::string name;
    std::vector<std::string> types;
    std::vector<std::string> attrNames;
    uint32_t id;

protected:
    class iterator_base : public Relation::iterator_base {
    private:
        RamRelationInterface* ramRelationInterface;
        RamRelation::iterator it;
        tuple tup;

    public:
        iterator_base(uint32_t arg_id, RamRelationInterface* r, RamRelation::iterator i)
                : Relation::iterator_base(arg_id), ramRelationInterface(r), it(i), tup(r) {}
        virtual ~iterator_base() {}

        // increments iterator to next relation
        void operator++();

        // gets relation pointed to by iterator
        tuple& operator*();

        iterator_base* clone() const;

    protected:
        bool equal(const Relation::iterator_base& o) const;
    };

public:
    RamRelationInterface(RamRelation& r, SymbolTable& s, std::string n, std::vector<std::string> t,
            std::vector<std::string> an, uint32_t i)
            : ramRelation(r), symTable(s), name(n), types(t), attrNames(an), id(i) {}
    virtual ~RamRelationInterface() {}

    // insert a new tuple into the relation
    void insert(const tuple& t);

    // check whether a tuple exists in the relation
    bool contains(const tuple& t) const;

    // begin and end iterator
    iterator begin();
    iterator end();

    // number of tuples in relation
    std::size_t size();

    // properties
    bool isOutput() const;
    bool isInput() const;
    std::string getName() const;
    const char* getAttrType(size_t) const;
    const char* getAttrName(size_t) const;
    size_t getArity() const;
    SymbolTable& getSymbolTable() const;
};

/**
 * Implementation of SouffleProgram interface for ram programs
 */
class SouffleInterpreterInterface : public SouffleProgram {
private:
    RamEnvironment& env;
    SymbolTable& symTable;

public:
    SouffleInterpreterInterface(RamEnvironment& r, SymbolTable& s) : env(r), symTable(s) {
        uint32_t id = 0;
        for (auto& rel_pair : r.getRelationMap()) {
            auto& rel = rel_pair.second;

            // construct types and names vectors
            std::vector<std::string> types;
            std::vector<std::string> attrNames;
            for (size_t i = 0; i < rel.getArity(); i++) {
                std::string t = rel.getID().getArgTypeQualifier(i);
                types.push_back(t);

                std::string n = rel.getID().getArg(i);
                attrNames.push_back(n);
            }
            addRelation(rel.getID().getName(),
                    new RamRelationInterface(rel, symTable, rel.getID().getName(), types, attrNames, id),
                    rel.getID().isInput(), rel.getID().isOutput());
            id++;
        }
    }
    virtual ~SouffleInterpreterInterface() {}

    // running an interpreter program doesn't make sense
    void run() {
        std::cerr << "Cannot run interpreter program" << std::endl;
    }

    // loading facts for interpreter program doesn't make sense
    void loadAll(std::string dirname = ".") {
        std::cerr << "Cannot load facts for interpreter program" << std::endl;
    }

    // print methods
    void printAll(std::string) {}
    void dumpInputs(std::ostream&) {}
    void dumpOutputs(std::ostream&) {}

    const SymbolTable& getSymbolTable() const {
        return symTable;
    }
};

}  // end of namespace souffle
