/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file NestedOperation.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include "ram/Operation.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <iosfwd>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class NestedOperation
 * @brief Abstract class for a nesting operations in a loop-nest
 *
 * In the following example, the nested operation
 * of "IF C1" is "IF C2":
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    IF C1
 *     IF C2
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * TODO (b-scholz): profile text is awkward; perhaps there is a better way of
 *                  storing profile information for RAM operations since
 *                  it is not always used for all RAM operations.
 */
class NestedOperation : public Operation {
public:
    NestedOperation(Own<Operation> nested, std::string profileText = "")
            : nestedOperation(std::move(nested)), profileText(std::move(profileText)) {
        assert(nestedOperation != nullptr);
    }

    NestedOperation* cloning() const override = 0;

    /** @brief Get nested operation */
    Operation& getOperation() const {
        return *nestedOperation;
    }

    /** @brief Get profile text */
    const std::string& getProfileText() const {
        return profileText;
    }

    void apply(const NodeMapper& map) override {
        nestedOperation = map(std::move(nestedOperation));
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        Operation::print(nestedOperation.get(), os, tabpos);
    }

    void print_sexpr(std::ostream& os, int tabpos) const override {
        Operation::print_sexpr(nestedOperation.get(), os, tabpos);
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<NestedOperation>(node);
        return equal_ptr(nestedOperation, other.nestedOperation) && profileText == other.profileText;
    }

    NodeVec getChildren() const override {
        return {nestedOperation.get()};
    }

    /** Nested operation */
    Own<Operation> nestedOperation;

    /** Text used by the profiler */
    const std::string profileText;
};

}  // namespace souffle::ram
