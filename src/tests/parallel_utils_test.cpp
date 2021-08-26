/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file parallel_utils_test.cpp
 *
 * A test case testing the B-trees utilization as sets.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "souffle/utility/ParallelUtil.h"
#include <string>

namespace souffle {

namespace test {

TEST(ParallelUtils, SpinLock) {
    const int N = 1000000;

    SpinLock lock;

    volatile int c = 0;

#ifdef _OPENMP
#pragma omp parallel for num_threads(4)
#endif
    for (int i = 0; i < N; i++) {
        lock.lock();
        c++;
        lock.unlock();
    }

    EXPECT_EQ(N, c);
}

TEST(ParallelUtils, ReadWriteLock) {
    const int N = 1000000;
    const int K = 10;

    ReadWriteLock lock;

    volatile int c = 0;

#ifdef _OPENMP
#pragma omp parallel for num_threads(4)
#endif
    for (int i = 0; i < N; i++) {
        if (i % K == 0) {  // 10% write probability
            lock.start_write();
            c++;
            lock.end_write();
        } else {
            lock.start_read();
            // nothing to do here ..
            lock.end_read();
        }
    }

    EXPECT_EQ(N / K, c);
}

TEST(ParallelUtils, OptimisticReadWriteLock) {
    const int N = 1000000;
    const int K = 10;

    OptimisticReadWriteLock lock;

    volatile int c = 0;

#ifdef _OPENMP
#pragma omp parallel for num_threads(4)
#endif
    for (int i = 0; i < N; i++) {
        if (i % K == 0) {  // 10% write probability
            lock.start_write();
            c++;
            c++;
            lock.end_write();
        } else {
            bool succ = false;
            do {
                auto lease = lock.start_read();
                // nothing to do here ..
                int x = c;
                succ = lock.end_read(lease);
                EXPECT_TRUE((x % 2 == 0) || !succ);
            } while (!succ);
        }
    }

    EXPECT_EQ(2 * (N / K), c);
}
}  // namespace test
}  // end namespace souffle
