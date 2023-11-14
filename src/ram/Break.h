/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Break.h
 *
 ***********************************************************************/

#pragma once

#include "ram/AbstractConditional.h"
#include "ram/Condition.h"
#include "ram/NestedOperation.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <iosfwd>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

namespace souffle::ram {

/**
 * @class Break
 * @brief Breaks out of the loop if a condition holds
 *
 * The following example will break out of the inner-most
 * loop if the condition (t1.1 = 4) holds:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * FOR t0 in A
 *   FOR t1 in B
 *     IF t0.1 = 4 BREAK
 *     ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class Break : public AbstractConditional {
public:
    Break(Own<Condition> cond, Own<Operation> nested, std::string profileText = "")
            : AbstractConditional(std::move(cond), std::move(nested), std::move(profileText)) {}

    Break* cloning() const override {
        return new Break(clone(condition), clone(getOperation()), getProfileText());
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "IF " << getCondition() << " BREAK" << std::endl;
        NestedOperation::print(os, tabpos + 1);
    }

    void print_sexpr(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "(IF_BREAK " << getCondition() << " " << std::endl;
        NestedOperation::print_sexpr(os, tabpos + 1);
        os << ")" << std::endl;
    }
};

}  // namespace souffle::ram
