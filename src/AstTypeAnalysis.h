/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstTypeAnalysis.h
 *
 * A collection of type analyses operating on AST constructs.
 *
 ***********************************************************************/

#pragma once

#include "AstAnalysis.h"
#include "AstTypeEnvironmentAnalysis.h"
#include "TypeSystem.h"
#include <map>
#include <memory>
#include <sstream>
#include <vector>

namespace souffle {

class AstArgument;
class AstClause;
class AstProgram;
class AstTranslationUnit;

class TypeAnalysis : public AstAnalysis {
private:
    std::map<const AstArgument*, TypeSet> argumentTypes;
    std::vector<std::unique_ptr<AstClause>> annotatedClauses;
    std::stringstream analysisLogs;

public:
    static constexpr const char* name = "type-analysis";

    void run(const AstTranslationUnit& translationUnit) override;

    void print(std::ostream& os) const override;

    /**
     * Get the computed types for the given argument.
     */
    TypeSet getTypes(const AstArgument* argument) const {
        auto found = argumentTypes.find(argument);
        assert(found != argumentTypes.end());
        return found->second;
    }

    /**
     * Analyse the given clause and computes for each contained argument
     * a set of potential types. If the set associated to an argument is empty,
     * no consistent typing can be found and the rule can not be properly typed.
     *
     * @param env a typing environment describing the set of available types
     * @param clause the clause to be typed
     * @param program the program
     * @return a map mapping each contained argument to a a set of types
     */
    static std::map<const AstArgument*, TypeSet> analyseTypes(const TypeEnvironment& env,
            const AstClause& clause, const AstProgram* program, std::ostream* logs = nullptr);
};

}  // end of namespace souffle
