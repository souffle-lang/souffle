/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file MagicSet.h
 *
 * Define classes and functionality related to the magic set transformation.
 *
 ***********************************************************************/

#pragma once

#include "AstAnalysis.h"
#include "AstArgument.h"
#include "AstClause.h"
#include "AstLiteral.h"
#include "AstQualifiedName.h"
#include "AstVisitor.h"
#include "Util.h"
#include <cstddef>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

class AstTranslationUnit;

class AdornedPredicate {
private:
    AstQualifiedName predicateName;
    std::string adornment;

public:
    AdornedPredicate(AstQualifiedName name, std::string adornment)
            : predicateName(std::move(name)), adornment(std::move(adornment)) {}

    ~AdornedPredicate() = default;

    AstQualifiedName getQualifiedName() const {
        return predicateName;
    }

    std::string getAdornment() const {
        return adornment;
    }

    friend std::ostream& operator<<(std::ostream& out, const AdornedPredicate& arg) {
        out << "(" << arg.predicateName << ", " << arg.adornment << ")";
        return out;
    }

    friend bool operator<(const AdornedPredicate& p1, const AdornedPredicate& p2) {
        if (p1.getQualifiedName() != p2.getQualifiedName()) {
            return p1.getQualifiedName() < p2.getQualifiedName();
        } else {
            return p1.getAdornment() < p2.getAdornment();
        }
    }
};

class AdornedClause {
private:
    AstClause* clause;
    std::string headAdornment;
    std::vector<std::string> bodyAdornment;
    std::vector<unsigned int> ordering;

public:
    AdornedClause(AstClause* clause, std::string headAdornment, std::vector<std::string> bodyAdornment,
            std::vector<unsigned int> ordering)
            : clause(clause), headAdornment(std::move(headAdornment)),
              bodyAdornment(std::move(bodyAdornment)), ordering(std::move(ordering)) {}

    AstClause* getClause() const {
        return clause;
    }

    std::string getHeadAdornment() const {
        return headAdornment;
    }

    std::vector<std::string> getBodyAdornment() const {
        return bodyAdornment;
    }

    std::vector<unsigned int> getOrdering() const {
        return ordering;
    }

    friend std::ostream& operator<<(std::ostream& out, const AdornedClause& arg) {
        size_t currpos = 0;
        bool firstAdded = true;
        out << arg.clause->getHead()->getQualifiedName() << "{" << arg.headAdornment << "} :- ";

        std::vector<AstLiteral*> bodyLiterals = arg.clause->getBodyLiterals();
        for (AstLiteral* literal : bodyLiterals) {
            if (auto* atom = dynamic_cast<AstAtom*>(literal)) {
                if (!firstAdded) {
                    out << ", ";
                }
                firstAdded = false;
                out << atom->getQualifiedName() << "{_}";
            } else if (auto* neg = dynamic_cast<AstNegation*>(literal)) {
                if (!firstAdded) {
                    out << ", ";
                }
                firstAdded = false;
                out << neg->getAtom()->getQualifiedName() << "{" << arg.bodyAdornment[currpos++] << "}";
            }
        }
        out << ". [order: " << arg.ordering << "]";

        return out;
    }
};

class BindingStore {
private:
    std::map<std::string, std::unique_ptr<AstArgument>> originalArguments;
    std::map<std::string, std::set<std::string>> varDependencies;
    std::set<std::string> variableBoundComposites;

public:
    AstArgument* cloneOriginalArgument(const std::string& argName) const {
        return originalArguments.at(argName)->clone();
    }

    const std::set<std::string>& getVariableDependencies(const std::string& argName) const {
        return varDependencies.at(argName);
    }

    void addBinding(const std::string& newVariableName, const AstArgument* arg) {
        originalArguments[newVariableName] = std::unique_ptr<AstArgument>(arg->clone());

        // find the variable dependencies
        std::set<std::string> dependencies;
        visitDepthFirst(*arg, [&](const AstVariable& var) { dependencies.insert(var.getName()); });
        varDependencies[newVariableName] = dependencies;
    }

    void addVariableBoundComposite(const std::string& functorName) {
        variableBoundComposites.insert(functorName);
    }

    bool isVariableBoundComposite(const std::string& functorName) const {
        return (variableBoundComposites.find(functorName) != variableBoundComposites.end());
    }
};

class Adornment : public AstAnalysis {
private:
    std::vector<std::vector<AdornedClause>> adornmentClauses;
    std::vector<AstQualifiedName> adornmentRelations;
    std::set<AstQualifiedName> adornmentEdb;
    std::set<AstQualifiedName> adornmentIdb;
    std::set<AstQualifiedName> negatedAtoms;
    std::set<AstQualifiedName> ignoredAtoms;
    BindingStore bindings;

public:
    static constexpr const char* name = "adorned-clauses";

    ~Adornment() override = default;

    void run(const AstTranslationUnit& translationUnit) override;

    void print(std::ostream& os) const override;

    const std::vector<std::vector<AdornedClause>>& getAdornedClauses() const {
        return adornmentClauses;
    }

    const std::vector<AstQualifiedName>& getRelations() const {
        return adornmentRelations;
    }

    const std::set<AstQualifiedName>& getEDB() const {
        return adornmentEdb;
    }

    const std::set<AstQualifiedName>& getIDB() const {
        return adornmentIdb;
    }

    const std::set<AstQualifiedName>& getNegatedAtoms() const {
        return negatedAtoms;
    }

    const std::set<AstQualifiedName>& getIgnoredAtoms() const {
        return ignoredAtoms;
    }

    const BindingStore& getBindings() const {
        return bindings;
    }
};
}  // namespace souffle
