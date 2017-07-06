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

#include "AstTypes.h"
#include "CompiledRamOptions.h"
#include "CompiledRamRecord.h"
#include "CompiledRamRelation.h"
#include "ParallelUtils.h"
#include "RamLogger.h"
#include "SignalHandler.h"
#include "SouffleInterface.h"
#include "SymbolTable.h"

#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <regex>

#if defined(_OPENMP)
#include <omp.h>
#endif

namespace souffle {

extern "C" {
#ifdef JNI_INTERFACE
souffle::SouffleProgram* getInstance(const char* p) {
    return souffle::ProgramFactory::newInstance(p);
}
#else
inline souffle::SouffleProgram* getInstance(const char* p) {
    return souffle::ProgramFactory::newInstance(p);
}
#endif
}

/**
 * Relation wrapper used internally in the generated Datalog program
 */
template <uint32_t id, class RelType, class TupleType, size_t Arity, bool IsInputRel, bool IsOutputRel>
class RelationWrapper : public Relation {
private:
    RelType& relation;
    SymbolTable& symTable;
    std::string name;
    std::array<const char*, Arity> tupleType;
    std::array<const char*, Arity> tupleName;

    class iterator_wrapper : public iterator_base {
        typename RelType::iterator it;
        Relation* relation;
        tuple t;

    public:
        iterator_wrapper(uint32_t arg_id, Relation* rel, const typename RelType::iterator& arg_it)
                : iterator_base(arg_id), it(arg_it), relation(rel), t(rel) {}
        void operator++() {
            ++it;
        }
        tuple& operator*() {
            t.rewind();
            for (size_t i = 0; i < Arity; i++) {
                t[i] = (*it)[i];
            }
            return t;
        }
        iterator_base* clone() const {
            return new iterator_wrapper(*this);
        }

    protected:
        bool equal(const iterator_base& o) const {
            const iterator_wrapper& casted = static_cast<const iterator_wrapper&>(o);
            return it == casted.it;
        }
    };

public:
    RelationWrapper(RelType& r, SymbolTable& s, std::string name, const std::array<const char*, Arity>& t,
            const std::array<const char*, Arity>& n)
            : relation(r), symTable(s), name(name), tupleType(t), tupleName(n) {}
    iterator begin() {
        return iterator(new iterator_wrapper(id, this, relation.begin()));
    }
    iterator end() {
        return iterator(new iterator_wrapper(id, this, relation.end()));
    }
    void insert(const tuple& arg) {
        TupleType t;
        assert(arg.size() == Arity && "wrong tuple arity");
        for (size_t i = 0; i < Arity; i++) {
            t[i] = arg[i];
        }
        relation.insert(t);
    }
    bool contains(const tuple& arg) const {
        TupleType t;
        assert(arg.size() == Arity && "wrong tuple arity");
        for (size_t i = 0; i < Arity; i++) {
            t[i] = arg[i];
        }
        return relation.contains(t);
    }
    bool isInput() const {
        return IsInputRel;
    }
    bool isOutput() const {
        return IsOutputRel;
    }
    std::size_t size() {
        return relation.size();
    }
    std::string getName() const {
        return name;
    }
    const char* getAttrType(size_t arg) const {
        assert(0 <= arg && arg < Arity && "attribute out of bound");
        return tupleType[arg];
    }
    const char* getAttrName(size_t arg) const {
        assert(0 <= arg && arg < Arity && "attribute out of bound");
        return tupleName[arg];
    }
    size_t getArity() const {
        return Arity;
    }
    SymbolTable& getSymbolTable() const {
        return symTable;
    }
};
}  // namespace souffle
