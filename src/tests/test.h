/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file test.h
 *
 * Simple unit test infrastructure
 *
 ***********************************************************************/
#pragma once
#include "souffle/utility/CacheUtil.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/FileUtil.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/ParallelUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/StringUtil.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace testutil {

template <typename T>
std::vector<T> generateRandomVector(const std::size_t vectorSize, const int seed) {
    std::vector<T> values(vectorSize);

    std::default_random_engine randomGenerator(seed);

    if constexpr (std::is_floating_point<T>::value) {
        // For distribution bonds, following must hold:
        // a ≤ b and b − a ≤ numeric_limits<RealType>::max()
        // (in particular: if given values bounds, it will crash).
        // TODO (darth_tytus): Investigate a better solution.
        std::uniform_real_distribution<T> distribution(-1000, 1000);
        std::generate(values.begin(), values.end(),
                [&distribution, &randomGenerator]() { return distribution(randomGenerator); });
        return values;
    } else {
        std::uniform_int_distribution<T> distribution(
                std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max());
        std::generate(values.begin(), values.end(),
                [&distribution, &randomGenerator]() { return distribution(randomGenerator); });
        return values;
    }
}

/** Generate some corner-case test values. */
template <typename T>
std::vector<T> generateValues() {
    std::vector<T> values;

    if constexpr (std::is_floating_point<T>::value) {
        // corner cases
        values.push_back(std::numeric_limits<T>::min());
        values.push_back(0.0);
        values.push_back(1.0);
        values.push_back(1.0);
        values.push_back(3.0);
        values.push_back(12.0);
        values.push_back(1.0 + std::numeric_limits<T>::epsilon());
        values.push_back(1.0 - std::numeric_limits<T>::epsilon());
        values.push_back(std::numeric_limits<T>::max());
        values.push_back(std::numeric_limits<T>::max() / 2.0);

        if constexpr (std::is_signed<T>::value) {
            values.push_back(std::numeric_limits<T>::lowest());
            values.push_back(std::numeric_limits<T>::lowest() / 2.0);
            values.push_back(-1.0);
            values.push_back(-3.0);
            values.push_back(-12.0);
        }

        return values;
    } else {
        // corner cases
        values.push_back(0);
        values.push_back(1);
        values.push_back(3);
        values.push_back(12);
        values.push_back(0xAAAAAAAA);
        values.push_back(0x55555555);
        values.push_back(std::numeric_limits<T>::max());
        values.push_back(std::numeric_limits<T>::max() / 2);

        if constexpr (std::is_signed<T>::value) {
            values.push_back(std::numeric_limits<T>::lowest());
            values.push_back(std::numeric_limits<T>::lowest() / 2);
            values.push_back(-1);
            values.push_back(-3);
            values.push_back(-12);
        }

        return values;
    }
}

// easy function to suppress unused var warnings (when we REALLY don't need to use them!)
template <class T>
void ignore(const T&) {}
}  // namespace testutil

/* singly linked list for linking test cases */

static class TestCase* base = nullptr;

class TestCaseInterface {
public:
    /**
     * Checks condition
     *
     * checks whether condition holds and update counters.
     */
    struct test_result {
        bool success;
        std::ostream& out;
        test_result(bool success, std::ostream& out) : success(success), out(out) {}
        ~test_result() {
            if (!success) out << "\n\n";
        }
        operator bool() const {
            return success;
        }
    };

    virtual test_result evaluate(bool condition) = 0;
    virtual std::ostream& fatal(
            bool condition, const std::string& /* txt */, const std::string& /* loc */) = 0;
    virtual std::ostream& log() = 0;
};

class TestCase : public TestCaseInterface {
private:
    TestCase* next;          // next test case (linked by constructor)
    std::string group;       // group name of test
    std::string test;        // test name of test
    std::size_t num_checks;  // number of checks
    std::size_t num_failed;  // number of failed checks

protected:
    std::ostream& logstream;  // logfile

public:
    TestCase(std::string g, std::string t)
            : group(std::move(g)), test(std::move(t)), num_checks(0), num_failed(0), logstream(std::cerr) {
        next = base;
        base = this;
    }
    virtual ~TestCase() = default;

    TestCaseInterface& testInterface() {
        return *this;
    }

    /**
     * Checks the condition and keeps record of passed and failed checkes.
     */
    test_result evaluate(bool condition) override {
        num_checks++;
        if (!condition) num_failed++;
        return test_result(condition, logstream);
    }

    /**
     * Fatal condition
     *
     * Same as check() except in case of a condition that evaluates to false, the method
     * aborts the test.
     */
    std::ostream& fatal(bool condition, const std::string& /* txt */, const std::string& /* loc */) override {
        if (!condition) {
            std::cerr << "Tests failed.\n";
            exit(99);
        }
        return logstream;
    }

    std::ostream& log() override {
        return logstream;
    }

    /**
     * Run method
     */
    virtual void run() = 0;

    /**
     * Next Test Case in singly linked list
     */
    TestCase* nextTestCase() {
        return next;
    }

    /**
     * get test name
     */
    const std::string& getTest() const {
        return test;
    }

    /**
     * get name of test group
     */
    const std::string& getGroup() const {
        return group;
    }

    /**
     * get number of checks
     */
    std::size_t getChecks() const {
        return num_checks;
    }

    /**
     * get number of failed checks
     */
    std::size_t getFailed() const {
        return num_failed;
    }
};

#define PASTE(x, y) x##y
#define PASTE2(x, y) PASTE(x, y)

#define TEST(a, b)                                                       \
    class test_##a##_##b : public TestCase {                             \
    public:                                                              \
        test_##a##_##b(std::string g, std::string t) : TestCase(g, t) {} \
        void run();                                                      \
    } Test_##a##_##b(#a, #b);                                            \
    void test_##a##_##b::run()

#define DERIVED_TEST(a, b, Base)                                                  \
    class test_##a##_##b : public TestCase, public Base {                         \
    public:                                                                       \
        test_##a##_##b(std::string g, std::string t)                              \
                : TestCase(g, t), Base(*static_cast<TestCaseInterface*>(this)) {} \
        void run();                                                               \
    } Test_##a##_##b(#a, #b);                                                     \
    void test_##a##_##b::run()

#define TEMPLATE_TEST(a, b, Param, P)                                                       \
    template <Param>                                                                        \
    class test_##a##_##b : public TestCase {                                                \
    public:                                                                                 \
        test_##a##_##b(std::string g, std::string t, std::string k) : TestCase(g, t + k) {} \
        void run();                                                                         \
    };                                                                                      \
    template <Param>                                                                        \
    void test_##a##_##b<P>::run()

#define SPECIALIZE_TEMPLATE_TEST(a, b, P) \
    template <>                           \
    void test_##a##_##b<P>::run()

#define INSTANTIATE_TEMPLATE_TEST(a, b, V) test_##a##_##b<V> PASTE2(Test_##a##_##b##_, __LINE__)(#a, #b, #V)

#define S(x) #x
#define S_(x) S(x)
#define S__LINE__ S_(__LINE__)

#define LOC S__LINE__
#define _EXPECT(condition, loc)                             \
    if (auto __res = testInterface().evaluate(condition)) { \
    } else                                                  \
        testInterface().log() << "\t\tTEST FAILED @ line " << (loc) << " : "

#define EXPECT_TRUE(a) _EXPECT(a, LOC) << "expecting " << #a << " to be true, evaluated to false"
#define EXPECT_FALSE(a) _EXPECT(!(a), LOC) << "expecting " << #a << " to be false, evaluated to true"
#define EXPECT_EQ(a, b)                                                                                 \
    _EXPECT((a) == (b), LOC) << "expected " << #a << " == " << #b << " where\n\t\t\t" << #a             \
                             << " evaluates to " << toString(a) << "\n\t\t\t" << #b << " evaluates to " \
                             << toString(b)
#define EXPECT_NE(a, b)                                                                                 \
    _EXPECT((a) != (b), LOC) << "expected " << #a << " != " << #b << " where\n\t\t\t" << #a             \
                             << " evaluates to " << toString(a) << "\n\t\t\t" << #b << " evaluates to " \
                             << toString(b)
#define EXPECT_LT(a, b)                                                                                \
    _EXPECT((a) < (b), LOC) << "expected " << #a << " < " << #b << " where\n\t\t\t" << #a              \
                            << " evaluates to " << toString(a) << "\n\t\t\t" << #b << " evaluates to " \
                            << toString(b)
#define EXPECT_STREQ(a, b)                                                                           \
    _EXPECT(std::string(a) == std::string(b), LOC)                                                   \
            << "expected std::string(" << #a << ") == std::string(" << #b << ") where\n\t\t\t" << #a \
            << " evaluates to " << toString(a) << "\n\t\t\t" << #b << " evaluates to " << toString(b)
#define EXPECT_PRED2(p, a, b)                                                                        \
    _EXPECT(p(a, b), LOC) << "expected " << (#p "(" #a "," #b ")") << " where\n\t\t\t" << #a         \
                          << " evaluates to " << toString(a) << "\n\t\t\t" << #b << " evaluates to " \
                          << toString(b)

#define ASSERT_TRUE(a) fatal(a, #a, LOC)
#define ASSERT_LE(a, b) fatal((a) <= (b), "LE(" #a "," #b ")", LOC)

/**
 * Main program of a unit test
 */
int main(int /* argc */, char** /* argv */) {
    // add all groups to a set
    std::set<std::string> groups;
    for (TestCase* p = base; p != nullptr; p = p->nextTestCase()) {
        groups.insert(p->getGroup());
    }

    // traverse groups and execute associated test cases
    int failure = 0;
    for (auto& group : groups) {
        std::cout << group << "\n";
        for (TestCase* p = base; p != nullptr; p = p->nextTestCase()) {
            if (p->getGroup() == group) {
                p->run();
                std::cerr << "\t" << ((p->getFailed() == 0) ? "OK" : "FAILED");
                std::cerr << " (" << p->getChecks() - p->getFailed();
                std::cerr << "/" << p->getChecks();
                std::cerr << ")\t" << p->getTest() << "\n";
                if (p->getFailed() != 0) {
                    failure = 99;
                }
            }
        }
    }

    if (failure != 0) {
        std::cerr << "Tests failed.\n";
    }

    return failure;
}
