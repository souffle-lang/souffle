/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Aggregate.h
 *
 * Defines methods for determining the scope of variables
 * occurring in a clause (whether they are injected, local, or witnesses)
 *
 ***********************************************************************/

#pragma once

#include "ast/Aggregator.h"
#include "ast/Argument.h"
#include "ast/Clause.h"
#include "ast/Program.h"
#include "ast/TranslationUnit.h"
#include "ast/Variable.h"
#include <set>
#include <string>

namespace souffle::ast::analysis {
/**
 *  Computes the set of local variables
 *  in an aggregate expression.
 **/
std::set<std::string> getLocalVariables(
        const TranslationUnit& tu, const Clause& clause, const Aggregator& aggregate);

/**
 * Computes the set of witness variables that are used in the aggregate
 **/
std::set<std::string> getWitnessVariables(
        const TranslationUnit& tu, const Clause& clause, const Aggregator& aggregate);
/**
 * Find the set of variable names occurring outside of the given aggregate.
 * This is equivalent to taking the set minus of the set of all variable names
 * occurring in the clause minus the set of all variable names occurring in the aggregate.
 **/
std::set<std::string> getVariablesOutsideAggregate(const Clause& clause, const Aggregator& aggregate);

/**
 *  Find a new relation name. I use this when I create new relations either for
 *  aggregate bodies or singleton aggregates.
 **/
std::string findUniqueRelationName(const Program& program, const std::string& base);
/**
 * Find a variable name using base to form a string like base1
 * Use this when you need to limit the scope of a variable to the inside of an aggregate.
 **/
std::string findUniqueVariableName(const Clause& clause, const std::string& base);
/**
 *  Given an aggregate and a clause, we find all the variables that have been
 *  injected into the aggregate.
 *  This means that the variable occurs grounded in an outer scope.
 **/
std::set<std::string> getInjectedVariables(
        const TranslationUnit& tu, const Clause& clause, const Aggregator& aggregate);

}  // namespace souffle::ast::analysis
