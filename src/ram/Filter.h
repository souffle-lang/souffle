/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Filter.h
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
 * @class Filter
 * @brief Checks whether a given condition holds
 *
 * The Filter is essentially an "if" statement.
 *
 * The following example checks that both C1 and C2 hold
 * before proceeding deeper in the loop nest:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * IF C1 AND C2
 *  ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class Filter : public AbstractConditional {
public:
    Filter(Own<Condition> cond, Own<Operation> nested, std::string profileText = "")
            : AbstractConditional(std::move(cond), std::move(nested), std::move(profileText)) {}

    Filter* cloning() const override {
        return new Filter(clone(condition), clone(getOperation()), getProfileText());
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "IF " << getCondition() << std::endl;
        NestedOperation::print(os, tabpos + 1);
    }

    void print_sexpr(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "(IF " << getCondition() << std::endl;
        NestedOperation::print_sexpr(os, tabpos + 1);
        os << times(" ", tabpos) << ")";
    }
};

}  // namespace souffle::ram
