/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ParallelUtil.h
 *
 * A set of utilities abstracting from the underlying parallel library.
 * Currently supported APIs: OpenMP and Cilk
 *
 ***********************************************************************/

#pragma once

#include <atomic>

#ifdef _OPENMP

/**
 * Implementation of parallel control flow constructs utilizing OpenMP
 */

#include <omp.h>

#ifdef __APPLE__
#define pthread_yield pthread_yield_np
#endif

// support for a parallel region
#define PARALLEL_START _Pragma("omp parallel") {
#define PARALLEL_END }

// support for parallel loops
#define pfor _Pragma("omp for schedule(dynamic)") for

// spawn and sync are processed sequentially (overhead to expensive)
#define task_spawn
#define task_sync

// section start / end => corresponding OpenMP pragmas
// NOTE: disabled since it causes performance losses
//#define SECTIONS_START _Pragma("omp parallel sections") {
// NOTE: we stick to flat-level parallelism since it is faster due to thread pooling
#define SECTIONS_START {
#define SECTIONS_END }

// the markers for a single section
//#define SECTION_START _Pragma("omp section") {
#define SECTION_START {
#define SECTION_END }

// a macro to create an operation context
#define CREATE_OP_CONTEXT(NAME, INIT) auto NAME = INIT;
#define READ_OP_CONTEXT(NAME) NAME

#else

// support for a parallel region => sequential execution
#define PARALLEL_START {
#define PARALLEL_END }

// support for parallel loops => simple sequential loop
#define pfor for

// spawn and sync not supported
#define task_spawn
#define task_sync

// sections are processed sequentially
#define SECTIONS_START {
#define SECTIONS_END }

// sections are inlined
#define SECTION_START {
#define SECTION_END }

// a macro to create an operation context
#define CREATE_OP_CONTEXT(NAME, INIT) auto NAME = INIT;
#define READ_OP_CONTEXT(NAME) NAME

// mark es sequential
#define IS_SEQUENTIAL

#endif

#ifndef IS_SEQUENTIAL
#define IS_PARALLEL
#endif

#ifdef IS_PARALLEL
#define MAX_THREADS (omp_get_max_threads())
#else
#define MAX_THREADS (1)
#endif

#ifdef IS_PARALLEL

#include <mutex>

namespace souffle {

/**
 * A small utility class for implementing simple locks.
 */
class Lock {
    // the underlying mutex
    std::mutex mux;

public:
    struct Lease {
        Lease(std::mutex& mux) : mux(&mux) {
            mux.lock();
        }
        Lease(Lease&& other) : mux(other.mux) {
            other.mux = nullptr;
        }
        Lease(const Lease& other) = delete;
        ~Lease() {
            if (mux != nullptr) {
                mux->unlock();
            }
        }

    protected:
        std::mutex* mux;
    };

    // acquired the lock for the live-cycle of the returned guard
    Lease acquire() {
        return Lease(mux);
    }

    void lock() {
        mux.lock();
    }

    bool try_lock() {
        return mux.try_lock();
    }

    void unlock() {
        mux.unlock();
    }
};

//    /* valuable source: http://locklessinc.com/articles/locks/ */

namespace detail {

/* Pause instruction to prevent excess processor bus usage */
#ifdef __x86_64__
#define cpu_relax() asm volatile("pause\n" : : : "memory")
#else
#define cpu_relax() asm volatile("" : : : "memory")
#endif

/**
 * A utility class managing waiting operations for spin locks.
 */
class Waiter {
    int i = 0;

public:
    Waiter() = default;

    /**
     * Conducts a wait operation.
     */
    void operator()() {
        ++i;
        if ((i % 1000) == 0) {
            // there was no progress => let others work
            pthread_yield();
        } else {
            // relax this CPU
            cpu_relax();
        }
    }
};
}  // namespace detail

/* compare: http://en.cppreference.com/w/cpp/atomic/atomic_flag */
class SpinLock {
    std::atomic<int> lck{0};

public:
    SpinLock() = default;

    void lock() {
        detail::Waiter wait;
        while (!try_lock()) {
            wait();
        }
    }

    bool try_lock() {
        int should = 0;
        return lck.compare_exchange_weak(should, 1, std::memory_order_acquire);
    }

    void unlock() {
        lck.store(0, std::memory_order_release);
    }
};

/**
 * A read/write lock for increased access performance on a
 * read-heavy use case.
 */
class ReadWriteLock {
    /**
     * Based on paper:
     *         Scalable Reader-Writer Synchronization
     *         for Shared-Memory Multiprocessors
     *
     * Layout of the lock:
     *      31        ...             2                    1                    0
     *      +-------------------------+--------------------+--------------------+
     *      | interested reader count |   waiting writer   | active writer flag |
     *      +-------------------------+--------------------+--------------------+
     */

    std::atomic<int> lck{0};

public:
    ReadWriteLock() = default;

    void start_read() {
        // add reader
        auto r = lck.fetch_add(4, std::memory_order_acquire);

        // wait until there is no writer any more
        detail::Waiter wait;
        while (r & 0x3) {
            // release reader
            end_read();

            // wait a bit
            wait();

            // apply as a reader again
            r = lck.fetch_add(4, std::memory_order_acquire);

        }  // while there is a writer => spin
    }

    void end_read() {
        lck.fetch_sub(4, std::memory_order_release);
    }

    void start_write() {
        detail::Waiter wait;

        // set wait-for-write bit
        auto stat = lck.fetch_or(2, std::memory_order_acquire);
        while (stat & 0x2) {
            wait();
            stat = lck.fetch_or(2, std::memory_order_acquire);
        }

        // the caller may starve here ...
        int should = 2;
        while (!lck.compare_exchange_strong(
                should, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
            wait();
            should = 2;
        }
    }

    bool try_write() {
        int should = 0;
        return lck.compare_exchange_strong(should, 1, std::memory_order_acquire, std::memory_order_relaxed);
    }

    void end_write() {
        lck.fetch_sub(1, std::memory_order_release);
    }

    bool try_upgrade_to_write() {
        int should = 4;
        return lck.compare_exchange_strong(should, 1, std::memory_order_acquire, std::memory_order_relaxed);
    }

    void downgrade_to_read() {
        // delete write bit + set num readers to 1
        lck.fetch_add(3, std::memory_order_release);
    }
};

/**
 * An implementation of an optimistic r/w lock.
 */
class OptimisticReadWriteLock {
    /**
     * The version number utilized for the synchronization.
     *
     * Usage:
     *      - even version numbers are stable versions, not being updated
     *      - odd version numbers are temporary versions, currently being updated
     */
    std::atomic<int> version{0};

public:
    /**
     * The lease utilized to link start and end of read phases.
     */
    class Lease {
        friend class OptimisticReadWriteLock;
        int version;

    public:
        Lease(int version = 0) : version(version) {}
        Lease(const Lease& lease) = default;
        Lease& operator=(const Lease& other) = default;
        Lease& operator=(Lease&& other) = default;
    };

    /**
     * A default constructor initializing the lock.
     */
    OptimisticReadWriteLock() = default;

    /**
     * Starts a read phase, making sure that there is currently no
     * active concurrent modification going on. The resulting lease
     * enables the invoking process to later-on verify that no
     * concurrent modifications took place.
     */
    Lease start_read() {
        detail::Waiter wait;

        // get a snapshot of the lease version
        auto v = version.load(std::memory_order_acquire);

        // spin while there is a write in progress
        while ((v & 0x1) == 1) {
            // wait for a moment
            wait();
            // get an updated version
            v = version.load(std::memory_order_acquire);
        }

        // done
        return Lease(v);
    }

    /**
     * Tests whether there have been concurrent modifications since
     * the given lease has been issued.
     *
     * @return true if no updates have been conducted, false otherwise
     */
    bool validate(const Lease& lease) {
        // check whether version number has changed in the mean-while
        std::atomic_thread_fence(std::memory_order_acquire);
        return lease.version == version.load(std::memory_order_relaxed);
    }

    /**
     * Ends a read phase by validating the given lease.
     *
     * @return true if no updates have been conducted since the
     *         issuing of the lease, false otherwise
     */
    bool end_read(const Lease& lease) {
        // check lease in the end
        return validate(lease);
    }

    /**
     * Starts a write phase on this lock be ensuring exclusive access
     * and invalidating any existing read lease.
     */
    void start_write() {
        detail::Waiter wait;

        // set last bit => make it odd
        auto v = version.fetch_or(0x1, std::memory_order_acquire);

        // check for concurrent writes
        while ((v & 0x1) == 1) {
            // wait for a moment
            wait();
            // get an updated version
            v = version.fetch_or(0x1, std::memory_order_acquire);
        }

        // done
    }

    /**
     * Tries to start a write phase unless there is a currently ongoing
     * write operation. In this case no write permission will be obtained.
     *
     * @return true if write permission has been granted, false otherwise.
     */
    bool try_start_write() {
        auto v = version.fetch_or(0x1, std::memory_order_acquire);
        return !(v & 0x1);
    }

    /**
     * Updates a read-lease to a write permission by a) validating that the
     * given lease is still valid and b) making sure that there is no currently
     * ongoing write operation.
     *
     * @return true if the lease was still valid and write permissions could
     *      be granted, false otherwise.
     */
    bool try_upgrade_to_write(const Lease& lease) {
        auto v = version.fetch_or(0x1, std::memory_order_acquire);

        // check whether write privileges have been gained
        if (v & 0x1) return false;  // there is another writer already

        // check whether there was no write since the gain of the read lock
        if (lease.version == v) return true;

        // if there was, undo write update
        abort_write();

        // operation failed
        return false;
    }

    /**
     * Aborts a write operation by reverting to the version number before
     * starting the ongoing write, thereby re-validating existing leases.
     */
    void abort_write() {
        // reset version number
        version.fetch_sub(1, std::memory_order_release);
    }

    /**
     * Ends a write operation by giving up the associated exclusive access
     * to the protected data and abandoning the provided write permission.
     */
    void end_write() {
        // update version number another time
        version.fetch_add(1, std::memory_order_release);
    }

    /**
     * Tests whether currently write permissions have been granted to any
     * client by this lock.
     *
     * @return true if so, false otherwise
     */
    bool is_write_locked() const {
        return version & 0x1;
    }
};

#else

namespace souffle {

/**
 * A small utility class for implementing simple locks.
 */
struct Lock {
    class Lease {};

    // no locking if there is no parallel execution
    Lease acquire() {
        return Lease();
    }

    void lock() {}

    bool try_lock() {
        return true;
    }

    void unlock() {}
};

/**
 * A 'sequential' non-locking implementation for a spin lock.
 */
class SpinLock {
public:
    SpinLock() = default;

    void lock() {}

    bool try_lock() {
        return true;
    }

    void unlock() {}
};

class ReadWriteLock {
public:
    ReadWriteLock() = default;

    void start_read() {}

    void end_read() {}

    void start_write() {}

    bool try_write() {
        return true;
    }

    void end_write() {}

    bool try_upgrade_to_write() {
        return true;
    }

    void downgrade_to_read() {}
};

/**
 * A 'sequential' non-locking implementation for an optimistic r/w lock.
 */
class OptimisticReadWriteLock {
public:
    class Lease {};

    OptimisticReadWriteLock() = default;

    Lease start_read() {
        return Lease();
    }

    bool validate(const Lease& /*lease*/) {
        return true;
    }

    bool end_read(const Lease& /*lease*/) {
        return true;
    }

    void start_write() {}

    bool try_start_write() {
        return true;
    }

    bool try_upgrade_to_write(const Lease& /*lease*/) {
        return true;
    }

    void abort_write() {}

    void end_write() {}

    bool is_write_locked() const {
        return true;
    }
};

#endif

/**
 * Obtains a reference to the lock synchronizing output operations.
 */
inline Lock& getOutputLock() {
    static Lock outputLock;
    return outputLock;
}

}  // end of namespace souffle
