/*
* Souffle - A Datalog Compiler
* Copyright (c) 2016, The Souffle Developers. All rights reserved
* Licensed under the Universal Permissive License v 1.0 as shown at:
* - https://opensource.org/licenses/UPL
* - <souffle root>/licenses/SOUFFLE-UPL.txt
*/

#pragma once

#include "SouffleInterface.h"
#include "RamRelation.h"

namespace souffle {

class RamRelationInterface : public Relation {
private:
    RamRelation* ramRelation;
    SymbolTable& symTable;
    std::string name;

protected:
    class iterator_base : public Relation::iterator_base {
    private:
        RamRelationInterface* ramRelationInterface;
        RamRelation::iterator it;
        tuple tup;
    public:
        iterator_base(uint32_t arg_id, RamRelationInterface* r, RamRelation::iterator i)
                : Relation::iterator_base(arg_id), ramRelationInterface(r), it(i), tup(r) {}
        ~iterator_base();

        void operator++();
        tuple& operator*();
        iterator_base* clone() const;

    protected:
        bool equal(const Relation::iterator_base& o) const;
    };

public:
    RamRelationInterface(RamRelation* r, SymbolTable& s, std::string n) : ramRelation(r), symTable(s), name(n) {}
    ~RamRelationInterface();

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

} // end of namespace souffle
