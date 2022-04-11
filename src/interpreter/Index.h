/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Index.h
 *
 * Interpreter index with generic interface.
 *
 ***********************************************************************/
#pragma once

#include "interpreter/Util.h"
#include "souffle/RamTypes.h"
#include "souffle/datastructure/EquivalenceRelation.h"
#include "souffle/datastructure/UnionFind.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iosfwd>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

namespace souffle::interpreter {
/**
 * An order to be enforced for storing tuples within
 * indexes. The order is defined by the sequence of
 * component to be considered in sorting tuples.
 */
class Order {
    using Attribute = std::size_t;
    using AttributeOrder = std::vector<Attribute>;
    AttributeOrder order;

public:
    Order() = default;
    Order(AttributeOrder pos) : order(std::move(pos)) {
        assert(valid());
    }

    // Creates a natural order for the given arity.
    static Order create(std::size_t arity) {
        Order res;
        res.order.resize(arity);
        for (std::size_t i = 0; i < arity; i++) {
            res.order[i] = i;
        }
        return res;
    }

    std::size_t size() const {
        return order.size();
    }

    /**
     * Determines whether this order is a valid order.
     */
    bool valid() const {
        // Check that all indices are in range.
        for (auto i : order) {
            if (i >= order.size()) {
                return false;
            }
        }
        // Check that there are no duplicates.
        for (std::size_t i = 0; i < order.size(); i++) {
            for (std::size_t j = i + 1; j < order.size(); j++) {
                if (order[i] == order[j]) {
                    return false;
                }
            }
        }
        return true;
    }

    /**
     * Encode the tuple with order
     */
    template <std::size_t Arity>
    Tuple<RamDomain, Arity> encode(const Tuple<RamDomain, Arity>& entry) const {
        Tuple<RamDomain, Arity> res{};
        for (std::size_t i = 0; i < Arity; ++i) {
            res[i] = entry[order[i]];
        }
        return res;
    }

    /**
     * Decode the tuple by order
     */
    template <std::size_t Arity>
    Tuple<RamDomain, Arity> decode(const Tuple<RamDomain, Arity>& entry) const {
        Tuple<RamDomain, Arity> res{};
        for (std::size_t i = 0; i < Arity; ++i) {
            res[order[i]] = entry[i];
        }
        return res;
    }

    const AttributeOrder& getOrder() const {
        return this->order;
    }

    bool operator==(const Order& other) const {
        return order == other.order;
    }

    bool operator!=(const Order& other) const {
        return !(*this == other);
    }

    Attribute operator[](const std::size_t idx) const {
        return order[idx];
    }

    friend std::ostream& operator<<(std::ostream& out, const Order& order);
};

inline std::ostream& operator<<(std::ostream& out, const Order& order) {
    return out << "[" << join(order.order) << "]";
}

/**
 * A dummy wrapper for indexViews.
 */
struct ViewWrapper {
    virtual ~ViewWrapper() = default;
};

/**
 * An index is an abstraction of a data structure
 */
template <std::size_t _Arity, template <std::size_t> typename Structure>
class Index {
public:
    static constexpr std::size_t Arity = _Arity;
    using Data = Structure<Arity>;
    using Tuple = typename souffle::Tuple<RamDomain, Arity>;
    using iterator = typename Data::iterator;
    using Hints = typename Data::operation_hints;
    using Comparator = comparator<Arity>;

    Index(Order order) : order(std::move(order)) {}

protected:
    Order order;
    Data data;
    Comparator cmp;

public:
    /**
     * A view on a relation caching local access patterns (not thread safe!).
     * Each thread should create and use its own view for accessing relations
     * to exploit access patterns via operation hints.
     */
    class View : public ViewWrapper {
        mutable Hints hints;
        const Data& data;
        Comparator cmp;

    public:
        View(const Data& data) : data(data) {}

        /** Tests whether the given entry is contained in this index. */
        bool contains(const Tuple& entry) {
            return data.contains(entry, hints);
        }

        /** Tests whether any element in the given range is contained in this index. */
        bool contains(const Tuple& low, const Tuple& high) {
            return !range(low, high).empty();
        }

        /** Obtains a pair of iterators representing the given range within this index. */
        souffle::range<iterator> range(const Tuple& low, const Tuple& high) {
            if (cmp(low, high) > 0) {
                return {data.end(), data.end()};
            }
            return {data.lower_bound(low, hints), data.upper_bound(high, hints)};
        }
    };

public:
    /**
     * Requests the creation of a view on this index.
     */
    View createView() {
        return View(this->data);
    }

    iterator begin() const {
        return data.begin();
    }

    iterator end() const {
        return data.end();
    }

    /**
     * Obtains the lex order of this index.
     */
    Order getOrder() const {
        return order;
    }

    /**
     * Tests whether this index is empty or not.
     */
    bool empty() const {
        return data.empty();
    }

    /**
     * Obtains the number of elements stored in this index.
     */
    std::size_t size() const {
        return data.size();
    }

    /**
     * Inserts a tuple into this index.
     */
    bool insert(const Tuple& tuple) {
        return data.insert(order.encode(tuple));
    }

    /**
     * Inserts all elements of the given index.
     */
    void insert(const Index<Arity, Structure>& src) {
        for (const auto& tuple : src) {
            this->insert(tuple);
        }
    }

    /**
     * Tests whether the given tuple is present in this index or not.
     */
    bool contains(const Tuple& tuple) const {
        return data.contains(tuple);
    }

    /**
     * Tests whether this index contains any tuple within the given bounds.
     */
    bool contains(const Tuple& low, const Tuple& high) const {
        return !range(low, high).empty();
    }

    /**
     * Returns a pair of iterators covering the entire index content.
     */
    souffle::range<iterator> scan() const {
        return {data.begin(), data.end()};
    }

    /**
     * Returns a pair of iterators covering elements in the range [low,high)
     */
    souffle::range<iterator> range(const Tuple& low, const Tuple& high) const {
        if (cmp(low, high) > 0) {
            return {data.end(), data.end()};
        }
        return {data.lower_bound(low), data.upper_bound(high)};
    }

    /**
     * Retruns a partitioned list of iterators for parallel computation
     */
    std::vector<souffle::range<iterator>> partitionScan(std::size_t partitionCount) const {
        auto chunks = data.partition(partitionCount);
        std::vector<souffle::range<iterator>> res;
        res.reserve(chunks.size());
        for (const auto& cur : chunks) {
            res.push_back({cur.begin(), cur.end()});
        }
        return res;
    }

    /**
     * Returns a partitioned list of iterators coving elements in range [low, high]
     */
    std::vector<souffle::range<iterator>> partitionRange(
            const Tuple& low, const Tuple& high, std::size_t partitionCount) const {
        auto ranges = this->range(low, high);
        auto chunks = ranges.partition(partitionCount);
        std::vector<souffle::range<iterator>> res;
        res.reserve(chunks.size());
        for (const auto& cur : chunks) {
            res.push_back({cur.begin(), cur.end()});
        }
        return res;
    }

    /**
     * Clears the content of this index, turning it empty.
     */
    void clear() {
        data.clear();
    }
};

/**
 * A partial specialize template for nullary indexes.
 * No complex data structure is required.
 */
template <template <std::size_t> typename Structure>
class Index<0, Structure> {
public:
    static constexpr std::size_t Arity = 0;
    using Tuple = typename souffle::Tuple<RamDomain, 0>;

protected:
    // indicates whether the one single element is present or not.
    std::atomic<bool> data{false};

public:
    Index(Order /* order */) {}

    // Specialized iterator class for nullary.
    class iterator {
        bool value;
        const Tuple dummy{};

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Tuple;
        using difference_type = int64_t;
        using pointer = Tuple*;
        using reference = Tuple&;

        iterator() : value(false) {}
        iterator(bool v) : value(v) {}
        iterator(const iterator& other) : value(other.value), dummy(other.dummy) {}

        const Tuple& operator*() {
            return dummy;
        }

        bool operator==(const iterator& other) const {
            return other.value == value;
        }

        bool operator!=(const iterator& other) const {
            return other.value != value;
        }

        iterator& operator++() {
            if (value) {
                value = false;
            }
            return *this;
        }
    };

    iterator begin() const {
        return iterator(data);
    }
    iterator end() const {
        return iterator();
    }

    // The nullary index view -- does not require any hints.
    struct View : public ViewWrapper {
        const std::atomic<bool>& data;

    public:
        View(const std::atomic<bool>& data) : data(data) {}

        bool contains(const Tuple& /* t */) const {
            return data;
        }

        bool contains(const Tuple& /* l */, const Tuple& /* h */) const {
            return data;
        }

        souffle::range<iterator> range(const Tuple& /* l */, const Tuple& /* h */) const {
            return {iterator(data), iterator()};
        }
    };

public:
    View createView() {
        return View(this->data);
    }

    Order getOrder() const {
        return Order({0});
    }

    bool empty() const {
        return !data;
    }

    std::size_t size() const {
        return data ? 1 : 0;
    }

    bool insert(const Tuple& /* t */) {
        return data = true;
    }

    void insert(const Index& src) {
        data = src.data;
    }

    bool contains(const Tuple& /* t */) const {
        return data;
    }

    bool contains(const Tuple& /* l */, const Tuple& /* h */) const {
        return data;
    }

    souffle::range<iterator> scan() const {
        return {this->begin(), this->end()};
    }

    souffle::range<iterator> range(const Tuple& /* l */, const Tuple& /* h */) const {
        return {this->begin(), this->end()};
    }

    std::vector<souffle::range<iterator>> partitionScan(std::size_t /* partitionCount */) const {
        std::vector<souffle::range<iterator>> res;
        res.push_back(scan());
        return res;
    }

    std::vector<souffle::range<iterator>> partitionRange(
            const Tuple& /* l */, const Tuple& /* h */, std::size_t /* partitionCount */) const {
        return this->partitionScan(0);
    }

    void clear() {
        data = false;
    }
};

/**
 * For EqrelIndex we do inheritence since EqrelIndex only diff with one extra function.
 */
class EqrelIndex : public interpreter::Index<2, Eqrel> {
public:
    using Index<2, Eqrel>::Index;

    /**
     * Extend this relation with another index, expanding this index and
     * inserting it into the other index.
     *
     * Merge and extend this index with another index, expanding this
     * equivalence relation. The supplied relation is the old knowledge, whilst
     * this relation only contains explicitly new knowledge. After this
     * operation the "implicitly new tuples" are now explicitly inserted this
     * relation, and all of the new tuples in this relation are inserted into
     * the old relation.
     */
    void extendAndInsert(EqrelIndex* otherIndex) {
        this->data.extendAndInsert(otherIndex->data);
    }
};

/**
 * A BtreeDelete index
 */
template <std::size_t _Arity>
class BtreeDeleteIndex : public interpreter::Index<_Arity, BtreeDelete> {
public:
    using Index<_Arity, BtreeDelete>::Index;
    using Index<_Arity, BtreeDelete>::data;
    using Index<_Arity, BtreeDelete>::order;
    using Tuple = typename souffle::Tuple<RamDomain, _Arity>;

    /**
     * Erase a tuple from this index.
     */
    bool erase(const Tuple& tuple) {
        return data.erase(order.encode(tuple)) > 0;
    }
};

}  // namespace souffle::interpreter
