/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file InterpreterRecords.cpp
 *
 * Utilities for handling records in the interpreter
 *
 ***********************************************************************/

#include "InterpreterRecords.h"
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
    /** Static map holding RecordMaps for different arities */
    static map<int, RecordMap> maps;

    /** The arity of the stored tuples */
    int arity;

    /** The mapping from tuples to references/indices */
    map<vector<RamDomain>, RamDomain> r2i;

    /** The mapping from indices to tuples */
    vector<vector<RamDomain>> i2r;

public:
    RecordMap(int arity) : arity(arity), i2r(1) {}  // note: index 0 element left free

    static RecordMap& getForArity(int arity) {
	// get container if present
	auto pos = RecordMap::maps.find(arity);
	if (pos != RecordMap::maps.end()) {
	    return pos->second;
	}

	// create new container if required
	RecordMap::maps.emplace(arity, arity);
	return getForArity(arity);
    }

    static void printRecords(const std::unique_ptr<RecordWriteStream>& writer) {
	std::string delimiter = writer->getDelimiter();
	for (auto it = maps.begin(); it != maps.end(); ++it) {
	    const std::vector<std::vector<RamDomain>> records = it->second.i2r;
	    for (size_t idx = 0; idx < records.size(); ++idx) {
		std::vector<RamDomain> record = records[idx];
		std::string str = std::to_string(idx) + delimiter + std::to_string(record.size()) + delimiter;
		for (size_t i = 0; i < record.size(); ++i) {
		    str += std::to_string(record[i]);
		    if (i < record.size() - 1) str += delimiter;;
		}

		writer->writeNextLine(str);
	    }
	}
	writer->writeSymbolTable();
    }

    static void printRecords() {
	for (auto it = maps.begin(); it != maps.end(); ++it) {
	    std::cout << "Arity " << it->first << "\n";
	    RecordMap rm = it->second;
	    for (vector<RamDomain> record: rm.i2r) {
		for (RamDomain ele: record) {
		    std::cout << ele << " ";
		}
		std::cout << "\n";
	    }
		std::cout << "\n";
	}
    }

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

map<int, RecordMap> souffle::RecordMap::maps = map<int, RecordMap>();
}  // namespace

RamDomain pack(RamDomain* tuple, int arity) {
    // conduct the packing
    return souffle::RecordMap::getForArity(arity).pack(tuple);
}

RamDomain* unpack(RamDomain ref, int arity) {
    // conduct the unpacking
    return souffle::RecordMap::getForArity(arity).unpack(ref);
}

RamDomain getNull() {
    return 0;
}

bool isNull(RamDomain ref) {
    return ref == 0;
}

void printRecords(const std::unique_ptr<RecordWriteStream>& writer) {
    souffle::RecordMap::printRecords(writer);
}

void printRecords() {
    souffle::RecordMap::printRecords();
}
}  // end of namespace souffle
