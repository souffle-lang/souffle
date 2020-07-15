/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file CompiledTuple.h
 *
 * The central file covering the data structure utilized by
 * the souffle compiler for representing relations in compiled queries.
 *
 ***********************************************************************/

#pragma once

#include <cstddef>
#include <functional>
#include <iostream>
#include <system_error>

namespace souffle {

/**
 * The type of object stored within relations representing the actual
 * tuple value. Each tuple consists of a constant number of components.
 *
 * @tparam Domain the domain of the component values
 * @tparam arity the number of components within an instance
 */
template <typename Domain, std::size_t _arity>
struct Tuple {
    // some features for template meta programming
    using value_type = Domain;
    static constexpr size_t arity = _arity;

    // the stored data
    Domain data[arity];

    // constructors, destructors and assignment are default

    // provide access to components
    const Domain& operator[](std::size_t index) const {
        return data[index];
    }

    // provide access to components
    Domain& operator[](std::size_t index) {
        return data[index];
    }

    // a comparison operation
    bool operator==(const Tuple& other) const {
        for (std::size_t i = 0; i < arity; i++) {
            if (data[i] != other.data[i]) return false;
        }
        return true;
    }

    // inequality comparison
    bool operator!=(const Tuple& other) const {
        return !(*this == other);
    }

    // required to put tuples into e.g. a std::set container
    bool operator<(const Tuple& other) const {
        for (std::size_t i = 0; i < arity; ++i) {
            if (data[i] < other.data[i]) return true;
            if (data[i] > other.data[i]) return false;
        }
        return false;
    }

    // required to put tuples into e.g. a btree container
    bool operator>(const Tuple& other) const {
        for (std::size_t i = 0; i < arity; ++i) {
            if (data[i] > other.data[i]) return true;
            if (data[i] < other.data[i]) return false;
        }
        return false;
    }

    // allow tuples to be printed
    friend std::ostream& operator<<(std::ostream& out, const Tuple& tuple) {
        if (arity == 0) return out << "[]";
        out << "[";
        for (std::size_t i = 0; i < (std::size_t)(arity - 1); ++i) {
            out << tuple.data[i];
            out << ",";
        }
        return out << tuple.data[arity - 1] << "]";
    }
};

#ifdef _MSC_VER
/**
 * A template specialization for 0-arity tuples when compiling with microsoft's
 * compiler, because it doesn't like the 0 length array even though it is the
 * last member of the struct.
 */
template <typename Domain>
struct Tuple<Domain, 0> {
    // some features for template meta programming
    using value_type = Domain;
    enum { arity = 0 };

    // the stored data
    Domain data[1];

    // constructores, destructors and assignment are default

    // provide access to components
    const Domain& operator[](std::size_t index) const {
        return data[index];
    }

    // provide access to components
    Domain& operator[](std::size_t index) {
        return data[index];
    }

    // a comparison operation
    bool operator==(const Tuple& other) const {
        for (std::size_t i = 0; i < arity; i++) {
            if (data[i] != other.data[i]) return false;
        }
        return true;
    }

    // inequality comparison
    bool operator!=(const Tuple& other) const {
        return !(*this == other);
    }

    // required to put tuples into e.g. a std::set container
    bool operator<(const Tuple& other) const {
        for (std::size_t i = 0; i < arity; ++i) {
            if (data[i] < other.data[i]) return true;
            if (data[i] > other.data[i]) return false;
        }
        return false;
    }

    // required to put tuples into e.g. a btree container
    bool operator>(const Tuple& other) const {
        for (std::size_t i = 0; i < arity; ++i) {
            if (data[i] > other.data[i]) return true;
            if (data[i] < other.data[i]) return false;
        }
        return false;
    }

    // allow tuples to be printed
    friend std::ostream& operator<<(std::ostream& out, const Tuple& tuple) {
        if (arity == 0) return out << "[]";
        out << "[";
        for (std::size_t i = 0; i < (std::size_t)(arity - 1); ++i) {
            out << tuple.data[i];
            out << ",";
        }
        return out << tuple.data[arity - 1] << "]";
    }
};
#endif  // _MSC_VER
}  // end of namespace souffle

// -- add hashing support --

namespace std {

template <typename Domain, std::size_t arity>
struct hash<souffle::Tuple<Domain, arity>> {
    size_t operator()(const souffle::Tuple<Domain, arity>& value) const {
        std::hash<Domain> hash;
        size_t res = 0;
        for (unsigned i = 0; i < arity; i++) {
            // from boost hash combine
            res ^= hash(value[i]) + 0x9e3779b9 + (res << 6) + (res >> 2);
        }
        return res;
    }
};
}  // namespace std
