/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file profile_util_test.cpp
 *
 * Test cases for the profile utilities.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "souffle/profile/CellInterface.h"
#include "souffle/profile/StringUtils.h"
#include <chrono>
#include <cmath>
#include <iosfwd>
#include <string>
#include <vector>

using namespace souffle;
using namespace souffle::profile;

TEST(StringUtil, formatNum) {
    EXPECT_EQ("0", Tools::formatNum(0));

    EXPECT_EQ("12", Tools::formatNum(12.0));
    EXPECT_EQ("12.45", Tools::formatNum(12.45));

    EXPECT_EQ("12", Tools::formatNum(1, 12));
    EXPECT_EQ("12", Tools::formatNum(2, 12));
    EXPECT_EQ("123", Tools::formatNum(2, 123));
    EXPECT_EQ("1.23K", Tools::formatNum(3, 1234));

    EXPECT_EQ("1.2K", Tools::formatNum(2, 1200));
    EXPECT_EQ("12K", Tools::formatNum(2, 12345));
    EXPECT_EQ("100K", Tools::formatNum(1, 123456));
    EXPECT_EQ("120K", Tools::formatNum(2, 123456));
    EXPECT_EQ("123K", Tools::formatNum(3, 123456));
    EXPECT_EQ("1.2B", Tools::formatNum(2, 1234560000));
    EXPECT_EQ("1.23B", Tools::formatNum(3, 1234560000));
    EXPECT_EQ("1.234B", Tools::formatNum(4, 1234560000));
    EXPECT_EQ("1.2t", Tools::formatNum(2, 1234560000000));
    EXPECT_EQ("1.23q", Tools::formatNum(3, 1234560000000000));
}

TEST(StringUtil, formatMemory) {
    EXPECT_EQ("0kB", Tools::formatMemory(0));
    EXPECT_EQ("1kB", Tools::formatMemory(1));
    EXPECT_EQ("1000kB", Tools::formatMemory(1000));
    EXPECT_EQ("1900kB", Tools::formatMemory(1900));

    EXPECT_EQ("2MB", Tools::formatMemory(2 * 1024));
    EXPECT_EQ("1000MB", Tools::formatMemory(1000 * 1024));
    EXPECT_EQ("1900MB", Tools::formatMemory(1900 * 1024));

    EXPECT_EQ("2GB", Tools::formatMemory(2 * 1024 * 1024));
    EXPECT_EQ("1000GB", Tools::formatMemory(1000 * 1024 * 1024));
    EXPECT_EQ("1900GB", Tools::formatMemory(1900 * 1024 * 1024));

    EXPECT_EQ("2TB", Tools::formatMemory(2UL * 1024UL * 1024UL * 1024UL));
    EXPECT_EQ("1000TB", Tools::formatMemory(1000ULL * 1024ULL * 1024ULL * 1024ULL));
    EXPECT_EQ("1900TB", Tools::formatMemory(1900ULL * 1024ULL * 1024ULL * 1024ULL));
}

TEST(StringUtil, formatTime) {
    std::chrono::microseconds time{0};
    EXPECT_EQ(".000s", Tools::formatTime(time));
    time = std::chrono::microseconds(1);
    EXPECT_EQ(".000s", Tools::formatTime(time));
    time = std::chrono::microseconds(1000);
    EXPECT_EQ(".001s", Tools::formatTime(time));
    time = std::chrono::microseconds(12340);
    EXPECT_EQ(".012s", Tools::formatTime(time));
    time = std::chrono::microseconds(123400);
    EXPECT_EQ(".123s", Tools::formatTime(time));
    time = std::chrono::microseconds(1234000);
    EXPECT_EQ("1.23s", Tools::formatTime(time));
    time = std::chrono::microseconds(12340000);
    EXPECT_EQ("12s", Tools::formatTime(time));

    time = std::chrono::microseconds(123400000);
    EXPECT_EQ("2.5m", Tools::formatTime(time));

    time = std::chrono::hours(25);
    EXPECT_EQ("25h", Tools::formatTime(time));

    time = std::chrono::hours(25 * 24);
    EXPECT_EQ("25D", Tools::formatTime(time));
}

TEST(StringUtil, split) {
    std::vector<std::string> expected{"a", "b", "c"};
    EXPECT_EQ(expected, Tools::split("a b c", " "));
    EXPECT_EQ(expected, Tools::split("a,b,c", ","));
    EXPECT_EQ(expected, Tools::split("aTESTbTESTc", "TEST"));

    expected = {"", "a", "b", "c", ""};
    EXPECT_EQ(expected, Tools::split("  a b c  ", " "));
    EXPECT_EQ(expected, Tools::split("TESTTESTaTESTbTESTcTESTTEST", "TEST"));

    expected = {"a", "b", "", "c"};
    EXPECT_EQ(expected, Tools::split(" a b  c ", " "));
    EXPECT_EQ(expected, Tools::split("TESTaTESTbTESTTESTcTEST", "TEST"));
}

TEST(StringUtil, trimWhitespace) {
    EXPECT_EQ("a b c", Tools::trimWhitespace("\t \t  a b c"));
    EXPECT_EQ("a b c", Tools::trimWhitespace("\t \t  a b c\t\t\t"));
    EXPECT_EQ("a b c", Tools::trimWhitespace("\t \t  a b c\t \t\t "));
    EXPECT_EQ("a b c", Tools::trimWhitespace("\t \t  a b c     "));
    EXPECT_EQ("a b c", Tools::trimWhitespace("  a b c     "));
}

TEST(StringUtil, cleanString) {
    EXPECT_EQ("   a b c", Tools::cleanString("\t\n\ta\nb\\nc"));
    EXPECT_EQ("   a b c", Tools::cleanString("\"\t\n\ta\nb\\nc\""));
}

TEST(StringUtil, cleanJsonOutString) {
    EXPECT_EQ("ab", Tools::cleanJsonOut("ab"));
    EXPECT_EQ("abc", Tools::cleanJsonOut("abc"));

    EXPECT_EQ("ab", Tools::cleanJsonOut("\"ab\""));

    EXPECT_EQ("\\\\", Tools::cleanJsonOut("\\"));

    EXPECT_EQ("\\\"", Tools::cleanJsonOut("\""));
}

TEST(StringUtil, cleanJsonOutDouble) {
    EXPECT_EQ("NaN", Tools::cleanJsonOut(NAN));
    EXPECT_EQ("1.234567e+02", Tools::cleanJsonOut(123.4567));
}
