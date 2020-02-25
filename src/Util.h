/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Util.h
 *
 * @brief Datalog project utilities
 *
 ***********************************************************************/

#pragma once

#include "RamTypes.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <cassert>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#ifndef _WIN32
#include <libgen.h>
#include <unistd.h>
#else
#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#include <windows.h>

/**
 * Windows headers define these and they interfere with the standard library
 * functions.
 */
#undef min
#undef max

#define X_OK 1 /* execute permission - unsupported in windows*/

#define PATH_MAX 260

/**
 * access and realpath are missing on windows, we use their windows equivalents
 * as work-arounds.
 */
#define access _access
inline char* realpath(const char* path, char* resolved_path) {
    return _fullpath(resolved_path, path, PATH_MAX);
}

/**
 * On windows, the following gcc builtins are missing.
 *
 * In the case of popcountll, __popcnt64 is the windows equivalent.
 *
 * For ctz and ctzll, BitScanForward and BitScanForward64 are the respective
 * windows equivalents.  However ctz is used in a constexpr context, and we can't
 * use BitScanForward, so we implement it ourselves.
 */
#define __builtin_popcountll __popcnt64

constexpr unsigned long __builtin_ctz(unsigned long value) {
    unsigned long trailing_zeroes = 0;
    while ((value = value >> 1) ^ 1) {
        ++trailing_zeroes;
    }
    return trailing_zeroes;
}

inline unsigned long __builtin_ctzll(unsigned long long value) {
    unsigned long trailing_zero = 0;

    if (_BitScanForward64(&trailing_zero, value)) {
        return trailing_zero;
    } else {
        return 64;
    }
}
#endif

namespace souffle {

/**
 * Converts a string to a RamDomain
 */
inline RamDomain RamDomainFromString(
        const std::string& str, std::size_t* position = nullptr, const int base = 10) {
    RamDomain val;
#if RAM_DOMAIN_SIZE == 64
    val = std::stoll(str, position, base);
#else
    val = std::stoi(str, position, base);
#endif
    return static_cast<RamDomain>(val);
}

/**
 * Converts a string to a RamFloat
 */
inline RamFloat RamFloatFromString(const std::string& str, std::size_t* position = nullptr) {
    RamFloat val;
#if RAM_DOMAIN_SIZE == 64
    val = std::stod(str, position);
#else
    val = std::stof(str, position);
#endif
    return static_cast<RamFloat>(val);
}

/**
 * Converts a string to a RamUnsigned
 */
inline RamUnsigned RamUnsignedFromString(
        const std::string& str, std::size_t* position = nullptr, const int base = 10) {
    RamUnsigned val;
#if RAM_DOMAIN_SIZE == 64
    val = std::stoul(str, position, base);
#else
    val = std::stoull(str, position, base);
#endif
    return static_cast<RamUnsigned>(val);
}

#if RAM_DOMAIN_SIZE == 64
inline RamDomain stord(const std::string& str, std::size_t* pos = nullptr, int base = 10) {
    return static_cast<RamDomain>(std::stoull(str, pos, base));
}
#elif RAM_DOMAIN_SIZE == 32
inline RamDomain stord(const std::string& str, std::size_t* pos = nullptr, int base = 10) {
    return static_cast<RamDomain>(std::stoul(str, pos, base));
}
#else
#error RAM Domain is neither 32bit nor 64bit
#endif

/**
 * Check whether a string is a sequence of digits
 */
inline bool isNumber(const char* str) {
    if (str == nullptr) {
        return false;
    }

    while (*str != 0) {
        if (isdigit(*str) == 0) {
            return false;
        }
        str++;
    }
    return true;
}

// -------------------------------------------------------------------------------
//                           General Container Utilities
// -------------------------------------------------------------------------------

/**
 * A utility to check generically whether a given element is contained in a given
 * container.
 */
template <typename C>
bool contains(const C& container, const typename C::value_type& element) {
    return std::find(container.begin(), container.end(), element) != container.end();
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
std::vector<T*> toPtrVector(const std::vector<std::unique_ptr<T>>& v) {
    std::vector<T*> res;
    for (auto& e : v) {
        res.push_back(e.get());
    }
    return res;
}

// -------------------------------------------------------------
//                             Ranges
// -------------------------------------------------------------

/**
 * A utility class enabling representation of ranges by pairing
 * two iterator instances marking lower and upper boundaries.
 */
template <typename Iter>
struct range {
    // the lower and upper boundary
    Iter a, b;

    // a constructor accepting a lower and upper boundary
    range(Iter a, Iter b) : a(std::move(a)), b(std::move(b)) {}

    // default copy / move and assignment support
    range(const range&) = default;
    range(range&&) = default;
    range& operator=(const range&) = default;

    // get the lower boundary (for for-all loop)
    Iter& begin() {
        return a;
    }
    const Iter& begin() const {
        return a;
    }

    // get the upper boundary (for for-all loop)
    Iter& end() {
        return b;
    }
    const Iter& end() const {
        return b;
    }

    // emptiness check
    bool empty() const {
        return a == b;
    }

    // splits up this range into the given number of partitions
    std::vector<range> partition(int np = 100) {
        // obtain the size
        int n = 0;
        for (auto i = a; i != b; ++i) {
            n++;
        }

        // split it up
        auto s = n / np;
        auto r = n % np;
        std::vector<range> res;
        res.reserve(np);
        auto cur = a;
        auto last = cur;
        int i = 0;
        int p = 0;
        while (cur != b) {
            ++cur;
            i++;
            if (i >= (s + (p < r ? 1 : 0))) {
                res.push_back({last, cur});
                last = cur;
                p++;
                i = 0;
            }
        }
        if (cur != last) {
            res.push_back({last, cur});
        }
        return res;
    }
};

/**
 * A utility function enabling the construction of ranges
 * without explicitly specifying the iterator type.
 *
 * @tparam Iter .. the iterator type
 * @param a .. the lower boundary
 * @param b .. the upper boundary
 */
template <typename Iter>
range<Iter> make_range(const Iter& a, const Iter& b) {
    return range<Iter>(a, b);
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
    if (auto castedLeft = dynamic_cast<const toType*>(left)) {
        if (auto castedRight = dynamic_cast<const toType*>(right)) {
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
 * A function testing whether two vectors are equal (same vector of elements).
 */
template <typename T, typename Comp = std::equal_to<T>>
bool equal(const std::vector<T>& a, const std::vector<T>& b, const Comp& comp = Comp()) {
    // check reference
    if (&a == &b) {
        return true;
    }

    // check size
    if (a.size() != b.size()) {
        return false;
    }

    // check content
    for (std::size_t i = 0; i < a.size(); ++i) {
        // if there is a difference
        if (!comp(a[i], b[i])) {
            return false;
        }
    }
    return true;
}

/**
 * A function testing whether two maps are equal.
 */
template <typename T1, typename T2, typename Comp = std::equal_to<T2>>
bool equal(const std::map<T1, T2>& a, const std::map<T1, T2>& b, const Comp& comp = Comp()) {
    // check reference
    if (&a == &b) {
        return true;
    }

    // check size
    if (a.size() != b.size()) {
        return false;
    }

    // check contents
    auto itB = b.begin();
    for (auto itA = a.begin(); itA != a.end(); ++itA, ++itB) {
        if (itA->first != itB->first) {
            return false;
        }
        if (!comp(itA->second, itB->second)) {
            return false;
        }
    }
    return true;
}

/**
 * A function testing whether two sets are equal (same set of elements).
 */
template <typename T, typename Comp = std::equal_to<T>>
bool equal(const std::set<T>& a, const std::set<T>& b, const Comp& comp = Comp()) {
    // check reference
    if (&a == &b) {
        return true;
    }

    // check size
    if (a.size() != b.size()) {
        return false;
    }

    // check content
    for (auto it_i = a.begin(); it_i != a.end(); ++it_i) {
        for (auto it_j = a.begin(); it_j != a.end(); ++it_j) {
            // if there is a difference
            if (!comp(*it_i, *it_j)) {
                return false;
            }
        }
    }

    // all the same
    return true;
}

/**
 * A function testing whether two vector of pointers are referencing to equivalent
 * targets.
 */
template <typename T>
bool equal_targets(const std::vector<T*>& a, const std::vector<T*>& b) {
    return equal(a, b, comp_deref<T*>());
}

/**
 * A function testing whether two vector of unique pointers are referencing to equivalent
 * targets.
 */
template <typename T>
bool equal_targets(const std::vector<std::unique_ptr<T>>& a, const std::vector<std::unique_ptr<T>>& b) {
    return equal(a, b, comp_deref<std::unique_ptr<T>>());
}

/**
 * A function testing whether two maps of unique pointers are referencing to equivalent
 * targets.
 */
template <typename T1, typename T2>
bool equal_targets(const std::map<T1, std::unique_ptr<T2>>& a, const std::map<T1, std::unique_ptr<T2>>& b) {
    return equal(a, b, comp_deref<std::unique_ptr<T2>>());
}

/**
 * A function testing whether two set of pointers are referencing to equivalent
 * targets.
 */
template <typename T>
bool equal_targets(const std::set<T*>& a, const std::set<T*>& b) {
    return equal(a, b, comp_deref<T*>());
}

/**
 * A function testing whether two set of pointers are referencing to equivalent
 * targets.
 */
template <typename T>
bool equal_targets(const std::set<std::unique_ptr<T>>& a, const std::set<std::unique_ptr<T>>& b) {
    return equal(a, b, comp_deref<std::unique_ptr<T>>());
}

/**
 * Compares two values referenced by a pointer where the case where both
 * pointers are null is also considered equivalent.
 */
template <typename T>
bool equal_ptr(const T* a, const T* b) {
    if (!a && !b) {
        return true;
    }
    if (a && b) {
        return *a == *b;
    }
    return false;
}

/**
 * Compares two values referenced by a pointer where the case where both
 * pointers are null is also considered equivalent.
 */
template <typename T>
bool equal_ptr(const std::unique_ptr<T>& a, const std::unique_ptr<T>& b) {
    if (!a && !b) {
        return true;
    }
    if (a && b) {
        return *a == *b;
    }
    return false;
}

// -------------------------------------------------------------------------------
//                           General Print Utilities
// -------------------------------------------------------------------------------

namespace detail {

/**
 * A auxiliary class to be returned by the join function aggregating the information
 * required to print a list of elements as well as the implementation of the printing
 * itself.
 */
template <typename Iter, typename Printer>
class joined_sequence {
    /** The begin of the range to be printed */
    Iter begin;

    /** The end of the range to be printed */
    Iter end;

    /** The seperator to be utilized between elements */
    std::string sep;

    /** A functor printing an element */
    Printer p;

public:
    /** A constructor setting up all fields of this class */
    joined_sequence(const Iter& a, const Iter& b, std::string sep, Printer p)
            : begin(a), end(b), sep(std::move(sep)), p(std::move(p)) {}

    /** The actual print method */
    friend std::ostream& operator<<(std::ostream& out, const joined_sequence& s) {
        auto cur = s.begin;
        if (cur == s.end) {
            return out;
        }

        s.p(out, *cur);
        ++cur;
        for (; cur != s.end; ++cur) {
            out << s.sep;
            s.p(out, *cur);
        }
        return out;
    }
};

/**
 * A generic element printer.
 *
 * @tparam Extractor a functor preparing a given value before being printed.
 */
template <typename Extractor>
struct print {
    template <typename T>
    void operator()(std::ostream& out, const T& value) const {
        // extract element to be printed from the given value and print it
        Extractor ext;
        out << ext(value);
    }
};
}  // namespace detail

/**
 * A functor representing the identity function for a generic type T.
 *
 * @tparam T some arbitrary type
 */
template <typename T>
struct id {
    T& operator()(T& t) const {
        return t;
    }
    const T& operator()(const T& t) const {
        return t;
    }
};

/**
 * A functor dereferencing a given type
 *
 * @tparam T some arbitrary type with an overloaded * operator (deref)
 */
template <typename T>
struct deref {
    auto operator()(T& t) const -> decltype(*t) {
        return *t;
    }
    auto operator()(const T& t) const -> decltype(*t) {
        return *t;
    }
};

/**
 * A functor printing elements after dereferencing it. This functor
 * is mainly intended to be utilized when printing sequences of elements
 * of a pointer type when using the join function below.
 */
template <typename T>
struct print_deref : public detail::print<deref<T>> {};

/**
 * Creates an object to be forwarded to some output stream for printing
 * sequences of elements interspersed by a given separator.
 *
 * For use cases see the test case {util_test.cpp}.
 */
template <typename Iter, typename Printer>
detail::joined_sequence<Iter, Printer> join(
        const Iter& a, const Iter& b, const std::string& sep, const Printer& p) {
    return souffle::detail::joined_sequence<Iter, Printer>(a, b, sep, p);
}

/**
 * Creates an object to be forwarded to some output stream for printing
 * sequences of elements interspersed by a given separator.
 *
 * For use cases see the test case {util_test.cpp}.
 */
template <typename Iter, typename T = typename Iter::value_type>
detail::joined_sequence<Iter, detail::print<id<T>>> join(
        const Iter& a, const Iter& b, const std::string& sep = ",") {
    return join(a, b, sep, detail::print<id<T>>());
}

/**
 * Creates an object to be forwarded to some output stream for printing
 * the content of containers interspersed by a given separator.
 *
 * For use cases see the test case {util_test.cpp}.
 */
template <typename Container, typename Printer, typename Iter = typename Container::const_iterator>
detail::joined_sequence<Iter, Printer> join(const Container& c, const std::string& sep, const Printer& p) {
    return join(c.begin(), c.end(), sep, p);
}

/**
 * Creates an object to be forwarded to some output stream for printing
 * the content of containers interspersed by a given separator.
 *
 * For use cases see the test case {util_test.cpp}.
 */
template <typename Container, typename Iter = typename Container::const_iterator,
        typename T = typename Iter::value_type>
detail::joined_sequence<Iter, detail::print<id<T>>> join(const Container& c, const std::string& sep = ",") {
    return join(c.begin(), c.end(), sep, detail::print<id<T>>());
}

}  // end namespace souffle

#ifndef __EMBEDDED_SOUFFLE__

namespace std {

/**
 * Introduces support for printing pairs as long as their components can be printed.
 */
template <typename A, typename B>
ostream& operator<<(ostream& out, const pair<A, B>& p) {
    return out << "(" << p.first << "," << p.second << ")";
}

/**
 * Enables the generic printing of vectors assuming their element types
 * are printable.
 */
template <typename T, typename A>
ostream& operator<<(ostream& out, const vector<T, A>& v) {
    return out << "[" << souffle::join(v) << "]";
}

/**
 * Enables the generic printing of sets assuming their element types
 * are printable.
 */
template <typename K, typename C, typename A>
ostream& operator<<(ostream& out, const set<K, C, A>& s) {
    return out << "{" << souffle::join(s) << "}";
}

/**
 * Enables the generic printing of maps assuming their element types
 * are printable.
 */
template <typename K, typename T, typename C, typename A>
ostream& operator<<(ostream& out, const map<K, T, C, A>& m) {
    return out << "{" << souffle::join(m, ",", [](ostream& out, const pair<K, T>& cur) {
        out << cur.first << "->" << cur.second;
    }) << "}";
}

}  // end namespace std

#endif

namespace souffle {

/**
 * A generic function converting strings into strings (trivial case).
 */
inline const std::string& toString(const std::string& str) {
    return str;
}

namespace detail {

/**
 * A type trait to check whether a given type is printable.
 * In this general case, nothing is printable.
 */
template <typename T, typename filter = void>
struct is_printable : public std::false_type {};

/**
 * A type trait to check whether a given type is printable.
 * This specialization makes types with an output operator printable.
 */
template <typename T>
struct is_printable<T, typename std::conditional<false,
                               decltype(std::declval<std::ostream&>() << std::declval<T>()), void>::type>
        : public std::true_type {};
}  // namespace detail

/**
 * A generic function converting arbitrary objects to strings by utilizing
 * their print capability.
 *
 * This function is mainly intended for implementing test cases and debugging
 * operations.
 */
template <typename T>
typename std::enable_if<detail::is_printable<T>::value, std::string>::type toString(const T& value) {
    // write value into stream and return result
    std::stringstream ss;
    ss << value;
    return ss.str();
}

/**
 * A fallback for the to-string function in case an unprintable object is supposed
 * to be printed.
 */
template <typename T>
typename std::enable_if<!detail::is_printable<T>::value, std::string>::type toString(const T& value) {
    std::stringstream ss;
    ss << "(print for type ";
    ss << typeid(T).name();
    ss << " not supported)";
    return ss.str();
}

namespace detail {

/**
 * A utility class required for the implementation of the times function.
 */
template <typename T>
struct multiplying_printer {
    const T& value;
    unsigned times;
    multiplying_printer(const T& value, unsigned times) : value(value), times(times) {}

    friend std::ostream& operator<<(std::ostream& out, const multiplying_printer& printer) {
        for (unsigned i = 0; i < printer.times; i++) {
            out << printer.value;
        }
        return out;
    }
};
}  // namespace detail

/**
 * A utility printing a given value multiple times.
 */
template <typename T>
detail::multiplying_printer<T> times(const T& value, unsigned num) {
    return detail::multiplying_printer<T>(value, num);
}

// -------------------------------------------------------------------------------
//                              String Utils
// -------------------------------------------------------------------------------

/**
 * Determines whether the given value string ends with the given
 * end string.
 */
inline bool endsWith(const std::string& value, const std::string& ending) {
    if (value.size() < ending.size()) {
        return false;
    }
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

/**
 * Splits a string given a delimiter
 */
inline std::vector<std::string> splitString(const std::string& str, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream strstr(str);
    std::string token;
    while (std::getline(strstr, token, delimiter)) {
        parts.push_back(token);
    }
    return parts;
}

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
 * A generic test checking whether any elements within a container is equal to a
 * given value.
 *
 * @param c the container
 * @param x the test value
 * @return true if there is an element y in c such that (x == y) is true, false
 *          otherwise; for empty containers the result is always false
 */
template <typename Container, typename A>
bool any_equals(const Container& c, A const& x) {
    return std::any_of(c.begin(), c.end(), [&](auto& y) { return x == y; });
}

// -------------------------------------------------------------------------------
//                               Timing Utils
// -------------------------------------------------------------------------------

// a type def for a time point
using time_point = std::chrono::high_resolution_clock::time_point;
using std::chrono::microseconds;

// a shortcut for taking the current time
inline time_point now() {
    return std::chrono::high_resolution_clock::now();
}

// a shortcut for obtaining the time difference in milliseconds
inline long duration_in_us(const time_point& start, const time_point& end) {
    return static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
}

// a shortcut for obtaining the time difference in nanoseconds
inline long duration_in_ns(const time_point& start, const time_point& end) {
    return static_cast<long>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
}

// -------------------------------------------------------------------------------
//                               File Utils
// -------------------------------------------------------------------------------

/**
 *  Check whether a file exists in the file system
 */
inline bool existFile(const std::string& name) {
    struct stat buffer = {};
    if (stat(name.c_str(), &buffer) == 0) {
        if ((buffer.st_mode & S_IFMT) != 0) {
            return true;
        }
    }
    return false;
}

/**
 *  Check whether a directory exists in the file system
 */
inline bool existDir(const std::string& name) {
    struct stat buffer = {};
    if (stat(name.c_str(), &buffer) == 0) {
        if ((buffer.st_mode & S_IFDIR) != 0) {
            return true;
        }
    }
    return false;
}

/**
 * Check whether a given file exists and it is an executable
 */
inline bool isExecutable(const std::string& name) {
    return existFile(name) && (access(name.c_str(), X_OK) == 0);
}

/**
 * Simple implementation of a which tool
 */
inline std::string which(const std::string& name) {
    char buf[PATH_MAX];
    if ((::realpath(name.c_str(), buf) != nullptr) && isExecutable(buf)) {
        return buf;
    }
    const char* syspath = ::getenv("PATH");
    if (syspath == nullptr) {
        return "";
    }
    std::stringstream sstr;
    sstr << syspath;
    std::string sub;
    while (std::getline(sstr, sub, ':')) {
        std::string path = sub + "/" + name;
        if (isExecutable(path) && (realpath(path.c_str(), buf) != nullptr)) {
            return buf;
        }
    }
    return "";
}

/**
 *  C++-style dirname
 */
inline std::string dirName(const std::string& name) {
    if (name.empty()) {
        return ".";
    }
    size_t lastNotSlash = name.find_last_not_of('/');
    // All '/'
    if (lastNotSlash == std::string::npos) {
        return "/";
    }
    size_t leadingSlash = name.find_last_of('/', lastNotSlash);
    // No '/'
    if (leadingSlash == std::string::npos) {
        return ".";
    }
    // dirname is '/'
    if (leadingSlash == 0) {
        return "/";
    }
    return name.substr(0, leadingSlash);
}

/**
 *  C++-style realpath
 */
inline std::string absPath(const std::string& path) {
    char buf[PATH_MAX];
    char* res = realpath(path.c_str(), buf);
    return (res == nullptr) ? "" : std::string(buf);
}

/**
 *  Join two paths together; note that this does not resolve overlaps or relative paths.
 */
inline std::string pathJoin(const std::string& first, const std::string& second) {
    unsigned firstPos = static_cast<unsigned>(first.size()) - 1;
    while (first.at(firstPos) == '/') {
        firstPos--;
    }
    unsigned secondPos = 0;
    while (second.at(secondPos) == '/') {
        secondPos++;
    }
    return first.substr(0, firstPos + 1) + '/' + second.substr(secondPos);
}

/*
 * Find out if an executable given by @p tool exists in the path given @p path
 * relative to the directory given by @ base. A path here refers a
 * colon-separated list of directories.
 */
inline std::string findTool(const std::string& tool, const std::string& base, const std::string& path) {
    std::string dir = dirName(base);
    std::stringstream sstr(path);
    std::string sub;

    while (std::getline(sstr, sub, ':')) {
        std::string subpath = dir + "/" + sub + '/' + tool;
        if (isExecutable(subpath)) {
            return absPath(subpath);
        }
    }
    return "";
}

/*
 * Get the basename of a fully qualified filename
 */
inline std::string baseName(const std::string& filename) {
    if (filename.empty()) {
        return ".";
    }

    size_t lastNotSlash = filename.find_last_not_of('/');
    if (lastNotSlash == std::string::npos) {
        return "/";
    }

    size_t lastSlashBeforeBasename = filename.find_last_of('/', lastNotSlash - 1);
    if (lastSlashBeforeBasename == std::string::npos) {
        lastSlashBeforeBasename = static_cast<size_t>(-1);
    }
    return filename.substr(lastSlashBeforeBasename + 1, lastNotSlash - lastSlashBeforeBasename);
}

/**
 * File name, with extension removed.
 */
inline std::string simpleName(const std::string& path) {
    std::string name = baseName(path);
    const size_t lastDot = name.find_last_of('.');
    // file has no extension
    if (lastDot == std::string::npos) {
        return name;
    }
    const size_t lastSlash = name.find_last_of('/');
    // last slash occurs after last dot, so no extension
    if (lastSlash != std::string::npos && lastSlash > lastDot) {
        return name;
    }
    // last dot after last slash, or no slash
    return name.substr(0, lastDot);
}

/**
 * File extension, with all else removed.
 */
inline std::string fileExtension(const std::string& path) {
    std::string name = path;
    const size_t lastDot = name.find_last_of('.');
    // file has no extension
    if (lastDot == std::string::npos) {
        return std::string();
    }
    const size_t lastSlash = name.find_last_of('/');
    // last slash occurs after last dot, so no extension
    if (lastSlash != std::string::npos && lastSlash > lastDot) {
        return std::string();
    }
    // last dot after last slash, or no slash
    return name.substr(lastDot + 1);
}

/**
 * Generate temporary file.
 */
inline std::string tempFile() {
#ifdef _WIN32
    std::string templ;
    std::FILE* f = nullptr;
    while (f == nullptr) {
        templ = std::tmpnam(nullptr);
        f = fopen(templ.c_str(), "wx");
    }
    fclose(f);
    return templ;
#else
    char templ[40] = "./souffleXXXXXX";
    close(mkstemp(templ));
    return std::string(templ);
#endif
}

/**
 * Stringify a string using escapes for newline, tab, double-quotes and semicolons
 */
inline std::string stringify(const std::string& input) {
    std::string str(input);

    // replace semicolons returns by escape sequence
    size_t start_pos = 0;
    while ((start_pos = str.find(';', start_pos)) != std::string::npos) {
        str.replace(start_pos, 1, "\\;");
        start_pos += 2;
    }
    // replace double-quotes returns by escape sequence
    start_pos = 0;
    while ((start_pos = str.find('"', start_pos)) != std::string::npos) {
        str.replace(start_pos, 1, "\\\"");
        start_pos += 2;
    }
    // replace newline returns by escape sequence
    start_pos = 0;
    while ((start_pos = str.find('\n', start_pos)) != std::string::npos) {
        str.replace(start_pos, 1, "\\n");
        start_pos += 2;
    }
    // replace tab returns by escape sequence
    start_pos = 0;
    while ((start_pos = str.find('\t', start_pos)) != std::string::npos) {
        str.replace(start_pos, 1, "\\t");
        start_pos += 2;
    }
    return str;
}

/**
 * Escape JSON string.
 */
inline std::string escapeJSONstring(const std::string& JSONstr) {
    std::ostringstream destination;

    // Iterate over all characters except first and last
    for (char c : JSONstr) {
        if (c == '\"') {
            destination << "\\";
        }
        destination << c;
    }
    return destination.str();
}

/** Valid C++ identifier, note that this does not ensure the uniqueness of identifiers returned. */
inline std::string identifier(std::string id) {
    for (size_t i = 0; i < id.length(); i++) {
        if (((isalpha(id[i]) == 0) && i == 0) || ((isalnum(id[i]) == 0) && id[i] != '_')) {
            id[i] = '_';
        }
    }
    return id;
}

/** string operation for I/O directives */
inline std::string unescape(
        const std::string& inputString, const std::string& needle, const std::string& replacement) {
    std::string result = inputString;
    size_t pos = 0;
    while ((pos = result.find(needle, pos)) != std::string::npos) {
        result = result.replace(pos, needle.length(), replacement);
        pos += replacement.length();
    }
    return result;
}

inline std::string unescape(const std::string& inputString) {
    std::string unescaped = unescape(inputString, "\\\"", "\"");
    unescaped = unescape(unescaped, "\\t", "\t");
    unescaped = unescape(unescaped, "\\r", "\r");
    unescaped = unescape(unescaped, "\\n", "\n");
    return unescaped;
}

// -------------------------------------------------------------------------------
//                              Hint / Cache
// -------------------------------------------------------------------------------

/**
 * An Least-Recently-Used cache for arbitrary element types. Elements can be signaled
 * to be accessed and iterated through in their LRU order.
 */
template <typename T, unsigned size = 1>
class LRUCache {
    // the list of pointers maintained
    std::array<T, size> entries;

    // pointer to predecessor / successor in the entries list
    std::array<std::size_t, size> priv;  // < predecessor of element i
    std::array<std::size_t, size> post;  // < successor of element i

    std::size_t first{0};        // < index of the first element
    std::size_t last{size - 1};  // < index of the last element

public:
    // creates a new, empty cache
    LRUCache(const T& val = T()) {
        for (unsigned i = 0; i < size; i++) {
            entries[i] = val;
            priv[i] = i - 1;
            post[i] = i + 1;
        }
        priv[first] = last;
        post[last] = first;
    }

    // clears the content of this cache
    void clear(const T& val = T()) {
        for (auto& cur : entries) {
            cur = val;
        }
    }

    // registers an access to the given element
    void access(const T& val) {
        // test whether it is contained
        for (std::size_t i = 0; i < size; i++) {
            if (entries[i] != val) {
                continue;
            }

            // -- move this one to the front --

            // if it is the first, nothing to handle
            if (i == first) {
                return;
            }

            // if this is the last, just first and last need to change
            if (i == last) {
                auto tmp = last;
                last = priv[last];
                first = tmp;
                return;
            }

            // otherwise we need to update the linked list

            // remove from current position
            post[priv[i]] = post[i];
            priv[post[i]] = priv[i];

            // insert in first position
            post[i] = first;
            priv[i] = last;
            priv[first] = i;
            post[last] = i;

            // update first pointer
            first = i;
            return;
        }
        // not present => drop last, make it first
        entries[last] = val;
        auto tmp = last;
        last = priv[last];
        first = tmp;
    }

    /**
     * Iterates over the elements within this cache in LRU order.
     * The operator is applied on each element. If the operation
     * returns false, iteration is continued. If the operator return
     * true, iteration is stopped -- similar to the any operator.
     *
     * @param op the operator to be applied on every element
     * @return true if op returned true for any entry, false otherwise
     */
    template <typename Op>
    bool forEachInOrder(const Op& op) const {
        std::size_t i = first;
        while (i != last) {
            if (op(entries[i])) return true;
            i = post[i];
        }
        return op(entries[i]);
    }

    // equivalent to forEachInOrder
    template <typename Op>
    bool any(const Op& op) const {
        return forEachInOrder(op);
    }
};

template <typename T, unsigned size>
std::ostream& operator<<(std::ostream& out, const LRUCache<T, size>& cache) {
    bool first = true;
    cache.forEachInOrder([&](const T& val) {
        if (!first) {
            out << ",";
        }
        first = false;
        out << val;
        return false;
    });
    return out;
}

// a specialization for a single-entry cache
template <typename T>
class LRUCache<T, 1> {
    // the single entry in this cache
    T entry;

public:
    // creates a new, empty cache
    LRUCache() : entry() {}

    // creates a new, empty cache storing the given value
    LRUCache(const T& val) : entry(val) {}

    // clears the content of this cache
    void clear(const T& val = T()) {
        entry = val;
    }

    // registers an access to the given element
    void access(const T& val) {
        entry = val;
    }

    /**
     * See description in most general case.
     */
    template <typename Op>
    bool forEachInOrder(const Op& op) const {
        return op(entry);
    }

    // equivalent to forEachInOrder
    template <typename Op>
    bool any(const Op& op) const {
        return forEachInOrder(op);
    }

    // --- print support ---

    friend std::ostream& operator<<(std::ostream& out, const LRUCache& cache) {
        return out << cache.entry;
    }
};

// a specialization for no-entry caches.
template <typename T>
class LRUCache<T, 0> {
public:
    // creates a new, empty cache
    LRUCache(const T& = T()) {}

    // clears the content of this cache
    void clear(const T& = T()) {
        // nothing to do
    }

    // registers an access to the given element
    void access(const T&) {
        // nothing to do
    }

    /**
     * Always returns false.
     */
    template <typename Op>
    bool forEachInOrder(const Op&) const {
        return false;
    }

    // equivalent to forEachInOrder
    template <typename Op>
    bool any(const Op& op) const {
        return forEachInOrder(op);
    }

    // --- print support ---

    friend std::ostream& operator<<(std::ostream& out, const LRUCache& cache) {
        return out << "-empty-";
    }
};

// -------------------------------------------------------------------------------
//                           Hint / Cache Profiling
// -------------------------------------------------------------------------------

/**
 * A utility function to determine whether hints-profiling is enabled or
 * disabled;
 */
inline bool isHintsProfilingEnabled() {
    return std::getenv("SOUFFLE_PROFILE_HINTS") != nullptr;
}

/**
 * A utility class to keep track of cache hits/misses.
 */
class CacheAccessCounter {
    bool active;
    std::atomic<std::size_t> hits;
    std::atomic<std::size_t> misses;

public:
    CacheAccessCounter(bool active = isHintsProfilingEnabled()) : active(active), hits(0), misses(0) {}

    CacheAccessCounter(const CacheAccessCounter& other)
            : active(other.active), hits((active) ? other.getHits() : 0),
              misses((active) ? other.getMisses() : 0) {}

    void addHit() {
        if (active) {
            hits.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void addMiss() {
        if (active) {
            misses.fetch_add(1, std::memory_order_relaxed);
        }
    }

    std::size_t getHits() const {
        assert(active);
        return hits;
    }

    std::size_t getMisses() const {
        assert(active);
        return misses;
    }

    std::size_t getAccesses() const {
        assert(active);
        return getHits() + getMisses();
    }

    void reset() {
        hits = 0;
        misses = 0;
    }
};

}  // end namespace souffle
