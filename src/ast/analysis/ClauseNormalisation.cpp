/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ClauseNormalisation.cpp
 *
 * Defines functionality for classes related to clause normalisation
 *
 ***********************************************************************/

#include "ast/analysis/ClauseNormalisation.h"
#include "AggregateOp.h"
#include "ast/Aggregator.h"
#include "ast/Atom.h"
#include "ast/BinaryConstraint.h"
#include "ast/Clause.h"
#include "ast/Literal.h"
#include "ast/Negation.h"
#include "ast/NilConstant.h"
#include "ast/Node.h"
#include "ast/NumericConstant.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/StringConstant.h"
#include "ast/TranslationUnit.h"
#include "ast/UnnamedVariable.h"
#include "ast/Variable.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/StringUtil.h"
#include <algorithm>
#include <cassert>
#include <memory>
#include <ostream>

namespace souffle::ast::analysis {

NormalisedClause::NormalisedClause(const Clause* clause) {
    // head
    QualifiedName name("@min:head");
    std::vector<std::string> headVars;
    for (const auto* arg : clause->getHead()->getArguments()) {
        headVars.push_back(normaliseArgument(arg));
    }
    clauseElements.push_back({name, headVars});

    // body
    for (const auto* lit : clause->getBodyLiterals()) {
        addClauseBodyLiteral("@min:scope:0", lit);
    }
}

void NormalisedClause::addClauseAtom(
        const std::string& qualifier, const std::string& scopeID, const Atom* atom) {
    QualifiedName name(atom->getQualifiedName());
    name.prepend(qualifier);

    std::vector<std::string> vars;
    vars.push_back(scopeID);
    for (const auto* arg : atom->getArguments()) {
        vars.push_back(normaliseArgument(arg));
    }
    clauseElements.push_back({name, vars});
}

void NormalisedClause::addClauseBodyLiteral(const std::string& scopeID, const Literal* lit) {
    if (const auto* atom = as<Atom>(lit)) {
        addClauseAtom("@min:atom", scopeID, atom);
    } else if (const auto* neg = as<Negation>(lit)) {
        addClauseAtom("@min:neg", scopeID, neg->getAtom());
    } else if (const auto* bc = as<BinaryConstraint>(lit)) {
        QualifiedName name(toBinaryConstraintSymbol(bc->getBaseOperator()));
        name.prepend("@min:operator");
        std::vector<std::string> vars;
        vars.push_back(scopeID);
        vars.push_back(normaliseArgument(bc->getLHS()));
        vars.push_back(normaliseArgument(bc->getRHS()));
        clauseElements.push_back({name, vars});
    } else {
        assert(lit != nullptr && "unexpected nullptr lit");
        fullyNormalised = false;
        std::stringstream qualifier;
        qualifier << "@min:unhandled:lit:" << scopeID;
        QualifiedName name(toString(*lit));
        name.prepend(qualifier.str());
        clauseElements.push_back({name, std::vector<std::string>()});
    }
}

std::string NormalisedClause::normaliseArgument(const Argument* arg) {
    if (auto* stringCst = as<StringConstant>(arg)) {
        std::stringstream name;
        name << "@min:cst:str" << *stringCst;
        constants.insert(name.str());
        return name.str();
    } else if (auto* numericCst = as<NumericConstant>(arg)) {
        std::stringstream name;
        name << "@min:cst:num:" << *numericCst;
        constants.insert(name.str());
        return name.str();
    } else if (isA<NilConstant>(arg)) {
        constants.insert("@min:cst:nil");
        return "@min:cst:nil";
    } else if (auto* var = as<ast::Variable>(arg)) {
        auto name = var->getName();
        variables.insert(name);
        return name;
    } else if (as<UnnamedVariable>(arg)) {
        static std::size_t countUnnamed = 0;
        std::stringstream name;
        name << "@min:unnamed:" << countUnnamed++;
        variables.insert(name.str());
        return name.str();
    } else if (auto* aggr = as<Aggregator>(arg)) {
        // Set the scope to uniquely identify the aggregator
        std::stringstream scopeID;
        scopeID << "@min:scope:" << ++aggrScopeCount;
        variables.insert(scopeID.str());

        // Set the type signature of this aggregator
        std::stringstream aggrTypeSignature;
        aggrTypeSignature << "@min:aggrtype";
        std::vector<std::string> aggrTypeSignatureComponents;

        // - the operator is fixed and cannot be changed
        aggrTypeSignature << ":" << aggr->getBaseOperator();

        // - the scope can be remapped as a variable
        aggrTypeSignatureComponents.push_back(scopeID.str());

        // - the normalised target expression can be remapped as a variable
        if (aggr->getTargetExpression() != nullptr) {
            std::string normalisedExpr = normaliseArgument(aggr->getTargetExpression());
            aggrTypeSignatureComponents.push_back(normalisedExpr);
        }

        // Type signature is its own special atom
        clauseElements.push_back({aggrTypeSignature.str(), aggrTypeSignatureComponents});

        // Add each contained normalised clause literal, tying it with the new scope ID
        for (const auto* literal : aggr->getBodyLiterals()) {
            addClauseBodyLiteral(scopeID.str(), literal);
        }

        // Aggregator identified by the scope ID
        return scopeID.str();
    } else {
        fullyNormalised = false;
        return "@min:unhandled:arg";
    }
}

void ClauseNormalisationAnalysis::run(const TranslationUnit& translationUnit) {
    const auto& program = translationUnit.getProgram();
    for (const auto* clause : program.getClauses()) {
        assert(!contains(normalisations, clause) && "clause already processed");
        normalisations[clause] = NormalisedClause(clause);
    }
}

void ClauseNormalisationAnalysis::print(std::ostream& os) const {
    for (const auto& [clause, norm] : normalisations) {
        os << "Normalise(" << *clause << ") = {";
        const auto& els = norm.getElements();
        for (std::size_t i = 0; i < els.size(); i++) {
            if (i != 0) {
                os << ", ";
            }
            os << els[i].name << ":" << els[i].params;
        }
        os << "}" << std::endl;
    }
}

const NormalisedClause& ClauseNormalisationAnalysis::getNormalisation(const Clause* clause) const {
    assert(contains(normalisations, clause) && "clause not normalised");
    return normalisations.at(clause);
}

}  // namespace souffle::ast::analysis
