/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IterUtils.h
 *
 * A (growing) collection of generic iterator utilities.
 *
 ***********************************************************************/

#pragma once

namespace souffle {

/**
 * A wrapper for an iterator obtaining pointers of a certain type,
 * dereferencing values before forwarding them to the consumer.
 *
 * @tparam Iter ... the type of wrapped iterator
 * @tparam T    ... the value to be accessed by the resulting iterator
 */
template <typename Iter, typename T = typename std::remove_pointer<typename Iter::value_type>::type>
struct IterDerefWrapper : public std::iterator<std::forward_iterator_tag, T> {
    /* The nested iterator. */
    Iter iter;

public:
    // some constructores
    IterDerefWrapper() = default;
    IterDerefWrapper(const Iter& iter) : iter(iter) {}

    // defaulted copy and move constructors
    IterDerefWrapper(const IterDerefWrapper&) = default;
    IterDerefWrapper(IterDerefWrapper&&) = default;

    // default assignment operators
    IterDerefWrapper& operator=(const IterDerefWrapper&) = default;
    IterDerefWrapper& operator=(IterDerefWrapper&&) = default;

    /* The equality operator as required by the iterator concept. */
    bool operator==(const IterDerefWrapper& other) const {
        return iter == other.iter;
    }

    /* The not-equality operator as required by the iterator concept. */
    bool operator!=(const IterDerefWrapper& other) const {
        return iter != other.iter;
    }

    /* The deref operator as required by the iterator concept. */
    const T& operator*() const {
        return **iter;
    }

    /* Support for the pointer operator. */
    const T* operator->() const {
        return &(**iter);
    }

    /* The increment operator as required by the iterator concept. */
    IterDerefWrapper& operator++() {
        ++iter;
        return *this;
    }
};

/**
 * A factory function enabling the construction of a dereferencing
 * iterator utilizing the automated deduction of template parameters.
 */
template <typename Iter>
IterDerefWrapper<Iter> derefIter(const Iter& iter) {
    return IterDerefWrapper<Iter>(iter);
}

// ---------------------------------------------------------------------
//                        Single-Value-Iterator
// ---------------------------------------------------------------------

/**
 * An iterator to be utilized if there is only a single element to iterate over.
 */
template <typename T>
class SingleValueIterator : public std::iterator<std::forward_iterator_tag, T> {
    T value;

    bool end;

public:
    SingleValueIterator() : end(true) {}

    SingleValueIterator(const T& value) : value(value), end(false) {}

    // a copy constructor
    SingleValueIterator(const SingleValueIterator& other) = default;

    // an assignment operator
    SingleValueIterator& operator=(const SingleValueIterator& other) = default;

    // the equality operator as required by the iterator concept
    bool operator==(const SingleValueIterator& other) const {
        // only equivalent if pointing to the end
        return end && other.end;
    }

    // the not-equality operator as required by the iterator concept
    bool operator!=(const SingleValueIterator& other) const {
        return !(*this == other);
    }

    // the deref operator as required by the iterator concept
    const T& operator*() const {
        return value;
    }

    // support for the pointer operator
    const T* operator->() const {
        return &value;
    }

    // the increment operator as required by the iterator concept
    SingleValueIterator& operator++() {
        end = true;
        return *this;
    }
};

}  // end of namespace souffle
