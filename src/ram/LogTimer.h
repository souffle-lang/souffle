/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file LogTimer.h
 *
 ***********************************************************************/

#pragma once

#include "ram/AbstractLog.h"
#include "ram/Node.h"
#include "ram/Statement.h"
#include "ram/utility/NodeMapper.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/StringUtil.h"
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class LogTimer
 * @brief Execution time logger for a statement
 *
 * Logs the execution time of a statement. Before and after
 * the execution of the logging statement the wall-clock time
 * is taken to compute the time duration for the statement.
 * Duration and logging message is printed after the execution
 * of the statement.
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * START_TIMER "@runtime\;"
 *   BEGIN_STRATUM 0
 *     ...
 *   ...
 * END_TIMER
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class LogTimer : public Statement, public AbstractLog {
public:
    LogTimer(Own<Statement> stmt, std::string msg) : AbstractLog(std::move(stmt), std::move(msg)) {}

    LogTimer* cloning() const override {
        return new LogTimer(clone(statement), message);
    }

    void apply(const NodeMapper& map) override {
        AbstractLog::apply(map);
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "TIMER \"" << stringify(message) << "\"" << std::endl;
        Statement::print(statement.get(), os, tabpos + 1);
        os << times(" ", tabpos) << "END TIMER" << std::endl;
    }

    NodeVec getChildNodesImpl() const override {
        return AbstractLog::getChildNodesImpl();
    }
};

}  // namespace souffle::ram
