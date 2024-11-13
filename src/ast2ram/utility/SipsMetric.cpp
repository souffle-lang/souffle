/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SipsMetric.cpp
 *
 * Defines the SipsMetric class, which specifies cost functions for atom orderings in a clause.
 *
 ***********************************************************************/

#include "ast2ram/utility/SipsMetric.h"
#include "Global.h"
#include "ast/Clause.h"
#include "ast/TranslationUnit.h"
#include "ast/Variable.h"
#include "ast/analysis/IOType.h"
#include "ast/analysis/ProfileUse.h"
#include "ast/analysis/SCCGraph.h"
#include "ast/analysis/typesystem/PolymorphicObjects.h"
#include "ast/utility/BindingStore.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "ast2ram/utility/SipGraph.h"
#include "ast2ram/utility/Utils.h"
#include "ram/Expression.h"
#include "ram/FloatConstant.h"
#include "ram/SignedConstant.h"
#include "ram/StringConstant.h"
#include "ram/UnsignedConstant.h"
#include "souffle/utility/SubsetCache.h"
#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_set>
#include <vector>

namespace souffle::ast {

SipsMetric::SipsMetric(const TranslationUnit& tu) : program(tu.getProgram()) {
    sccGraph = &tu.getAnalysis<ast::analysis::SCCGraphAnalysis>();
}

std::vector<std::size_t> StaticSipsMetric::getReordering(
        const Clause* clause, const std::vector<std::string>& atomNames) const {
    std::size_t relStratum = sccGraph->getSCC(program.getRelation(*clause));
    auto sccRelations = sccGraph->getInternalRelations(relStratum);

    auto sccAtoms = filter(ast::getBodyLiterals<ast::Atom>(*clause),
            [&](auto* atom) { return contains(sccRelations, program.getRelation(*atom)); });

    BindingStore bindingStore(clause);
    auto atoms = getBodyLiterals<Atom>(*clause);
    std::vector<std::size_t> newOrder(atoms.size());

    std::size_t numAdded = 0;
    while (numAdded < atoms.size()) {
        // grab the index of the next atom, based on the SIPS function
        const auto& costs = evaluateCosts(atoms, bindingStore, atomNames);
        assert(atoms.size() == costs.size() && "each atom should have exactly one cost");
        std::size_t minIdx = static_cast<std::size_t>(
                std::distance(costs.begin(), std::min_element(costs.begin(), costs.end())));
        const auto* nextAtom = atoms[minIdx];
        assert(nextAtom != nullptr && "nullptr atoms should have maximal cost");

        // set all arguments that are variables as bound
        for (const auto* arg : nextAtom->getArguments()) {
            if (const auto* var = as<Variable>(arg)) {
                bindingStore.bindVariableStrongly(var->getName());
            }
        }

        newOrder[numAdded] = minIdx;  // add to the ordering
        atoms[minIdx] = nullptr;      // mark as done
        numAdded++;                   // move on
    }

    return newOrder;
}

SelingerProfileSipsMetric::SelingerProfileSipsMetric(const TranslationUnit& tu) : SipsMetric(tu) {
    profileUseAnalysis = &tu.getAnalysis<ast::analysis::ProfileUseAnalysis>();
    polyAnalysis = &tu.getAnalysis<ast::analysis::PolymorphicObjectsAnalysis>();
}

std::vector<std::size_t> SelingerProfileSipsMetric::getReordering(
        const Clause* clause, const std::vector<std::string>& atomNames) const {
    auto atoms = ast::getBodyLiterals<ast::Atom>(*clause);

    // remember to exit for single atom bodies
    if (atoms.size() <= 1) {
        std::vector<std::size_t> res;
        res.resize(atoms.size());
        std::iota(res.begin(), res.end(), 0);
        return res;
    }

    // create ast constant translator
    auto* poly = polyAnalysis;
    auto astConstantTranslator = [poly](const ast::Constant& constant) -> Own<ram::Expression> {
        if (auto strConstant = as<ast::StringConstant>(constant)) {
            return mk<ram::StringConstant>(strConstant->getConstant());
        } else if (isA<ast::NilConstant>(&constant)) {
            return mk<ram::SignedConstant>(0);
        } else if (auto* numConstant = as<ast::NumericConstant>(constant)) {
            switch (poly->getInferredType(*numConstant)) {
                case ast::NumericConstant::Type::Int:
                    return mk<ram::SignedConstant>(
                            RamSignedFromString(numConstant->getConstant(), nullptr, 0));
                case ast::NumericConstant::Type::Uint:
                    return mk<ram::UnsignedConstant>(
                            RamUnsignedFromString(numConstant->getConstant(), nullptr, 0));
                case ast::NumericConstant::Type::Float:
                    return mk<ram::FloatConstant>(RamFloatFromString(numConstant->getConstant()));
            }
        }
        fatal("unaccounted-for constant");
        return nullptr;
    };

    SipGraph sipGraph(clause, astConstantTranslator);

    auto constraints = ast::getBodyLiterals<ast::BinaryConstraint>(*clause);
    std::size_t relStratum = sccGraph->getSCC(program.getRelation(*clause));
    auto sccRelations = sccGraph->getInternalRelations(relStratum);
    auto sccAtoms = filter(ast::getBodyLiterals<ast::Atom>(*clause),
            [&](auto* atom) { return contains(sccRelations, program.getRelation(*atom)); });

    assert(profileUseAnalysis->hasAutoSchedulerStats() && "Must have stats in order to auto-schedule!");

    auto* prof = profileUseAnalysis;
    auto getJoinSize = [&prof](bool isRecursive, const std::string& rel, std::set<std::size_t> joinKeys,
                               const std::map<std::size_t, const ram::Expression*>& constantsMap,
                               const std::string& iteration) {
        std::map<std::size_t, std::string> constantsStringMap;
        for (auto& [k, v] : constantsMap) {
            joinKeys.insert(k);
            std::stringstream ss;
            ss << *v;
            constantsStringMap.insert(std::make_pair(k, ss.str()));
        }

        if (joinKeys.empty() && !isRecursive) {
            return static_cast<double>(prof->getRelationSize(QualifiedName::fromString(rel)));
        }

        std::stringstream ss;
        ss << joinKeys;
        std::string attributes = ss.str();

        std::stringstream cc;
        cc << constantsStringMap;
        std::string constants = cc.str();

        try {
            if (isRecursive) {
                return prof->getRecursiveJoinSize(rel, attributes, constants, iteration);
            } else {
                return prof->getNonRecursiveJoinSize(rel, attributes, constants);
            }
        } catch (...) {
            fatal("Error: profile used for auto-scheduling doesn't match the provided program.");
        }
    };

    AtomSet recursiveInCurrentStratum;
    for (auto* a : sccAtoms) {
        for (AtomIdx i = 0; i < atoms.size(); ++i) {
            if (*atoms[i] == *a) {
                recursiveInCurrentStratum.insert(i);
            }
        }
    }

    // #atoms -> variables to join -> plan, cost
    std::map<std::size_t, std::map<AtomSet, PlanTuplesCost>> cache;

    // get number of iterations of rule
    std::size_t iterations = 1;
    for (std::size_t i = 0; i < atoms.size(); ++i) {
        std::string name = atomNames[i];
        bool isRecursive = recursiveInCurrentStratum.count(i) > 0;
        if (isRecursive) {
            iterations = prof->getIterations(name);
            break;
        }
    }

    // store the access cost for each individual relation
    AtomIdx atomIdx = 0;
    for (auto* atom : atoms) {
        std::string name = atomNames[atomIdx];
        bool isRecursive = recursiveInCurrentStratum.count(atomIdx) > 0;
        AtomSet singleton = {atomIdx};
        std::vector<AtomIdx> plan = {atomIdx};
        PlanTuplesCost p;
        p.plan = plan;
        for (std::size_t iter = 0; iter < iterations; ++iter) {
            double tuples =
                    getJoinSize(isRecursive, name, {}, sipGraph.getConstantsMap(atom), std::to_string(iter));
            double cost = static_cast<double>(tuples * atom->getArity());
            p.tuplesPerIteration.push_back(tuples);
            p.costsPerIteration.push_back(cost);
        }
        cache[1].insert(std::make_pair(singleton, p));
        ++atomIdx;
    }

    // do selinger's algorithm
    SubsetCache subsetCache;
    auto N = atoms.size();
    for (std::size_t K = 2; K <= N; ++K) {
        // for each K sized subset
        for (auto& subset : subsetCache.getSubsets(N, K)) {
            // remove an entry from the subset
            for (AtomIdx i = 0; i < subset.size(); ++i) {
                // construct the set S \ S[i]
                AtomSet smallerSubset;
                for (AtomIdx j = 0; j < subset.size(); ++j) {
                    if (i == j) {
                        continue;
                    }
                    smallerSubset.insert(subset[j]);
                }

                AtomIdx atomIdx = subset[i];
                auto* to = atoms[atomIdx];
                auto constantsMap = sipGraph.getConstantsMap(to);
                auto unnamedIndices = sipGraph.getUnnamedIndices(to);

                std::set<const Atom*> subsetAtoms;
                for (auto idx : smallerSubset) {
                    subsetAtoms.insert(atoms[idx]);
                }

                auto joinColumns = sipGraph.getBoundIndices(subsetAtoms, to);

                std::size_t numBound = constantsMap.size() + joinColumns.size() + unnamedIndices.size();

                // lookup the cost in the cache
                auto& planTuplesCost = cache[K - 1].at(smallerSubset);
                auto& oldPlan = planTuplesCost.plan;
                auto oldTuples = planTuplesCost.tuplesPerIteration;
                auto oldCost = planTuplesCost.costsPerIteration;

                PlanTuplesCost p;
                bool isRecursive = recursiveInCurrentStratum.count(atomIdx) > 0;
                std::vector<ArgIdx> empty;
                double expectedTuples = 0;
                double newTotalCost = 0.0;
                for (std::size_t iter = 0; iter < iterations; ++iter) {
                    if (numBound == to->getArity()) {
                        expectedTuples = 1;
                    } else {
                        // get the join size from the profile
                        expectedTuples = getJoinSize(isRecursive, atomNames[atomIdx], joinColumns,
                                constantsMap, std::to_string(iter));
                    }

                    // calculate new number of tuples
                    double newTuples = oldTuples[iter] * expectedTuples;

                    // calculate new cost
                    double newCost = oldCost[iter] + newTuples * to->getArity();

                    // add to vector of costs/tuples
                    p.tuplesPerIteration.push_back(newTuples);
                    p.costsPerIteration.push_back(newCost);
                    newTotalCost += newCost;
                }

                // calculate new plan
                std::vector<AtomIdx> newPlan(oldPlan.begin(), oldPlan.end());
                newPlan.push_back(atomIdx);
                p.plan = newPlan;

                // if no plan then insert it
                AtomSet currentSet(subset.begin(), subset.end());
                if (cache[K].count(currentSet) == 0) {
                    cache[K].insert(std::make_pair(currentSet, p));
                } else {
                    // if we have a lower cost
                    auto& costVector = cache[K].at(currentSet).costsPerIteration;
                    double oldTotalCost = std::accumulate(costVector.begin(), costVector.end(), 0.0);
                    if (oldTotalCost >= newTotalCost) {
                        cache[K].erase(currentSet);
                        cache[K].insert(std::make_pair(currentSet, p));
                    }
                }
            }
        }
    }

    std::vector<AtomIdx> newOrder;
    assert(cache[N].size() == 1);
    auto& bestPlanTuplesCost = cache[N].begin()->second;
    auto& bestPlan = bestPlanTuplesCost.plan;
    for (AtomIdx elem : bestPlan) {
        newOrder.push_back(elem);
    }

    return newOrder;
}

/** Create a SIPS metric based on a given heuristic. */
std::unique_ptr<SipsMetric> SipsMetric::create(const std::string& heuristic, const TranslationUnit& tu) {
    if (tu.global().config().has("auto-schedule")) {
        if (tu.getAnalysis<ast::analysis::ProfileUseAnalysis>().hasAutoSchedulerStats()) {
            return mk<SelingerProfileSipsMetric>(tu);
        } else {
            std::cerr << "WARNING: `--auto-schedule` cannot be used due to missing scheduler stats; falling "
                         "back "
                         "to heuristic '"
                      << heuristic << "'" << ::std::endl;
        }
    }
    if (heuristic == "strict")
        return mk<StrictSips>(tu);
    else if (heuristic == "all-bound")
        return mk<AllBoundSips>(tu);
    else if (heuristic == "naive")
        return mk<NaiveSips>(tu);
    else if (heuristic == "max-bound")
        return mk<MaxBoundSips>(tu);
    else if (heuristic == "delta-max-bound")
        return mk<DeltaMaxBoundSips>(tu);
    else if (heuristic == "max-ratio")
        return mk<MaxRatioSips>(tu);
    else if (heuristic == "least-free")
        return mk<LeastFreeSips>(tu);
    else if (heuristic == "least-free-vars")
        return mk<LeastFreeVarsSips>(tu);
    else if (heuristic == "input")
        return mk<InputSips>(tu);

    // default is all-bound
    return create("all-bound", tu);
}

std::vector<double> StrictSips::evaluateCosts(const std::vector<Atom*> atoms,
        const BindingStore& /* bindingStore */, const std::vector<std::string>& /*atomNames*/) const {
    // Goal: Always choose the left-most atom
    std::vector<double> cost;
    for (const auto* atom : atoms) {
        cost.push_back(atom == nullptr ? std::numeric_limits<double>::max() : 0);
    }
    assert(atoms.size() == cost.size() && "each atom should have exactly one cost");
    return cost;
}

std::vector<double> AllBoundSips::evaluateCosts(const std::vector<Atom*> atoms,
        const BindingStore& bindingStore, const std::vector<std::string>& /*atomNames*/) const {
    // Goal: Prioritise atoms with all arguments bound
    std::vector<double> cost;
    for (const auto* atom : atoms) {
        if (atom == nullptr) {
            cost.push_back(std::numeric_limits<double>::max());
            continue;
        }

        std::size_t arity = atom->getArity();
        std::size_t numBound = bindingStore.numBoundArguments(atom);
        cost.push_back(arity == numBound ? 0 : 1);
    }
    assert(atoms.size() == cost.size() && "each atom should have exactly one cost");
    return cost;
}

std::vector<double> NaiveSips::evaluateCosts(const std::vector<Atom*> atoms, const BindingStore& bindingStore,
        const std::vector<std::string>& /*atomNames*/) const {
    // Goal: Prioritise (1) all bound, then (2) atoms with at least one bound argument, then (3) left-most
    std::vector<double> cost;
    for (const auto* atom : atoms) {
        if (atom == nullptr) {
            cost.push_back(std::numeric_limits<double>::max());
            continue;
        }

        std::size_t arity = atom->getArity();
        std::size_t numBound = bindingStore.numBoundArguments(atom);
        if (arity == numBound) {
            cost.push_back(0);
        } else if (numBound >= 1) {
            cost.push_back(1);
        } else {
            cost.push_back(2);
        }
    }
    assert(atoms.size() == cost.size() && "each atom should have exactly one cost");
    return cost;
}

std::vector<double> MaxBoundSips::evaluateCosts(const std::vector<Atom*> atoms,
        const BindingStore& bindingStore, const std::vector<std::string>& /*atomNames*/) const {
    // Goal: prioritise (1) all-bound, then (2) max number of bound vars, then (3) left-most
    std::vector<double> cost;
    for (const auto* atom : atoms) {
        if (atom == nullptr) {
            cost.push_back(std::numeric_limits<double>::max());
            continue;
        }

        std::size_t arity = atom->getArity();
        std::size_t numBound = bindingStore.numBoundArguments(atom);
        if (arity == numBound) {
            // Always better than anything else
            cost.push_back(0);
        } else if (numBound == 0) {
            // Always worse than any number of bound vars
            cost.push_back(2);
        } else {
            // Between 0 and 1, decreasing with more num bound
            cost.push_back(1.0 / numBound);
        }
    }
    assert(atoms.size() == cost.size() && "each atom should have exactly one cost");
    return cost;
}

std::vector<double> MaxRatioSips::evaluateCosts(const std::vector<Atom*> atoms,
        const BindingStore& bindingStore, const std::vector<std::string>& /*atomNames*/) const {
    // Goal: prioritise max ratio of bound args
    std::vector<double> cost;
    for (const auto* atom : atoms) {
        if (atom == nullptr) {
            cost.push_back(std::numeric_limits<double>::max());
            continue;
        }

        std::size_t arity = atom->getArity();
        std::size_t numBound = bindingStore.numBoundArguments(atom);
        if (arity == 0) {
            // Always better than anything else
            cost.push_back(0);
        } else if (numBound == 0) {
            // Always worse than anything else
            cost.push_back(2);
        } else {
            // Between 0 and 1, decreasing as the ratio increases
            cost.push_back(1.0 - numBound / arity);
        }
    }
    assert(atoms.size() == cost.size() && "each atom should have exactly one cost");
    return cost;
}

std::vector<double> LeastFreeSips::evaluateCosts(const std::vector<Atom*> atoms,
        const BindingStore& bindingStore, const std::vector<std::string>& /*atomNames*/) const {
    // Goal: choose the atom with the least number of unbound arguments
    std::vector<double> cost;
    for (const auto* atom : atoms) {
        if (atom == nullptr) {
            cost.push_back(std::numeric_limits<double>::max());
            continue;
        }

        cost.push_back((double)(atom->getArity() - bindingStore.numBoundArguments(atom)));
    }
    return cost;
}

std::vector<double> LeastFreeVarsSips::evaluateCosts(const std::vector<Atom*> atoms,
        const BindingStore& bindingStore, const std::vector<std::string>& /*atomNames*/) const {
    // Goal: choose the atom with the least amount of unbound variables
    std::vector<double> cost;
    for (const auto* atom : atoms) {
        if (atom == nullptr) {
            cost.push_back(std::numeric_limits<double>::max());
            continue;
        }

        // use a set to hold all free variables to avoid double-counting
        std::set<std::string> freeVars;
        visit(*atom, [&](const Variable& var) {
            if (bindingStore.isBound(var.getName())) {
                freeVars.insert(var.getName());
            }
        });
        cost.push_back((double)freeVars.size());
    }
    return cost;
}

InputSips::InputSips(const TranslationUnit& tu)
        : StaticSipsMetric(tu), ioTypes(tu.getAnalysis<analysis::IOTypeAnalysis>()) {}

std::vector<double> InputSips::evaluateCosts(const std::vector<Atom*> atoms, const BindingStore& bindingStore,
        const std::vector<std::string>& /*atomNames*/) const {
    // Goal: prioritise (1) all-bound, (2) input, then (3) rest
    std::vector<double> cost;
    for (const auto* atom : atoms) {
        if (atom == nullptr) {
            cost.push_back(std::numeric_limits<double>::max());
            continue;
        }

        const auto& relName = atom->getQualifiedName();
        std::size_t arity = atom->getArity();
        std::size_t numBound = bindingStore.numBoundArguments(atom);
        if (arity == numBound) {
            // prioritise all-bound
            cost.push_back(0);
        } else if (ioTypes.isInput(program.getRelation(relName))) {
            // then input
            cost.push_back(1);
        } else {
            cost.push_back(2);
        }
    }
    return cost;
}

std::vector<double> DeltaMaxBoundSips::evaluateCosts(const std::vector<Atom*> atoms,
        const BindingStore& bindingStore, const std::vector<std::string>& atomNames) const {
    std::vector<double> cost;
    for (std::size_t i = 0; i < atoms.size(); ++i) {
        const auto* atom = atoms[i];
        if (atom == nullptr) {
            cost.push_back(std::numeric_limits<double>::max());
            continue;
        }

        std::size_t arity = atom->getArity();
        std::size_t numBound = bindingStore.numBoundArguments(atom);
        if (arity == numBound) {
            // Always better than anything else
            cost.push_back(0.0);
        } else if (isPrefix("@delta_", atomNames[i])) {
            // Better than any other atom that is not fully bounded
            cost.push_back(1.0);
        } else if (numBound == 0) {
            // Always worse than any number of bound vars
            cost.push_back(4.0);
        } else {
            // Between 2 and 3, decreasing with more num bound
            cost.push_back(2.0 + (1.0 / (double)numBound));
        }
    }
    assert(atoms.size() == cost.size() && "each atom should have exactly one cost");
    return cost;
}

}  // namespace souffle::ast
