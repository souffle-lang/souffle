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

#include "souffle/Brie.h"
#include "souffle/CompiledTuple.h"
#include "souffle/EquivalenceRelation.h"
#include "souffle/IOSystem.h"
#include "souffle/RamTypes.h"
#include "souffle/RecordTable.h"
#include "souffle/SignalHandler.h"
#include "souffle/SouffleInterface.h"
#include "souffle/SymbolTable.h"
#include "souffle/Table.h"
#include "souffle/WriteStream.h"
#include "souffle/utility/CacheUtil.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/EvaluatorUtil.h"
#include "souffle/utility/FileUtil.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/ParallelUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/StringUtil.h"
#ifndef __EMBEDDED_SOUFFLE__
#include "souffle/CompiledOptions.h"
#include "souffle/Logger.h"
#include "souffle/ProfileEvent.h"
#endif
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <iterator>
#include <memory>
#include <regex>
#include <string>
#include <utility>
#include <vector>

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
template <uint32_t id, class RelType, class TupleType, size_t Arity, size_t NumAuxAttributes>
class RelationWrapper : public souffle::Relation {
private:
    RelType& relation;
    SymbolTable& symTable;
    std::string name;
    std::array<const char*, Arity> tupleType;
    std::array<const char*, Arity> tupleName;

    class iterator_wrapper : public iterator_base {
        typename RelType::iterator it;
        const Relation* relation;
        tuple t;

    public:
        iterator_wrapper(uint32_t arg_id, const Relation* rel, const typename RelType::iterator& arg_it)
                : iterator_base(arg_id), it(arg_it), relation(rel), t(rel) {}
        void operator++() override {
            ++it;
        }
        tuple& operator*() override {
            t.rewind();
            for (size_t i = 0; i < Arity; i++) {
                t[i] = (*it)[i];
            }
            return t;
        }
        iterator_base* clone() const override {
            return new iterator_wrapper(*this);
        }

    protected:
        bool equal(const iterator_base& o) const override {
            const auto& casted = static_cast<const iterator_wrapper&>(o);
            return it == casted.it;
        }
    };

public:
    RelationWrapper(RelType& r, SymbolTable& s, std::string name, const std::array<const char*, Arity>& t,
            const std::array<const char*, Arity>& n)
            : relation(r), symTable(s), name(std::move(name)), tupleType(t), tupleName(n) {}
    iterator begin() const override {
        return iterator(new iterator_wrapper(id, this, relation.begin()));
    }
    iterator end() const override {
        return iterator(new iterator_wrapper(id, this, relation.end()));
    }
    void insert(const tuple& arg) override {
        TupleType t;
        assert(&arg.getRelation() == this && "wrong relation");
        assert(arg.size() == Arity && "wrong tuple arity");
        for (size_t i = 0; i < Arity; i++) {
            t[i] = arg[i];
        }
        relation.insert(t);
    }
    bool contains(const tuple& arg) const override {
        TupleType t;
        assert(arg.size() == Arity && "wrong tuple arity");
        for (size_t i = 0; i < Arity; i++) {
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
    const char* getAttrType(size_t arg) const override {
        assert(arg < Arity && "attribute out of bound");
        return tupleType[arg];
    }
    const char* getAttrName(size_t arg) const override {
        assert(arg < Arity && "attribute out of bound");
        return tupleName[arg];
    }
    size_t getArity() const override {
        return Arity;
    }
    size_t getAuxiliaryArity() const override {
        return NumAuxAttributes;
    }
    SymbolTable& getSymbolTable() const override {
        return symTable;
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
    t_nullaries() = default;
    using t_tuple = Tuple<RamDomain, 0>;
    struct context {};
    context createContext() {
        return context();
    }
    class iterator : public std::iterator<std::forward_iterator_tag, RamDomain*> {
        bool value;

    public:
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
    void printHintStatistics(std::ostream& /* o */, std::string /* prefix */) const {}
};

/** info relations */
template <int Arity>
class t_info {
private:
    std::vector<Tuple<RamDomain, Arity>> data;
    Lock insert_lock;

public:
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
        for (size_t i = 0; i < Arity; ++i) {
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
    void printHintStatistics(std::ostream& /* o */, std::string /* prefix */) const {}
};

}  // namespace souffle
