/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file InterpreterIndex.cpp
 *
 * Interpreter index with generic interface.
 *
 ***********************************************************************/

#include "interpreter/InterpreterIndex.h"
#include "BTree.h"
#include "Brie.h"
#include "EquivalenceRelation.h"
#include "PiggyList.h"
#include "utility/ContainerUtil.h"
#include "utility/MiscUtil.h"
#include "utility/StreamUtil.h"
#include <algorithm>
#include <atomic>
#include <ostream>

namespace souffle {

/**
 * A namespace enclosing utilities required by indices.
 */
namespace index_utils {

// -------- generic tuple comparator ----------

template <unsigned... Columns>
struct comparator;

template <unsigned First, unsigned... Rest>
struct comparator<First, Rest...> {
    template <typename T>
    int operator()(const T& a, const T& b) const {
        return (a[First] < b[First]) ? -1 : ((a[First] > b[First]) ? 1 : comparator<Rest...>()(a, b));
    }
    template <typename T>
    bool less(const T& a, const T& b) const {
        return a[First] < b[First] || (a[First] == b[First] && comparator<Rest...>().less(a, b));
    }
    template <typename T>
    bool equal(const T& a, const T& b) const {
        return a[First] == b[First] && comparator<Rest...>().equal(a, b);
    }
};

template <>
struct comparator<> {
    template <typename T>
    int operator()(const T&, const T&) const {
        return 0;
    }
    template <typename T>
    bool less(const T&, const T&) const {
        return false;
    }
    template <typename T>
    bool equal(const T&, const T&) const {
        return true;
    }
};

}  // namespace index_utils

/**
 * The index class is utilized as a template-meta-programming structure
 * to specify and realize indices.
 *
 * @tparam Columns ... the order in which elements of the relation to be indexed
 * 				shell be considered by this index.
 */
template <unsigned... Columns>
struct index {
    // the comparator associated to this index
    using comparator = index_utils::comparator<Columns...>;
};

/**
 * A namespace enclosing utilities required relations to handle indices.
 */
namespace index_utils {

// -- a utility extending a given index by another column --
//   e.g. index<1,0>   =>    index<1,0,2>

template <typename Index, unsigned column>
struct extend;

template <unsigned... Columns, unsigned Col>
struct extend<index<Columns...>, Col> {
    using type = index<Columns..., Col>;
};

// -- obtains a full index for a given arity --

template <unsigned arity>
struct get_full_index {
    using type = typename extend<typename get_full_index<arity - 1>::type, arity - 1>::type;
};

template <>
struct get_full_index<0> {
    using type = index<>;
};

}  // namespace index_utils

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
class NullaryIndex : public InterpreterIndex {
    // indicates whether the one single element is present or not.
    std::atomic<bool> present = false;

    // a source adaptation, iterating through the optionally present
    // entry in this relation.
    class Source : public Stream::Source {
        bool present;

    public:
        Source(bool present) : present(present) {}
        int load(TupleRef* buffer, int /* max */) override {
            if (!present) {
                return 0;
            }
            buffer[0] = TupleRef(nullptr, 0);
            present = false;
            return 1;
        }

        int reload(TupleRef* buffer, int /* max */) override {
            if (!present) {
                return 0;
            }
            buffer[0] = TupleRef(nullptr, 0);
            return 1;
        }

        std::unique_ptr<Stream::Source> clone() override {
            return std::make_unique<Source>(present);
        }
    };

    // The nullary index view -- does not require any hints.
    struct NullaryIndexView : public IndexView {
        const NullaryIndex& index;

        NullaryIndexView(const NullaryIndex& index) : index(index) {}

        bool contains(const TupleRef& entry) const override {
            return index.contains(entry);
        }

        bool contains(const TupleRef& low, const TupleRef& high) const override {
            return index.contains(low, high);
        }

        Stream range(const TupleRef& low, const TupleRef& high) const override {
            return index.range(low, high);
        }

        size_t getArity() const override {
            return 0;
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

    IndexViewPtr createView() const override {
        return std::make_unique<NullaryIndexView>(*this);
    }

    bool insert(const TupleRef& tuple) override {
        assert(tuple.size() == 0);
        bool res = present;
        present = true;
        return res;
    }

    void insert(const InterpreterIndex& src) override {
        assert(src.getArity() == 0);
        present = present | !src.empty();
    }

    bool contains(const TupleRef& tuple) const override {
        assert(tuple.size() == 0);
        return present;
    }

    bool contains(const TupleRef&, const TupleRef&) const override {
        return present;
    }

    Stream scan() const override {
        return std::make_unique<Source>(present);
    }

    PartitionedStream partitionScan(int) const override {
        std::vector<Stream> res;
        res.push_back(scan());
        return res;
    }

    Stream range(const TupleRef& /* low */, const TupleRef& /* high */) const override {
        return scan();
    }

    PartitionedStream partitionRange(const TupleRef& low, const TupleRef& high, int) const override {
        std::vector<Stream> res;
        res.push_back(range(low, high));
        return res;
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
class GenericIndex : public InterpreterIndex {
protected:
    using Entry = typename Structure::element_type;
    using Hints = typename Structure::operation_hints;
    static constexpr int Arity = Entry::arity;

    // the order to be simulated
    Order order;

    // the internal data structure
    Structure data;

    using iter = typename Structure::iterator;

    // a source adapter for streaming through data
    class Source : public Stream::Source {
        const Order& order;

        // the begin and end of the stream
        iter cur;
        iter end;

        // an internal buffer for re-ordered elements
        std::array<Entry, Stream::BUFFER_SIZE> buffer;

    public:
        Source(const Order& order, iter begin, iter end)
                : order(order), cur(std::move(begin)), end(std::move(end)) {}

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

        int reload(TupleRef* out, int max) override {
            int c = 0;
            max = std::min(max, Stream::BUFFER_SIZE);
            while (c < max) {
                out[c] = buffer[c];
                ++c;
            }
            return c;
        }

        std::unique_ptr<Stream::Source> clone() override {
            auto source = std::make_unique<Source>(order, cur, end);
            source->buffer = this->buffer;
            return source;
        }
    };

    virtual souffle::range<iter> bounds(const TupleRef& low, const TupleRef& high, Hints& hints) const {
        Entry a = order.encode(low.asTuple<Arity>());
        Entry b = order.encode(high.asTuple<Arity>());
        return {data.lower_bound(a, hints), data.upper_bound(b, hints)};
    }

    // The index view associated to this view type.
    struct GenericIndexView : public IndexView {
        const GenericIndex& index;
        mutable Hints hints;

        GenericIndexView(const GenericIndex& index) : index(index) {}

        bool contains(const TupleRef& tuple) const override {
            return index.data.contains(index.order.encode(tuple.asTuple<Arity>()), hints);
        }

        bool contains(const TupleRef& low, const TupleRef& high) const override {
            return !index.bounds(low, high, hints).empty();
        }

        Stream range(const TupleRef& low, const TupleRef& high) const override {
            auto range = index.bounds(low, high, hints);
            return std::make_unique<Source>(index.order, range.begin(), range.end());
        }

        size_t getArity() const override {
            return Arity;
        }
    };

public:
    GenericIndex(Order order) : order(std::move(order)) {}

    IndexViewPtr createView() const override {
        return std::make_unique<GenericIndexView>(*this);
    }

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

    void insert(const InterpreterIndex& src) override {
        // TODO: make smarter
        for (const auto& cur : src.scan()) {
            insert(cur);
        }
    }

    bool contains(const TupleRef& tuple) const override {
        return GenericIndexView(*this).contains(tuple);
    }

    bool contains(const TupleRef& low, const TupleRef& high) const override {
        return GenericIndexView(*this).contains(low, high);
    }

    Stream scan() const override {
        return std::make_unique<Source>(order, data.begin(), data.end());
    }

    PartitionedStream partitionScan(int partitionCount) const override {
        auto chunks = data.partition(partitionCount);
        std::vector<Stream> res;
        res.reserve(chunks.size());
        for (const auto& cur : chunks) {
            res.push_back(std::make_unique<Source>(order, cur.begin(), cur.end()));
        }
        return res;
    }

    Stream range(const TupleRef& low, const TupleRef& high) const override {
        return GenericIndexView(*this).range(low, high);
    }

    PartitionedStream partitionRange(
            const TupleRef& low, const TupleRef& high, int partitionCount) const override {
        Hints hints;
        auto range = bounds(low, high, hints);
        std::vector<Stream> res;
        res.reserve(partitionCount);
        for (const auto& cur : range.partition(partitionCount)) {
            res.push_back(std::make_unique<Source>(order, cur.begin(), cur.end()));
        }
        return res;
    }

    void clear() override {
        data.clear();
    }
};

/* B-Tree Indirect indexes */
class IndirectIndex : public InterpreterIndex {
public:
    using AttributeIndex = uint32_t;
    using AttributeOrder = std::vector<AttributeIndex>;
    /* lexicographical comparison operation on two tuple pointers */
    struct comparator {
        const AttributeOrder order;

        /* constructor to initialize state */
        comparator(AttributeOrder order) : order(std::move(order)) {}

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

    /* btree for storing tuple pointers with a given lexicographical order */
    using index_set = btree_multiset<TupleRef, comparator, std::allocator<TupleRef>, 512>;
    using Hints = typename index_set::operation_hints;

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

        int reload(TupleRef* out, int max) override {
            int c = 0;
            max = std::min(max, Stream::BUFFER_SIZE);
            while (c < max) {
                out[c] = buffer[c];
                ++c;
            }
            return c;
        }

        std::unique_ptr<Stream::Source> clone() override {
            auto* source = new Source(cur, end);
            source->buffer = this->buffer;
            return std::unique_ptr<Stream::Source>(source);
        }
    };

    // The index view associated to this view type.
    struct IndirectIndexView : public IndexView {
        const IndirectIndex& index;
        mutable Hints hints;

        IndirectIndexView(const IndirectIndex& index) : index(index) {}

        bool contains(const TupleRef& tuple) const override {
            return index.set.contains(tuple, hints);
        }

        bool contains(const TupleRef& /* low */, const TupleRef& /* high */) const override {
            fatal("Not implemented!");
        }

        Stream range(const TupleRef& low, const TupleRef& high) const override {
            return std::make_unique<Source>(
                    index.set.lower_bound(low, hints), index.set.upper_bound(high, hints));
        }

        size_t getArity() const override {
            return index.getArity();
        }
    };

    IndirectIndex(AttributeOrder order)
            : theOrder(std::move(order)), set(comparator(theOrder), comparator(theOrder)),
              arity(order.size()) {}

    IndexViewPtr createView() const override {
        return std::make_unique<IndirectIndexView>(*this);
    }

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

    void insert(const InterpreterIndex& src) override {
        for (const auto& cur : src.scan()) {
            insert(cur);
        }
    }

    bool contains(const TupleRef& tuple) const override {
        return IndirectIndexView(*this).contains(tuple);
    }

    bool contains(const TupleRef& low, const TupleRef& high) const override {
        return IndirectIndexView(*this).contains(low, high);
    }

    Stream scan() const override {
        return std::make_unique<Source>(set.begin(), set.end());
    }

    PartitionedStream partitionScan(int) const override {
        assert(false && "Does only produce a single subset!");
        std::vector<Stream> res;
        res.push_back(scan());
        return res;
    }

    Stream range(const TupleRef& low, const TupleRef& high) const override {
        return IndirectIndexView(*this).range(low, high);
    }

    PartitionedStream partitionRange(const TupleRef& low, const TupleRef& high, int) const override {
        assert(false && "Does only produce a single subset!");
        std::vector<Stream> res;
        res.push_back(range(low, high));
        return res;
    }

    void clear() override {
        set.clear();
    }

private:
    /** retain the index order used to construct an object of this class */
    const AttributeOrder theOrder;

    /** set storing tuple pointers of table */
    index_set set;

    /** Operation hints */
    index_set::btree_operation_hints<1> operation_hints;

    /** Arity as the relation arity, not necessary the order size in indirect index */
    size_t arity;
};

// The comparator to be used for B-tree nodes.
template <std::size_t Arity>
using comparator = typename index_utils::get_full_index<Arity>::type::comparator;

// Node type
template <std::size_t Arity>
using t_tuple = typename souffle::Tuple<RamDomain, Arity>;

// Updater for Provenance
template <std::size_t Arity>
struct InterpreterProvenanceUpdater {
    void update(t_tuple<Arity>& old_t, const t_tuple<Arity>& new_t) {
        old_t[Arity - 2] = new_t[Arity - 2];
        old_t[Arity - 1] = new_t[Arity - 1];
    }
};

/**
 * A index adapter for B-trees, using the generic index adapter.
 */
template <std::size_t Arity>
class BTreeIndex : public GenericIndex<btree_set<t_tuple<Arity>, comparator<Arity>>> {
public:
    using GenericIndex<btree_set<t_tuple<Arity>, comparator<Arity>>>::GenericIndex;
};

/**
 * Btree index for provenance relation
 */
template <std::size_t Arity>
class BTreeProvenanceIndex
        : public GenericIndex<btree_set<t_tuple<Arity>, comparator<Arity>, std::allocator<t_tuple<Arity>>,
                  256, typename detail::default_strategy<t_tuple<Arity>>::type, comparator<Arity - 2>,
                  InterpreterProvenanceUpdater<Arity>>> {
public:
    using GenericIndex<btree_set<t_tuple<Arity>, comparator<Arity>, std::allocator<t_tuple<Arity>>, 256,
            typename detail::default_strategy<t_tuple<Arity>>::type, comparator<Arity - 2>,
            InterpreterProvenanceUpdater<Arity>>>::GenericIndex;
};

/**
 * A index adapter for Bries, using the generic index adapter.
 */
template <std::size_t Arity>
class BrieIndex : public GenericIndex<Trie<Arity>> {
public:
    using GenericIndex<Trie<Arity>>::GenericIndex;
};

/**
 * A index adapter for EquivalenceRelation, using the generic index adapter.
 */
class EqrelIndex : public GenericIndex<EquivalenceRelation<t_tuple<2>>> {
public:
    using GenericIndex<EquivalenceRelation<t_tuple<2>>>::GenericIndex;

    void extend(InterpreterIndex* other) override {
        auto otherIndex = dynamic_cast<EqrelIndex*>(other);
        assert(otherIndex != nullptr && "Can only extend to EqrelIndex");
        this->data.extend(otherIndex->data);
    }
};

std::unique_ptr<InterpreterIndex> createBTreeIndex(const Order& order) {
    switch (order.size()) {
        case 0: return std::make_unique<NullaryIndex>();
        case 1: return std::make_unique<BTreeIndex<1>>(order);
        case 2: return std::make_unique<BTreeIndex<2>>(order);
        case 3: return std::make_unique<BTreeIndex<3>>(order);
        case 4: return std::make_unique<BTreeIndex<4>>(order);
        case 5: return std::make_unique<BTreeIndex<5>>(order);
        case 6: return std::make_unique<BTreeIndex<6>>(order);
        case 7: return std::make_unique<BTreeIndex<7>>(order);
        case 8: return std::make_unique<BTreeIndex<8>>(order);
        case 9: return std::make_unique<BTreeIndex<9>>(order);
        case 10: return std::make_unique<BTreeIndex<10>>(order);
        case 11: return std::make_unique<BTreeIndex<11>>(order);
        case 12: return std::make_unique<BTreeIndex<12>>(order);
        case 13: return std::make_unique<BTreeIndex<13>>(order);
        case 14: return std::make_unique<BTreeIndex<14>>(order);
        case 15: return std::make_unique<BTreeIndex<15>>(order);
        case 16: return std::make_unique<BTreeIndex<16>>(order);
        case 17: return std::make_unique<BTreeIndex<17>>(order);
        case 18: return std::make_unique<BTreeIndex<18>>(order);
        case 19: return std::make_unique<BTreeIndex<19>>(order);
        case 20: return std::make_unique<BTreeIndex<20>>(order);
    }

    fatal("Requested arity not yet supported. Feel free to add it.");
}

std::unique_ptr<InterpreterIndex> createBTreeProvenanceIndex(const Order& order) {
    switch (order.size()) {
        case 0:
        case 1: fatal("Provenance relation with arity < 2.");
        case 2: return std::make_unique<BTreeProvenanceIndex<2>>(order);
        case 3: return std::make_unique<BTreeProvenanceIndex<3>>(order);
        case 4: return std::make_unique<BTreeProvenanceIndex<4>>(order);
        case 5: return std::make_unique<BTreeProvenanceIndex<5>>(order);
        case 6: return std::make_unique<BTreeProvenanceIndex<6>>(order);
        case 7: return std::make_unique<BTreeProvenanceIndex<7>>(order);
        case 8: return std::make_unique<BTreeProvenanceIndex<8>>(order);
        case 9: return std::make_unique<BTreeProvenanceIndex<9>>(order);
        case 10: return std::make_unique<BTreeProvenanceIndex<10>>(order);
        case 11: return std::make_unique<BTreeProvenanceIndex<11>>(order);
        case 12: return std::make_unique<BTreeProvenanceIndex<12>>(order);
        case 13: return std::make_unique<BTreeProvenanceIndex<13>>(order);
        case 14: return std::make_unique<BTreeProvenanceIndex<14>>(order);
        case 15: return std::make_unique<BTreeProvenanceIndex<15>>(order);
        case 16: return std::make_unique<BTreeProvenanceIndex<16>>(order);
        case 17: return std::make_unique<BTreeProvenanceIndex<17>>(order);
        case 18: return std::make_unique<BTreeProvenanceIndex<18>>(order);
        case 19: return std::make_unique<BTreeProvenanceIndex<19>>(order);
        case 20: return std::make_unique<BTreeProvenanceIndex<20>>(order);
        case 21: return std::make_unique<BTreeProvenanceIndex<21>>(order);
        case 22: return std::make_unique<BTreeProvenanceIndex<22>>(order);
        case 23: return std::make_unique<BTreeProvenanceIndex<23>>(order);
        case 24: return std::make_unique<BTreeProvenanceIndex<24>>(order);
        case 25: return std::make_unique<BTreeProvenanceIndex<25>>(order);
        case 26: return std::make_unique<BTreeProvenanceIndex<26>>(order);
        case 27: return std::make_unique<BTreeProvenanceIndex<27>>(order);
        case 28: return std::make_unique<BTreeProvenanceIndex<28>>(order);
        case 29: return std::make_unique<BTreeProvenanceIndex<29>>(order);
        case 30: return std::make_unique<BTreeProvenanceIndex<30>>(order);
    }

    fatal("Requested arity not yet supported. Feel free to add it.");
}

std::unique_ptr<InterpreterIndex> createBrieIndex(const Order& order) {
    switch (order.size()) {
        case 0: return std::make_unique<NullaryIndex>();
        case 1: return std::make_unique<BrieIndex<1>>(order);
        case 2: return std::make_unique<BrieIndex<2>>(order);
        case 3: return std::make_unique<BrieIndex<3>>(order);
        case 4: return std::make_unique<BrieIndex<4>>(order);
        case 5: return std::make_unique<BrieIndex<5>>(order);
        case 6: return std::make_unique<BrieIndex<6>>(order);
        case 7: return std::make_unique<BrieIndex<7>>(order);
        case 8: return std::make_unique<BrieIndex<8>>(order);
        case 9: return std::make_unique<BrieIndex<9>>(order);
        case 10: return std::make_unique<BrieIndex<10>>(order);
        case 11: return std::make_unique<BrieIndex<11>>(order);
        case 12: return std::make_unique<BrieIndex<12>>(order);
        case 13: return std::make_unique<BrieIndex<13>>(order);
        case 14: return std::make_unique<BrieIndex<14>>(order);
        case 15: return std::make_unique<BrieIndex<15>>(order);
        case 16: return std::make_unique<BrieIndex<16>>(order);
        case 17: return std::make_unique<BrieIndex<17>>(order);
        case 18: return std::make_unique<BrieIndex<18>>(order);
        case 19: return std::make_unique<BrieIndex<19>>(order);
        case 20: return std::make_unique<BrieIndex<20>>(order);
    }

    fatal("Requested arity not yet supported. Feel free to add it.");
}

std::unique_ptr<InterpreterIndex> createIndirectIndex(const Order& order) {
    assert(order.size() != 0 && "IndirectIndex does not work with nullary relation\n");
    return std::make_unique<IndirectIndex>(order.getOrder());
}

std::unique_ptr<InterpreterIndex> createEqrelIndex(const Order& order) {
    assert(order.size() == 2 && "Eqrel index must have tuple of 2 arities");
    return std::make_unique<EqrelIndex>(order);
}

}  // namespace souffle
