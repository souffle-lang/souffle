/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file MinimiseProgram.cpp
 *
 * Define classes and functionality related to program minimisation.
 *
 ***********************************************************************/

#include "ast/transform/MinimiseProgram.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/Literal.h"
#include "ast/Node.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/analysis/ClauseNormalisation.h"
#include "ast/analysis/IOType.h"
#include "ast/utility/Utils.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <stack>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ast::transform {

using namespace analysis;

bool MinimiseProgramTransformer::existsValidPermutation(const NormalisedClause& left,
        const NormalisedClause& right, const std::vector<std::vector<unsigned int>>& permutationMatrix) {
    std::size_t clauseSize = permutationMatrix.size();
    // keep track of the possible end-positions of each atom in the first clause
    std::vector<std::vector<unsigned int>> validMoves;
    for (std::size_t i = 0; i < clauseSize; i++) {
        std::vector<unsigned int> currentRow;
        for (std::size_t j = 0; j < clauseSize; j++) {
            if (permutationMatrix[i][j] == 1) {
                currentRow.push_back(j);
            }
        }
        validMoves.push_back(currentRow);
    }

    // extract the possible permutations, DFS style
    std::vector<unsigned int> seen(clauseSize);
    std::vector<unsigned int> currentPermutation;
    std::stack<std::vector<unsigned int>> todoStack;

    todoStack.push(validMoves[0]);

    std::size_t currentIdx = 0;
    while (!todoStack.empty()) {
        if (currentIdx == clauseSize) {
            // permutation is complete, check if it's valid
            if (isValidPermutation(left, right, currentPermutation)) {
                // valid permutation with valid mapping
                // therefore, the two clauses are equivalent!
                return true;
            }

            // Not valid yet, keep checking for more permutations
            if (currentIdx == 0) {
                // already at starting position, so no more permutations possible
                break;
            }

            // undo the last number added to the permutation
            currentIdx--;
            seen[currentPermutation[currentIdx]] = 0;
            currentPermutation.pop_back();

            // see if we can pick up other permutations
            continue;
        }

        // pull out the possibilities for the current point of the permutation
        std::vector<unsigned int> possibilities = todoStack.top();
        todoStack.pop();
        if (possibilities.empty()) {
            // no more possibilities at this point, so undo our last move

            if (currentIdx == 0) {
                // already at starting position, so no more permutations possible
                break;
            }

            currentIdx--;
            seen[currentPermutation[currentIdx]] = 0;
            currentPermutation.pop_back();

            // continue looking for permutations
            continue;
        }

        // try the next possibility
        unsigned int nextNum = possibilities[0];

        // update the possibility vector for the current position
        possibilities.erase(possibilities.begin());
        todoStack.push(possibilities);

        if (seen[nextNum] != 0u) {
            // number already seen in this permutation
            continue;
        } else {
            // number can be used
            seen[nextNum] = 1;
            currentPermutation.push_back(nextNum);
            currentIdx++;

            // if we havent reached the end of the permutation,
            // push up the valid moves for the next position
            if (currentIdx < clauseSize) {
                todoStack.push(validMoves[currentIdx]);
            }
        }
    }

    // checked all permutations, none were valid
    return false;
}

bool MinimiseProgramTransformer::areEquivalentRelations(
        const Relation* firstRelation, const Relation* secondRelation) {
    // check whether relations have same qualifiers, representation, and attribute types
    if (firstRelation->getQualifiers() == secondRelation->getQualifiers() &&
            firstRelation->getRepresentation() == secondRelation->getRepresentation()) {
        auto firstAttributes = firstRelation->getAttributes();
        auto secondAttributes = secondRelation->getAttributes();
        if (firstAttributes.size() == secondAttributes.size()) {
            for (std::size_t i = 0; i < firstAttributes.size(); i++) {
                if (firstAttributes[i]->getTypeName() != secondAttributes[i]->getTypeName()) {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

bool MinimiseProgramTransformer::isValidPermutation(const NormalisedClause& left,
        const NormalisedClause& right, const std::vector<unsigned int>& permutation) {
    const auto& leftElements = left.getElements();
    const auto& rightElements = right.getElements();

    assert(leftElements.size() == rightElements.size() && "clauses should have equal size");
    std::size_t size = leftElements.size();

    std::map<std::string, std::string> variableMap;

    // Constants should be fixed to the identically-named constant
    for (const auto& cst : left.getConstants()) {
        variableMap[cst] = cst;
    }

    // Variables start off mapping to nothing
    for (const auto& var : left.getVariables()) {
        variableMap[var] = "";
    }

    // Pass through the all arguments in the first clause in sequence, mapping each to the corresponding
    // argument in the second clause under the literal permutation
    for (std::size_t i = 0; i < size; i++) {
        const auto& leftArgs = leftElements[i].params;
        const auto& rightArgs = rightElements[permutation[i]].params;
        for (std::size_t j = 0; j < leftArgs.size(); j++) {
            auto leftArg = leftArgs[j];
            auto rightArg = rightArgs[j];
            std::string currentMap = variableMap[leftArg];
            if (currentMap.empty()) {
                // unassigned yet, so assign it appropriately
                variableMap[leftArg] = rightArg;
            } else if (currentMap != rightArg) {
                // inconsistent mapping!
                // clauses cannot be equivalent under this permutation
                return false;
            }
        }
    }

    return true;
}

bool MinimiseProgramTransformer::areBijectivelyEquivalent(
        const NormalisedClause& left, const NormalisedClause& right) {
    const auto& leftElements = left.getElements();
    const auto& rightElements = right.getElements();

    const auto& leftVars = left.getVariables();
    const auto& rightVars = right.getVariables();

    // rules must be fully normalised
    if (!left.isFullyNormalised() || !right.isFullyNormalised()) {
        return false;
    }

    // rules must be the same length to be equal
    if (leftElements.size() != rightElements.size()) {
        return false;
    }

    // head atoms must have the same arity (names do not matter)
    if (leftElements[0].params.size() != rightElements[0].params.size()) {
        return false;
    }

    // rules must have the same number of distinct variables
    if (leftVars.size() != rightVars.size()) {
        return false;
    }

    // rules must have the exact same set of constants
    if (left.getConstants() != right.getConstants()) {
        return false;
    }

    // set up the n x n permutation matrix, where n is the number of clause elements
    std::size_t size = leftElements.size();
    auto permutationMatrix = std::vector<std::vector<unsigned int>>(size);
    for (auto& i : permutationMatrix) {
        i = std::vector<unsigned int>(size);
    }

    // create permutation matrix
    for (std::size_t i = 0; i < size; i++) {
        for (std::size_t j = 0; j < size; j++) {
            if (leftElements[i].name == rightElements[j].name) {
                permutationMatrix[i][j] = 1;
            }
        }
    }

    // check if any of these permutations have valid variable mappings
    return existsValidPermutation(left, right, permutationMatrix);
}

bool MinimiseProgramTransformer::reduceLocallyEquivalentClauses(TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();
    const auto& normalisations = translationUnit.getAnalysis<analysis::ClauseNormalisationAnalysis>();

    std::vector<Clause*> clausesToDelete;

    // split up each relation's rules into equivalence classes
    // TODO (azreika): consider turning this into an ast analysis instead
    for (Relation* rel : program.getRelations()) {
        std::vector<std::vector<Clause*>> equivalenceClasses;

        for (Clause* clause : getClauses(program, *rel)) {
            bool added = false;

            for (std::vector<Clause*>& eqClass : equivalenceClasses) {
                const auto& normedRep = normalisations.getNormalisation(eqClass[0]);
                const auto& normedClause = normalisations.getNormalisation(clause);
                if (areBijectivelyEquivalent(normedRep, normedClause)) {
                    // clause belongs to an existing equivalence class, so delete it
                    eqClass.push_back(clause);
                    clausesToDelete.push_back(clause);
                    added = true;
                    break;
                }
            }

            if (!added) {
                // clause does not belong to any existing equivalence class, so keep it
                std::vector<Clause*> clauseToAdd = {clause};
                equivalenceClasses.push_back(clauseToAdd);
            }
        }
    }

    // remove non-representative clauses
    for (auto clause : clausesToDelete) {
        program.removeClause(clause);
    }

    // changed iff any clauses were deleted
    return !clausesToDelete.empty();
}

bool MinimiseProgramTransformer::reduceSingletonRelations(TranslationUnit& translationUnit) {
    // Note: This reduction is particularly useful in conjunction with the
    // body-partitioning transformation
    Program& program = translationUnit.getProgram();
    const auto& ioTypes = translationUnit.getAnalysis<analysis::IOTypeAnalysis>();
    const auto& normalisations = translationUnit.getAnalysis<analysis::ClauseNormalisationAnalysis>();

    // Find all singleton relations to consider
    std::vector<Clause*> singletonRelationClauses;
    for (Relation* rel : program.getRelations()) {
        if (!ioTypes.isIO(rel) && getClauses(program, *rel).size() == 1) {
            Clause* clause = getClauses(program, *rel)[0];
            singletonRelationClauses.push_back(clause);
        }
    }

    // Keep track of clauses found to be redundant
    std::set<const Clause*> redundantClauses;

    // Keep track of canonical relation name for each redundant clause
    std::map<QualifiedName, QualifiedName> canonicalName;

    // Check pairwise equivalence of each singleton relation
    for (std::size_t i = 0; i < singletonRelationClauses.size(); i++) {
        const auto* first = singletonRelationClauses[i];
        if (redundantClauses.find(first) != redundantClauses.end()) {
            // Already found to be redundant, no need to check
            continue;
        }

        for (std::size_t j = i + 1; j < singletonRelationClauses.size(); j++) {
            const auto* second = singletonRelationClauses[j];

            // Note: Bijective-equivalence check does not care about the head relation name
            const auto& normedFirst = normalisations.getNormalisation(first);
            const auto& normedSecond = normalisations.getNormalisation(second);
            if (areBijectivelyEquivalent(normedFirst, normedSecond)) {
                QualifiedName firstName = first->getHead()->getQualifiedName();
                QualifiedName secondName = second->getHead()->getQualifiedName();
                if (areEquivalentRelations(
                            getRelation(program, firstName), getRelation(program, secondName))) {
                    redundantClauses.insert(second);
                    canonicalName.insert(std::pair(secondName, firstName));
                }
            }
        }
    }

    // Remove redundant relation definitions
    for (const auto* clause : redundantClauses) {
        auto relName = clause->getHead()->getQualifiedName();
        assert(getRelation(program, relName) && "relation does not exist in program");
        removeRelation(translationUnit, relName);
    }

    // Replace each redundant relation appearance with its canonical name
    struct replaceRedundantRelations : public NodeMapper {
        const std::map<QualifiedName, QualifiedName>& canonicalName;

        replaceRedundantRelations(const std::map<QualifiedName, QualifiedName>& canonicalName)
                : canonicalName(canonicalName) {}

        Own<Node> operator()(Own<Node> node) const override {
            // Remove appearances from children nodes
            node->apply(*this);

            if (auto* atom = as<Atom>(node)) {
                auto pos = canonicalName.find(atom->getQualifiedName());
                if (pos != canonicalName.end()) {
                    auto newAtom = clone(atom);
                    newAtom->setQualifiedName(pos->second);
                    return newAtom;
                }
            }

            return node;
        }
    };
    replaceRedundantRelations update(canonicalName);
    program.apply(update);

    // Program was changed iff a relation was replaced
    return !canonicalName.empty();
}

bool MinimiseProgramTransformer::removeRedundantClauses(TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();
    auto isRedundant = [&](const Clause* clause) {
#if 0
        if (clause->isLeq()) {
            return false;
        }
#endif 
        const auto* head = clause->getHead();
        for (const auto* lit : clause->getBodyLiterals()) {
            if (*head == *lit) {
                return true;
            }
        }
        return false;
    };

    std::set<Own<Clause>> clausesToRemove;
    for (const auto* clause : program.getClauses()) {
        if (isRedundant(clause)) {
            clausesToRemove.insert(clone(clause));
        }
    }

    for (auto& clause : clausesToRemove) {
        program.removeClause(clause.get());
    }
    return !clausesToRemove.empty();
}

bool MinimiseProgramTransformer::reduceClauseBodies(TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();
    std::set<Own<Clause>> clausesToAdd;
    std::set<Own<Clause>> clausesToRemove;

    for (const auto* clause : program.getClauses()) {
        auto bodyLiterals = clause->getBodyLiterals();
        std::set<std::size_t> redundantPositions;
        for (std::size_t i = 0; i < bodyLiterals.size(); i++) {
            for (std::size_t j = 0; j < i; j++) {
                if (*bodyLiterals[i] == *bodyLiterals[j]) {
                    redundantPositions.insert(j);
                    break;
                }
            }
        }

        if (!redundantPositions.empty()) {
            auto minimisedClause = mk<Clause>(clone(clause->getHead()));
            for (std::size_t i = 0; i < bodyLiterals.size(); i++) {
                if (!contains(redundantPositions, i)) {
                    minimisedClause->addToBody(clone(bodyLiterals[i]));
                }
            }
            clausesToAdd.insert(std::move(minimisedClause));
            clausesToRemove.insert(clone(clause));
        }
    }

    for (auto& clause : clausesToRemove) {
        program.removeClause(clause.get());
    }
    for (auto& clause : clausesToAdd) {
        program.addClause(clone(clause));
    }

    return !clausesToAdd.empty();
}

bool MinimiseProgramTransformer::transform(TranslationUnit& translationUnit) {
    bool changed = false;
    changed |= reduceClauseBodies(translationUnit);
    if (changed) translationUnit.invalidateAnalyses();
    changed |= removeRedundantClauses(translationUnit);
    if (changed) translationUnit.invalidateAnalyses();
    changed |= reduceLocallyEquivalentClauses(translationUnit);
    if (changed) translationUnit.invalidateAnalyses();
    changed |= reduceSingletonRelations(translationUnit);
    return changed;
}

}  // namespace souffle::ast::transform
