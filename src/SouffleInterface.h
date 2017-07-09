/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file CompiledSouffle.h
 *
 * Main include file for generated C++ classes of Souffle
 *
 ***********************************************************************/

#pragma once

#include "RamTypes.h"
#include "SymbolTable.h"

#include <initializer_list>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <assert.h>

namespace souffle {

class tuple;

/**
 * Object-oriented wrapper class for Souffle's templatized relations
 */
class Relation {
protected:
    // abstract iterator class
    class iterator_base {
    protected:
        // required for identifying type of iterator
        // (NB: LLVM has no typeinfo)
        uint32_t id;

    public:
        virtual uint32_t getId() const {
            return id;
        }
        iterator_base(uint32_t arg_id) : id(arg_id) {}
        virtual ~iterator_base() {}
        virtual void operator++() = 0;
        virtual tuple& operator*() = 0;
        bool operator==(const iterator_base& o) const {
            return this->getId() == o.getId() && equal(o);
        }
        virtual iterator_base* clone() const = 0;

    protected:
        virtual bool equal(const iterator_base& o) const = 0;
    };

public:
    virtual ~Relation() {}

    // wrapper class for abstract iterator
    class iterator {
    protected:
        iterator_base* iter;

    public:
        iterator() : iter(nullptr) {}
        iterator(iterator_base* arg) : iter(arg) {}
        ~iterator() {
            delete iter;
        }
        iterator(const iterator& o) : iter(o.iter->clone()) {}
        iterator& operator=(const iterator& o) {
            delete iter;
            iter = o.iter->clone();
            return *this;
        }
        iterator& operator++() {
            ++(*iter);
            return *this;
        }
        tuple& operator*() const {
            return *(*iter);
        }
        bool operator==(const iterator& o) const {
            return (iter == o.iter) || (*iter == *o.iter);
        }
        bool operator!=(const iterator& o) const {
            return !(*this == o);
        }
    };

    // insert a new tuple into the relation
    virtual void insert(const tuple& t) = 0;

    // check whether a tuple exists in the relation
    virtual bool contains(const tuple& t) const = 0;

    // begin and end iterator
    virtual iterator begin() = 0;
    virtual iterator end() = 0;

    // number of tuples in relation
    virtual std::size_t size() = 0;

    // properties
    virtual bool isOutput() const = 0;
    virtual bool isInput() const = 0;
    virtual std::string getName() const = 0;
    virtual const char* getAttrType(size_t) const = 0;
    virtual const char* getAttrName(size_t) const = 0;
    virtual size_t getArity() const = 0;
    virtual SymbolTable& getSymbolTable() const = 0;
    std::string getSignature() {
        std::string signature = "<" + std::string(getAttrType(0));
        for (size_t i = 1; i < getArity(); i++) {
            signature += "," + std::string(getAttrType(i));
        }
        signature += ">";
        return signature;
    }
};

/**
 * Defines a tuple for the OO interface such that
 * relations with varying columns can be accessed.
 */
class tuple {
    Relation& relation;
    std::vector<RamDomain> array;
    size_t pos;

public:
    tuple(Relation* r) : relation(*r), array(r->getArity()), pos(0) {}
    tuple(const tuple& t) : relation(t.relation), array(t.array), pos(t.pos) {}

    /**
     * return number of elements in the tuple
     */
    size_t size() const {
        return array.size();
    }

    /**
     * direct access to tuple elements via index
     * TODO: this interface should be hidden and
     * only be used by friendly classes such as
     * iterators; users should not use this interface.
     */
    RamDomain& operator[](size_t idx) {
        return array[idx];
    }

    const RamDomain& operator[](size_t idx) const {
        return array[idx];
    }

    /**
     * reset stream pointer to first element of tuple
     */
    void rewind() {
        pos = 0;
    }

    /**
     * place a symbol into the current element of the tuple
     */
    tuple& operator<<(const std::string& str) {
        assert(pos < size() && "exceeded tuple's size");
        assert(*relation.getAttrType(pos) == 's' && "wrong element type");
        array[pos++] = relation.getSymbolTable().lookup(str.c_str());
        return *this;
    }

    /**
     * place a number into the current element of the tuple
     */
    tuple& operator<<(RamDomain number) {
        assert(pos < size() && "exceeded tuple's size");
        assert((*relation.getAttrType(pos) == 'i' || *relation.getAttrType(pos) == 'r') &&
                "wrong element type");
        array[pos++] = number;
        return *this;
    }

    /**
     * read a symbol from the tuple
     */
    tuple& operator>>(std::string& str) {
        assert(pos < size() && "exceeded tuple's size");
        assert(*relation.getAttrType(pos) == 's' && "wrong element type");
        str = relation.getSymbolTable().resolve(array[pos++]);
        return *this;
    }

    /**
     * read a number from the tuple
     */
    tuple& operator>>(RamDomain& number) {
        assert(pos < size() && "exceeded tuple's size");
        assert((*relation.getAttrType(pos) == 'i' || *relation.getAttrType(pos) == 'r') &&
                "wrong element type");
        number = array[pos++];
        return *this;
    }

    /**
     * (insert) iterator for direct access to tuple's data (experimental)
     */
    decltype(array)::iterator begin() {
        return array.begin();
    }

    /**
     * direct constructor using initialization list (experimental)
     */
    tuple(Relation* r, std::initializer_list<RamDomain> il) : relation(*r), array(il), pos(il.size()) {
        assert(il.size() == r->getArity() && "wrong tuple arity");
    }
};

/**
 * Abstract base class for generated Datalog programs
 */
class SouffleProgram {
private:
    // define a relation map for external access
    std::map<std::string, Relation*> relationMap;
    std::vector<Relation*> inputRelations;
    std::vector<Relation*> outputRelations;
    std::vector<Relation*> internalRelations;

protected:
    // add relation to relation map
    void addRelation(const std::string& name, Relation* rel, bool isInput, bool isOutput) {
        relationMap[name] = rel;
        if (isInput) {
            inputRelations.push_back(rel);
        }
        if (isOutput) {
            outputRelations.push_back(rel);
        }
        if (!isInput && !isOutput) {
            internalRelations.push_back(rel);
        }
    }

public:
    virtual ~SouffleProgram() {}

    // execute Datalog program
    virtual void run() = 0;

    // load all relations
    virtual void loadAll(std::string dirname = ".") = 0;

    // print all relations
    virtual void printAll(std::string dirname = ".") = 0;

    // print input relations (for debug purposes)
    virtual void dumpInputs(std::ostream& out = std::cout) = 0;

    // print output relations (for debug purposes)
    virtual void dumpOutputs(std::ostream& out = std::cout) = 0;

    // get Relation
    Relation* getRelation(const std::string& name) const {
        auto it = relationMap.find(name);
        if (it != relationMap.end()) {
            return (*it).second;
        } else {
            return nullptr;
        }
    };

    std::vector<Relation*> getOutputRelations() const {
        return outputRelations;
    }

    std::vector<Relation*> getInputRelations() const {
        return inputRelations;
    }

    std::vector<Relation*> getInternalRelations() const {
        return internalRelations;
    }

    std::vector<Relation*> getAllRelations() const {
        std::vector<Relation*> allRelations;
        allRelations.insert(allRelations.end(), inputRelations.begin(), inputRelations.end());
        allRelations.insert(allRelations.end(), internalRelations.begin(), internalRelations.end());
        allRelations.insert(allRelations.end(), outputRelations.begin(), outputRelations.end());
        return allRelations;
    }

    virtual const SymbolTable& getSymbolTable() const = 0;
};

/**
 * Abstract program factory class
 */
class ProgramFactory {
protected:
    // simply linked-list to store all program factories
    // Note that STL data-structures are not possible due
    // to static initialization order fiasco. The static
    // container needs to be a primitive type such as pointer
    // set to NULL.
    ProgramFactory* link;  // link to next factory
    std::string name;      // name of factory

protected:
    /**
     * Constructor adds factory to static singly-linked list
     * for registration.
     */
    ProgramFactory(const std::string& name) : name(name) {
        registerFactory(this);
    }

private:
    static inline std::map<std::string, ProgramFactory*>& getFactoryRegistry() {
        static std::map<std::string, ProgramFactory*> factoryReg;
        return factoryReg;
    }

protected:
    static inline void registerFactory(ProgramFactory* factory) {
        auto& entry = getFactoryRegistry()[factory->name];
        assert(!entry && "double-linked/defined souffle analyis");
        entry = factory;
    }

    /**
     * Find a factory by its name
     */
    static inline ProgramFactory* find(const std::string& factoryName) {
        const auto& reg = getFactoryRegistry();
        auto pos = reg.find(factoryName);
        return (pos == reg.end()) ? nullptr : pos->second;
    }

    /**
     * Create new instance (abstract)
     */
    virtual SouffleProgram* newInstance() = 0;

public:
    virtual ~ProgramFactory() {}

    /**
     * Create instance
     */
    static SouffleProgram* newInstance(const std::string& name) {
        ProgramFactory* factory = find(name);
        if (factory != nullptr) {
            return factory->newInstance();
        } else {
            return nullptr;
        }
    }
};
}
