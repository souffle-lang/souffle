/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamRecords.cpp
 *
 * Utilities for handling records in the interpreter
 *
 ***********************************************************************/

#include "RamRecords.h"
#include "Util.h"

#include <iostream>
#include <limits>
#include <map>
#include <vector>

namespace souffle {

namespace {

using namespace std;

/**
 * A bidirectional mapping between tuples and reference indices.
 */
class RecordMap {
    /** The arity of the stored tuples */
    int arity;

    /** The mapping from tuples to references/indices */
    map<vector<RamDomain>, RamDomain> r2i;

    /** The mapping from indices to tuples */
    vector<vector<RamDomain>> i2r;

public:
    RecordMap(int arity) : arity(arity), i2r(1) {}  // note: index 0 element left free

    /**
     * Packs the given tuple -- and may create a new reference if necessary.
     */
    RamDomain pack(const RamDomain* tuple) {
        vector<RamDomain> tmp(arity);
        for (int i = 0; i < arity; i++) {
            tmp[i] = tuple[i];
        }

        RamDomain index;
#pragma omp critical(record_pack)
        {
            auto pos = r2i.find(tmp);
            if (pos != r2i.end()) {
                index = pos->second;
            } else {
#pragma omp critical(record_unpack)
                {
                    i2r.push_back(tmp);
                    index = i2r.size() - 1;
                    r2i[tmp] = index;

                    // assert that new index is smaller than the range
                    assert(index != std::numeric_limits<RamDomain>::max());
                }
            }
        }

        return index;
    }

    /**
     * Obtains a pointer to the tuple addressed by the given index.
     */
    RamDomain* unpack(RamDomain index) {
        RamDomain* res;

#pragma omp critical(record_unpack)
        res = &(i2r[index][0]);

        return res;
    }
};

/**
 * The static access function for record maps of certain arities.
 */
RecordMap& getForArity(int arity) {
    // the static container -- filled on demand
    static map<int, RecordMap> maps;

    // get container if present
    auto pos = maps.find(arity);
    if (pos != maps.end()) {
        return pos->second;
    }

    // create new container if required
    maps.emplace(arity, arity);
    return getForArity(arity);
}
}  // namespace

RamDomain pack(RamDomain* tuple, int arity) {
    // conduct the packing
    return getForArity(arity).pack(tuple);
}

RamDomain* unpack(RamDomain ref, int arity) {
    // conduct the unpacking
    return getForArity(arity).unpack(ref);
}

RamDomain getNull() {
    return 0;
}

bool isNull(RamDomain ref) {
    return ref == 0;
}

}  // end of namespace souffle
