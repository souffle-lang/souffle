/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2016, souffle-lang. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamData.h
 *
 * Define the class RamData to model data coming from external
 * applications.
 *
 ***********************************************************************/
#pragma once

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <assert.h>

namespace souffle {

class PrimData {
public:
    PrimData() {}

    PrimData(std::vector<std::vector<std::string>> d) : data(d) {}

    std::vector<std::vector<std::string>> data;
};

class RamData {
private:
    std::map<std::string, PrimData*> data_map;

public:
    RamData() {}

    RamData* merge(RamData* d) {
        RamData* nd = new RamData();
        nd->data_map.insert(data_map.begin(), data_map.end());
        nd->data_map.insert(d->data_map.begin(), d->data_map.end());
        return nd;
    }

    void addTuples(std::string name, PrimData* d) {
        assert(d != nullptr);
        data_map[name] = d;
    }

    void addTuple(std::string name, std::vector<std::string> tuple) {
        if (data_map.find(name) == data_map.end()) {
            data_map[name] = new PrimData();
            data_map[name]->data.push_back(tuple);
        } else {
            data_map[name]->data.push_back(tuple);
        }
    }

    std::map<std::string, PrimData*> getDataMap() {
        return data_map;
    }

    size_t size() {
        return data_map.size();
    }

    PrimData* getTuples(std::string name) {
        if (data_map.find(name) == data_map.end()) {
            return nullptr;
        }

        if (data_map[name]->data.empty()) {
            return nullptr;
        }

        return data_map[name];
    }

    std::stringstream* getTuplesStr(std::string name) {
        std::stringstream* ss = new std::stringstream();
        std::vector<std::vector<std::string>> reldata = data_map[name]->data;

        for (std::vector<std::string> vec : data_map[name]->data) {
            for (std::string t : vec) {
                std::cout << t << "\t";
            }
            std::cout << "\n";
        }

        return ss;
    }
};
}  // namespace souffle
