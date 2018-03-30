/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
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

#include "Forall.h"
#include "InterpreterIndex.h"
#include "RamProgram.h"
#include "RamRelation.h"
#include "RamTranslationUnit.h"
#include "SymbolTable.h"

#include "RamStatement.h"

#include <functional>
#include <ostream>
#include <vector>

namespace souffle {

/**
 * Interpreter Relation
 */
class InterpreterRelation {
private:
    /** Arity of relation */
    size_t arity;

    /** Size of blocks containing tuples */
    static const int BLOCK_SIZE = 1024;

    /** Block data structure for storing tuples */
    struct Block {
        size_t size;
        size_t used;
        // TODO (#421): replace linked list by STL linked list
        // block becomes payload of STL linked list only
        std::unique_ptr<Block> next;
        std::unique_ptr<RamDomain[]> data;

        Block(size_t s = BLOCK_SIZE) : size(s), used(0), next(nullptr), data(new RamDomain[size]) {}

        size_t getFreeSpace() const {
            return size - used;
        }
    };

    /** Number of tuples in relation */
    size_t num_tuples;

    /** Head of block list */
    std::unique_ptr<Block> head;

    /** Tail of block list */
    Block* tail;

    /** List of all allocated blocks */
    std::list<RamDomain*> allocatedBlocks;

    /** List of indices */
    mutable std::map<InterpreterIndexOrder, std::unique_ptr<InterpreterIndex>> indices;

    /** Total index for existence checks */
    mutable InterpreterIndex* totalIndex;

    /** Forall-relation index */
    mutable ForallIndex forall;
    bool is_forall;

    /** Lock for parallel execution */
    mutable Lock lock;

public:
    InterpreterRelation(size_t relArity)
            : arity(relArity), num_tuples(0), head(std::make_unique<Block>()), tail(head.get()),
              totalIndex(nullptr), is_forall(false) {}

    InterpreterRelation(const InterpreterRelation& other) = delete;

    InterpreterRelation(InterpreterRelation&& other)
            : arity(other.arity), num_tuples(other.num_tuples), tail(other.tail),
              totalIndex(other.totalIndex), forall(other.forall), is_forall(other.is_forall) {
        // take over ownership
        head.swap(other.head);
        indices.swap(other.indices);

        allocatedBlocks.swap(other.allocatedBlocks);
    }

    virtual ~InterpreterRelation() {
        for (auto x : allocatedBlocks) delete[] x;
    }

    void setForall() {
	is_forall = true;
    }

    // TODO (#421): check whether still required
    InterpreterRelation& operator=(const InterpreterRelation& other) = delete;

    InterpreterRelation& operator=(InterpreterRelation&& other) {
        ASSERT(getArity() == other.getArity());

        num_tuples = other.num_tuples;
        tail = other.tail;
        totalIndex = other.totalIndex;

        // take over ownership
        head.swap(other.head);
        indices.swap(other.indices);

        return *this;
    }

    /** Get arity of relation */
    size_t getArity() const {
        return arity;
    }

    /** Check whether relation is empty */
    bool empty() const {
        return num_tuples == 0;
    }

    /** Gets the number of contained tuples */
    size_t size() const {
        return num_tuples;
    }

    /** Insert tuple */
    virtual void insert(const RamDomain* tuple) {
        // check for null-arity
        if (arity == 0) {
            // set number of tuples to one -- that's it
            num_tuples = 1;
            return;
        }

        ASSERT(tuple);

	// add forall index, if needed
	const RamDomain* t = tuple;
	if (is_forall && arity >= 3) {
	    RamDomain *newT = reinterpret_cast<RamDomain*>(alloca(sizeof(RamDomain)*arity));
	    for (size_t i = 0; i < arity; i++) {
		newT[i] = tuple[i];
	    }
	    auto cur_prev = forall.insert(tuple[0], tuple[1]);
	    newT[2] = cur_prev.first;
	    if (arity >= 4) {
		newT[3] = cur_prev.second;
	    }
	    t = newT;
	}

        // make existence check
        if (exists(t)) {
            return;
        }

        // prepare tail
        if (tail->getFreeSpace() < arity || arity == 0) {
            tail->next = std::make_unique<Block>();
            tail = tail->next.get();
        }

        // insert element into tail
        RamDomain* newTuple = &tail->data[tail->used];
        for (size_t i = 0; i < arity; ++i) {
            newTuple[i] = t[i];
        }
        tail->used += arity;

        // update all indexes with new tuple
        for (const auto& cur : indices) {
            cur.second->insert(newTuple);
        }

        // increment relation size
        num_tuples++;
    }

    /** Insert tuple via arguments */
    template <typename... Args>
    void insert(RamDomain first, Args... rest) {
        RamDomain tuple[] = {first, RamDomain(rest)...};
        insert(tuple);
    }

    /** Merge another relation into this relation */
    void insert(const InterpreterRelation& other) {
        assert(getArity() == other.getArity());
        for (const auto& cur : other) {
            insert(cur);
        }
    }

    /** Purge table */
    void purge() {
        std::unique_ptr<Block> newHead = std::make_unique<Block>();
        head.swap(newHead);
        tail = head.get();
        for (const auto& cur : indices) {
            cur.second->purge();
        }
        num_tuples = 0;
    }

    /** get index for a given set of keys using a cached index as a helper. Keys are encoded as bits for each
     * column */
    InterpreterIndex* getIndex(const SearchColumns& key, InterpreterIndex* cachedIndex) const {
        if (!cachedIndex) {
            return getIndex(key);
        }
        return getIndex(cachedIndex->order());
    }

    /** get index for a given set of keys. Keys are encoded as bits for each column */
    InterpreterIndex* getIndex(const SearchColumns& key) const {
        // suffix for order, if no matching prefix exists
        std::vector<unsigned char> suffix;
        suffix.reserve(getArity());

        // convert to order
        InterpreterIndexOrder order;
        for (size_t k = 1, i = 0; i < getArity(); i++, k *= 2) {
            if (key & k) {
                order.append(i);
            } else {
                suffix.push_back(i);
            }
        }

        // see whether there is an order with a matching prefix
        InterpreterIndex* res = nullptr;
        {
            auto lease = lock.acquire();
            (void)lease;
            for (auto it = indices.begin(); !res && it != indices.end(); ++it) {
                if (order.isCompatible(it->first)) {
                    res = it->second.get();
                }
            }
        }
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
    InterpreterIndex* getIndex(const InterpreterIndexOrder& order) const {
        // TODO: improve index usage by re-using indices with common prefix
        InterpreterIndex* res = nullptr;
        {
            auto lease = lock.acquire();
            (void)lease;
            auto pos = indices.find(order);
            if (pos == indices.end()) {
                std::unique_ptr<InterpreterIndex>& newIndex = indices[order];
                newIndex = std::make_unique<InterpreterIndex>(order);
                newIndex->insert(this->begin(), this->end());
                res = newIndex.get();
            } else {
                res = pos->second.get();
            }
        }
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

    /** Extend tuple */
    virtual std::vector<RamDomain*> extend(const RamDomain* tuple) {
        std::vector<RamDomain*> newTuples;

        // A standard relation does not generate extra new knowledge on insertion.
        newTuples.push_back(new RamDomain[2]{tuple[0], tuple[1]});

        return newTuples;
    }

    /** Extend relation */
    virtual void extend(const InterpreterRelation& rel) {}
};

/**
 * Interpreter Equivalence Relation
 */

class InterpreterEqRelation : public InterpreterRelation {
public:
    InterpreterEqRelation(size_t relArity) : InterpreterRelation(relArity) {}

    /** Insert tuple */
    void insert(const RamDomain* tuple) override {
        // TODO: (pnappa) an eqrel check here is all that appears to be needed for implicit additions
        // TODO: future optimisation would require this as a member datatype
        // brave soul required to pass this quest
        // // specialisation for eqrel defs
        // std::unique_ptr<binaryrelation> eqreltuples;
        // in addition, it requires insert functions to insert into that, and functions
        // which allow reading of stored values must be changed to accommodate.
        // e.g. insert =>  eqRelTuples->insert(tuple[0], tuple[1]);

        // for now, we just have a naive & extremely slow version, otherwise known as a O(n^2) insertion
        // ):

        for (auto* newTuple : extend(tuple)) {
            InterpreterRelation::insert(newTuple);
            delete[] newTuple;
        }
    }

    /** Find the new knowledge generated by inserting a tuple */
    std::vector<RamDomain*> extend(const RamDomain* tuple) override {
        std::vector<RamDomain*> newTuples;

        newTuples.push_back(new RamDomain[2]{tuple[0], tuple[0]});
        newTuples.push_back(new RamDomain[2]{tuple[0], tuple[1]});
        newTuples.push_back(new RamDomain[2]{tuple[1], tuple[0]});
        newTuples.push_back(new RamDomain[2]{tuple[1], tuple[1]});

        std::vector<const RamDomain*> relevantStored;
        for (const RamDomain* vals : *this) {
            if (vals[0] == tuple[0] || vals[0] == tuple[1] || vals[1] == tuple[0] || vals[1] == tuple[1]) {
                relevantStored.push_back(vals);
            }
        }

        for (const auto vals : relevantStored) {
            newTuples.push_back(new RamDomain[2]{vals[0], tuple[0]});
            newTuples.push_back(new RamDomain[2]{vals[0], tuple[1]});
            newTuples.push_back(new RamDomain[2]{vals[1], tuple[0]});
            newTuples.push_back(new RamDomain[2]{vals[1], tuple[1]});
            newTuples.push_back(new RamDomain[2]{tuple[0], vals[0]});
            newTuples.push_back(new RamDomain[2]{tuple[0], vals[1]});
            newTuples.push_back(new RamDomain[2]{tuple[1], vals[0]});
            newTuples.push_back(new RamDomain[2]{tuple[1], vals[1]});
        }

        return newTuples;
    }
    /** Extend this relation with new knowledge generated by inserting all tuples from a relation */
    void extend(const InterpreterRelation& rel) override {
        std::vector<RamDomain*> newTuples;
        // store all values that will be implicitly relevant to the those that we will insert
        for (const auto* tuple : rel) {
            for (auto* newTuple : extend(tuple)) {
                newTuples.push_back(newTuple);
            }
        }
        for (const auto* newTuple : newTuples) {
            InterpreterRelation::insert(newTuple);
            delete[] newTuple;
        }
    }
};

/**
 * An environment encapsulates all the context information required for
 * processing a RAM program.
 */
class InterpreterEnvironment {
    /** The type utilized for storing relations */
    typedef std::map<std::string, InterpreterRelation*> relation_map;

    /** The symbol table to be utilized by an evaluation */
    SymbolTable& symbolTable;

    /** The relations manipulated by a ram program */
    relation_map data;

    /** The increment counter utilized by some RAM language constructs */
    int counter;

public:
    InterpreterEnvironment(SymbolTable& symbolTable) : symbolTable(symbolTable), counter(0) {}

    virtual ~InterpreterEnvironment() {
        for (auto& x : data) {
            delete x.second;
        }
    }

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
    InterpreterRelation& getRelation(const RamRelation& id) {
        InterpreterRelation* res = nullptr;
        auto pos = data.find(id.getName());
        if (pos != data.end()) {
            res = (pos->second);
        } else {
            if (!id.isEqRel()) {
                res = new InterpreterRelation(id.getArity());
            } else {
                res = new InterpreterEqRelation(id.getArity());
            }
	    if (id.isForall()) {
		res->setForall();
	    }
            data[id.getName()] = res;
        }
        // return result
        return *res;
    }

    /**
     * Obtains an immutable reference to the relation identified by
     * the given identifier. If no such relation exist, a reference
     * to an empty relation will be returned (not exhibiting the proper
     * id, but the correct content).
     */
    const InterpreterRelation& getRelation(const RamRelation& id) const {
        // look up relation
        auto pos = data.find(id.getName());
        assert(pos != data.end());

        // cache result
        return *pos->second;
    }

    /**
     * Obtains an immutable reference to the relation identified by
     * the given identifier. If no such relation exist, a reference
     * to an empty relation will be returned (not exhibiting the proper
     * id, but the correct content).
     */
    const InterpreterRelation& getRelation(const std::string& name) const {
        auto pos = data.find(name);
        assert(pos != data.end());
        return *pos->second;
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
    void dropRelation(const RamRelation& id) {
        data.erase(id.getName());
    }
};

/**
 * A class representing the order of predicates in the body of a rule
 */
class Order {
    /** The covered order */
    std::vector<unsigned> order;

public:
    static Order getIdentity(unsigned size) {
        Order res;
        for (unsigned i = 0; i < size; i++) {
            res.append(i);
        }
        return res;
    }

    void append(unsigned pos) {
        order.push_back(pos);
    }

    unsigned operator[](unsigned index) const {
        return order[index];
    }

    std::size_t size() const {
        return order.size();
    }

    bool isComplete() const {
        for (size_t i = 0; i < order.size(); i++) {
            if (!contains(order, i)) {
                return false;
            }
        }
        return true;
    }

    const std::vector<unsigned>& getOrder() const {
        return order;
    }

    void print(std::ostream& out) const {
        out << order;
    }

    friend std::ostream& operator<<(std::ostream& out, const Order& order) {
        order.print(out);
        return out;
    }
};

/**
 * The summary to be returned from a statement
 */
struct ExecutionSummary {
    Order order;
    long time;
};

/** Defines the type of execution strategies for interpreter */
typedef std::function<ExecutionSummary(const RamInsert&, InterpreterEnvironment& env, std::ostream*)>
        QueryExecutionStrategy;

/** With this strategy queries will be processed without profiling */
extern const QueryExecutionStrategy DirectExecution;

/** With this strategy queries will be dynamically with profiling */
extern const QueryExecutionStrategy ScheduledExecution;

/** The type to reference indices */
typedef unsigned Column;

/**
 * A summary of statistical properties of a ram relation.
 */
class RelationStats {
    /** The arity - accurate */
    uint8_t arity;

    /** The number of tuples - accurate */
    uint64_t size;

    /** The sample size estimations are based on */
    uint32_t sample_size;

    /** The cardinality of the various components of the tuples - estimated */
    std::vector<uint64_t> cardinalities;

public:
    RelationStats() : arity(0), size(0), sample_size(0) {}

    RelationStats(uint64_t size, const std::vector<uint64_t>& cards)
            : arity(cards.size()), size(size), sample_size(0), cardinalities(cards) {}

    RelationStats(const RelationStats&) = default;
    RelationStats(RelationStats&&) = default;

    RelationStats& operator=(const RelationStats&) = default;
    RelationStats& operator=(RelationStats&&) = default;

    /**
     * A factory function extracting statistical information form the given relation
     * base on a given sample size. If the sample size is not specified, the full
     * relation will be processed.
     */
    static RelationStats extractFrom(
            const InterpreterRelation& rel, uint32_t sample_size = std::numeric_limits<uint32_t>::max());

    uint8_t getArity() const {
        return arity;
    }

    uint64_t getCardinality() const {
        return size;
    }

    uint32_t getSampleSize() const {
        return sample_size;
    }

    uint64_t getEstimatedCardinality(Column c) const {
        if (c >= cardinalities.size()) {
            return 0;
        }
        return cardinalities[c];
    }

    void print(std::ostream& out) const {
        out << cardinalities;
    }

    friend std::ostream& operator<<(std::ostream& out, const RelationStats& stats) {
        stats.print(out);
        return out;
    }
};

/**
 * A RAM interpreter. The RAM program will
 * be processed within the callers process. Before every query operation, an
 * optional scheduling step will be conducted.
 */
class Interpreter {
protected:
    /** An optional stream to print logging information to an output stream */
    std::ostream* report;

public:
    /**
     * Update logging stream
     */
    void setReportTarget(std::ostream& report) {
        this->report = &report;
    }

    /**
     * Runs the given RAM statement on an empty environment and returns
     * this environment after the completion of the execution.
     */
    std::unique_ptr<InterpreterEnvironment> execute(SymbolTable& table, const RamProgram& prog) const {
        auto env = std::make_unique<InterpreterEnvironment>(table);
        invoke(prog, *env);
        return env;
    }

    /**
     * Runs the given RAM statement on an empty environment and returns
     * this environment after the completion of the execution.
     */
    std::unique_ptr<InterpreterEnvironment> execute(const RamTranslationUnit& tu) const {
        return execute(tu.getSymbolTable(), *tu.getProgram());
    }

    /** An execution strategy for the interpreter */
    QueryExecutionStrategy queryStrategy;

public:
    /** A constructor accepting a query strategy */
    Interpreter(const QueryExecutionStrategy& queryStrategy)
            : report(nullptr), queryStrategy(queryStrategy) {}

    /** run the program for a given interpreter environment */
    void invoke(const RamProgram& prog, InterpreterEnvironment& env) const;

    /**
     * Runs a subroutine of a RamProgram
     */
    virtual void executeSubroutine(InterpreterEnvironment& env, const RamStatement& stmt,
            const std::vector<RamDomain>& arguments, std::vector<RamDomain>& returnValues,
            std::vector<bool>& returnErrors) const;
};

}  // end of namespace souffle
