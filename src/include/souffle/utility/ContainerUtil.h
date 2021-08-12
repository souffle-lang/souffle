/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ContainerUtil.h
 *
 * @brief Datalog project utilities
 *
 ***********************************************************************/

#pragma once

#include "souffle/utility/DynamicCasting.h"
#include "souffle/utility/Iteration.h"
#include "souffle/utility/MiscUtil.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

namespace souffle {

// -------------------------------------------------------------------------------
//                           General Container Utilities
// -------------------------------------------------------------------------------

/**
 * Use to range-for iterate in reverse.
 * Assumes `std::rbegin` and `std::rend` are defined for type `A`.
 */
template <typename A>
struct reverse {
    reverse(A& iterable) : iterable(iterable) {}
    A& iterable;

    auto begin() {
        return std::rbegin(iterable);
    }

    auto end() {
        return std::rend(iterable);
    }
};

/**
 * A utility to check generically whether a given element is contained in a given
 * container.
 */
template <typename C>
bool contains(const C& container, const typename C::value_type& element) {
    return std::find(container.begin(), container.end(), element) != container.end();
}

// TODO: Detect and generalise to other set types?
template <typename A>
bool contains(const std::set<A>& container, const A& element) {
    return container.find(element) != container.end();
}

/**
 * Version of contains specialised for maps.
 *
 * This workaround is needed because of set container, for which value_type == key_type,
 * which is ambiguous in this context.
 */
template <typename C>
bool contains(const C& container, const typename C::value_type::first_type& element) {
    return container.find(element) != container.end();
}

/**
 * Returns the first element in a container that satisfies a given predicate,
 * nullptr otherwise.
 */
template <typename C>
typename C::value_type getIf(const C& container, std::function<bool(const typename C::value_type)> pred) {
    auto res = std::find_if(container.begin(), container.end(),
            [&](const typename C::value_type item) { return pred(item); });
    return res == container.end() ? nullptr : *res;
}

/**
 * Get value for a given key; if not found, return default value.
 */
template <typename C>
typename C::mapped_type const& getOr(
        const C& container, typename C::key_type key, const typename C::mapped_type& defaultValue) {
    auto it = container.find(key);

    if (it != container.end()) {
        return it->second;
    } else {
        return defaultValue;
    }
}

namespace detail {
inline auto allOfBool = [](bool b) { return b; };
}

/**
 * Return true if all elements (optionally after applying up)
 * are true
 */
template <typename R, typename UnaryP = decltype(detail::allOfBool) const&>
bool all(R const& range, UnaryP&& up = detail::allOfBool) {
    return std::all_of(range.begin(), range.end(), std::forward<UnaryP>(up));
}

/**
 * Append elements to a container
 */
template <class C, typename R>
void append(C& container, R&& range) {
    container.insert(container.end(), std::begin(range), std::end(range));
}

/**
 * A utility function enabling the creation of a vector with a fixed set of
 * elements within a single expression. This is the base case covering empty
 * vectors.
 */
template <typename T>
std::vector<T> toVector() {
    return std::vector<T>();
}

/**
 * A utility function enabling the creation of a vector with a fixed set of
 * elements within a single expression. This is the step case covering vectors
 * of arbitrary length.
 */
template <typename T, typename... R>
std::vector<T> toVector(const T& first, const R&... rest) {
    return {first, rest...};
}

/**
 * A utility function enabling the creation of a vector of pointers.
 */
template <typename T>
std::vector<T*> toPtrVector(const VecOwn<T>& v) {
    std::vector<T*> res;
    for (auto& e : v) {
        res.push_back(e.get());
    }
    return res;
}

/**
 * Applies a function to each element of a vector and returns the results.
 */
template <typename A, typename F /* : A -> B */>
auto map(const std::vector<A>& xs, F&& f) {
    // FIXME: We can rewrite this using makeTransformRange now,
    // or remove the usage of this completely
    std::vector<decltype(f(xs[0]))> ys;
    ys.reserve(xs.size());
    for (auto&& x : xs) {
        ys.emplace_back(f(x));
    }
    return ys;
}

// -------------------------------------------------------------------------------
//                             Equality Utilities
// -------------------------------------------------------------------------------

/**
 * Cast the values, from baseType to toType and compare using ==. (if casting fails -> return false.)
 *
 * @tparam baseType, initial Type of values
 * @tparam toType, type where equality comparison takes place.
 */
template <typename toType, typename baseType>
bool castEq(const baseType* left, const baseType* right) {
    if (auto castedLeft = as<toType>(left)) {
        if (auto castedRight = as<toType>(right)) {
            return castedLeft == castedRight;
        }
    }
    return false;
}

/**
 * A functor class supporting the values pointers are pointing to.
 */
template <typename T>
struct comp_deref {
    bool operator()(const T& a, const T& b) const {
        if (a == nullptr) {
            return false;
        }
        if (b == nullptr) {
            return false;
        }
        return *a == *b;
    }
};

/**
 * A function testing whether two containers are equal with the given Comparator.
 */
template <typename Container, typename Comparator>
bool equal_targets(const Container& a, const Container& b, const Comparator& comp) {
    // check reference
    if (&a == &b) {
        return true;
    }

    // check size
    if (a.size() != b.size()) {
        return false;
    }

    // check content
    return std::equal(a.begin(), a.end(), b.begin(), comp);
}

/**
 * A function testing whether two containers of pointers are referencing equivalent
 * targets.
 */
template <typename T, template <typename...> class Container>
bool equal_targets(const Container<T*>& a, const Container<T*>& b) {
    return equal_targets(a, b, comp_deref<T*>());
}

/**
 * A function testing whether two containers of unique pointers are referencing equivalent
 * targets.
 */
template <typename T, template <typename...> class Container>
bool equal_targets(const Container<Own<T>>& a, const Container<Own<T>>& b) {
    return equal_targets(a, b, comp_deref<Own<T>>());
}

/**
 * A function testing whether two maps of unique pointers are referencing to equivalent
 * targets.
 */
template <typename Key, typename Value>
bool equal_targets(const std::map<Key, Own<Value>>& a, const std::map<Key, Own<Value>>& b) {
    auto comp = comp_deref<Own<Value>>();
    return equal_targets(
            a, b, [&comp](auto& a, auto& b) { return a.first == b.first && comp(a.second, b.second); });
}

// -------------------------------------------------------------------------------
//                             Checking Utilities
// -------------------------------------------------------------------------------
template <typename R>
bool allValidPtrs(R const& range) {
    return all(makeTransformRange(range, [](auto const& ptr) { return ptr != nullptr; }));
}

}  // namespace souffle

namespace std {
template <typename Iter, typename F>
struct iterator_traits<souffle::TransformIterator<Iter, F>> {
    using iter_t = std::iterator_traits<Iter>;
    using iter_tag = typename iter_t::iterator_category;
    using difference_type = typename iter_t::difference_type;
    using reference = decltype(std::declval<F&>()(*std::declval<Iter>()));
    using value_type = std::remove_cv_t<std::remove_reference_t<reference>>;
    using iterator_category = std::conditional_t<std::is_base_of_v<std::random_access_iterator_tag, iter_tag>,
            std::random_access_iterator_tag, iter_tag>;
};
}  // namespace std
