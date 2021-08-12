/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file NodeMapper.h
 *
 * Declaration of RAM node and mappers for RAM nodes
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include "ram/Query.h"
#include "souffle/utility/NodeMapper.h"

namespace souffle::ram {

class Program;

namespace detail {
std::vector<Query*> queries(Program&);
}

template <typename F>
void forEachQuery(Program& program, F&& f) {
    for (auto&& query : detail::queries(program))
        f(*query);
}

template <typename F>
void forEachQueryMap(Program& program, F&& f) {
    forEachQuery(program, [&](Query& query) { query.apply(nodeMapper<Node>(f)); });
}

}  // namespace souffle::ram
