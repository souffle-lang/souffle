/*
    Copyright (c) 2017 The Souffle Developers and/or its affiliates. All Rights reserved

    The Universal Permissive License (UPL), Version 1.0

    Subject to the condition set forth below, permission is hereby granted to any person obtaining a copy of
   this software,
    associated documentation and/or data (collectively the "Software"), free of charge and under any and all
   copyright rights in the
    Software, and any and all patent rights owned or freely licensable by each licensor hereunder covering
   either (i) the unmodified
    Software as contributed to or provided by such licensor, or (ii) the Larger Works (as defined below), to
   deal in both

    (a) the Software, and
    (b) any piece of software and/or hardware listed in the lrgrwrks.txt file if one is included with the
   Software (each a “Larger
    Work” to which the Software is contributed by such licensors),

    without restriction, including without limitation the rights to copy, create derivative works of, display,
   perform, and
    distribute the Software and make, use, sell, offer for sale, import, export, have made, and have sold the
   Software and the
    Larger Work(s), and to sublicense the foregoing rights on either these or other terms.

    This license is subject to the following condition:
    The above copyright notice and either this complete permission notice or at a minimum a reference to the
   UPL must be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
   LIMITED TO THE WARRANTIES
    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
   COPYRIGHT HOLDERS BE
    LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR
    IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <exception>
#include <iostream>
#include <vector>

namespace souffle {

/**
 * A concurrent list data structure, which is implemented through a chunked linked-list.
 * On their own, get/push_back are threadsafe, however clearing/destructing is undefined
 * behaviour if get/push_backs are in progress.
 */
template <class T>
class concurrent_list {
    // how large each cv_data->m_data array is
    const size_t chunk_size = 1000;

    struct cv_data {
        // how many elements are currently in m_data
        std::atomic<size_t> m_size;
        T* m_data;
        // ptr to the next cv_data
        std::atomic<cv_data*> next;
    };

    // number of elements in total
    std::atomic<size_t> container_size;

    // only necessary for those that change vectors
    mutable std::mutex writeMutex;

    std::atomic<cv_data*> cData;

public:
    concurrent_list() {
        cv_data* curr = new cv_data;
        curr->m_size = 0;
        curr->m_data = new T[chunk_size];
        curr->next = nullptr;
        cData = curr;
        container_size = 0;
    }

    concurrent_list(concurrent_list&& other) : concurrent_list() {
        std::unique_lock<std::mutex> lk1(other.writeMutex);
        std::unique_lock<std::mutex> lk2(writeMutex);

        // no std::swap as atomics are unmoveable
        size_t tmp = this->container_size.load();
        this->container_size.store(other.container_size);
        other.container_size.store(tmp);

        // no std::swap as atomics are unmoveable
        cv_data* tmpPtr = this->cData.load();
        this->cData.store(other.cData);
        other.cData.store(tmpPtr);
    }

    concurrent_list& operator=(concurrent_list other) {
        size_t tmp = this->container_size.load();
        this->container_size.store(other.container_size);
        other.container_size.store(tmp);

        cv_data* tmpPtr = this->cData.load();
        this->cData.store(other.cData);
        other.cData.store(tmpPtr);

        return *this;
    }

    /* it goes without saying this is not threadsafe */
    ~concurrent_list() {
        // lock just to let writes finish
        std::unique_lock<std::mutex> lk(writeMutex);

        auto curr = cData.load();
        while (curr != nullptr) {
            cv_data* tmpnext = curr->next;
            delete[] curr->m_data;
            delete curr;

            curr = tmpnext;
        }
    }

    /**
     * the concept of size() in a threaded environment is peculiar
     * However, it is guaranteed that the size returned is a valid index+1
     * if the data structure has not been concurrently reduced.
     */
    size_t size() const {
        return container_size;
    }

    /*
     * Append the value onto the end of the list, allocating more space if required.
     * @param val the value to insert
     * thread safe for many writers
     */
    void push_back(T val) {
        std::unique_lock<std::mutex> lk(writeMutex);

        cv_data* curr = cData.load();
        // fast forward curr until non-null
        while (curr->next.load() != nullptr) curr = curr->next.load();

        cv_data* shadow = nullptr;
        cv_data* front = curr;

        // do we need to expand?
        if (curr->m_size == chunk_size) {
            shadow = new cv_data;
            shadow->m_size = 0;
            shadow->m_data = new T[chunk_size];
            shadow->next = nullptr;

            front = shadow;
        }

        front->m_data[front->m_size] = val;
        front->m_size += 1;
        if (shadow != nullptr) curr->next = shadow;
        container_size += 1;
    }

    /*
     * Retrieve reference of the locally stored element
     * @param index the index of the element to be returned
     * Is thread safe for many readers
     */
    T& at(size_t index) const {
        return this->operator[](index);
    }

    /* thread safe, if you treat T& as */
    T& operator[](size_t index) const {
        if (index >= container_size) throw "invalid index";

        auto curr = cData.load();
        while (index >= chunk_size) {
            curr = curr->next.load();
            index -= chunk_size;
        }

        return curr->m_data[index];
    }

    /*
     * Delete all elements from the data structure
     * Warning! This is not thread-safe in the event of reads occuring during call of this operation
     */
    void clear() {
        std::unique_lock<std::mutex> lk(writeMutex);

        cv_data* curr = cData;
        while (curr != nullptr) {
            cv_data* tmpnext = curr->next;
            delete[] curr->m_data;
            delete curr;

            curr = tmpnext;
        }

        curr = new cv_data;
        curr->m_size = 0;
        curr->m_data = new T[chunk_size];
        curr->next = nullptr;

        container_size = 0;

        cData = curr;
    }
};
/* end concurrent_list MIT copyright */

// number of elements in each array of the vector
static constexpr uint8_t BLOCKBITS = 10u;
static constexpr size_t BLOCKSIZE = (1u << BLOCKBITS);
// block_t stores parent in the upper half, rank in the lower half
typedef uint64_t block_t;

/**
 * A class that is designed to mimic std::list, but with better destructor speed
 * It achieves this by allocating comparatively large chunks of memory as it needs
 * This is not thread safe, except, when there is guarantee of at most one writer AND
 *      the TBB data structure is in use.
 *
 */
template <class T>
class BlockList {
    souffle::concurrent_list<T*> listData;

    mutable size_t m_size = 0;

public:
    BlockList() : listData() {}

    /** copy constructor */
    BlockList(const BlockList& other) {
        size_t othblocks = other.listData.size();
        for (size_t i = 0; i < othblocks; ++i) {
            listData.push_back(new T[BLOCKSIZE]);
            memcpy(listData.at(i), other.listData.at(i), BLOCKSIZE);
        }
        this->m_size = other.m_size;
    }

    /** move constructor */
    BlockList(BlockList&& other) : BlockList() {
        std::swap(listData, other.listData);
        std::swap(m_size, other.m_size);
    }

    BlockList& operator=(BlockList other) {
        std::swap(this->listData, other.listData);
        std::swap(this->m_size, other.m_size);
        return *this;
    }

    ~BlockList() {
        for (size_t i = 0; i < listData.size(); ++i) {
            T* r = listData.at(i);
            delete[] r;
        }
    }

    /**
     * Size of List
     * @return the number of elements currently stored
     */
    inline size_t size() const {
        return m_size;
    };

    /**
     * Add a value to the blocklist
     * @param val : value to be added
     */
    void add(const T& val) {
        // store it in the vector
        size_t blocknum = m_size >> BLOCKBITS;
        size_t blockindex = m_size & (BLOCKSIZE - 1);

        // we need to add a new block
        if (blockindex == 0) {
            this->listData.push_back(new T[BLOCKSIZE]);
        }

        this->listData[blocknum][blockindex] = T(val);

        ++m_size;
    }

    /**
     * Retrieve a reference to the stored value at index
     * @param index position to search
     * @return the value at index
     */
    T& get(const size_t index) const {
        if (index < 0 || index >= this->size()) throw std::runtime_error("out of bounds");

        size_t blocknum = index >> BLOCKBITS;
        size_t blockindex = index & (BLOCKSIZE - 1);

        return listData[blocknum][blockindex];
    }

    /**
     * Clear all elements from the BlockList
     */
    void clear() {
        for (size_t i = 0; i < listData.size(); ++i) {
            delete[] listData.at(i);
        }
        listData.clear();
        m_size = 0;
    }

    /**
     * Remove the last element from the data
     * and destroying the block if its the last one in it
     * @return the copied value
     */
    T pop() {
        if (m_size == 0) throw std::runtime_error("pop called on empty blocklist");

        --m_size;
        size_t blocknum = m_size >> BLOCKBITS;
        size_t blockindex = m_size & (BLOCKSIZE - 1);

        // we need to destroy the block - we're the last one standing
        if (blockindex == 0) {
            T retVal = listData[blocknum][blockindex];

            delete[] listData[blocknum];

            listData.pop_back();

            return retVal;

        } else {
            return listData[blocknum][blockindex];
        }
    }

    /**
     * Move the other list's elements here
     * REMOVING them from the other
     * Similar to std::list.splice()
     * @param other the other list
     */
    void niptuck(BlockList& other) {
        if (other.size() == 0) return;

        // fill up the remainder of this BLs last block with elements from the end of other's
        size_t requiredEmpties = BLOCKSIZE - (m_size & (BLOCKSIZE - 1));
        // we ignore the case of having to remove the entire block - its more efficient to do later
        if (requiredEmpties != BLOCKSIZE) {
            // pop and add
            for (size_t rem = requiredEmpties; rem > 0 && other.size() > 0; --rem) {
                this->add(other.pop());
            }
        }

        // negative values mess up the shift
        if (other.size() == 0) {
            other.listData.clear();
            other.m_size = 0;
            return;
        }

        // if we have remainder, we've got to make sure that we simply plonk in the pointers
        // size-1, because BLOCKSIZE >> BLOCKBITS should equal 1
        size_t otherblocks = (other.m_size - 1) >> BLOCKBITS;
        ++otherblocks;
        for (size_t i = 0; i < otherblocks; ++i) {
            listData.push_back(other.listData[i]);
        }

        this->m_size += other.m_size;
        other.listData.clear();
        other.m_size = 0;
    }

    class iterator : std::iterator<std::forward_iterator_tag, T> {
        size_t cIndex = 0;
        BlockList* bl;

    public:
        // default ctor, to silence
        iterator(){};

        /* begin iterator for iterating over all elements */
        iterator(BlockList* bl) : bl(bl){};
        /* ender iterator for marking the end of the iteration */
        iterator(BlockList* bl, size_t beginInd) : cIndex(beginInd), bl(bl){};

        T operator*() {
            return bl->get(cIndex);
        };
        const T operator*() const {
            return bl->get(cIndex);
        };

        iterator& operator++(int) {
            ++cIndex;
            return *this;
        };

        iterator operator++() {
            iterator ret(*this);
            ++cIndex;
            return ret;
        };

        friend bool operator==(const iterator& x, const iterator& y) {
            return x.cIndex == y.cIndex && x.bl == y.bl;
        };

        friend bool operator!=(const iterator& x, const iterator& y) {
            return !(x == y);
        };
    };

    iterator begin() {
        return iterator(this);
    };
    iterator end() {
        return iterator(this, size());
    };
};

/** this is necessary for atomics, as we cannot use the copy ctor */
template <>
inline void BlockList<std::atomic<block_t>>::add(const std::atomic<block_t>& val) {
    // store it in the vector
    size_t blocknum = m_size >> BLOCKBITS;
    size_t blockindex = m_size & (BLOCKSIZE - 1);

    // we need to add a new block
    if (blockindex == 0) {
        this->listData.push_back(new std::atomic<block_t>[BLOCKSIZE]);
    }
    // we need to assign inplace for atomics
    this->listData[blocknum][blockindex] = val.load();

    ++m_size;
}
}
