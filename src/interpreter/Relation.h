/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Relation.h
 *
 * Defines Interpreter Relations
 *
 ***********************************************************************/

#pragma once

#include "interpreter/Index.h"
#include "ram/analysis/Index.h"
#include "souffle/RamTypes.h"
#include "souffle/SouffleInterface.h"
#include "souffle/utility/MiscUtil.h"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle::interpreter {

/**
 * Wrapper for InterpreterRelation.
 *
 * This class unifies the InterpreterRelation template classes.
 * It also defines virtual interfaces for ProgInterface and some virtual helper functions for interpreter
 * execution.
 */
struct RelationWrapper {
    using arity_type = souffle::Relation::arity_type;

public:
    RelationWrapper(arity_type arity, arity_type auxiliaryArity, std::string relName)
            : relName(std::move(relName)), arity(arity), auxiliaryArity(auxiliaryArity) {}

    virtual ~RelationWrapper() = default;

    // -- Define methods and interfaces for ProgInterface. --
public:
    /**
     * A virtualized iterator class that can be used by the Proginterface.
     * Define behaviors to uniformly access the underlying tuple regardless its structure and arity.
     */
    class iterator_base {
    public:
        virtual ~iterator_base() = default;

        virtual iterator_base& operator++() = 0;

        virtual const RamDomain* operator*() = 0;

        /*
         * A clone method is required by ProgInterface.
         */
        virtual iterator_base* clone() const = 0;

        virtual bool equal(const iterator_base& other) const = 0;
    };

    /**
     * The iterator interface.
     *
     * Other class should use this iterator class to traverse the relation.
     */
    class Iterator {
        Own<iterator_base> iter;

    public:
        Iterator(const Iterator& other) : iter(other.iter->clone()) {}
        Iterator(iterator_base* iter) : iter(iter) {}

        Iterator& operator++() {
            ++(*iter);
            return *this;
        }

        const RamDomain* operator*() {
            return **iter;
        }

        bool operator==(const Iterator& other) const {
            return iter->equal(*other.iter);
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }
    };

    virtual Iterator begin() const = 0;

    virtual Iterator end() const = 0;

    virtual void insert(const RamDomain*) = 0;

    virtual bool contains(const RamDomain*) const = 0;

    virtual std::size_t size() const = 0;

    virtual void purge() = 0;

    const std::string& getName() const {
        return relName;
    }

    arity_type getArity() const {
        return arity;
    }

    arity_type getAuxiliaryArity() const {
        return auxiliaryArity;
    }

    // -- Defines methods and interfaces for Interpreter execution. --
public:
    using IndexViewPtr = Own<ViewWrapper>;

    /**
     * Return the order of an index.
     */
    virtual Order getIndexOrder(std::size_t) const = 0;

    /**
     * Obtains a view on an index of this relation, facilitating hint-supported accesses.
     *
     * This function is virtual because view creation require at least one indirect dispatch.
     */
    virtual IndexViewPtr createView(const std::size_t&) const = 0;

protected:
    std::string relName;

    arity_type arity;
    arity_type auxiliaryArity;
};

/**
 * A relation, composed of a collection of indexes.
 */
template <std::size_t _Arity, template <std::size_t> typename Structure>
class Relation : public RelationWrapper {
public:
    static constexpr std::size_t Arity = _Arity;
    using Attribute = std::size_t;
    using AttributeSet = std::set<Attribute>;
    using Index = interpreter::Index<Arity, Structure>;
    using Tuple = souffle::Tuple<RamDomain, Arity>;
    using View = typename Index::View;
    using iterator = typename Index::iterator;

    /**
     * Construct a typed tuple from a raw data.
     */
    static Tuple constructTuple(const RamDomain* data) {
        Tuple tuple{};
        std::copy_n(data, Arity, tuple.begin());
        return tuple;
    }

    /**
     * Cast an abstract view into a view of Index::View type.
     */
    static View* castView(ViewWrapper* view) {
        return static_cast<View*>(view);
    }

    /**
     * Creates a relation, build all necessary indexes.
     */
    Relation(std::size_t auxiliaryArity, const std::string& name,
            const ram::analysis::IndexCluster& indexSelection)
            : RelationWrapper(Arity, auxiliaryArity, name) {
        for (const auto& order : indexSelection.getAllOrders()) {
            ram::analysis::LexOrder fullOrder = order;
            // Expand the order to a total order
            ram::analysis::AttributeSet set{order.begin(), order.end()};

            // This operation is not performance critical.
            // Not using constexpr Arity to avoid compiler warning. (When Arity == 0)
            for (std::size_t i = 0; i < getArity(); ++i) {
                if (set.find(i) == set.end()) {
                    fullOrder.push_back(i);
                }
            }

            indexes.push_back(mk<Index>(fullOrder));
        }

        // Use the first index as default main index
        main = indexes[0].get();
    }

    Relation(Relation& other) = delete;

    // -- Implement all virtual interface from Wrapper. --
    // -- Operations defined in this section are not performance-oriented.
public:
    void purge() override {
        __purge();
    }

    void insert(const RamDomain* data) override {
        insert(constructTuple(data));
    }

    bool contains(const RamDomain* data) const override {
        return contains(constructTuple(data));
    }

    IndexViewPtr createView(const std::size_t& indexPos) const override {
        return mk<View>(indexes[indexPos]->createView());
    }

    std::size_t size() const override {
        return __size();
    }

    Order getIndexOrder(std::size_t idx) const override {
        return indexes[idx]->getOrder();
    }

    class iterator_base : public RelationWrapper::iterator_base {
        iterator iter;
        Order order;
        RamDomain data[Arity];

    public:
        iterator_base(typename Index::iterator iter, Order order)
                : iter(std::move(iter)), order(std::move(order)) {}

        iterator_base& operator++() override {
            ++iter;
            return *this;
        }

        const RamDomain* operator*() override {
            const auto& tuple = *iter;
            // Not using constexpr Arity to avoid compiler warning. (When Arity == 0)
            for (std::size_t i = 0; i < order.size(); ++i) {
                data[order[i]] = tuple[i];
            }
            return data;
        }

        iterator_base* clone() const override {
            return new iterator_base(iter, order);
        }

        bool equal(const RelationWrapper::iterator_base& other) const override {
            if (auto* o = as<iterator_base>(other)) {
                return iter == o->iter;
            }
            return false;
        }
    };

    Iterator begin() const override {
        return Iterator(new iterator_base(main->begin(), main->getOrder()));
    }

    Iterator end() const override {
        return Iterator(new iterator_base(main->end(), main->getOrder()));
    }

    // -----
    // Following section defines and implement interfaces for interpreter execution.
    //
    // These functions are performance efficient but requires compile time knowledge and
    // are not expected to be used other then the interpreter generator/engine.
    // -----
public:
    /**
     * Add the given tuple to this relation.
     */
    bool insert(const Tuple& tuple) {
        if (!(main->insert(tuple))) {
            return false;
        }
        for (std::size_t i = 1; i < indexes.size(); ++i) {
            indexes[i]->insert(tuple);
        }
        return true;
    }

    /**
     * Add all entries of the given relation to this relation.
     */
    void insert(const Relation<Arity, Structure>& other) {
        for (const auto& tuple : other.scan()) {
            this->insert(tuple);
        }
    }

    /**
     * Tests whether this relation contains the given tuple.
     */
    bool contains(const Tuple& tuple) const {
        return main->contains(tuple);
    }

    /**
     * Tests whether this relation contains any element between the given boundaries.
     */
    bool contains(const std::size_t& indexPos, const Tuple& low, const Tuple& high) const {
        return indexes[indexPos]->contains(low, high);
    }

    /**
     * Obtains a pair of iterators to scan the entire relation.
     *
     * Return 'raw iterator' that returns tuples in undecoded form.
     */
    souffle::range<iterator> scan() const {
        return main->scan();
    }

    /**
     * Returns a partitioned list of iterators for parallel computation
     */
    std::vector<souffle::range<iterator>> partitionScan(std::size_t partitionCount) const {
        return main->partitionScan(partitionCount);
    }

    /**
     * Obtains a pair of iterators covering the interval between the two given entries.
     */
    souffle::range<iterator> range(const std::size_t& indexPos, const Tuple& low, const Tuple& high) const {
        return indexes[indexPos]->range(low, high);
    }

    /**
     * Returns a partitioned list of iterators coving elements in range [low, high]
     */
    std::vector<souffle::range<iterator>> partitionRange(const std::size_t& indexPos, const Tuple& low,
            const Tuple& high, std::size_t partitionCount) const {
        return indexes[indexPos]->partitionRange(low, high, partitionCount);
    }

    /**
     * Swaps the content of this and the given relation, including the
     * installed indexes.
     */
    void swap(Relation<Arity, Structure>& other) {
        indexes.swap(other.indexes);
    }

    /**
     * Return number of tuples in relation (full-order)
     */
    std::size_t __size() const {
        return main->size();
    }

    /**
     * Check if the relation is empty
     */
    bool empty() const {
        return main->empty();
    }

    /**
     * Clear all indexes
     */
    void __purge() {
        for (auto& idx : indexes) {
            idx->clear();
        }
    }

    /**
     * Check if a tuple exists in relation
     */
    bool exists(const Tuple& tuple) const {
        return main->contains(tuple);
    }

    Index* getIndex(std::size_t idx) const {
        return indexes.at(idx).get();
    }

protected:
    // Number of height parameters of relation
    std::size_t auxiliaryArity;

    // a map of managed indexes
    VecOwn<Index> indexes;

    // a pointer to the main index within the managed index
    Index* main;
};

template <std::size_t _Arity>
class BtreeDeleteRelation : public Relation<_Arity, BtreeDelete> {
public:
    using Relation<_Arity, BtreeDelete>::Relation;
    using Relation<_Arity, BtreeDelete>::main;
    using Relation<_Arity, BtreeDelete>::indexes;
    using Tuple = souffle::Tuple<RamDomain, _Arity>;

    /**
     * Erase the given tuple from this relation.
     */
    bool erase(const Tuple& tuple) {
        using DeleteIndex = BtreeDeleteIndex<_Arity>;
        if (!(static_cast<DeleteIndex*>(main))->erase(tuple)) {
            return false;
        }
        for (std::size_t i = 1; i < indexes.size(); ++i) {
            static_cast<DeleteIndex*>(indexes[i].get())->erase(tuple);
        }
        return true;
    }
};

class EqrelRelation : public Relation<2, Eqrel> {
public:
    using Relation<2, Eqrel>::Relation;

    void extendAndInsert(const EqrelRelation& rel) {
        auto src = static_cast<EqrelIndex*>(this->main);
        for (auto& trgIndex : rel.indexes) {
            auto trg = static_cast<EqrelIndex*>(trgIndex.get());
            src->extendAndInsert(trg);
        }
    }
};

// The type of relation factory functions.
using RelationFactory = Own<RelationWrapper> (*)(
        const ram::Relation& id, const ram::analysis::IndexCluster& indexSelection);

// A factory for BTree based relation.
Own<RelationWrapper> createBTreeRelation(
        const ram::Relation& id, const ram::analysis::IndexCluster& indexSelection);

// A factory for BTreeDelete based relation.
Own<RelationWrapper> createBTreeDeleteRelation(
        const ram::Relation& id, const ram::analysis::IndexCluster& indexSelection);

// A factory for BTree provenance index.
Own<RelationWrapper> createProvenanceRelation(
        const ram::Relation& id, const ram::analysis::IndexCluster& indexSelection);

// A factory for Brie based index.
Own<RelationWrapper> createBrieRelation(
        const ram::Relation& id, const ram::analysis::IndexCluster& indexSelection);

// A factory for Eqrel index.
Own<RelationWrapper> createEqrelRelation(
        const ram::Relation& id, const ram::analysis::IndexCluster& indexSelection);
}  // namespace souffle::interpreter
