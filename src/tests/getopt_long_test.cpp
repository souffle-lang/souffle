/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2022, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file getopt_long_test.cpp
 *
 * A test case testing the custom getopt_long implementation
 *
 ***********************************************************************/

#ifndef USE_CUSTOM_GETOPTLONG
#define USE_CUSTOM_GETOPTLONG
#endif

#include "tests/test.h"

#include "souffle/utility/GetOptLongImpl.h"

namespace souffle::test {

TEST(GetOptLong, Short) {
    optind = 0;
    opterr = 0;

    char* const argv[] = {const_cast<char*>("test"), const_cast<char*>("-z"), const_cast<char*>("-x"),
            const_cast<char*>("argx"), const_cast<char*>("-yargy"), const_cast<char*>("-z"),
            const_cast<char*>("argz"), const_cast<char*>("-zargz")};

    const int argc = 8;
    const char* optstr = "x:y:z::";
    struct option longopts[1];
    memset(longopts, 0, sizeof(struct option));

    char c;
    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 'z');
    EXPECT_EQ(optarg, nullptr);

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 'x');
    EXPECT_EQ(std::string(optarg), std::string("argx"));

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 'y');
    EXPECT_EQ(std::string(optarg), std::string("argy"));

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 'z');
    // z has an optional argument and it's not in the same argv cell so optarg is set to null
    // and optind is the position of the possible argument in argv
    EXPECT_EQ(std::string(argv[optind]), std::string("argz"));
    // accept the argument
    optind += 1;

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 'z');
    // z has an optional argument and it in the same argv cell so optarg is set
    EXPECT_EQ(std::string(optarg), std::string("argz"));

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, -1);
}

TEST(GetOptLong, SimplePermutation) {
    optind = 0;
    opterr = 0;

    // expect arguments to be permuted and show up as if -x, -y, arg1, arg2, arg3, arg4
    char* const argv[] = {
            const_cast<char*>("test"),
            const_cast<char*>("arg1"),
            const_cast<char*>("-x"),
            const_cast<char*>("arg2"),
            const_cast<char*>("arg3"),
            const_cast<char*>("-y"),
            const_cast<char*>("arg4"),
    };

    const int argc = 7;

    struct option longopts[1];
    const char* optstr = "xy";
    memset(longopts, 0, sizeof(struct option));

    char c;
    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 'x');
    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 'y');
    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, -1);
    EXPECT_EQ(optind, 3);
    EXPECT_EQ(std::string(argv[optind]), std::string("arg1"));
    EXPECT_EQ(std::string(argv[optind + 1]), std::string("arg2"));
    EXPECT_EQ(std::string(argv[optind + 2]), std::string("arg3"));
    EXPECT_EQ(std::string(argv[optind + 3]), std::string("arg4"));
}

TEST(GetOptLong, ComplexPermutation) {
    optind = 0;
    opterr = 0;

    // expect arguments to be permuted and show up as if -x argx, -y, arg1
    char* const argv[] = {
            const_cast<char*>("test"),
            const_cast<char*>("arg1"),
            const_cast<char*>("-x"),
            const_cast<char*>("argx"),
            const_cast<char*>("-y"),
    };

    const int argc = 5;
    const char* optstr = "x:y::";
    struct option longopts[1];
    memset(longopts, 0, sizeof(struct option));

    char c;
    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 'x');
    EXPECT_NE(optarg, nullptr);
    EXPECT_EQ(std::string(optarg), std::string("argx"));

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 'y');
    EXPECT_EQ(optarg, nullptr);

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, -1);
    EXPECT_EQ(optarg, nullptr);
    EXPECT_EQ(optind, 4);
    EXPECT_EQ(std::string(argv[optind]), std::string("arg1"));
}

TEST(GetOptLong, Short2) {
    optind = 0;
    opterr = 0;

    char* const argv[] = {const_cast<char*>("test"), const_cast<char*>("-nnntarg")};

    const int argc = 2;
    const char* optstr = "nt:";
    struct option longopts[1];
    memset(longopts, 0, sizeof(struct option));

    char c;
    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 'n');
    EXPECT_EQ(optarg, nullptr);

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 'n');
    EXPECT_EQ(optarg, nullptr);

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 'n');
    EXPECT_EQ(optarg, nullptr);

    // reset
    optind = 0;

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 'n');
    EXPECT_EQ(optarg, nullptr);

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 'n');
    EXPECT_EQ(optarg, nullptr);

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 'n');
    EXPECT_EQ(optarg, nullptr);

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 't');
    EXPECT_EQ(std::string(optarg), std::string("arg"));

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, -1);
}

TEST(GetOptLong, Empty) {
    optind = 0;
    opterr = 0;

    char* const argv[] = {const_cast<char*>("test")};

    const int argc = 1;
    const char* optstr = "nt:";
    struct option longopts[1];
    memset(longopts, 0, sizeof(struct option));

    char c;
    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, -1);

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, -1);

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, -1);
}

TEST(GetOptLong, NonOpts) {
    optind = 0;
    opterr = 0;

    char* const argv[] = {const_cast<char*>("test"), const_cast<char*>("nonopt1"),
            const_cast<char*>("nonopt2"), const_cast<char*>("nonopt3"), const_cast<char*>("-n")};

    const int argc = 5;
    const char* optstr = "nt:";
    struct option longopts[1];
    memset(longopts, 0, sizeof(struct option));

    char c;

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 'n');
    EXPECT_EQ(optind, 5);
    EXPECT_EQ(optarg, nullptr);

    for (int i = 0; i < 3; ++i) {
        c = getopt_long(argc, argv, optstr, longopts, nullptr);
        EXPECT_EQ(c, -1);
        EXPECT_EQ(optind, 2);
        EXPECT_EQ(std::string(argv[optind]), std::string("nonopt1"));
        EXPECT_EQ(std::string(argv[optind + 1]), std::string("nonopt2"));
        EXPECT_EQ(std::string(argv[optind + 2]), std::string("nonopt3"));
    }
}

TEST(GetOptLong, MissingArg) {
    optind = 0;
    opterr = 0;

    char* const argv[] = {const_cast<char*>("test"), const_cast<char*>("-t")};

    const int argc = 2;
    const char* optstr = "nt:";
    struct option longopts[1];
    memset(longopts, 0, sizeof(struct option));

    char c;

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, '?');
}

TEST(GetOptLong, MissingArg2) {
    optind = 0;
    opterr = 0;

    char* const argv[] = {const_cast<char*>("test"), const_cast<char*>("-t")};

    const int argc = 2;
    const char* optstr = ":nt:";
    struct option longopts[1];
    memset(longopts, 0, sizeof(struct option));

    char c;

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, ':');
}

TEST(GetOptLong, MissingArg3) {
    optind = 0;
    opterr = 0;

    char* const argv[] = {const_cast<char*>("test"), const_cast<char*>("-t"), const_cast<char*>("-t")};

    const int argc = 3;
    const char* optstr = "nt:";
    struct option longopts[1];
    memset(longopts, 0, sizeof(struct option));

    char c;

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, '?');
}

TEST(GetOptLong, MissingArg4) {
    optind = 0;
    opterr = 0;

    char* const argv[] = {const_cast<char*>("test"), const_cast<char*>("-t"), const_cast<char*>("-n")};

    const int argc = 3;
    const char* optstr = "nt:";
    struct option longopts[1];
    memset(longopts, 0, sizeof(struct option));

    char c;

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, '?');
}

TEST(GetOptLong, MissingArg5) {
    optind = 0;
    opterr = 0;

    char* const argv[] = {const_cast<char*>("test"), const_cast<char*>("-t"), const_cast<char*>("--long")};

    const int argc = 3;
    const char* optstr = "nt:";
    struct option longopts[2];
    memset(longopts, 0, 2 * sizeof(struct option));
    longopts[1].name = "long";
    longopts[1].val = 1;

    char c;

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, '?');
}

TEST(GetOptLong, DashArg) {
    optind = 0;
    opterr = 0;

    char* const argv[] = {
            const_cast<char*>("test"),
            const_cast<char*>("-t"),
            const_cast<char*>("-"),
            const_cast<char*>("-t"),
            const_cast<char*>("--"),
    };

    const int argc = 5;
    const char* optstr = "nt:";
    struct option longopts[1];
    memset(longopts, 0, sizeof(struct option));

    char c;

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 't');
    EXPECT_EQ(optind, 3);
    EXPECT_EQ(std::string(optarg), std::string("-"));

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 't');
    EXPECT_EQ(optind, 5);
    EXPECT_EQ(std::string(optarg), std::string("--"));
}

TEST(GetOptLong, NonOptionDash) {
    optind = 0;
    opterr = 0;

    char* const argv[] = {const_cast<char*>("test"), const_cast<char*>("-"), const_cast<char*>("-n")};

    const int argc = 3;
    const char* optstr = "nt:";
    struct option longopts[1];
    memset(longopts, 0, sizeof(struct option));

    char c;

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 'n');

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, -1);
    EXPECT_EQ(optind, 2);
    EXPECT_EQ(std::string(argv[2]), std::string("-"));
}

TEST(GetOptLong, DashDashEndScan) {
    optind = 0;
    opterr = 0;

    char* const argv[] = {const_cast<char*>("test"), const_cast<char*>("-n"), const_cast<char*>("--"),
            const_cast<char*>("-t"), const_cast<char*>("opt")};

    const int argc = 5;
    const char* optstr = "nt:";
    struct option longopts[1];
    memset(longopts, 0, sizeof(struct option));

    char c;

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, 'n');

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, -1);
    EXPECT_EQ(optind, 2);
    EXPECT_EQ(std::string(argv[2]), std::string("--"));
    EXPECT_EQ(std::string(argv[3]), std::string("-t"));
    EXPECT_EQ(std::string(argv[4]), std::string("opt"));
}

TEST(GetOptLong, NotAnOption) {
    optind = 0;
    opterr = 0;

    char* const argv[] = {const_cast<char*>("test"), const_cast<char*>("-x")};

    const int argc = 2;
    const char* optstr = "nt:";
    struct option longopts[1];
    memset(longopts, 0, sizeof(struct option));

    char c;

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, '?');
}

TEST(GetOptLong, NotAnOption2) {
    optind = 0;
    opterr = 0;

    char* const argv[] = {const_cast<char*>("test"), const_cast<char*>("-x")};

    const int argc = 2;
    const char* optstr = "nt:";
    struct option longopts[1];
    memset(longopts, 0, sizeof(struct option));

    char c;

    c = getopt_long(argc, argv, optstr, longopts, nullptr);
    EXPECT_EQ(c, '?');
}

TEST(GetOptLong, LongOptions) {
    optind = 0;
    opterr = 0;

    char* const argv[] = {const_cast<char*>("souffle"), const_cast<char*>("--fact-dir"),
            const_cast<char*>("-"), const_cast<char*>("--version"), const_cast<char*>("input.dl"),
            const_cast<char*>("--generate"), const_cast<char*>("--output-dir=out"),
            const_cast<char*>("--jobs=4")};

    int jobs_flag = -1;
    int version_flag = -1;
    int show_flag = -1;

    const int argc = 8;
    const char* optstr = "F:D:j:vg::";
    struct option longopts[] = {{"fact-dir", required_argument, nullptr, 'F'},
            {"output-dir", required_argument, nullptr, 'D'}, {"jobs", required_argument, &jobs_flag, 'j'},
            {"version", no_argument, &version_flag, 'v'}, {"generate", optional_argument, nullptr, 'g'},
            {"show", required_argument, &show_flag, 0x4}, {nullptr, 0, nullptr, 0}};

    char c;
    int longindex;

    c = getopt_long(argc, argv, optstr, longopts, &longindex);
    EXPECT_EQ(c, 'F');
    EXPECT_EQ(longindex, 0);
    EXPECT_EQ(std::string(optarg), std::string("-"));

    c = getopt_long(argc, argv, optstr, longopts, &longindex);
    EXPECT_EQ(c, 0);
    EXPECT_EQ(longindex, 3);
    EXPECT_EQ(version_flag, 'v');

    c = getopt_long(argc, argv, optstr, longopts, &longindex);
    EXPECT_EQ(c, 'g');
    EXPECT_EQ(longindex, 4);
    EXPECT_EQ(optind, 6);
    EXPECT_EQ(optarg, nullptr);

    c = getopt_long(argc, argv, optstr, longopts, &longindex);
    EXPECT_EQ(c, 'D');
    EXPECT_EQ(longindex, 1);
    EXPECT_EQ(std::string(optarg), std::string("out"));

    c = getopt_long(argc, argv, optstr, longopts, &longindex);
    EXPECT_EQ(c, 0);
    EXPECT_EQ(longindex, 2);
    EXPECT_EQ(jobs_flag, 'j');
    EXPECT_EQ(std::string(optarg), std::string("4"));

    c = getopt_long(argc, argv, optstr, longopts, &longindex);
    EXPECT_EQ(c, -1);
    EXPECT_EQ(optind, 7);
    EXPECT_EQ(std::string(argv[7]), std::string("input.dl"));

    EXPECT_EQ(show_flag, -1);
}

}  // namespace souffle::test
