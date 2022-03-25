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

#include "souffle/RamTypes.h"
#include "souffle/RecordTable.h"
#include "souffle/SignalHandler.h"
#include "souffle/SouffleInterface.h"
#include "souffle/SymbolTable.h"
#include "souffle/datastructure/BTreeDelete.h"
#include "souffle/datastructure/Brie.h"
#include "souffle/datastructure/EquivalenceRelation.h"
#include "souffle/datastructure/RecordTableImpl.h"
#include "souffle/datastructure/SymbolTableImpl.h"
#include "souffle/datastructure/Table.h"
#include "souffle/io/IOSystem.h"
#include "souffle/io/WriteStream.h"
#include "souffle/utility/EvaluatorUtil.h"
#ifndef __EMBEDDED_SOUFFLE__
#include "souffle/CompiledOptions.h"
#endif

#if defined(_OPENMP)
#include <omp.h>
#endif

namespace souffle {

extern "C" {
inline souffle::SouffleProgram* getInstance(const char* p) {
    return souffle::ProgramFactory::newInstance(p);
}
}

/**
 * Relation wrapper used internally in the generated Datalog program
 */
template <class RelType>
class RelationWrapper : public souffle::Relation {
public:
    static constexpr arity_type Arity = RelType::Arity;
    using TupleType = Tuple<RamDomain, Arity>;
    using AttrStrSeq = std::array<const char*, Arity>;

private:
    RelType& relation;
    SouffleProgram& program;
    std::string name;
    AttrStrSeq attrTypes;
    AttrStrSeq attrNames;
    const uint32_t id;
    const arity_type numAuxAttribs;

    // NB: internal wrapper. does not satisfy the `iterator` concept.
    class iterator_wrapper : public iterator_base {
        typename RelType::iterator it;
        const Relation* relation;
        tuple t;

    public:
        iterator_wrapper(uint32_t arg_id, const Relation* rel, typename RelType::iterator arg_it)
                : iterator_base(arg_id), it(std::move(arg_it)), relation(rel), t(rel) {}
        void operator++() override {
            ++it;
        }
        tuple& operator*() override {
            auto&& value = *it;
            t.rewind();
            for (std::size_t i = 0; i < Arity; i++)
                t[i] = value[i];
            return t;
        }
        iterator_base* clone() const override {
            return new iterator_wrapper(*this);
        }

    protected:
        bool equal(const iterator_base& o) const override {
            const auto& casted = asAssert<iterator_wrapper>(o);
            return it == casted.it;
        }
    };

public:
    RelationWrapper(uint32_t id, RelType& r, SouffleProgram& p, std::string name, const AttrStrSeq& t,
            const AttrStrSeq& n, arity_type numAuxAttribs)
            : relation(r), program(p), name(std::move(name)), attrTypes(t), attrNames(n), id(id),
              numAuxAttribs(numAuxAttribs) {}

    iterator begin() const override {
        return iterator(mk<iterator_wrapper>(id, this, relation.begin()));
    }
    iterator end() const override {
        return iterator(mk<iterator_wrapper>(id, this, relation.end()));
    }

    void insert(const tuple& arg) override {
        TupleType t;
        assert(&arg.getRelation() == this && "wrong relation");
        assert(arg.size() == Arity && "wrong tuple arity");
        for (std::size_t i = 0; i < Arity; i++) {
            t[i] = arg[i];
        }
        relation.insert(t);
    }
    bool contains(const tuple& arg) const override {
        TupleType t;
        assert(arg.size() == Arity && "wrong tuple arity");
        for (std::size_t i = 0; i < Arity; i++) {
            t[i] = arg[i];
        }
        return relation.contains(t);
    }
    std::size_t size() const override {
        return relation.size();
    }
    std::string getName() const override {
        return name;
    }
    const char* getAttrType(std::size_t arg) const override {
        assert(arg < Arity && "attribute out of bound");
        return attrTypes[arg];
    }
    const char* getAttrName(std::size_t arg) const override {
        assert(arg < Arity && "attribute out of bound");
        return attrNames[arg];
    }
    arity_type getArity() const override {
        return Arity;
    }
    arity_type getAuxiliaryArity() const override {
        return numAuxAttribs;
    }
    SymbolTable& getSymbolTable() const override {
        return program.getSymbolTable();
    }

    /** Eliminate all the tuples in relation*/
    void purge() override {
        relation.purge();
    }
};

/** Nullary relations */
class t_nullaries {
private:
    std::atomic<bool> data{false};

public:
    static constexpr Relation::arity_type Arity = 0;

    t_nullaries() = default;
    using t_tuple = Tuple<RamDomain, 0>;
    struct context {};
    context createContext() {
        return context();
    }
    class iterator {
        bool value;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = RamDomain*;
        using difference_type = ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        iterator(bool v = false) : value(v) {}

        const RamDomain* operator*() {
            return nullptr;
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
    void insert(const t_tuple& /* t */) {
        data = true;
    }
    void insert(const t_tuple& /* t */, context& /* ctxt */) {
        data = true;
    }
    void insert(const RamDomain* /* ramDomain */) {
        data = true;
    }
    bool insert() {
        bool result = data;
        data = true;
        return !result;
    }
    bool contains(const t_tuple& /* t */) const {
        return data;
    }
    bool contains(const t_tuple& /* t */, context& /* ctxt */) const {
        return data;
    }
    std::size_t size() const {
        return data ? 1 : 0;
    }
    bool empty() const {
        return !data;
    }
    void purge() {
        data = false;
    }
    void printStatistics(std::ostream& /* o */) const {}
};

/** Info relations */
template <Relation::arity_type Arity_>
class t_info {
public:
    static constexpr Relation::arity_type Arity = Arity_;

    t_info() = default;
    using t_tuple = Tuple<RamDomain, Arity>;
    struct context {};
    context createContext() {
        return context();
    }
    class iterator : public std::iterator<std::forward_iterator_tag, Tuple<RamDomain, Arity>> {
        typename std::vector<Tuple<RamDomain, Arity>>::const_iterator it;

    public:
        iterator(const typename std::vector<t_tuple>::const_iterator& o) : it(o) {}

        const t_tuple operator*() {
            return *it;
        }

        bool operator==(const iterator& other) const {
            return other.it == it;
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

        iterator& operator++() {
            it++;
            return *this;
        }
    };
    iterator begin() const {
        return iterator(data.begin());
    }
    iterator end() const {
        return iterator(data.end());
    }
    void insert(const t_tuple& t) {
        insert_lock.lock();
        if (!contains(t)) {
            data.push_back(t);
        }
        insert_lock.unlock();
    }
    void insert(const t_tuple& t, context& /* ctxt */) {
        insert(t);
    }
    void insert(const RamDomain* ramDomain) {
        insert_lock.lock();
        t_tuple t;
        for (std::size_t i = 0; i < Arity; ++i) {
            t.data[i] = ramDomain[i];
        }
        data.push_back(t);
        insert_lock.unlock();
    }
    bool contains(const t_tuple& t) const {
        for (const auto& o : data) {
            if (t == o) {
                return true;
            }
        }
        return false;
    }
    bool contains(const t_tuple& t, context& /* ctxt */) const {
        return contains(t);
    }
    std::size_t size() const {
        return data.size();
    }
    bool empty() const {
        return data.size() == 0;
    }
    void purge() {
        data.clear();
    }
    void printStatistics(std::ostream& /* o */) const {}

private:
    std::vector<Tuple<RamDomain, Arity>> data;
    Lock insert_lock;
};

/** Equivalence relations */
struct t_eqrel {
    static constexpr Relation::arity_type Arity = 2;
    using t_tuple = Tuple<RamDomain, 2>;
    using t_ind = EquivalenceRelation<t_tuple>;
    t_ind ind;
    class iterator_0 : public std::iterator<std::forward_iterator_tag, t_tuple> {
        using nested_iterator = typename t_ind::iterator;
        nested_iterator nested;
        t_tuple value;

    public:
        iterator_0(const nested_iterator& iter) : nested(iter), value(*iter) {}
        iterator_0(const iterator_0& other) = default;
        iterator_0& operator=(const iterator_0& other) = default;
        bool operator==(const iterator_0& other) const {
            return nested == other.nested;
        }
        bool operator!=(const iterator_0& other) const {
            return !(*this == other);
        }
        const t_tuple& operator*() const {
            return value;
        }
        const t_tuple* operator->() const {
            return &value;
        }
        iterator_0& operator++() {
            ++nested;
            value = *nested;
            return *this;
        }
    };
    class iterator_1 : public std::iterator<std::forward_iterator_tag, t_tuple> {
        using nested_iterator = typename t_ind::iterator;
        nested_iterator nested;
        t_tuple value;

    public:
        iterator_1(const nested_iterator& iter) : nested(iter), value(reorder(*iter)) {}
        iterator_1(const iterator_1& other) = default;
        iterator_1& operator=(const iterator_1& other) = default;
        bool operator==(const iterator_1& other) const {
            return nested == other.nested;
        }
        bool operator!=(const iterator_1& other) const {
            return !(*this == other);
        }
        const t_tuple& operator*() const {
            return value;
        }
        const t_tuple* operator->() const {
            return &value;
        }
        iterator_1& operator++() {
            ++nested;
            value = reorder(*nested);
            return *this;
        }
    };
    using iterator = iterator_0;
    struct context {
        t_ind::operation_hints hints;
    };
    context createContext() {
        return context();
    }
    bool insert(const t_tuple& t) {
        return ind.insert(t[0], t[1]);
    }
    bool insert(const t_tuple& t, context& h) {
        return ind.insert(t[0], t[1], h.hints);
    }
    bool insert(const RamDomain* ramDomain) {
        RamDomain data[2];
        std::copy(ramDomain, ramDomain + 2, data);
        auto& tuple = reinterpret_cast<const t_tuple&>(data);
        context h;
        return insert(tuple, h);
    }
    bool insert(RamDomain a1, RamDomain a2) {
        RamDomain data[2] = {a1, a2};
        return insert(data);
    }
    void extendAndInsert(t_eqrel& other) {
        ind.extendAndInsert(other.ind);
    }
    bool contains(const t_tuple& t) const {
        return ind.contains(t[0], t[1]);
    }
    bool contains(const t_tuple& t, context&) const {
        return ind.contains(t[0], t[1]);
    }
    std::size_t size() const {
        return ind.size();
    }
    iterator find(const t_tuple& t) const {
        return ind.find(t);
    }
    iterator find(const t_tuple& t, context&) const {
        return ind.find(t);
    }
    range<iterator> lowerUpperRange_10(const t_tuple& lower, const t_tuple& /*upper*/, context& h) const {
        auto r = ind.template getBoundaries<1>((lower), h.hints);
        return make_range(iterator(r.begin()), iterator(r.end()));
    }
    range<iterator> lowerUpperRange_10(const t_tuple& lower, const t_tuple& upper) const {
        context h;
        return lowerUpperRange_10(lower, upper, h);
    }
    range<iterator_1> lowerUpperRange_01(const t_tuple& lower, const t_tuple& /*upper*/, context& h) const {
        auto r = ind.template getBoundaries<1>(reorder(lower), h.hints);
        return make_range(iterator_1(r.begin()), iterator_1(r.end()));
    }
    range<iterator_1> lowerUpperRange_01(const t_tuple& lower, const t_tuple& upper) const {
        context h;
        return lowerUpperRange_01(lower, upper, h);
    }
    range<iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& /*upper*/, context& h) const {
        auto r = ind.template getBoundaries<2>((lower), h.hints);
        return make_range(iterator(r.begin()), iterator(r.end()));
    }
    range<iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper) const {
        context h;
        return lowerUpperRange_11(lower, upper, h);
    }
    bool empty() const {
        return ind.size() == 0;
    }
    std::vector<range<iterator>> partition() const {
        std::vector<range<iterator>> res;
        for (const auto& cur : ind.partition(10000)) {
            res.push_back(make_range(iterator(cur.begin()), iterator(cur.end())));
        }
        return res;
    }
    void purge() {
        ind.clear();
    }
    iterator begin() const {
        return iterator(ind.begin());
    }
    iterator end() const {
        return iterator(ind.end());
    }
    static t_tuple reorder(const t_tuple& t) {
        t_tuple res;
        res[0] = t[1];
        res[1] = t[0];
        return res;
    }
    void printStatistics(std::ostream& /* o */) const {}
};

}  // namespace souffle
