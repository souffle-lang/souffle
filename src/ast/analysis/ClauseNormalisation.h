/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ClauseNormalisation.h
 *
 * Defines a clause-normalisation analysis, providing a normal form for
 * each clause in the program.
 *
 ***********************************************************************/

#pragma once

#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/Literal.h"
#include "ast/QualifiedName.h"
#include "ast/TranslationUnit.h"
#include <cstddef>
#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace souffle::ast {

class TranslationUnit;

namespace analysis {

class NormalisedClause {
public:
    struct NormalisedClauseElement {
        QualifiedName name;
        std::vector<std::string> params;
    };

    NormalisedClause() = default;

    NormalisedClause(const Clause* clause);

    bool isFullyNormalised() const {
        return fullyNormalised;
    }

    const std::set<std::string>& getVariables() const {
        return variables;
    }

    const std::set<std::string>& getConstants() const {
        return constants;
    }

    const std::vector<NormalisedClauseElement>& getElements() const {
        return clauseElements;
    }

private:
    bool fullyNormalised{true};
    std::size_t aggrScopeCount{0};
    std::set<std::string> variables{};
    std::set<std::string> constants{};
    std::vector<NormalisedClauseElement> clauseElements{};

    /**
     * Parse an atom with a preset name qualifier into the element list.
     */
    void addClauseAtom(const std::string& qualifier, const std::string& scopeID, const Atom* atom);

    /**
     * Parse a body literal into the element list.
     */
    void addClauseBodyLiteral(const std::string& scopeID, const Literal* lit);

    /**
     * Return a normalised string repr of an argument.
     */
    std::string normaliseArgument(const Argument* arg);
};

class ClauseNormalisationAnalysis : public Analysis {
public:
    static constexpr const char* name = "clause-normalisation";

    ClauseNormalisationAnalysis() : Analysis(name) {}

    void run(const TranslationUnit& translationUnit) override;

    void print(std::ostream& os) const override;

    const NormalisedClause& getNormalisation(const Clause* clause) const;

private:
    std::map<const Clause*, NormalisedClause> normalisations;
};

}  // namespace analysis
}  // namespace souffle::ast
