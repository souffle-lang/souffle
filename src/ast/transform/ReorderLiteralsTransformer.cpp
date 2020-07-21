/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ReorderLiteralsTransformer.cpp
 *
 * Define classes and functionality related to the ReorderLiterals
 * transformer.
 *
 ***********************************************************************/

#include "Global.h"
#include "ast/AstAbstract.h"
#include "ast/AstArgument.h"
#include "ast/AstClause.h"
#include "ast/AstLiteral.h"
#include "ast/AstProgram.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstUtils.h"
#include "ast/AstVisitor.h"
#include "ast/analysis/AstProfileUse.h"
#include "ast/transform/AstTransforms.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle {
class AstRelation;

// Type for SIPS functions
// - Take in a vector of atoms to choose from and a set of bound variables
// - Return the best atom based on a SIPS-specific cost metric
using sips_t = std::function<unsigned int(std::vector<AstAtom*>, const std::set<std::string>&)>;

inline bool isProposition(const AstAtom* atom) {
    return atom->getArguments().empty();
}

/**
 * Counts the number of bound arguments in a given atom.
 */
unsigned int numBoundArguments(const AstAtom* atom, const std::set<std::string>& boundVariables) {
    int count = 0;

    for (const AstArgument* arg : atom->getArguments()) {
        // argument is bound iff all contained variables are bound
        bool isBound = true;

        visitDepthFirst(*arg, [&](const AstVariable& var) {
            if (boundVariables.find(var.getName()) == boundVariables.end()) {
                // found an unbound variable, so argument is unbound
                isBound = false;
            }
        });

        if (isBound) {
            count++;
        }
    }

    return count;
}

/**
 * Returns a SIPS function based on the SIPS option provided.
 * The SIPS function will return the index of the appropriate atom in a clause
 * given a goal.
 *
 * E.g. the 'max-bound' SIPS function will return the atom in the clause with
 * the maximum number of bound arguments.
 */
sips_t getSipsFunction(const std::string& sipsChosen) {
    // --- Create the appropriate SIPS function ---

    // Each SIPS function has a priority metric (e.g. max-bound atoms).
    // Arguments:
    //      - a vector of atoms to choose from (nullpointers in the vector will be ignored)
    //      - the set of variables bound so far
    // Returns: the index of the atom maximising the priority metric
    sips_t getNextAtomSips;

    if (sipsChosen == "naive") {
        // Goal: choose the first atom with at least one bound argument, or with no arguments
        getNextAtomSips = [&](std::vector<AstAtom*> atoms, const std::set<std::string>& boundVariables) {
            for (unsigned int i = 0; i < atoms.size(); i++) {
                const AstAtom* currAtom = atoms[i];

                if (currAtom == nullptr) {
                    // already processed - move on
                    continue;
                }

                if (isProposition(currAtom)) {
                    // no arguments
                    return i;
                }

                if (numBoundArguments(currAtom, boundVariables) >= 1) {
                    // at least one bound argument
                    return i;
                }
            }

            // none found; choose the first non-null
            for (unsigned int i = 0; i < atoms.size(); i++) {
                if (atoms[i] != nullptr) {
                    return i;
                }
            }

            // fall back to the first
            return 0U;
        };
    } else if (sipsChosen == "all-bound") {
        // Goal: prioritise atoms with all arguments bound
        getNextAtomSips = [&](std::vector<AstAtom*> atoms, const std::set<std::string>& boundVariables) {
            for (unsigned int i = 0; i < atoms.size(); i++) {
                const AstAtom* currAtom = atoms[i];

                if (currAtom == nullptr) {
                    // already processed - move on
                    continue;
                }

                if (isProposition(currAtom)) {
                    // no arguments, so all are trivially bound
                    return i;
                }

                int arity = currAtom->getArity();
                int numBound = numBoundArguments(currAtom, boundVariables);
                if (numBound == arity) {
                    // all arguments are bound!
                    return i;
                }
            }

            // none found; choose the first non-null
            for (unsigned int i = 0; i < atoms.size(); i++) {
                if (atoms[i] != nullptr) {
                    return i;
                }
            }

            // fall back to the first
            return 0U;
        };
    } else if (sipsChosen == "max-bound") {
        // Goal: choose the atom with the maximum number of bound variables
        //       - exception: propositions should be prioritised
        getNextAtomSips = [&](std::vector<AstAtom*> atoms, const std::set<std::string>& boundVariables) {
            int currMaxBound = -1;
            unsigned int currMaxIdx = 0U;

            for (unsigned int i = 0; i < atoms.size(); i++) {
                const AstAtom* currAtom = atoms[i];

                if (currAtom == nullptr) {
                    // already processed - move on
                    continue;
                }

                if (isProposition(currAtom)) {
                    // propositions should be prioritised
                    return i;
                }

                int numBound = numBoundArguments(currAtom, boundVariables);
                if (numBound > currMaxBound) {
                    currMaxBound = numBound;
                    currMaxIdx = i;
                }
            }

            return currMaxIdx;
        };
    } else if (sipsChosen == "max-ratio") {
        // Goal: choose the atom with the maximum ratio of bound to unbound
        getNextAtomSips = [&](std::vector<AstAtom*> atoms, const std::set<std::string>& boundVariables) {
            auto isLargerRatio = [&](std::pair<int, int> lhs, std::pair<int, int> rhs) {
                return (lhs.first * rhs.second > lhs.second * rhs.first);
            };

            auto currMaxRatio = std::pair<int, int>(-1, 1);  // set to -1
            unsigned int currMaxIdx = 0U;

            for (unsigned int i = 0; i < atoms.size(); i++) {
                const AstAtom* currAtom = atoms[i];

                if (currAtom == nullptr) {
                    // already processed - move on
                    continue;
                }

                if (isProposition(currAtom)) {
                    // propositions are as bound as possible
                    return i;
                }

                int numBound = numBoundArguments(currAtom, boundVariables);
                int numArgs = currAtom->getArity();
                auto currRatio = std::pair<int, int>(numBound, numArgs);
                if (isLargerRatio(currRatio, currMaxRatio)) {
                    currMaxRatio = currRatio;
                    currMaxIdx = i;
                }
            }

            return currMaxIdx;
        };
    } else if (sipsChosen == "least-free") {
        // Goal: choose the atom with the least number of unbound arguments
        getNextAtomSips = [&](std::vector<AstAtom*> atoms, const std::set<std::string>& boundVariables) {
            int currLeastFree = -1;
            unsigned int currLeastIdx = 0U;

            for (unsigned int i = 0; i < atoms.size(); i++) {
                const AstAtom* currAtom = atoms[i];

                if (currAtom == nullptr) {
                    // already processed - move on
                    continue;
                }

                if (isProposition(currAtom)) {
                    // propositions have 0 unbound arguments, which is minimal
                    return i;
                }

                int numBound = numBoundArguments(currAtom, boundVariables);
                int numFree = currAtom->getArity() - numBound;
                if (currLeastFree == -1 || numFree < currLeastFree) {
                    currLeastFree = numFree;
                    currLeastIdx = i;
                }
            }

            return currLeastIdx;
        };
    } else if (sipsChosen == "least-free-vars") {
        // Goal: choose the atom with the least amount of unbound variables
        getNextAtomSips = [&](std::vector<AstAtom*> atoms, const std::set<std::string>& boundVariables) {
            int currLeastFree = -1;
            unsigned int currLeastIdx = 0U;

            for (unsigned int i = 0; i < atoms.size(); i++) {
                const AstAtom* currAtom = atoms[i];

                if (currAtom == nullptr) {
                    // already processed - move on
                    continue;
                }

                if (isProposition(currAtom)) {
                    // propositions have 0 unbound variables, which is minimal
                    return i;
                }

                // use a set to hold all free variables to avoid double-counting
                std::set<std::string> freeVars;
                visitDepthFirst(*currAtom, [&](const AstVariable& var) {
                    if (boundVariables.find(var.getName()) == boundVariables.end()) {
                        freeVars.insert(var.getName());
                    }
                });

                int numFreeVars = freeVars.size();
                if (currLeastFree == -1 || numFreeVars < currLeastFree) {
                    currLeastFree = numFreeVars;
                    currLeastIdx = i;
                }
            }

            return currLeastIdx;
        };
    } else {
        // chosen SIPS is not implemented, so keep the same order
        // Goal: leftmost atom first
        getNextAtomSips = [&](std::vector<AstAtom*> atoms,
                                  const std::set<std::string>& /* boundVariables */) {
            for (unsigned int i = 0; i < atoms.size(); i++) {
                if (atoms[i] == nullptr) {
                    // already processed - move on
                    continue;
                }

                return i;
            }

            // fall back to the first
            return 0U;
        };
    }

    return getNextAtomSips;
}

/**
 * Finds the new ordering of a vector of atoms after the given SIPS is applied.
 */
std::vector<unsigned int> applySips(sips_t sipsFunction, std::vector<AstAtom*> atoms) {
    std::set<std::string> boundVariables;
    std::vector<unsigned int> newOrder(atoms.size());

    unsigned int numAdded = 0;
    while (numAdded < atoms.size()) {
        // grab the next atom, based on the SIPS function
        unsigned int nextIdx = sipsFunction(atoms, boundVariables);
        AstAtom* nextAtom = atoms[nextIdx];

        // set all arguments that are variables as bound
        // note: arguments that are functors, etc., do not newly bind anything
        for (AstArgument* arg : nextAtom->getArguments()) {
            if (auto* var = dynamic_cast<AstVariable*>(arg)) {
                boundVariables.insert(var->getName());
            }
        }

        newOrder[numAdded] = nextIdx;  // add to the ordering
        atoms[nextIdx] = nullptr;      // mark as done
        numAdded++;                    // move on
    }

    return newOrder;
}

AstClause* reorderClauseWithSips(sips_t sipsFunction, AstClause* clause) {
    // ignore clauses with fixed execution plans
    if (clause->getExecutionPlan() != nullptr) {
        return nullptr;
    }

    // get the ordering corresponding to the SIPS
    std::vector<unsigned int> newOrdering = applySips(sipsFunction, getBodyLiterals<AstAtom>(*clause));

    // check if we need a change
    bool changeNeeded = false;
    for (unsigned int i = 0; i < newOrdering.size(); i++) {
        if (newOrdering[i] != i) {
            changeNeeded = true;
        }
    }

    // reorder if needed
    return changeNeeded ? reorderAtoms(clause, newOrdering) : nullptr;
}

bool ReorderLiteralsTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;
    AstProgram& program = *translationUnit.getProgram();

    // --- SIPS-based static reordering ---
    // ordering is based on the given SIPS

    // default SIPS to choose is 'all-bound'
    std::string sipsChosen = "all-bound";
    if (Global::config().has("SIPS")) {
        sipsChosen = Global::config().get("SIPS");
    }
    auto sipsFunction = getSipsFunction(sipsChosen);

    // literal reordering is a rule-local transformation
    std::vector<AstClause*> clausesToRemove;

    for (const AstRelation* rel : program.getRelations()) {
        for (AstClause* clause : getClauses(program, *rel)) {
            AstClause* newClause = reorderClauseWithSips(sipsFunction, clause);
            if (newClause != nullptr) {
                // reordering needed - swap around
                clausesToRemove.push_back(clause);
                program.addClause(std::unique_ptr<AstClause>(newClause));
            }
        }
    }

    changed |= !clausesToRemove.empty();
    for (auto* clause : clausesToRemove) {
        program.removeClause(clause);
    }

    // --- profile-guided reordering ---
    if (Global::config().has("profile-use")) {
        // parse supplied profile information
        auto* profileUse = translationUnit.getAnalysis<AstProfileUse>();

        auto profilerSips = [&](std::vector<AstAtom*> atoms, const std::set<std::string>& boundVariables) {
            // Goal: reorder based on the given profiling information
            // Metric: cost(atom_R) = log(|atom_R|) * #free/#args
            //         - exception: propositions are prioritised

            double currOptimalVal = -1;
            unsigned int currOptimalIdx = 0U;
            bool set = false;

            for (unsigned int i = 0; i < atoms.size(); i++) {
                const AstAtom* currAtom = atoms[i];

                if (currAtom == nullptr) {
                    // already processed - move on
                    continue;
                }

                if (isProposition(currAtom)) {
                    // prioritise propositions
                    return i;
                }

                // calculate log(|R|) * #free/#args
                int numBound = numBoundArguments(currAtom, boundVariables);
                int numArgs = currAtom->getArity();
                int numFree = numArgs - numBound;
                double value = log(profileUse->getRelationSize(currAtom->getQualifiedName()));
                value *= (numFree * 1.0) / numArgs;

                if (!set || value < currOptimalVal) {
                    set = true;
                    currOptimalVal = value;
                    currOptimalIdx = i;
                }
            }

            return currOptimalIdx;
        };

        // change the ordering of literals within clauses
        std::vector<AstClause*> clausesToRemove;

        for (const AstRelation* rel : program.getRelations()) {
            for (AstClause* clause : getClauses(program, *rel)) {
                AstClause* newClause = reorderClauseWithSips(profilerSips, clause);
                if (newClause != nullptr) {
                    // reordering needed - swap around
                    clausesToRemove.push_back(clause);
                    program.addClause(std::unique_ptr<AstClause>(newClause));
                }
            }
        }

        changed |= !clausesToRemove.empty();
        for (auto* clause : clausesToRemove) {
            program.removeClause(clause);
        }
    }

    return changed;
}

}  // namespace souffle
