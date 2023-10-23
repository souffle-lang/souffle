/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2023, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ErrorAnalyzer.h
 *
 * Extract reasons why something went wrong.
 *
 ***********************************************************************/

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <sstream>
#include <vector>

#include "ConstraintSystem.h"
#include "ast/Argument.h"
#include "parser/SrcLocation.h"
#include "reports/ErrorReport.h"

namespace souffle::ast::analysis {

template <typename Var>
class ErrorAnalyzer {
private:
    // A custom less functor to have the same result depending on the run and the platform
    template <typename T>
    struct less {
        bool operator()(const std::shared_ptr<T>& lhs, const std::shared_ptr<T>& rhs) const {
            return std::less<std::string>{}(str(lhs), str(rhs));
        }
        std::string str(const std::shared_ptr<T>& v) const {
            std::stringstream ss;
            ss << *v;
            return ss.str();
        }
    };

public:
    using constraint_type = Constraint<Var>;
    using constraint_ptr_type = std::shared_ptr<constraint_type>;
    using unsat_core_type = typename std::set<constraint_ptr_type>;
    using internal_unsat_core_type = typename std::set<constraint_ptr_type, less<constraint_type>>;

    void addUnsatCore(const Argument* arg, const unsat_core_type& unsat_core) {
        unsatCores[arg].insert(unsat_core.begin(), unsat_core.end());
    }

    void print(std::ostream& os) const {
        for (const auto& [var, unsat_core] : unsatCores) {
            os << var << " -- " << *var << "(";
            for (const auto& constraint : unsat_core) {
                os << *constraint << ", ";
            }
            os << ")\n";
        }
    }

    void localizeConstraint(constraint_ptr_type constraint, const SrcLocation& loc) {
        constraintLocations[constraint] = loc;
    }

    void addEquivalentArgumentSet(std::set<const Argument*> equivalentSet) {
        for (const auto argument1 : equivalentSet) {
            for (const auto argument2 : equivalentSet) {
                if (argument1 == argument2) continue;
                equivalentArguments.emplace(argument1, argument2);
            }
        }
    }

    void markArgumentAsExplained(const Argument* argument) {
        explainedArguments.emplace(argument);
        auto range = equivalentArguments.equal_range(argument);
        for (auto it = range.first; it != range.second; ++it) {
            explainedArguments.emplace(it->second);
        }
    }

    bool argumentIsExplained(const Argument* argument) {
        return explainedArguments.find(argument) != explainedArguments.end();
    }

    void explain(ErrorReport& report, const Argument* var, std::string message) {
        if (argumentIsExplained(var)) return;
        std::vector<DiagnosticMessage> additionalMessages;
        bool doMarkAsExplained = true;
        if (auto it = unsatCores.find(var); it != unsatCores.end()) {
            if (!it->second.empty()) {
                additionalMessages.emplace_back("Following constraints are incompatible:");
                for (const auto& constraint : it->second) {
                    std::stringstream ss;
                    if (auto customMessage = constraint->customMessage(); customMessage) {
                        ss << "   " << *customMessage;
                    } else {
                        ss << "   " << *constraint;
                    }
                    if (auto it = constraintLocations.find(constraint); it != constraintLocations.end()) {
                        additionalMessages.emplace_back(ss.str(), it->second);
                    } else {
                        additionalMessages.emplace_back(ss.str());
                    }
                }
            } else {
                doMarkAsExplained = false;
            }
        }
        Diagnostic diag{Diagnostic::Type::ERROR, DiagnosticMessage{message, var->getSrcLoc()},
                std::move(additionalMessages)};
        report.addDiagnostic(diag);
        if (doMarkAsExplained) {
            markArgumentAsExplained(var);
        }
    }

private:
    std::map<const Argument*, internal_unsat_core_type> unsatCores;
    std::map<constraint_ptr_type, SrcLocation> constraintLocations;  // @todo maybe use pointer here
    std::multimap<const Argument*, const Argument*> equivalentArguments;
    std::set<const Argument*> explainedArguments;
};

}  // namespace souffle::ast::analysis
