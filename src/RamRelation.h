/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamTable.h
 *
 * Defines classes Table, TupleBlock, Index, and HashBlock for implementing
 * an ram relational database. A table consists of a linked list of
 * tuple blocks that contain tuples of the table. An index is a hash-index
 * whose hash table is stored in Index. The entry of a hash table entry 
 * refer to HashBlocks that are blocks of pointers that point to tuples 
 * in tuple blocks with the same hash.  
 *
 ***********************************************************************/

#pragma once

#include <string>
#include <map>
#include <pthread.h>

#include "Util.h"
#include "RamTypes.h"
#include "RamIndex.h"
#include "Table.h"
#include "SymbolTable.h"

namespace souffle {

// forward declaration
class RamEnvironment;
class RamRelation;


class RamRelationIdentifier {

    std::string name;
    unsigned arity;
    std::vector<std::string> attributeNames;
    std::vector<std::string> attributeTypeQualifiers;
    bool input;
    bool nullary;
    bool computed;
    bool output;

    // allow the ram environment to cache lookup results
    friend class RamEnvironment;
    mutable const RamEnvironment* last;
    mutable RamRelation* rel;

public:

    RamRelationIdentifier() : arity(0), input(false), nullary(false), computed(false), output(false), last(nullptr), rel(nullptr) {
      //std::cout << "created empty non nullary\n";
    }

    RamRelationIdentifier(const std::string& name, unsigned arity,
            std::vector<std::string> attributeNames = {},
            std::vector<std::string> attributeTypeQualifiers = {},
            bool input = false, bool nullary = false, bool computed = false, bool output = false)
        : name(name), arity(arity), attributeNames(attributeNames), attributeTypeQualifiers(attributeTypeQualifiers),
          input(input), nullary(nullary), computed(computed), output(output), last(nullptr), rel(nullptr)  {
        //nullary ? std::cout << "created nullary :: name = "<< name << "\n" :  std::cout << "created non nullary :: name = " << name << "\n";
        assert(this->attributeNames.size() == arity || this->attributeNames.empty());
        assert(this->attributeTypeQualifiers.size() == arity || this->attributeTypeQualifiers.empty());
    }

    const std::string& getName() const {
        return name;
    }
 
    const char getNullValue() const {
        return '0';
    }

    const std::string getArg(uint32_t i) const {
       if(!attributeNames.empty()) {
           return attributeNames[i];
       } else {
           return "c"+std::to_string(i); 
       }
    }

    const std::string getArgTypeQualifier(uint32_t i) const {
        if (!attributeTypeQualifiers.empty()) {
            return attributeTypeQualifiers[i];
        } else {
            //assert(0 && "has no type qualifiers");
            return "";
        }
    }

    bool isInput() const {
        return input;
    }

    bool isNullary() const { 
        return nullary; 
    }

    bool isComputed() const {
        return computed;
    }

    bool isOutput() const {
        return output;
    }

    unsigned getArity() const {
        return arity;
    }

    bool operator==(const RamRelationIdentifier& other) const {
        return name == other.name;
    }

    bool operator!=(const RamRelationIdentifier& other) const {
        return name != other.name;
    }

    bool operator<(const RamRelationIdentifier& other) const {
        return name < other.name;
    }

    void print(std::ostream& out) const {
        out << name << "(";
        out << getArg(0);
        for(unsigned i=1;i<arity;i++) {
            out << ",";
            out << getArg(i);
        }
        out << ")";
    }

    friend std::ostream& operator<<(std::ostream& out, const RamRelationIdentifier &rel) {
        rel.print(out);
        return out; 
    }

};


class SymbolMask {

    std::vector<bool> mask;

public:

    SymbolMask(size_t arity) : mask(arity) {}

    size_t getArity() const {
        return mask.size();
    }

    bool isSymbol(size_t index) const {
        return index < getArity() && mask[index];
    }

    void setSymbol(size_t index, bool value = true) {
        mask[index] = value;
    }

    void print(std::ostream& out) const {
        out << mask << "\n";
    }

    friend std::ostream& operator<<(std::ostream& out, const SymbolMask& mask) {
        mask.print(out);
        return out;
    }

};


class RamRelation {

    static const int BLOCK_SIZE = 1024;

    struct Block {

        size_t size;
        size_t used;
        std::unique_ptr<Block> next;
        std::unique_ptr<RamDomain[]> data;

        Block(size_t s = BLOCK_SIZE) : size(s), used(0), next(nullptr), data(new RamDomain[size]) { }

        size_t getFreeSpace() const {
            return size - used;
        }
    };


    /** The name / arity of this relation */
    RamRelationIdentifier id;

    size_t num_tuples;

    std::unique_ptr<Block> head;
    Block* tail;

    mutable std::map<RamIndexOrder, std::unique_ptr<RamIndex>> indices;

    mutable RamIndex* totalIndex;

    /** lock for parallel execution */
    mutable pthread_mutex_t lock;

public:
    using SymbolTable = souffle::SymbolTable; // XXX pending namespace cleanup

    RamRelation(const RamRelationIdentifier& id)
        : id(id), num_tuples(0), head(std::unique_ptr<Block>(new Block())), tail(head.get()), totalIndex(nullptr) {
        pthread_mutex_init(&lock, NULL);
    }

    RamRelation(const RamRelation& other) = delete;

    RamRelation(RamRelation&& other)
        : id(other.id), num_tuples(other.num_tuples), tail(other.tail), totalIndex(other.totalIndex) {
        pthread_mutex_init(&lock, NULL);

        // take over ownership
        head.swap(other.head);
        indices.swap(other.indices);
    }

    ~RamRelation() { }

    RamRelation& operator=(const RamRelation& other) = delete;
    RamRelation& operator=(RamRelation&& other) = delete;

    /** Obtains the identifier of this relation */
    const RamRelationIdentifier& getID() const {
        return id;
    }

    /** get name of relation */
    const std::string& getName() const {
        return id.getName();
    }

    /** get arity of relation */
    size_t getArity() const {
        return id.getArity();
    }

    /** determines whether this relation is empty */
    bool empty() const {
        return num_tuples == 0;
    }

    /** Gets the number of contained tuples */
    size_t size() const {
        return num_tuples;
    }

    /** insert a new tuple to table */
    void insert(const RamDomain *tuple) {
        // make existence check
        if (exists(tuple)) return;

        // prepare tail
        auto arity = getArity();

        if (tail->getFreeSpace() < arity || arity == 0) {
            tail->next = std::unique_ptr<Block>(new Block());
            tail = tail->next.get();
        }

        // insert element into tail
        RamDomain* newTuple = &tail->data[tail->used];
        for(size_t i=0; i<arity; ++i) newTuple[i] = tuple[i];
        tail->used += arity;

        // update all indexes with new tuple
        for(const auto& cur : indices) {
            cur.second->insert(newTuple);
        }

        // increment relation size
        num_tuples++;
    }

    /** a convenience function for inserting tuples */
    template<typename ... Args>
    void insert(RamDomain first, Args ... rest) {
        RamDomain tuple[] = {first, RamDomain(rest)...};
        insert(tuple);
    }

    /** Merges all elements of the given relation into this relation */
    void insert(const RamRelation& other) {
        assert(getArity() == other.getArity());
        for(const auto& cur : other) {
            insert(cur);
        }
    }

    /** purge table */
    void purge() {
        std::unique_ptr<Block> newHead(new Block());
        head.swap(newHead);
        tail = head.get();
        for(const auto& cur : indices) {
            cur.second->purge();
        }
        num_tuples = 0;
    }

    /** get index for a given set of keys. Keys are encoded as bits for each column */
    RamIndex* getIndex(SearchColumns key) const {

        // convert to order
        RamIndexOrder order;
        for(size_t k=1,i=0; i<getArity(); i++,k*=2) {
            if(key & k) {
                order.append(i);
            }
        }

        // see whether there is an order with a matching prefix
        RamIndex* res = nullptr;
        pthread_mutex_lock(&lock);
        for(auto it = indices.begin(); !res && it != indices.end(); ++it) {
            if (order.isCompatible(it->first)) res = it->second.get();
        }
        pthread_mutex_unlock(&lock);

        // if found, use compatible index
        if (res) return res;

        // extend index to full index
        for(size_t i=0; i<getArity(); i++) {
            if (!order.covers(i)) order.append(i);
        }
        assert(order.isComplete());

        // get a new index
        return getIndex(order);
    }

    /** get index for a given order. Keys are encoded as bits for each column */
    RamIndex* getIndex(const RamIndexOrder& order) const {
        // TODO: improve index usage by re-using indices with common prefix
        RamIndex* res;
        pthread_mutex_lock(&lock);
        auto pos = indices.find(order);
        if (pos == indices.end()) {
            std::unique_ptr<RamIndex> &newIndex = indices[order];
            newIndex = std::unique_ptr<RamIndex>(new RamIndex(order));
            for(const auto& cur : *this) newIndex->insert(cur);
            res = newIndex.get();
        }  else {
            res = pos->second.get();
        }
        pthread_mutex_unlock(&lock);
        return res;
    }

    /** Obtains a full index-key for this relation */
    SearchColumns getTotalIndexKey() const {
        return (1 << (getArity()))-1;
    }

    /** check whether a tuple exists in the relation */
    bool exists(const RamDomain* tuple) const {
        if (!totalIndex)
            totalIndex = getIndex(getTotalIndexKey());
        return totalIndex->exists(tuple);
    }

    /** input table in csv format from file */ 
    bool load(std::istream &is, SymbolTable& symTable, const SymbolMask& mask);

    /** print table in csv formt to file */
    void store(std::ostream &os, const SymbolTable& symTable, const SymbolMask& mask) const;


    // --- iterator ---

    /** Iterator for relation */ 
    class iterator : public std::iterator<std::forward_iterator_tag,RamDomain*> {

        Block *cur;
        RamDomain *tuple;
        size_t arity; 

    public: 

        iterator()
            : cur(nullptr), tuple(nullptr), arity(0) { }

        iterator(Block *c, RamDomain *t, size_t a)
            : cur(c), tuple(t), arity(a) {}

        const RamDomain *operator*() {
            return tuple; 
        } 

        bool operator==(const iterator& other) const {
            return tuple == other.tuple;
        }

        bool operator!=(const iterator &other) const {
            return (tuple != other.tuple);
        }

        iterator& operator++() {
            if (!cur) return *this;
            tuple += arity;
            if (tuple >= &cur->data[cur->used]) {
                cur = cur->next.get();
                tuple = (cur) ? cur->data.get() : nullptr;
            }
            return *this;
        }
    };

    /** get iterator begin of relation */
    inline iterator begin() const {
        if (empty()) return end();
        if(getArity() == 0 && size() == 1)
          return iterator(head.get(), &head->data[0], 1);
        else
          return iterator(head.get(), &head->data[0], getArity());
    }

    /** get iterator begin of relation */ 
    inline iterator end() const {
        return iterator(); 
    }
};

/**
 * An environment encapsulates all the context information required for
 * processing a RAM program.
 */
class RamEnvironment {
    using SymbolTable = souffle::SymbolTable;

    /** The type utilized for storing relations */
    typedef std::map<std::string, RamRelation> relation_map;

    /** The symbol table to be utilized by an evaluation */
    SymbolTable& symbolTable;

    /** The relations manipulated by a ram program */
    relation_map data;

    /** The increment counter utilized by some RAM language constructs */
    int counter;

public:

    RamEnvironment(SymbolTable& symbolTable)
        : symbolTable(symbolTable), counter(0) {}

    /**
     * Obtains a reference to the enclosed symbol table.
     */
    SymbolTable& getSymbolTable() {
        return symbolTable;
    }

    /**
     * Obtains the current value of the internal counter.
     */
    int getCounter() const {
        return counter;
    }

    /**
     * Increments the internal counter and obtains the
     * old value.
     */
    int incCounter() {
        return counter++;
    }

    /**
     * Obtains a mutable reference to one of the relations maintained
     * by this environment. If the addressed relation does not exist,
     * a new, empty relation will be created.
     */
    RamRelation& getRelation(const RamRelationIdentifier& id) {
        // use cached value
        if (id.last == this) return *id.rel;

        RamRelation* res = nullptr;
        auto pos = data.find(id.getName());
        if (pos != data.end()) {
            res = &(pos->second);
        } else {
            res = &(data.emplace(id.getName(), id).first->second);
        }

        // cache result
        id.last = this;
        id.rel = res;

        // return result
        return *res;
    }

    /**
     * Obtains an immutable reference to the relation identified by
     * the given identifier. If no such relation exist, a reference
     * to an empty relation will be returned (not exhibiting the proper
     * id, but the correct content).
     */
    const RamRelation& getRelation(const RamRelationIdentifier& id) const {

        // use cached value if available
        if (id.last == this) return *id.rel;

        // look up relation
        auto pos = data.find(id.getName());
        assert(pos != data.end());

        // cache result
        id.last = this;
        id.rel = const_cast<RamRelation*>(&(pos->second));
        return pos->second;
    }

    /**
     * Obtains an immutable reference to the relation identified by
     * the given identifier. If no such relation exist, a reference
     * to an empty relation will be returned (not exhibiting the proper
     * id, but the correct content).
     */
    const RamRelation& getRelation(const std::string& name) const {
        auto pos = data.find(name);
        assert(pos != data.end());
        return pos->second;
    }

    /**
     * Tests whether a relation with the given name is present.
     */
    bool hasRelation(const std::string& name) const {
        return data.find(name) != data.end();
    }

    /**
     * Deletes the referenced relation from this environment.
     */
    void dropRelation(const RamRelationIdentifier& id) {
        data.erase(id.getName());
        id.last = nullptr;
    }

};

} // end of namespace souffle

