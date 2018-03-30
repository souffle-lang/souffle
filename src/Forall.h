/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Forall.h
 *
 * @author Chris Fallin <cfallin@c1f.net>
 *
 * Support for magical auto-updating "forall" relations: underlying
 * forall index.
 *
 ***********************************************************************/


#pragma once

#include "RamTypes.h"

#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace souffle {

class ForallIndex {
    std::unordered_map<RamDomain, std::unordered_map<RamDomain, RamDomain>> values_by_key;
    mutable std::mutex lock;

public:
    ForallIndex() {}

    ForallIndex(const ForallIndex& other) : lock() {
	std::lock_guard<std::mutex> l(other.lock);
	values_by_key = other.values_by_key;
    }

    ForallIndex(ForallIndex&& other) : lock() {
	std::lock_guard<std::mutex> l(other.lock);
	values_by_key = std::move(other.values_by_key);
    }

    RamDomain insert(RamDomain key, RamDomain value) {
	std::lock_guard<std::mutex> l(lock);
	auto& values = values_by_key[key];
	auto it = values.find(value);
	if (it == values.end()) {
	    RamDomain id = static_cast<RamDomain>(values.size());
	    values[value] = id;
	    return id;
	} else {
	    return it->second;
	}
    }
};

}
