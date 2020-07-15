/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file FunctionalUtil.h
 *
 * @brief Datalog project utilities
 *
 ***********************************************************************/

#pragma once

#include <algorithm>
#include <functional>

namespace souffle {

// -------------------------------------------------------------------------------
//                              Functional Utils
// -------------------------------------------------------------------------------

/**
 * A functor comparing the dereferenced value of a pointer type utilizing a
 * given comparator. Its main use case are sets of non-null pointers which should
 * be ordered according to the value addressed by the pointer.
 */
template <typename T, typename C = std::less<T>>
struct deref_less {
    bool operator()(const T* a, const T* b) const {
        return C()(*a, *b);
    }
};

// -------------------------------------------------------------------------------
//                               Lambda Utils
// -------------------------------------------------------------------------------

namespace detail {

template <typename T>
struct lambda_traits_helper;

template <typename R>
struct lambda_traits_helper<R()> {
    using result_type = R;
};

template <typename R, typename A0>
struct lambda_traits_helper<R(A0)> {
    using result_type = R;
    using arg0_type = A0;
};

template <typename R, typename A0, typename A1>
struct lambda_traits_helper<R(A0, A1)> {
    using result_type = R;
    using arg0_type = A0;
    using arg1_type = A1;
};

template <typename R, typename... Args>
struct lambda_traits_helper<R(Args...)> {
    using result_type = R;
};

template <typename R, typename C, typename... Args>
struct lambda_traits_helper<R (C::*)(Args...)> : public lambda_traits_helper<R(Args...)> {};

template <typename R, typename C, typename... Args>
struct lambda_traits_helper<R (C::*)(Args...) const> : public lambda_traits_helper<R (C::*)(Args...)> {};
}  // namespace detail

/**
 * A type trait enabling the deduction of type properties of lambdas.
 * Those include so far:
 *      - the result type (result_type)
 *      - the first argument type (arg0_type)
 */
template <typename Lambda>
struct lambda_traits : public detail::lambda_traits_helper<decltype(&Lambda::operator())> {};

// -------------------------------------------------------------------------------
//                              General Algorithms
// -------------------------------------------------------------------------------

/**
 * A generic test checking whether all elements within a container satisfy a
 * certain predicate.
 *
 * @param c the container
 * @param p the predicate
 * @return true if for all elements x in c the predicate p(x) is true, false
 *          otherwise; for empty containers the result is always true
 */
template <typename Container, typename UnaryPredicate>
bool all_of(const Container& c, UnaryPredicate p) {
    return std::all_of(c.begin(), c.end(), p);
}

/**
 * A generic test checking whether any elements within a container satisfy a
 * certain predicate.
 *
 * @param c the container
 * @param p the predicate
 * @return true if there is an element x in c such that predicate p(x) is true, false
 *          otherwise; for empty containers the result is always false
 */
template <typename Container, typename UnaryPredicate>
bool any_of(const Container& c, UnaryPredicate p) {
    return std::any_of(c.begin(), c.end(), p);
}

/**
 * A generic test checking whether all elements within a container satisfy a
 * certain predicate.
 *
 * @param c the container
 * @param p the predicate
 * @return true if for all elements x in c the predicate p(x) is true, false
 *          otherwise; for empty containers the result is always true
 */
template <typename Container, typename UnaryPredicate>
bool none_of(const Container& c, UnaryPredicate p) {
    return std::none_of(c.begin(), c.end(), p);
}

/**
 * Filter a vector to exclude certain elements.
 */
template <typename A, typename F>
std::vector<A> filterNot(std::vector<A> xs, F&& f) {
    xs.erase(std::remove_if(xs.begin(), xs.end(), std::forward<F>(f)), xs.end());
    return xs;
}

/**
 * Filter a vector to include certain elements.
 */
template <typename A, typename F>
std::vector<A> filter(std::vector<A> xs, F&& f) {
    return filterNot(std::move(xs), [&](auto&& x) { return !f(x); });
}

}  // namespace souffle
