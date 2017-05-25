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
#include "Util.h"

#include <fstream>
#include <iostream>
#include <set>
#include <string>

/* singly linked list for linking test caes */

static class TestCase* base = nullptr;

class TestCase {
private:
    TestCase* next;     // next test case (linked by constructor)
    std::string group;  // group name of test
    std::string test;   // test name of test
    size_t num_checks;  // number of checks
    size_t num_failed;  // number of failed checks

protected:
    std::ostream& logstream;  // logfile

public:
    TestCase(std::string g, std::string t)
            : group(g), test(t), num_checks(0), num_failed(0), logstream(std::cerr) {
        next = base;
        base = this;
    }
    virtual ~TestCase() {}

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

    /**
     * Checks the condition and keeps record of passed and failed checkes.
     */
    test_result evaluate(bool condition) {
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
    std::ostream& fatal(bool condition, const std::string& txt, const std::string& loc) {
        if (!condition) {
            std::cerr << "Tests failed.\n";
            exit(99);
        }
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
    size_t getChecks() const {
        return num_checks;
    }

    /**
     * get number of failed checks
     */
    size_t getFailed() const {
        return num_failed;
    }
};

#define TEST(a, b)                                                       \
    class test_##a##_##b : public TestCase {                             \
    public:                                                              \
        test_##a##_##b(std::string g, std::string t) : TestCase(g, t) {} \
        void run();                                                      \
    } Test_##a##_##b(#a, #b);                                            \
    void test_##a##_##b::run()

#define S(x) #x
#define S_(x) S(x)
#define S__LINE__ S_(__LINE__)

#define LOC S__LINE__
#define _EXPECT(condition, loc)             \
    if (auto __res = evaluate(condition)) { \
    } else                                  \
    logstream << "\t\tTEST FAILED @ line " << (loc) << " : "

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

int main(int argc, char** argv) {
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
