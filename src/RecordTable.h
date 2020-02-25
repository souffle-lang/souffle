/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RecordTable.h
 *
 * Data container to store Records of the Datalog program.
 *
 ***********************************************************************/

#pragma once

#include "CompiledTuple.h"
#include "ParallelUtils.h"
#include "RamTypes.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <map>
#include <unordered_map>
#include <vector>

namespace souffle {

/**
 * A bidirectional mapping between tuples and reference indices.
 */
class RecordMap {
    /** The arity of the stored tuples */
    const size_t arity;

    /** The mapping from tuples to references/indices */
    std::map<std::vector<RamDomain>, RamDomain> recordToIndex;

    /** The mapping from indices to tuples */
    std::vector<std::vector<RamDomain>> indexToRecord;

public:
    explicit RecordMap(size_t arity) : arity(arity), indexToRecord(1) {}  // note: index 0 element left free

    /**
     * Pack the given vector -- create a new reference in necessary.
     */
    RamDomain pack(const std::vector<RamDomain>& vector) {
        RamDomain index;
#pragma omp critical(record_pack)
        {
            auto pos = recordToIndex.find(vector);
            if (pos != recordToIndex.end()) {
                index = pos->second;
            } else {
#pragma omp critical(record_unpack)
                {
                    indexToRecord.push_back(vector);
                    index = indexToRecord.size() - 1;
                    recordToIndex[vector] = index;

                    // assert that new index is smaller than the range
                    assert(index != std::numeric_limits<RamDomain>::max());
                }
            }
        }
        return index;
    }

    /**
     * Packs the given tuple -- and may create a new reference if necessary.
     */
    RamDomain pack(const RamDomain* tuple) {
        std::vector<RamDomain> tmp(arity);
        for (size_t i = 0; i < arity; i++) {
            tmp[i] = tuple[i];
        }

        return pack(tmp);
    }

    /**
     * Obtains a pointer to the tuple addressed by the given index.
     */
    RamDomain* unpack(RamDomain index) {
        RamDomain* res;

#pragma omp critical(record_unpack)
        res = indexToRecord[index].data();

        return res;
    }

    const RamDomain* unpack(RamDomain index) const {
        const RamDomain* res;

#pragma omp critical(record_unpack)
        res = indexToRecord[index].data();

        return res;
    }
};

class RecordTable {
public:
    RecordTable() = default;
    virtual ~RecordTable() = default;

    /**
     * A function packing a tuple of the given arity into a reference.
     */
    RamDomain pack(RamDomain* tuple, size_t arity) {
        return getForArity(arity).pack(tuple);
    }

    /**
     * A function packing a vector into a reference.
     */
    RamDomain pack(const std::vector<RamDomain>& vector) {
        return getForArity(vector.size()).pack(vector);
    }

    /**
     * A function packing a tuple of the given arity into a reference.
     */
    template <typename Domain, std::size_t Arity>
    RamDomain pack(ram::Tuple<Domain, Arity> tuple) {
        return getForArity(Arity).pack(static_cast<RamDomain*>(tuple.data));
    }

    /**
     * A function obtaining a pointer to the tuple addressed by the given reference.
     */
    RamDomain* unpack(RamDomain ref, size_t arity) {
        auto iter = maps.find(arity);
        assert(iter != maps.end() && "Attempting to unpack non-existing record");

        return (iter->second).unpack(ref);
    }

    /**
     * A function obtaining a pointer to the tuple addressed by the given reference.
     */
    const RamDomain* unpack(RamDomain ref, size_t arity) const {
        auto iter = maps.find(arity);
        assert(iter != maps.end() && "Attempting to unpack non-existing record");

        return (iter->second).unpack(ref);
    }

    /**
     * A function obtaining a pointer to the tuple addressed by the given reference.
     */
    template <typename Domain, std::size_t Arity>
    ram::Tuple<Domain, Arity> unpackTuple(RamDomain ref) {
        ram::Tuple<RamDomain, Arity> tuple;
        RamDomain* data = getForArity(Arity).unpack(ref);

        for (size_t i = 0; i < Arity; ++i) {
            tuple.data[i] = data[i];
        }
        return tuple;
    }

    /**
     * Determines whether the given reference is the nil reference encoding
     * the absence of any nested record.
     */
    bool isNil(RamDomain ref) const {
        return ref == getNil();
    }

    static constexpr RamDomain getNil() {
        return 0;
    }

private:
    std::unordered_map<size_t, RecordMap> maps;

    RecordMap& getForArity(size_t arity) {
        std::unordered_map<size_t, RecordMap>::iterator mapsIterator;
#pragma omp critical(RecordTableGetForArity)
        {
            // This will create a new map if it doesn't exist yet.
            mapsIterator = maps.emplace(arity, arity).first;
        }
        return mapsIterator->second;
    }
};

}  // namespace souffle
