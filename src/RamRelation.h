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

#include "IODirectives.h"
#include "RamIndex.h"
#include "RamTypes.h"
#include "SymbolMask.h"
#include "SymbolTable.h"
#include "Table.h"
#include "Util.h"

#include <list>
#include <map>
#include <string>
#include <pthread.h>

namespace souffle {

// forward declaration
class RamEnvironment;
class RamRelation;

class RamRelationIdentifier {
    std::string name;
    unsigned arity;
    std::vector<std::string> attributeNames;
    std::vector<std::string> attributeTypeQualifiers;
    SymbolMask mask;
    bool input;
    bool computed;
    bool output;
    bool btree;
    bool brie;
    bool eqrel;

    bool isdata;
    bool istemp;
    IODirectives inputDirectives;
    std::vector<IODirectives> outputDirectives;

    // allow the ram environment to cache lookup results
    friend class RamEnvironment;
    mutable const RamEnvironment* last;
    mutable RamRelation* rel;

public:
    RamRelationIdentifier()
            : arity(0), mask(arity), input(false), computed(false), output(false), btree(false), brie(false),
              eqrel(false), isdata(false), istemp(false), last(nullptr), rel(nullptr) {}

    RamRelationIdentifier(const std::string& name, unsigned arity, const bool istemp)
            : RamRelationIdentifier(name, arity) {
        this->istemp = istemp;
    }

    RamRelationIdentifier(const std::string& name, unsigned arity,
            std::vector<std::string> attributeNames = {},
            std::vector<std::string> attributeTypeQualifiers = {}, const SymbolMask& mask = SymbolMask(0),
            const bool input = false, const bool computed = false, const bool output = false,
            const bool btree = false, const bool brie = false, const bool eqrel = false,
            const bool isdata = false, const IODirectives inputDirectives = IODirectives(),
            const std::vector<IODirectives> outputDirectives = {}, const bool istemp = false)
            : name(name), arity(arity), attributeNames(attributeNames),
              attributeTypeQualifiers(attributeTypeQualifiers), mask(mask), input(input), computed(computed),
              output(output), btree(btree), brie(brie), eqrel(eqrel), isdata(isdata), istemp(istemp),
              inputDirectives(inputDirectives), outputDirectives(outputDirectives), last(nullptr),
              rel(nullptr) {
        assert(this->attributeNames.size() == arity || this->attributeNames.empty());
        assert(this->attributeTypeQualifiers.size() == arity || this->attributeTypeQualifiers.empty());
    }

    const std::string& getName() const {
        return name;
    }

    const std::string getArg(uint32_t i) const {
        if (!attributeNames.empty()) {
            return attributeNames[i];
        }
        if (arity == 0) {
            return "";
        }
        return "c" + std::to_string(i);
    }

    const std::string getArgTypeQualifier(uint32_t i) const {
        return (i < attributeTypeQualifiers.size()) ? attributeTypeQualifiers[i] : "";
    }

    const SymbolMask& getSymbolMask() const {
        return mask;
    }

    const bool isInput() const {
        return input;
    }

    const bool isComputed() const {
        return computed;
    }

    const bool isOutput() const {
        return output;
    }

    const bool isBTree() const {
        return btree;
    }

    const bool isBrie() const {
        return brie;
    }

    const bool isEqRel() const {
        return eqrel;
    }

    const bool isTemp() const {
        return istemp;
    }

    const bool isData() const {
        return isdata;
    }

    unsigned getArity() const {
        return arity;
    }

    const IODirectives& getInputDirectives() const {
        return inputDirectives;
    }

    const std::vector<IODirectives>& getOutputDirectives() const {
        return outputDirectives;
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
        for (unsigned i = 1; i < arity; i++) {
            out << ",";
            out << getArg(i);
        }
        out << ")";
    }

    friend std::ostream& operator<<(std::ostream& out, const RamRelationIdentifier& rel) {
        rel.print(out);
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

        Block(size_t s = BLOCK_SIZE) : size(s), used(0), next(nullptr), data(new RamDomain[size]) {}

        size_t getFreeSpace() const {
            return size - used;
        }
    };

    /** The name / arity of this relation */
    RamRelationIdentifier id;

    size_t num_tuples;

    std::unique_ptr<Block> head;
    Block* tail;

    /** keep an eye on the implicit tuples we create so that we can remove when dtor is called */
    std::list<RamDomain*> allocatedBlocks;

    mutable std::map<RamIndexOrder, std::unique_ptr<RamIndex>> indices;

    mutable RamIndex* totalIndex;

    /** lock for parallel execution */
    mutable pthread_mutex_t lock;

public:
    RamRelation(const RamRelationIdentifier& id)
            : id(id), num_tuples(0), head(std::unique_ptr<Block>(new Block())), tail(head.get()),
              totalIndex(nullptr) {
        pthread_mutex_init(&lock, nullptr);
    }

    RamRelation(const RamRelation& other) = delete;

    RamRelation(RamRelation&& other)
            : id(std::move(other.id)), num_tuples(other.num_tuples), tail(other.tail),
              totalIndex(other.totalIndex) {
        pthread_mutex_init(&lock, nullptr);

        // take over ownership
        head.swap(other.head);
        indices.swap(other.indices);

        allocatedBlocks.swap(other.allocatedBlocks);
    }

    ~RamRelation() {
        for (auto x : allocatedBlocks) delete[] x;
    }

    RamRelation& operator=(const RamRelation& other) = delete;

    RamRelation& operator=(RamRelation&& other) {
        ASSERT(getArity() == other.getArity());

        id = other.id;
        num_tuples = other.num_tuples;
        tail = other.tail;
        totalIndex = other.totalIndex;

        // take over ownership
        head.swap(other.head);
        indices.swap(other.indices);

        return *this;
    }

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

    /** only insert exactly one tuple, maintaining order **/
    void quickInsert(const RamDomain* tuple) {
        // check for null-arity
        auto arity = getArity();
        if (arity == 0) {
            // set number of tuples to one -- that's it
            num_tuples = 1;
            return;
        }

        // make existence check
        if (exists(tuple)) {
            return;
        }

        // prepare tail
        if (tail->getFreeSpace() < arity || arity == 0) {
            tail->next = std::unique_ptr<Block>(new Block());
            tail = tail->next.get();
        }

        // insert element into tail
        RamDomain* newTuple = &tail->data[tail->used];
        for (size_t i = 0; i < arity; ++i) {
            newTuple[i] = tuple[i];
        }
        tail->used += arity;

        // update all indexes with new tuple
        for (const auto& cur : indices) {
            cur.second->insert(newTuple);
        }

        // increment relation size
        num_tuples++;
    }

    /** insert a new tuple to table, possibly more than one tuple depending on relation type */
    void insert(const RamDomain* tuple) {
        // TODO: (pnappa) an eqrel check here is all that appears to be needed for implicit additions
        if (id.isEqRel()) {
            // TODO: future optimisation would require this as a member datatype
            // brave soul required to pass this quest
            // // specialisation for eqrel defs
            // std::unique_ptr<binaryrelation> eqreltuples;
            // in addition, it requires insert functions to insert into that, and functions
            // which allow reading of stored values must be changed to accommodate.
            // e.g. insert =>  eqRelTuples->insert(tuple[0], tuple[1]);

            // for now, we just have a naive & extremely slow version, otherwise known as a O(n^2) insertion
            // ):

            // store all values that will be implicitly relevant to the two that we will insert
            std::vector<const RamDomain*> relevantStored;
            for (const RamDomain* vals : *this) {
                if (vals[0] == tuple[0] || vals[0] == tuple[1] || vals[1] == tuple[0] ||
                        vals[1] == tuple[1]) {
                    relevantStored.push_back(vals);
                }
            }

            // we also need to keep a list of all tuples stored s.t. we can free on destruction
            std::list<RamDomain*> dtorLooks;

            for (const auto vals : relevantStored) {
                // insert all possible pairings between these and existing elements

                // ew, temp code
                dtorLooks.push_back(new RamDomain[2]);
                dtorLooks.back()[0] = vals[0];
                dtorLooks.back()[1] = tuple[0];
                dtorLooks.push_back(new RamDomain[2]);
                dtorLooks.back()[0] = vals[0];
                dtorLooks.back()[1] = tuple[1];
                dtorLooks.push_back(new RamDomain[2]);
                dtorLooks.back()[0] = vals[1];
                dtorLooks.back()[1] = tuple[0];
                dtorLooks.push_back(new RamDomain[2]);
                dtorLooks.back()[0] = vals[1];
                dtorLooks.back()[1] = tuple[1];
                dtorLooks.push_back(new RamDomain[2]);
                dtorLooks.back()[0] = tuple[0];
                dtorLooks.back()[1] = vals[0];
                dtorLooks.push_back(new RamDomain[2]);
                dtorLooks.back()[0] = tuple[0];
                dtorLooks.back()[1] = vals[1];
                dtorLooks.push_back(new RamDomain[2]);
                dtorLooks.back()[0] = tuple[1];
                dtorLooks.back()[1] = vals[0];
                dtorLooks.push_back(new RamDomain[2]);
                dtorLooks.back()[0] = tuple[1];
                dtorLooks.back()[1] = vals[1];
            }

            // and of course we need to actually insert this pair
            dtorLooks.push_back(new RamDomain[2]);
            dtorLooks.back()[0] = tuple[1];
            dtorLooks.back()[1] = tuple[0];
            dtorLooks.push_back(new RamDomain[2]);
            dtorLooks.back()[0] = tuple[0];
            dtorLooks.back()[1] = tuple[1];
            dtorLooks.push_back(new RamDomain[2]);
            dtorLooks.back()[0] = tuple[0];
            dtorLooks.back()[1] = tuple[0];
            dtorLooks.push_back(new RamDomain[2]);
            dtorLooks.back()[0] = tuple[1];
            dtorLooks.back()[1] = tuple[1];

            for (const auto x : dtorLooks) quickInsert(x);

            allocatedBlocks.insert(allocatedBlocks.end(), dtorLooks.begin(), dtorLooks.end());

        } else {
            quickInsert(tuple);
        }
    }

    /** a convenience function for inserting tuples */
    template <typename... Args>
    void insert(RamDomain first, Args... rest) {
        RamDomain tuple[] = {first, RamDomain(rest)...};
        insert(tuple);
    }

    /** Merges all elements of the given relation into this relation */
    void insert(const RamRelation& other) {
        assert(getArity() == other.getArity());
        for (const auto& cur : other) {
            insert(cur);
        }
    }

    /** purge table */
    void purge() {
        std::unique_ptr<Block> newHead(new Block());
        head.swap(newHead);
        tail = head.get();
        for (const auto& cur : indices) {
            cur.second->purge();
        }
        num_tuples = 0;
    }

    /** get index for a given set of keys using a cached index as a helper. Keys are encoded as bits for each
     * column */
    RamIndex* getIndex(const SearchColumns& key, RamIndex* cachedIndex) const {
        if (!cachedIndex) {
            return getIndex(key);
        }
        return getIndex(cachedIndex->order());
    }

    /** get index for a given set of keys. Keys are encoded as bits for each column */
    RamIndex* getIndex(const SearchColumns& key) const {
        // suffix for order, if no matching prefix exists
        std::vector<unsigned char> suffix;
        suffix.reserve(getArity());

        // convert to order
        RamIndexOrder order;
        for (size_t k = 1, i = 0; i < getArity(); i++, k *= 2) {
            if (key & k) {
                order.append(i);
            } else {
                suffix.push_back(i);
            }
        }

        // see whether there is an order with a matching prefix
        RamIndex* res = nullptr;
        pthread_mutex_lock(&lock);
        for (auto it = indices.begin(); !res && it != indices.end(); ++it) {
            if (order.isCompatible(it->first)) {
                res = it->second.get();
            }
        }
        pthread_mutex_unlock(&lock);

        // if found, use compatible index
        if (res) {
            return res;
        }

        // extend index to full index
        for (auto cur : suffix) {
            order.append(cur);
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
            std::unique_ptr<RamIndex>& newIndex = indices[order];
            newIndex = std::unique_ptr<RamIndex>(new RamIndex(order));
            newIndex->insert(this->begin(), this->end());
            res = newIndex.get();
        } else {
            res = pos->second.get();
        }
        pthread_mutex_unlock(&lock);
        return res;
    }

    /** Obtains a full index-key for this relation */
    SearchColumns getTotalIndexKey() const {
        return (1 << (getArity())) - 1;
    }

    /** check whether a tuple exists in the relation */
    bool exists(const RamDomain* tuple) const {
        // handle arity 0
        if (getArity() == 0) {
            return !empty();
        }

        // handle all other arities
        if (!totalIndex) {
            totalIndex = getIndex(getTotalIndexKey());
        }
        return totalIndex->exists(tuple);
    }

    /** input table as memory */
    bool load(std::vector<std::vector<std::string>> data, SymbolTable& symTable, const SymbolMask& mask);

    /** store data to vectors */
    void store(std::vector<std::vector<std::string>>& result, const SymbolTable& symTable,
            const SymbolMask& mask) const;

    // --- iterator ---

    /** Iterator for relation */
    class iterator : public std::iterator<std::forward_iterator_tag, RamDomain*> {
        Block* cur;
        RamDomain* tuple;
        size_t arity;

    public:
        iterator() : cur(nullptr), tuple(nullptr), arity(0) {}

        iterator(Block* c, RamDomain* t, size_t a) : cur(c), tuple(t), arity(a) {}

        const RamDomain* operator*() {
            return tuple;
        }

        bool operator==(const iterator& other) const {
            return tuple == other.tuple;
        }

        bool operator!=(const iterator& other) const {
            return (tuple != other.tuple);
        }

        iterator& operator++() {
            // check for end
            if (!cur) {
                return *this;
            }

            // support 0-arity
            if (arity == 0) {
                // move to end
                *this = iterator();
                return *this;
            }

            // support all other arities
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
        // check for emptiness
        if (empty()) {
            return end();
        }

        // support 0-arity
        auto arity = getArity();
        if (arity == 0) {
            Block dummyBlock;
            RamDomain dummyTuple;
            return iterator(&dummyBlock, &dummyTuple, 0);
        }

        // support non-empty non-zero arity relation
        return iterator(head.get(), &head->data[0], arity);
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
    /** The type utilized for storing relations */
    typedef std::map<std::string, RamRelation> relation_map;

    /** The symbol table to be utilized by an evaluation */
    SymbolTable& symbolTable;

    /** The relations manipulated by a ram program */
    relation_map data;

    /** The increment counter utilized by some RAM language constructs */
    int counter;

public:
    RamEnvironment(SymbolTable& symbolTable) : symbolTable(symbolTable), counter(0) {}

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
        if (id.last == this) {
            return *id.rel;
        }

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
        if (id.last == this) {
            return *id.rel;
        }

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
     * Returns the relation map
     */
    relation_map& getRelationMap() const {
        return const_cast<relation_map&>(data);
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

}  // end of namespace souffle
