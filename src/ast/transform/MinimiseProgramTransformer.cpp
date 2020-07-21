/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file MinimiseProgramTransformer.cpp
 *
 * Define classes and functionality related to program minimisation.
 *
 ***********************************************************************/

#include "ast/AstAbstract.h"
#include "ast/AstArgument.h"
#include "ast/AstClause.h"
#include "ast/AstLiteral.h"
#include "ast/AstNode.h"
#include "ast/AstProgram.h"
#include "ast/AstQualifiedName.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstUtils.h"
#include "ast/AstVisitor.h"
#include "ast/analysis/AstIOTypeAnalysis.h"
#include "ast/transform/AstTransforms.h"
#include "utility/ContainerUtil.h"
#include "utility/StringUtil.h"
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

namespace souffle {

class AstRelation;

class MinimiseProgramTransformer::NormalisedClauseRepr {
public:
    struct NormalisedClauseElementRepr {
        AstQualifiedName name;
        std::vector<std::string> params;
    };

    NormalisedClauseRepr(const AstClause* clause) {
        // head
        AstQualifiedName name("min:head");
        std::vector<std::string> headVars;
        for (const auto* arg : clause->getHead()->getArguments()) {
            headVars.push_back(normaliseArgument(arg));
        }
        clauseElements.push_back({.name = name, .params = headVars});

        // body
        for (const auto* lit : clause->getBodyLiterals()) {
            addClauseBodyLiteral(lit);
        }
    }

    bool isFullyNormalised() const {
        return fullyNormalised;
    }

    const std::set<std::string>& getVariables() const {
        return variables;
    }

    const std::set<std::string>& getConstants() const {
        return constants;
    }

    const std::vector<NormalisedClauseElementRepr>& getElements() const {
        return clauseElements;
    }

private:
    bool fullyNormalised{true};
    std::set<std::string> variables{};
    std::set<std::string> constants{};
    std::vector<NormalisedClauseElementRepr> clauseElements;

    /**
     * Parse an atom with a preset name qualifier into the element list.
     */
    void addClauseAtom(std::string qualifier, const AstAtom* atom);

    /**
     * Parse a body literal into the element list.
     */
    void addClauseBodyLiteral(const AstLiteral* lit);

    /**
     * Return a normalised string repr of an argument.
     */
    std::string normaliseArgument(const AstArgument* arg);
};

void MinimiseProgramTransformer::NormalisedClauseRepr::addClauseAtom(
        std::string qualifier, const AstAtom* atom) {
    AstQualifiedName name(atom->getQualifiedName());
    name.prepend(qualifier);

    std::vector<std::string> vars;
    for (const auto* arg : atom->getArguments()) {
        vars.push_back(normaliseArgument(arg));
    }
    clauseElements.push_back({.name = name, .params = vars});
}

void MinimiseProgramTransformer::NormalisedClauseRepr::addClauseBodyLiteral(const AstLiteral* lit) {
    if (const auto* atom = dynamic_cast<const AstAtom*>(lit)) {
        addClauseAtom("@min:atom", atom);
    } else if (const auto* neg = dynamic_cast<const AstNegation*>(lit)) {
        addClauseAtom("@min:neg", neg->getAtom());
    } else if (const auto* bc = dynamic_cast<const AstBinaryConstraint*>(lit)) {
        AstQualifiedName name(toBinaryConstraintSymbol(bc->getOperator()));
        name.prepend("@min:operator");
        std::vector<std::string> vars;
        vars.push_back(normaliseArgument(bc->getLHS()));
        vars.push_back(normaliseArgument(bc->getRHS()));
        clauseElements.push_back({.name = name, .params = vars});
    } else {
        fullyNormalised = false;
        AstQualifiedName name(toString(*lit));
        name.prepend("@min:unhandled:lit");
        clauseElements.push_back({.name = name, .params = std::vector<std::string>()});
    }
}

std::string MinimiseProgramTransformer::NormalisedClauseRepr::normaliseArgument(const AstArgument* arg) {
    if (auto* stringCst = dynamic_cast<const AstStringConstant*>(arg)) {
        std::stringstream name;
        name << "@min:cst:str" << *stringCst;
        constants.insert(name.str());
        return name.str();
    } else if (auto* numericCst = dynamic_cast<const AstNumericConstant*>(arg)) {
        std::stringstream name;
        name << "@min:cst:num:" << *numericCst;
        constants.insert(name.str());
        return name.str();
    } else if (dynamic_cast<const AstNilConstant*>(arg) != nullptr) {
        constants.insert("@min:cst:nil");
        return "@min:cst:nil";
    } else if (auto* var = dynamic_cast<const AstVariable*>(arg)) {
        auto name = var->getName();
        variables.insert(name);
        return name;
    } else if (dynamic_cast<const AstUnnamedVariable*>(arg)) {
        static size_t countUnnamed = 0;
        std::stringstream name;
        name << "@min:unnamed:" << countUnnamed++;
        variables.insert(name.str());
        return name.str();
    } else {
        fullyNormalised = false;
        return "@min:unhandled:arg";
    }
}

/**
 * Extract valid permutations from a given permutation matrix of valid moves.
 */
static std::vector<std::vector<unsigned int>> extractPermutations(
        const std::vector<std::vector<unsigned int>>& permutationMatrix) {
    size_t clauseSize = permutationMatrix.size();
    // keep track of the possible end-positions of each atom in the first clause
    std::vector<std::vector<unsigned int>> validMoves;
    for (size_t i = 0; i < clauseSize; i++) {
        std::vector<unsigned int> currentRow;
        for (size_t j = 0; j < clauseSize; j++) {
            if (permutationMatrix[i][j] == 1) {
                currentRow.push_back(j);
            }
        }
        validMoves.push_back(currentRow);
    }

    // extract the possible permutations, DFS style
    std::vector<std::vector<unsigned int>> permutations;
    std::vector<unsigned int> seen(clauseSize);
    std::vector<unsigned int> currentPermutation;
    std::stack<std::vector<unsigned int>> todoStack;

    todoStack.push(validMoves[0]);

    size_t currentIdx = 0;
    while (!todoStack.empty()) {
        if (currentIdx == clauseSize) {
            // permutation is complete
            permutations.push_back(currentPermutation);

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

    // found all permutations
    return permutations;
}

bool MinimiseProgramTransformer::isValidPermutation(const NormalisedClauseRepr& left,
        const NormalisedClauseRepr& right, const std::vector<unsigned int>& permutation) {
    const auto& leftElements = left.getElements();
    const auto& rightElements = right.getElements();

    assert(leftElements.size() == rightElements.size() && "clauses should have equal size");
    size_t size = leftElements.size();

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
    for (size_t i = 0; i < size; i++) {
        const auto& leftArgs = leftElements[i].params;
        const auto& rightArgs = rightElements[permutation[i]].params;
        for (size_t j = 0; j < leftArgs.size(); j++) {
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
        const NormalisedClauseRepr& left, const NormalisedClauseRepr& right) {
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
    size_t size = leftElements.size();
    auto permutationMatrix = std::vector<std::vector<unsigned int>>(size);
    for (auto& i : permutationMatrix) {
        i = std::vector<unsigned int>(size);
    }

    // create permutation matrix
    permutationMatrix[0][0] = 1;
    for (size_t i = 1; i < size; i++) {
        for (size_t j = 1; j < size; j++) {
            if (leftElements[i].name == rightElements[j].name) {
                permutationMatrix[i][j] = 1;
            }
        }
    }

    // check if any of these permutations have valid variable mappings
    std::vector<std::vector<unsigned int>> permutations = extractPermutations(permutationMatrix);
    for (auto permutation : permutations) {
        if (isValidPermutation(left, right, permutation)) {
            // valid permutation with valid mapping
            // therefore, the two clauses are equivalent!
            return true;
        }
    }
    return false;
}

bool MinimiseProgramTransformer::areBijectivelyEquivalent(
        const AstClause* leftClause, const AstClause* rightClause) {
    auto normalisedLeft = NormalisedClauseRepr(leftClause);
    auto normalisedRight = NormalisedClauseRepr(rightClause);
    return areBijectivelyEquivalent(normalisedLeft, normalisedRight);
}

bool MinimiseProgramTransformer::reduceLocallyEquivalentClauses(AstTranslationUnit& translationUnit) {
    AstProgram& program = *translationUnit.getProgram();

    std::vector<AstClause*> clausesToDelete;

    // split up each relation's rules into equivalence classes
    // TODO (azreika): consider turning this into an ast analysis instead
    for (AstRelation* rel : program.getRelations()) {
        std::vector<std::vector<AstClause*>> equivalenceClasses;

        for (AstClause* clause : getClauses(program, *rel)) {
            bool added = false;

            for (std::vector<AstClause*>& eqClass : equivalenceClasses) {
                AstClause* rep = eqClass[0];

                if (areBijectivelyEquivalent(rep, clause)) {
                    // clause belongs to an existing equivalence class, so delete it
                    eqClass.push_back(clause);
                    clausesToDelete.push_back(clause);
                    added = true;
                    break;
                }
            }

            if (!added) {
                // clause does not belong to any existing equivalence class, so keep it
                std::vector<AstClause*> clauseToAdd = {clause};
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

bool MinimiseProgramTransformer::reduceSingletonRelations(AstTranslationUnit& translationUnit) {
    // Note: This reduction is particularly useful in conjunction with the
    // body-partitioning transformation
    AstProgram& program = *translationUnit.getProgram();
    auto* ioTypes = translationUnit.getAnalysis<IOType>();

    // Find all singleton relations to consider
    std::vector<AstClause*> singletonRelationClauses;
    for (AstRelation* rel : program.getRelations()) {
        if (!ioTypes->isIO(rel) && getClauses(program, *rel).size() == 1) {
            AstClause* clause = getClauses(program, *rel)[0];
            singletonRelationClauses.push_back(clause);
        }
    }

    // Keep track of clauses found to be redundant
    std::set<AstClause*> redundantClauses;

    // Keep track of canonical relation name for each redundant clause
    std::map<AstQualifiedName, AstQualifiedName> canonicalName;

    // Check pairwise equivalence of each singleton relation
    for (size_t i = 0; i < singletonRelationClauses.size(); i++) {
        AstClause* first = singletonRelationClauses[i];
        if (redundantClauses.find(first) != redundantClauses.end()) {
            // Already found to be redundant, no need to check
            continue;
        }

        for (size_t j = i + 1; j < singletonRelationClauses.size(); j++) {
            AstClause* second = singletonRelationClauses[j];

            // Note: Bijective-equivalence check does not care about the head relation name
            if (areBijectivelyEquivalent(first, second)) {
                AstQualifiedName firstName = first->getHead()->getQualifiedName();
                AstQualifiedName secondName = second->getHead()->getQualifiedName();
                redundantClauses.insert(second);
                canonicalName.insert(std::pair(secondName, firstName));
            }
        }
    }

    // Remove redundant relation definitions
    for (AstClause* clause : redundantClauses) {
        auto relName = clause->getHead()->getQualifiedName();
        AstRelation* rel = getRelation(program, relName);
        assert(rel != nullptr && "relation does not exist in program");
        program.removeClause(clause);
        program.removeRelation(relName);
    }

    // Replace each redundant relation appearance with its canonical name
    struct replaceRedundantRelations : public AstNodeMapper {
        const std::map<AstQualifiedName, AstQualifiedName>& canonicalName;

        replaceRedundantRelations(const std::map<AstQualifiedName, AstQualifiedName>& canonicalName)
                : canonicalName(canonicalName) {}

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            // Remove appearances from children nodes
            node->apply(*this);

            if (auto* atom = dynamic_cast<AstAtom*>(node.get())) {
                auto pos = canonicalName.find(atom->getQualifiedName());
                if (pos != canonicalName.end()) {
                    auto newAtom = souffle::clone(atom);
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

bool MinimiseProgramTransformer::removeRedundantClauses(AstTranslationUnit& translationUnit) {
    auto& program = *translationUnit.getProgram();
    auto isRedundant = [&](const AstClause* clause) {
        const auto* head = clause->getHead();
        for (const auto* lit : clause->getBodyLiterals()) {
            if (*head == *lit) {
                return true;
            }
        }
        return false;
    };

    std::set<std::unique_ptr<AstClause>> clausesToRemove;
    for (const auto* clause : program.getClauses()) {
        if (isRedundant(clause)) {
            clausesToRemove.insert(souffle::clone(clause));
        }
    }

    for (auto& clause : clausesToRemove) {
        program.removeClause(clause.get());
    }
    return !clausesToRemove.empty();
}

bool MinimiseProgramTransformer::reduceClauseBodies(AstTranslationUnit& translationUnit) {
    auto& program = *translationUnit.getProgram();
    std::set<std::unique_ptr<AstClause>> clausesToAdd;
    std::set<std::unique_ptr<AstClause>> clausesToRemove;

    for (const auto* clause : program.getClauses()) {
        auto bodyLiterals = clause->getBodyLiterals();
        std::set<size_t> redundantPositions;
        for (size_t i = 0; i < bodyLiterals.size(); i++) {
            for (size_t j = 0; j < i; j++) {
                if (*bodyLiterals[i] == *bodyLiterals[j]) {
                    redundantPositions.insert(j);
                    break;
                }
            }
        }

        if (!redundantPositions.empty()) {
            auto minimisedClause = std::make_unique<AstClause>();
            minimisedClause->setHead(souffle::clone(clause->getHead()));
            for (size_t i = 0; i < bodyLiterals.size(); i++) {
                if (!contains(redundantPositions, i)) {
                    minimisedClause->addToBody(souffle::clone(bodyLiterals[i]));
                }
            }
            clausesToAdd.insert(std::move(minimisedClause));
            clausesToRemove.insert(souffle::clone(clause));
        }
    }

    for (auto& clause : clausesToRemove) {
        program.removeClause(clause.get());
    }
    for (auto& clause : clausesToAdd) {
        program.addClause(souffle::clone(clause));
    }

    return !clausesToAdd.empty();
}

bool MinimiseProgramTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;
    changed |= reduceClauseBodies(translationUnit);
    changed |= removeRedundantClauses(translationUnit);
    changed |= reduceLocallyEquivalentClauses(translationUnit);
    changed |= reduceSingletonRelations(translationUnit);
    return changed;
}

}  // namespace souffle
