/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file NodeMapper.cpp
 *
 * Definition of common RAM node-mapper helpers.
 *
 ***********************************************************************/

#include "ram/utility/NodeMapper.h"
#include "ram/Program.h"
#include "ram/Query.h"
#include "ram/Statement.h"

namespace souffle::ram {

namespace detail {

std::vector<Query*> queries(Program& program) {
    std::vector<Query*> queries;
    visitFrontier(program, [&](Node& n) {
        if (auto q = as<Query>(n)) {
            queries.push_back(q);
            return true;  // `Query`s aren't `Statement`s and only found as immediate children of `Statement`s
        }

        // `Statement`s can only be immediate children of `Statement`s or `Program`.
        return !as<Statement>(n) && !as<Program>(n);
    });

    return queries;
}

}  // namespace detail

}  // namespace souffle::ram
