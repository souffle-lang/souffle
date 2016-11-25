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

#include <stdlib.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>

#include <algorithm>
#include <string>
#include <sstream>
#include <map>
#include <set>
#include <vector>
#include <ostream>
#include <chrono>
#include <assert.h>
#include <memory>

/* Macro for ASSERT */ 
#ifndef ASSERT
#ifndef OPT 
#define ASSERT(x) assert(x)
#else
#define ASSERT(x) 
#endif
#endif

namespace souffle {

/**
 * Check whether a string is a sequence of numbers
 */
inline bool isNumber(const char *str)
{
    if (str==NULL) return false;

    while(*str) {
        if(!isdigit(*str))
            return false;
        str++;
    }
    return true;
}

// -------------------------------------------------------------------------------
//                           General Container Utilities
// -------------------------------------------------------------------------------

/**
 * A utility function enabling the creation of a vector with a fixed set of
 * elements within a single expression. This is the base case covering empty
 * vectors.
 */
template<typename T>
std::vector<T> toVector() {
    return std::vector<T>();
}

/**
 * A utility function enabling the creation of a vector with a fixed set of
 * elements within a single expression. This is the step case covering vectors
 * of arbitrary length.
 */
template<typename T, typename ... R>
std::vector<T> toVector(const T& first, const R& ... rest) {
    return { first, rest ... };
}

/**
 * A utility to check generically whether a given element is contained in a given
 * container.
 */
template<typename C>
bool contains(const C& container, const typename C::value_type& element) {
    return std::find(container.begin(), container.end(), element) != container.end();
}

template<typename T>
std::vector<T*> toPtrVector(const std::vector<std::unique_ptr<T>> &v) {
    std::vector<T*> res;
    for (auto &e : v) {
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
template<typename Iter>
struct range {

    // the lower and upper boundary
    Iter a,b;

    // a constructor accepting a lower and upper boundary
    range(const Iter& a, const Iter& b) : a(a), b(b) {}

    // default copy / move and assignment support
    range(const range&) = default;
    range(range&&) = default;
    range& operator=(const range&) = default;

    // get the lower boundary (for for-all loop)
    Iter& begin() { return a; }
    const Iter& begin() const { return a; }

    // get the upper boundary (for for-all loop)
    Iter& end() { return b; }
    const Iter& end() const { return b; }

    // emptiness check
    bool empty() const { return a == b; }
};

/**
 * A utility function enabling the construction of ranges
 * without explicitly specifying the iterator type.
 *
 * @tparam Iter .. the iterator type
 * @param a .. the lower boundary
 * @param b .. the upper boundary
 */
template<typename Iter>
range<Iter> make_range(const Iter& a, const Iter& b) {
    return range<Iter>(a,b);
}


// -------------------------------------------------------------------------------
//                             Equality Utilities
// -------------------------------------------------------------------------------

/**
 * A functor class supporting the values pointers are pointing to.
 */
template<typename T>
struct comp_deref {
    bool operator()(const T &a, const T &b) const {
        if (a == nullptr) return false;
        if (b == nullptr) return false;
        return *a == *b;
    }
};


/**
 * A function testing whether two vectors are equal (same list of elements).
 */
template<typename T, typename Comp = std::equal_to<T>>
bool equal(const std::vector<T>& a, const std::vector<T>& b, const Comp& comp = Comp()) {
    // check reference
    if (&a == &b) return true;

    // check size
    if (a.size() != b.size()) return false;

    // check content
    for(std::size_t i = 0; i<a.size(); ++i) {
        // if there is a difference
        if (!comp(a[i],b[i])) return false;
    }

    // all the same
    return true;
}

/**
 * A function testing whether two list of pointers are referencing to equivalent
 * targets.
 */
template<typename T>
bool equal_targets(const std::vector<T*>& a, const std::vector<T*>& b) {
    return equal(a,b,comp_deref<T*>());
}

/**
 * A function testing whether two list of pointers are referencing to equivalent
 * targets.
 */
template<typename T>
bool equal_targets(const std::vector<std::unique_ptr<T>>& a, const std::vector<std::unique_ptr<T>>& b) {
    return equal(a,b,comp_deref<std::unique_ptr<T>>());
}

/**
 * Compares two values referenced by a pointer where the case where both
 * pointers are null is also considered equivalent.
 */
template<typename T>
bool equal_ptr(const T* a, const T* b) {
    if (!a && !b) return true;
    if (a && b) return *a == *b;
    return false;
}

/**
 * Compares two values referenced by a pointer where the case where both
 * pointers are null is also considered equivalent.
 */
template<typename T>
bool equal_ptr(const std::unique_ptr<T> &a, const std::unique_ptr<T> &b) {
    if (!a && !b) return true;
    if (a && b) return *a == *b;
    return false;
}

// -------------------------------------------------------------------------------
//                               I/O Utils
// -------------------------------------------------------------------------------

/**
 * A stream ignoring everything written to it.
 * Note, avoiding the write in the first place may be more efficient.
 */
class NullStream : public std::ostream {
public:
    NullStream() : std::ostream(&buffer) {}
private:
    struct NullBuffer : public std::streambuf {
        int overflow(int c) { return c; }
    };
    NullBuffer buffer;
};

/**
 * A stream copying its input to multiple output streams.
 */
class SplitStream : public std::ostream, public std::streambuf {
private:
    std::vector<std::ostream *> streams;
public:
    SplitStream(std::vector<std::ostream *> streams) : std::ostream(this), streams(streams) { }
    SplitStream(std::ostream *stream1, std::ostream *stream2) : std::ostream(this) {
        streams.push_back(stream1);
        streams.push_back(stream2);
    }
    int overflow(int c) {
        for (auto stream : streams) {
            stream->put(c);
        }
        return c;
    }
};

// -------------------------------------------------------------------------------
//                           General Print Utilities
// -------------------------------------------------------------------------------


namespace detail {

    /**
     * A auxiliary class to be returned by the join function aggregating the information
     * required to print a list of elements as well as the implementation of the printing
     * itsefl.
     */
    template<typename Iter, typename Printer>
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
        joined_sequence(const Iter& a, const Iter& b, const std::string& sep, const Printer& p)
            : begin(a), end(b), sep(sep), p(p) {}

        /** The actual print method */
        friend std::ostream& operator<<(std::ostream& out, const joined_sequence& s) {
            auto cur = s.begin;
            if (cur == s.end) return out;

            s.p(out, *cur);
            ++cur;
            for(;cur != s.end; ++cur) {
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
    template<typename Extractor>
    struct print {
        template<typename T>
        void operator()(std::ostream& out, const T& value) const {
            // extract element to be printed from the given value and print it
            Extractor ext;
            out << ext(value);
        }
    };

}

/**
 * A functor representing the identity function for a generic type T.
 *
 * @tparam T some arbitrary type
 */
template<typename T>
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
template<typename T>
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
template<typename T>
struct print_deref : public detail::print<deref<T>> {};

/**
 * Creates an object to be forwarded to some output stream for printing
 * sequences of elements interspersed by a given separator.
 *
 * For use cases see the test case {util_test.cpp}.
 */
template<typename Iter, typename Printer>
detail::joined_sequence<Iter,Printer>
join(const Iter& a, const Iter& b, const std::string& sep, const Printer& p) {
    return detail::joined_sequence<Iter,Printer>(a,b,sep,p);
}

/**
 * Creates an object to be forwarded to some output stream for printing
 * sequences of elements interspersed by a given separator.
 *
 * For use cases see the test case {util_test.cpp}.
 */
template<typename Iter, typename T = typename Iter::value_type>
detail::joined_sequence<Iter,detail::print<id<T>>>
join(const Iter& a, const Iter& b, const std::string& sep = ",") {
    return join(a,b,sep,detail::print<id<T>>());
}

/**
 * Creates an object to be forwarded to some output stream for printing
 * the content of containers interspersed by a given separator.
 *
 * For use cases see the test case {util_test.cpp}.
 */
template<
    typename Container,
    typename Printer,
    typename Iter = typename Container::const_iterator
>
detail::joined_sequence<Iter,Printer>
join(const Container& c, const std::string& sep, const Printer& p) {
    return join(c.begin(), c.end(), sep, p);
}

/**
 * Creates an object to be forwarded to some output stream for printing
 * the content of containers interspersed by a given separator.
 *
 * For use cases see the test case {util_test.cpp}.
 */
template<
    typename Container,
    typename Iter = typename Container::const_iterator,
    typename T = typename Iter::value_type
>
detail::joined_sequence<Iter,detail::print<id<T>>>
join(const Container& c, const std::string& sep = ",") {
    return join(c.begin(), c.end(), sep, detail::print<id<T>>());
}


} // end namespace souffle

#ifndef __EMBEDDED_SOUFFLE__

namespace std {

    /**
     * Introduces support for printing pairs as long as their components can be printed.
     */
    template<typename A, typename B>
    ostream& operator<<(ostream& out, const pair<A,B>& p) {
        return out << "(" << p.first << "," << p.second << ")";
    }

    /**
     * Enables the generic printing of vectors assuming their element types
     * are printable.
     */
    template<typename T, typename A>
    ostream& operator<<(ostream& out, const vector<T,A>& v) {
        return out << "[" << souffle::join(v) << "]";
    }

    /**
     * Enables the generic printing of sets assuming their element types
     * are printable.
     */
    template<typename K, typename C, typename A>
    ostream& operator<<(ostream& out, const set<K,C,A>& s) {
        return out << "{" << souffle::join(s) << "}";
    }

    /**
     * Enables the generic printing of maps assuming their element types
     * are printable.
     */
    template<typename K, typename T, typename C, typename A>
    ostream& operator<<(ostream& out, const map<K,T,C,A>& m) {
        return out << "{" << souffle::join(m,",",[](ostream& out, const pair<K,T>& cur) {
            out << cur.first << "->" << cur.second;
        }) << "}";
    }

} // end namespace std

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
	template<typename T, typename filter = void>
	struct is_printable : public std::false_type {};

	/**
	 * A type trait to check whether a given type is printable.
	 * This specialization makes types with an output operator printable.
	 */
	template<typename T>
	struct is_printable<T,
			typename std::conditional<false,decltype(std::declval<std::ostream&>() << std::declval<T>()),void>::type
	> : public std::true_type {};
}


/**
 * A generic function converting arbitrary objects to strings by utilizing
 * their print capability.
 *
 * This function is mainly intended for implementing test cases and debugging
 * operations.
 */
template<typename T>
typename std::enable_if<detail::is_printable<T>::value,std::string>::type
toString(const T& value) {
    // write value into stream and return result
    std::stringstream ss;
    ss << value;
    return ss.str();
}

/**
 * A fallback for the to-string function in case an unprintable object is supposed
 * to be printed.
 */
template<typename T>
typename std::enable_if<!detail::is_printable<T>::value,std::string>::type
toString(const T& value) {
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
    template<typename T>
    struct multiplying_printer {
        const T& value;
        unsigned times;
        multiplying_printer(const T& value, unsigned times)
            : value(value), times(times) {}

        friend std::ostream& operator<<(std::ostream& out, const multiplying_printer& printer) {
            for(unsigned i = 0; i<printer.times; i++) {
                out << printer.value;
            }
            return out;
        }
    };

}

/**
 * A utility printing a given value multiple times.
 */
template<typename T>
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
	if (value.size() < ending.size()) return false;
	return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}


// -------------------------------------------------------------------------------
//                              Functional Utils
// -------------------------------------------------------------------------------


/**
 * A functor comparing the dereferenced value of a pointer type utilizing a
 * given comparator. Its main use case are sets of non-null pointers which should
 * be ordered according to the value addressed by the pointer.
 */
template<typename T, typename C = std::less<T>>
struct deref_less {
    bool operator()(const T* a, const T* b) const {
        return C()(*a,*b);
    }
};



// -------------------------------------------------------------------------------
//                               Lambda Utils
// -------------------------------------------------------------------------------


namespace detail {

    template<typename T>
    struct lambda_traits_helper;

    template<typename R>
    struct lambda_traits_helper<R()> {
        typedef R result_type;
    };

    template<typename R, typename A0>
    struct lambda_traits_helper<R(A0)> {
        typedef R result_type;
        typedef A0 arg0_type;
    };

    template<typename R, typename A0, typename A1>
    struct lambda_traits_helper<R(A0,A1)> {
        typedef R result_type;
        typedef A0 arg0_type;
        typedef A1 arg1_type;
    };

    template<typename R, typename ... Args>
    struct lambda_traits_helper<R(Args...)> {
        typedef R result_type;
    };

    template<typename R, typename C, typename ... Args>
    struct lambda_traits_helper<R(C::*)(Args...)>
        : public lambda_traits_helper<R(Args...)> {};

    template<typename R, typename C, typename ... Args>
    struct lambda_traits_helper<R(C::*)(Args...) const>
        : public lambda_traits_helper<R(C::*)(Args...)> {};

}

/**
 * A type trait enabling the deduction of type properties of lambdas.
 * Those include so far:
 *      - the result type (result_type)
 *      - the first argument type (arg0_type)
 */
template<typename Lambda>
struct lambda_traits : public detail::lambda_traits_helper<decltype(&Lambda::operator())> {};


// -------------------------------------------------------------------------------
//                              Functional Wrappers
// -------------------------------------------------------------------------------

/**
 * A struct wrapping a object and an associated member function pointer into a
 * callable object.
 */
template<typename Class, typename R, typename ... Args>
struct member_fun {
    typedef R(Class::* fun_type)(Args...);
    Class& obj;
    fun_type fun;
    R operator()(Args ... args) const {
        return (obj.*fun)(args...);
    }
};

/**
 * Wraps an object and matching member function pointer into a callable object.
 */
template<typename C, typename R, typename ... Args>
member_fun<C,R,Args...> mfun(C& obj, R(C::* f)(Args...)) {
    return member_fun<C,R,Args...>({obj, f});
}


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
template<typename Container, typename UnaryPredicate>
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
template<typename Container, typename UnaryPredicate>
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
template<typename Container, typename UnaryPredicate>
bool none_of(const Container& c, UnaryPredicate p) {
    return std::none_of(c.begin(), c.end(), p);
}



// -------------------------------------------------------------------------------
//                               Timing Utils
// -------------------------------------------------------------------------------

// a type def for a time point
typedef std::chrono::high_resolution_clock::time_point time_point;

// a shortcut for taking the current time
inline time_point now() {
    return std::chrono::high_resolution_clock::now();
}

// a shortcut for obtaining the time difference in milliseconds
inline long duration_in_ms(const time_point& start, const time_point& end) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
}

// a shortcut for obtaining the time difference in nanoseconds
inline long duration_in_ns(const time_point& start, const time_point& end) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();
}

// -------------------------------------------------------------------------------
//                               File Utils
// -------------------------------------------------------------------------------

/**
 *  Check whether a file exists in the file system
 */
inline bool existFile (const std::string& name) {
    struct stat buffer;
    if (stat (name.c_str(), &buffer) == 0) {
        if ((buffer.st_mode & S_IFREG) != 0) {
            return true;
        }
    }
    return false;
}

/**
 *  Check whether a directory exists in the file system
 */
inline bool existDir (const std::string& name) {
    struct stat buffer;
    if (stat (name.c_str(), &buffer) == 0) {
        if ((buffer.st_mode & S_IFDIR) != 0) {
            return true;
        }
    }
    return false;
}

/**
 * Check whether a given file exists and it is an executable
 */
inline int isExecutable(const std::string& name) {
    return existFile(name) && !access(name.c_str(), X_OK);
}

/**
 * Simple implementation of a which tool
 */
inline std::string which(const std::string& name) {
    char buf[PATH_MAX];
    if (::realpath(name.c_str(), buf) && isExecutable(buf))
        return std::string(buf);
    else {
        std::string syspath = ::getenv("PATH");
        std::stringstream sstr(syspath);
        std::string sub;
        while (std::getline(sstr, sub, ':')) {
            std::string path = sub + "/" + name;
            if (isExecutable(path) && realpath(path.c_str(), buf))
              return std::string(buf);
        }
    }
    return "";
}

/**
 *  C++-style dirname
 */
inline std::string dirName(std::string &name) {
    char buf[PATH_MAX];
    strcpy(buf, name.c_str());
    return std::string(dirname(buf));
}

/**
 *  C++-style realpath
 */
inline std::string absPath(std::string &path) {
    char buf[PATH_MAX];
    char *res = realpath(path.c_str(), buf);
    return (res == NULL) ? "" : std::string(buf);
}

/*
 * Find out if an executable given by @p tool exists in the path given @p path
 * relative to the directory given by @ base. A path here refers a
 * colon-separated list of directories.
 */
inline std::string findTool(std::string tool, std::string base, std::string path) {
    std::string dir = dirName(base);
    std::stringstream sstr(path);
    std::string sub;

    while (std::getline(sstr, sub, ':')) {
      std::string subpath = dir + "/" +  sub + '/' + tool;
      if (isExecutable(subpath))
          return absPath(subpath);
    }
    return "";
}

/*
 * Get the basename of a fully qualified filename
 */
inline std::string baseName(std::string &filename)
{
   char fn[filename.size()+1];
   strcpy(fn,filename.c_str()); 
   std::string result = basename(fn); 
   return result;
} 

} // end namespace souffle


