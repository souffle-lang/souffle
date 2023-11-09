/*
 * Souffle - A Datalog Compiler
 * Copyright Copyright (c) 2021,, Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Helper.h
 *
 * Workaround for memory management in parsing
 *
 ***********************************************************************/

#pragma once

#include <cstddef>
#include <map>
#include <utility>
#include <vector>

namespace souffle {

class ParserDriver;

namespace parser {
// FIXME: (when we can finally use Bison 3.2) Expunge this abombination.
// HACK:  Bison 3.0.2 is stupid and ugly and doesn't support move semantics
//        with the `lalr1.cc` skeleton and that makes me very mad.
//        Thankfully (or not) two can play stupid games:
//          Behold! std::auto_ptr 2: The Revengening
// NOTE:  Bison 3.2 came out in 2019. `std::unique_ptr` appeared in C++11.
//        How timely.
// NOTE:  There are specialisations wrappers that'll allow us to (almost)
//        transparently remove `Mov` once we switch to Bison 3.2+.

template <typename A>
struct Mov {
    mutable A value;

    Mov() = default;
    Mov(Mov&&) = default;
    template <typename B>
    Mov(B value) : value(std::move(value)) {}

    // CRIMES AGAINST COMPUTING HAPPENS HERE
    // HACK: Pretend you can copy it, but actually move it. Keeps Bison 3.0.2 happy.
    Mov(const Mov& x) : value(std::move(x.value)) {}
    Mov& operator=(Mov x) {
        value = std::move(x.value);
        return *this;
    }
    // detach/convert implicitly.
    operator A() {
        return std::move(value);
    }

    // support ptr-like behaviour
    A* operator->() {
        return &value;
    }
    A operator*() {
        return std::move(value);
    }
};

template <typename A>
A unwrap(Mov<A> x) {
    return *x;
}

template <typename A>
A unwrap(A x) {
    return x;
}

template <typename A>
struct Mov<Own<A>> {
    mutable Own<A> value;

    Mov() = default;
    Mov(Mov&&) = default;
    template <typename B>
    Mov(B value) : value(std::move(value)) {}

    // CRIMES AGAINST COMPUTING HAPPENS HERE
    // HACK: Pretend you can copy it, but actually move it. Keeps Bison 3.0.2 happy.
    Mov(const Mov& x) : value(std::move(x.value)) {}
    Mov& operator=(Mov x) {
        value = std::move(x.value);
        return *this;
    }
    // detach/convert implicitly.
    operator Own<A>() {
        return std::move(value);
    }
    Own<A> operator*() {
        return std::move(value);
    }

    // support ptr-like behaviour
    A* operator->() {
        return value.get();
    }
};

template <typename A>
struct Mov<std::vector<A>> {
    mutable std::vector<A> value;

    Mov() = default;
    Mov(Mov&&) = default;
    template <typename B>
    Mov(B value) : value(std::move(value)) {}

    // CRIMES AGAINST COMPUTING HAPPENS HERE
    // HACK: Pretend you can copy it, but actually move it. Keeps Bison 3.0.2 happy.
    Mov(const Mov& x) : value(std::move(x.value)) {}
    Mov& operator=(Mov x) {
        value = std::move(x.value);
        return *this;
    }
    // detach/convert implicitly.
    operator std::vector<A>() {
        return std::move(value);
    }
    auto operator*() {
        std::vector<decltype(unwrap(std::declval<A>()))> ys;
        for (auto&& x : value)
            ys.push_back(unwrap(std::move(x)));
        return ys;
    }

    // basic ops
    using iterator = typename std::vector<A>::iterator;
    typename std::vector<A>::value_type& operator[](std::size_t i) {
        return value[i];
    }
    iterator begin() {
        return value.begin();
    }
    iterator end() {
        return value.end();
    }
    void push_back(A x) {
        value.push_back(std::move(x));
    }
    std::size_t size() const {
        return value.size();
    }
    bool empty() const {
        return value.empty();
    }
};

template <typename K, typename V>
struct Mov<std::map<K, V>> {
    mutable std::map<K, V> value;

    Mov() = default;
    Mov(Mov&&) = default;
    template <typename B>
    Mov(B value) : value(std::move(value)) {}

    // CRIMES AGAINST COMPUTING HAPPENS HERE
    // HACK: Pretend you can copy it, but actually move it. Keeps Bison 3.0.2 happy.
    Mov(const Mov& x) : value(std::move(x.value)) {}
    Mov& operator=(Mov x) {
        value = std::move(x.value);
        return *this;
    }

    // detach/convert implicitly.
    operator std::map<K, V>() {
        return std::move(value);
    }

    void emplace(std::pair<K, V> p) {
        value.emplace(std::make_pair(p.first, std::move(p.second)));
    }
};

}  // namespace parser

template <typename A>
parser::Mov<A> clone(const parser::Mov<A>& x) {
    return clone(x.value);
}

}  // namespace souffle
