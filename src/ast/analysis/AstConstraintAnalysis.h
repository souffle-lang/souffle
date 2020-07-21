/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstConstraintAnalysis.h
 *
 * Contains AST Constraint Analysis Infrastructure for doing constraint analysis on AST objects
 *
 ***********************************************************************/

#pragma once

#include "Constraints.h"
#include "ast/AstAbstract.h"
#include "ast/AstArgument.h"
#include "ast/AstClause.h"
#include "ast/AstNode.h"
#include "ast/AstVisitor.h"
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

namespace souffle {

/**
 * A variable type to be utilized by AST constraint analysis. Each such variable is
 * associated with an AstArgument which's property it is describing.
 *
 * @tparam PropertySpace the property space associated to the analysis
 */
template <typename PropertySpace>
struct AstConstraintAnalysisVar : public Variable<const AstArgument*, PropertySpace> {
    explicit AstConstraintAnalysisVar(const AstArgument* arg)
            : Variable<const AstArgument*, PropertySpace>(arg) {}
    explicit AstConstraintAnalysisVar(const AstArgument& arg)
            : Variable<const AstArgument*, PropertySpace>(&arg) {}

    /** adds print support */
    void print(std::ostream& out) const override {
        out << "var(" << *(this->id) << ")";
    }
};

/**
 * A base class for AstConstraintAnalysis collecting constraints for an analysis
 * by visiting every node of a given AST. The collected constraints are
 * then utilized to obtain the desired analysis result.
 *
 * @tparam AnalysisVar the type of variable (and included property space)
 *      to be utilized by this analysis.
 */
template <typename AnalysisVar>
class AstConstraintAnalysis : public AstVisitor<void> {
public:
    using value_type = typename AnalysisVar::property_space::value_type;
    using constraint_type = std::shared_ptr<Constraint<AnalysisVar>>;
    using solution_type = std::map<const AstArgument*, value_type>;

    virtual void collectConstraints(const AstClause& clause) {
        visitDepthFirstPreOrder(clause, *this);
    }

    /**
     * Runs this constraint analysis on the given clause.
     *
     * @param clause the close to be analysed
     * @param debug a flag enabling the printing of debug information
     * @return an assignment mapping a property to each argument in the given clause
     */
    solution_type analyse(const AstClause& clause, std::ostream* debugOutput = nullptr) {
        collectConstraints(clause);

        assignment = constraints.solve();

        // print debug information if desired
        if (debugOutput != nullptr) {
            *debugOutput << "Clause: " << clause << "\n";
            *debugOutput << "Problem:\n" << constraints << "\n";
            *debugOutput << "Solution:\n" << assignment << "\n";
        }

        // convert assignment to result
        solution_type solution;
        visitDepthFirst(clause, [&](const AstArgument& arg) { solution[&arg] = assignment[getVar(arg)]; });
        return solution;
    }

protected:
    /**
     * A utility function mapping an AstArgument to its associated analysis variable.
     *
     * @param arg the AST argument to be mapped
     * @return the analysis variable representing its associated value
     */
    AnalysisVar getVar(const AstArgument& arg) {
        const auto* var = dynamic_cast<const AstVariable*>(&arg);
        if (var == nullptr) {
            // no mapping required
            return AnalysisVar(arg);
        }

        // filter through map => always take the same variable
        auto res = variables.insert({var->getName(), AnalysisVar(var)}).first;
        return res->second;
    }

    /**
     * A utility function mapping an AstArgument to its associated analysis variable.
     *
     * @param arg the AST argument to be mapped
     * @return the analysis variable representing its associated value
     */
    AnalysisVar getVar(const AstArgument* arg) {
        return getVar(*arg);
    }

    /** Adds another constraint to the internally maintained list of constraints */
    void addConstraint(const constraint_type& constraint) {
        constraints.add(constraint);
    }

    Assignment<AnalysisVar> assignment;

    /** The list of constraints making underlying this analysis */
    Problem<AnalysisVar> constraints;

    /** A map mapping variables to unique instances to facilitate the unification of variables */
    std::map<std::string, AnalysisVar> variables;
};

}  // end of namespace souffle
