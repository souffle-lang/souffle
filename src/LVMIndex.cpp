/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file LVMIndex.cpp
 *
 * LVM index with generic interface.
 *
 ***********************************************************************/

#include "LVMIndex.h"
#include "CompiledIndexUtils.h"

namespace souffle {

std::ostream& operator<<(std::ostream& out, TupleRef ref) {
    out << "[";
    for (std::size_t i = 0; i < ref.size(); i++) {
        if (i > 0) {
            out << ',';
        }
        out << ref[i];
    }
    return out << "]";
}

Order Order::create(size_t arity) {
    Order res;
    res.order.resize(arity);
    for (size_t i = 0; i < arity; i++) {
        res.order[i] = i;
    }
    return res;
}

std::size_t Order::size() const {
    return order.size();
}

bool Order::valid() const {
    // Check that all indices are in range.
    for (int i : order) {
        if (i < 0 || i >= int(order.size())) {
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

bool Order::operator==(const Order& other) const {
    return order == other.order;
}

bool Order::operator!=(const Order& other) const {
    return !(*this == other);
}

bool Order::operator<(const Order& other) const {
    return order < other.order;
}

std::ostream& operator<<(std::ostream& out, const Order& order) {
    return out << "[" << join(order.order) << "]";
}

/**
 * An index wrapper for nullary indexes. For those, no complex
 * nested data structure is required.
 */
class NullaryIndex : public LVMIndex {
    // indicates whether the one single element is present or not.
    bool present;

    // a source adaptation, iterating through the optionally present
    // entry in this relation.
    class Source : public Stream::Source {
        bool present;

    public:
        Source(bool present) : present(present) {}
        int load(TupleRef* buffer, int max) override {
            if (!present) {
                return 0;
            }
            buffer[0] = TupleRef(nullptr, 0);
            present = false;
            return 1;
        }

        std::unique_ptr<Stream::Source> clone() override {
            return std::unique_ptr<Stream::Source>(new Source(present));
        }
    };

public:
    size_t getArity() const override {
        return 0;
    }

    bool empty() const override {
        return !present;
    }

    std::size_t size() const override {
        return present ? 1 : 0;
    }

    bool insert(const TupleRef& tuple) override {
        assert(tuple.size() == 0);
        bool res = present;
        present = true;
        return res;
    }

    void insert(const LVMIndex& src) override {
        assert(src.getArity() == 0);
        present = present | !src.empty();
    }

    bool contains(const TupleRef& tuple) const override {
        assert(tuple.size() == 0);
        return present;
    }

    Stream scan() const override {
        return std::make_unique<Source>(present);
    }

    Stream range(const TupleRef& low, const TupleRef& high) const override {
        return scan();
    }

    void clear() override {
        present = false;
    }
};

/**
 * A generic data structure index adapter handling the boundary
 * level order conversion as well as iteration through nested
 * data structures.
 *
 * @tparam Structure the structure to be utilized
 */
template <typename Structure>
class GenericIndex : public LVMIndex {
    using Entry = typename Structure::element_type;
    static constexpr int Arity = Entry::arity;

    // the order to be simulated
    Order order;

    // the internal data structure
    Structure data;

    // a source adapter for streaming through data
    class Source : public Stream::Source {
        const Order& order;

        // the begin and end of the stream
        using iter = typename Structure::iterator;
        iter cur;
        iter end;

        // an internal buffer for re-ordered elements
        std::array<Entry, Stream::BUFFER_SIZE> buffer;

    public:
        Source(const Order& order, iter begin, iter end) : order(order), cur(begin), end(end) {}

        int load(TupleRef* out, int max) override {
            int c = 0;
            while (cur != end && c < max) {
                buffer[c] = order.decode(*cur);
                out[c] = buffer[c];
                ++cur;
                ++c;
            }
            return c;
        }

        std::unique_ptr<Stream::Source> clone() override {
            Source* source = new Source(order, cur, end);
            source->buffer = this->buffer;
            return std::unique_ptr<Stream::Source>(source);
        }
    };

public:
    GenericIndex(const Order& order) : order(order) {}

    size_t getArity() const override {
        return Arity;
    }

    bool empty() const override {
        return data.empty();
    }

    std::size_t size() const override {
        return data.size();
    }

    bool insert(const TupleRef& tuple) override {
        return data.insert(order.encode(tuple.asTuple<Arity>()));
    }

    void insert(const LVMIndex& src) override {
        // TODO: make smarter
        for (const auto& cur : src.scan()) {
            insert(cur);
        }
    }

    bool contains(const TupleRef& tuple) const override {
        return data.contains(order.encode(tuple.asTuple<Arity>()));
    }

    Stream scan() const override {
        return std::make_unique<Source>(order, data.begin(), data.end());
    }

    Stream range(const TupleRef& low, const TupleRef& high) const override {
        Entry a = order.encode(low.asTuple<Arity>());
        Entry b = order.encode(high.asTuple<Arity>());
        // Transfer upper_bound to a equivalent lower bound
        for (size_t i = Arity; i-- > 0;) {
            if (a[i] == MIN_RAM_DOMAIN && b[i] == MAX_RAM_DOMAIN) {
                b[i] = MIN_RAM_DOMAIN;
                continue;
            }
            if (a[i] == b[i]) {
                b[i] += 1;
                break;
            }
        }
        return std::make_unique<Source>(order, data.lower_bound(a), data.lower_bound(b));
    }

    void clear() override {
        data.clear();
    }
};

/* B-Tree Indirect indexes */
class IndirectIndex : public LVMIndex {
public:
    /* lexicographical comparison operation on two tuple pointers */
    struct comparator {
        const std::vector<int> order;

        /* constructor to initialize state */
        comparator(std::vector<int> order) : order(std::move(order)) {}

        /* comparison function */
        int operator()(const TupleRef& x, const TupleRef& y) const {
            for (auto& i : order) {
                if (x[i] < y[i]) {
                    return -1;
                }
                if (x[i] > y[i]) {
                    return 1;
                }
            }
            return 0;
        }

        /* less comparison */
        bool less(const TupleRef& x, const TupleRef& y) const {
            return operator()(x, y) < 0;
        }

        /* equal comparison */
        bool equal(const TupleRef& x, const TupleRef& y) const {
            for (auto& i : order) {
                if (x[i] != y[i]) {
                    return false;
                }
            }
            return true;
        }
    };

    class Source : public Stream::Source {
        // the begin and end of the stream
        using iter = btree_multiset<TupleRef, comparator, std::allocator<TupleRef>, 512>::iterator;
        iter cur;
        iter end;

        // an internal buffer for re-ordered elements
        std::array<TupleRef, Stream::BUFFER_SIZE> buffer;

    public:
        Source(iter begin, iter end) : cur(begin), end(end) {}

        int load(TupleRef* out, int max) override {
            int c = 0;
            while (cur != end && c < max) {
                buffer[c] = *cur;
                out[c] = buffer[c];
                ++cur;
                ++c;
            }
            return c;
        }

        std::unique_ptr<Stream::Source> clone() override {
            Source* source = new Source(cur, end);
            source->buffer = this->buffer;
            return std::unique_ptr<Stream::Source>(source);
        }
    };

    /* btree for storing tuple pointers with a given lexicographical order */
    using index_set = btree_multiset<TupleRef, comparator, std::allocator<TupleRef>, 512>;

    IndirectIndex(std::vector<int> order)
            : theOrder(std::move(order)), set(comparator(theOrder), comparator(theOrder)),
              arity(order.size()) {}

    size_t getArity() const override {
        return arity;
    };

    bool empty() const override {
        return set.empty();
    }

    size_t size() const override {
        return set.size();
    }

    bool insert(const TupleRef& tuple) override {
        return set.insert(tuple, operation_hints);
    }

    void insert(const LVMIndex& src) override {
        for (const auto& cur : src.scan()) {
            insert(cur);
        }
    }

    bool contains(const TupleRef& tuple) const override {
        return set.contains(tuple);
    }

    Stream scan() const override {
        return std::make_unique<Source>(set.begin(), set.end());
    }

    Stream range(const TupleRef& low, const TupleRef& high) const override {
        return std::make_unique<Source>(set.lower_bound(low), set.upper_bound(high));
    }

    void clear() override {
        set.clear();
    }

private:
    /** retain the index order used to construct an object of this class */
    const std::vector<int> theOrder;

    /** set storing tuple pointers of table */
    index_set set;

    /** Operation hints */
    index_set::btree_operation_hints<1> operation_hints;

    /** Arity as the relation arity, not necessary the order size in indirect index */
    size_t arity;
};

// The comparator to be used for B-tree nodes.
template <std::size_t Arity>
using comparator = typename ram::index_utils::get_full_index<Arity>::type::comparator;

/**
 * A index adapter for B-trees, using the generic index adapter.
 */
template <std::size_t Arity>
class BTreeIndex : public GenericIndex<btree_set<ram::Tuple<RamDomain, Arity>, comparator<Arity>>> {
public:
    using GenericIndex<btree_set<ram::Tuple<RamDomain, Arity>, comparator<Arity>>>::GenericIndex;
};

/**
 * A index adapter for Bries, using the generic index adapter.
 */
template <std::size_t Arity>
class BrieIndex : public GenericIndex<Trie<Arity>> {
public:
    using GenericIndex<Trie<Arity>>::GenericIndex;
};

std::unique_ptr<LVMIndex> createBTreeIndex(const Order& order) {
    switch (order.size()) {
        case 0:
            return std::make_unique<NullaryIndex>();
        case 1:
            return std::make_unique<BTreeIndex<1>>(order);
        case 2:
            return std::make_unique<BTreeIndex<2>>(order);
        case 3:
            return std::make_unique<BTreeIndex<3>>(order);
        case 4:
            return std::make_unique<BTreeIndex<4>>(order);
        case 5:
            return std::make_unique<BTreeIndex<5>>(order);
        case 6:
            return std::make_unique<BTreeIndex<6>>(order);
        case 7:
            return std::make_unique<BTreeIndex<7>>(order);
        case 8:
            return std::make_unique<BTreeIndex<8>>(order);
        case 9:
            return std::make_unique<BTreeIndex<9>>(order);
        case 10:
            return std::make_unique<BTreeIndex<10>>(order);
        case 11:
            return std::make_unique<BTreeIndex<11>>(order);
        case 12:
            return std::make_unique<BTreeIndex<12>>(order);
    }
    assert(false && "Requested arity not yet supported. Feel free to add it.");
}

std::unique_ptr<LVMIndex> createBrieIndex(const Order& order) {
    switch (order.size()) {
        case 0:
            return std::make_unique<NullaryIndex>();
        case 1:
            return std::make_unique<BrieIndex<1>>(order);
        case 2:
            return std::make_unique<BrieIndex<2>>(order);
        case 3:
            return std::make_unique<BrieIndex<3>>(order);
        case 4:
            return std::make_unique<BrieIndex<4>>(order);
        case 5:
            return std::make_unique<BrieIndex<5>>(order);
        case 6:
            return std::make_unique<BrieIndex<6>>(order);
        case 7:
            return std::make_unique<BrieIndex<7>>(order);
        case 8:
            return std::make_unique<BrieIndex<8>>(order);
        case 9:
            return std::make_unique<BrieIndex<9>>(order);
        case 10:
            return std::make_unique<BrieIndex<10>>(order);
        case 11:
            return std::make_unique<BrieIndex<11>>(order);
        case 12:
            return std::make_unique<BrieIndex<12>>(order);
    }
    assert(false && "Requested arity not yet supported. Feel free to add it.");
}

std::unique_ptr<LVMIndex> createIndirectIndex(const Order& order) {
    assert(order.size() != 0 && "IndirectIndex does not work with nullary relation\n");
    return std::make_unique<IndirectIndex>(order.getOrder());
}

}  // namespace souffle
