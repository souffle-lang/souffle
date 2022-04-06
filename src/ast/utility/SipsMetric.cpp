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

#include "ast/utility/SipsMetric.h"
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
#include "ast2ram/utility/Utils.h"
#include "ram/Expression.h"
#include "ram/FloatConstant.h"
#include "ram/SignedConstant.h"
#include "ram/StringConstant.h"
#include "ram/UnsignedConstant.h"
#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_set>
#include <vector>

namespace souffle::ast {

std::vector<std::size_t> StaticSipsMetric::getReordering(
        const Clause* clause, std::size_t version, ast2ram::TranslationMode mode) const {
    (void)version;
    (void)mode;

    BindingStore bindingStore(clause);
    auto atoms = getBodyLiterals<Atom>(*clause);
    std::vector<std::size_t> newOrder(atoms.size());

    std::size_t numAdded = 0;
    while (numAdded < atoms.size()) {
        // grab the index of the next atom, based on the SIPS function
        const auto& costs = evaluateCosts(atoms, bindingStore);
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

SelingerProfileSipsMetric::SelingerProfileSipsMetric(const TranslationUnit& tu) {
    profileUseAnalysis = &tu.getAnalysis<ast::analysis::ProfileUseAnalysis>();
    polyAnalysis = &tu.getAnalysis<ast::analysis::PolymorphicObjectsAnalysis>();
    sccGraph = &tu.getAnalysis<ast::analysis::SCCGraphAnalysis>();
    program = &tu.getProgram();
}

std::vector<std::size_t> SelingerProfileSipsMetric::getReordering(
        const Clause* clause, std::size_t version, ast2ram::TranslationMode mode) const {
    auto atoms = ast::getBodyLiterals<ast::Atom>(*clause);

    // remember to exit for single atom bodies
    if (atoms.size() <= 1) {
        std::vector<std::size_t> res;
        res.resize(atoms.size());
        std::iota(res.begin(), res.end(), 0);
        return res;
    }

    auto constraints = ast::getBodyLiterals<ast::BinaryConstraint>(*clause);
    std::size_t relStratum = sccGraph->getSCC(program->getRelation(*clause));
    auto sccRelations = sccGraph->getInternalRelations(relStratum);
    auto sccAtoms = filter(ast::getBodyLiterals<ast::Atom>(*clause),
            [&](auto* atom) { return contains(sccRelations, program->getRelation(*atom)); });

    assert(profileUseAnalysis->hasAutoSchedulerStats() && "Must have stats in order to auto-schedule!");

    auto* prof = profileUseAnalysis;
    auto getRelationSize = [&prof](bool isRecursive, const ast::QualifiedName& rel,
                                   const std::vector<std::size_t>& joinColumns,
                                   const std::map<std::size_t, std::string>& constantsMap) {
        std::set<std::size_t> joinKeys(joinColumns.begin(), joinColumns.end());
        for (auto& [k, _] : constantsMap) {
            joinKeys.insert(k);
        }

        if (joinKeys.empty() && !isRecursive) {
            return prof->getRelationSize(rel);
        }

        std::stringstream ss;
        ss << joinKeys;
        std::string attributes = ss.str();
        attributes[0] = '[';
        attributes[attributes.size() - 1] = ']';

        std::stringstream cc;
        cc << constantsMap;
        std::string constants = cc.str();
        constants[0] = '[';
        constants[constants.size() - 1] = ']';

        if (isRecursive) {
            return prof->getRecursiveUniqueKeys(rel.toString(), attributes, constants);
        }

        return prof->getNonRecursiveUniqueKeys(rel.toString(), attributes, constants);
    };

    using AtomIdx = std::size_t;
    using AtomSet = std::set<std::size_t>;

    AtomSet recursiveInCurrentStratum;

    for (auto* a : sccAtoms) {
        for (AtomIdx i = 0; i < atoms.size(); ++i) {
            if (*atoms[i] == *a) {
                recursiveInCurrentStratum.insert(i);
            }
        }
    }

    using VarName = std::string;
    using VarSet = std::set<VarName>;
    using ArgIdx = std::size_t;

    // map variable name to constants if possible
    std::unordered_map<VarName, ast::Constant*> varToConstant;

    // map variables to necessary variables on other side of the equality
    // i.e. x = y + z we should map x -> { y, z }
    std::unordered_map<VarName, VarSet> varToOtherVars;

    // map variable name to the lower and upper bounds of the inequality
    // i.e. EA < Addr < EA + Size we should map Addr -> { { EA }, { EA, Size } }
    std::unordered_map<VarName, std::pair<VarSet, VarSet>> ineqToUpperLower;

    for (auto* constraint : constraints) {
        auto* lhs = constraint->getLHS();
        auto* rhs = constraint->getRHS();

        if (isIneqConstraint(constraint->getBaseOperator())) {
            if (auto* var = as<ast::Variable>(lhs)) {
                VarSet otherVars;
                visit(rhs, [&](const ast::Variable& v) { otherVars.insert(v.getName()); });
                if (isLessThan(constraint->getBaseOperator()) || isLessEqual(constraint->getBaseOperator())) {
                    ineqToUpperLower[var->getName()].second = otherVars;
                }
                if (isGreaterThan(constraint->getBaseOperator()) ||
                        isGreaterEqual(constraint->getBaseOperator())) {
                    ineqToUpperLower[var->getName()].first = otherVars;
                }
            }

            if (auto* var = as<ast::Variable>(rhs)) {
                VarSet otherVars;
                visit(lhs, [&](const ast::Variable& v) { otherVars.insert(v.getName()); });
                if (isLessThan(constraint->getBaseOperator()) || isLessEqual(constraint->getBaseOperator())) {
                    ineqToUpperLower[var->getName()].first = otherVars;
                }
                if (isGreaterThan(constraint->getBaseOperator()) ||
                        isGreaterEqual(constraint->getBaseOperator())) {
                    ineqToUpperLower[var->getName()].second = otherVars;
                }
            }
        }

        // only consider = constraint
        if (!isEqConstraint(constraint->getBaseOperator())) {
            continue;
        }

        if (isA<ast::Variable>(lhs) && isA<ast::Constant>(rhs)) {
            varToConstant[as<ast::Variable>(lhs)->getName()] = as<ast::Constant>(rhs);
            continue;
        }

        if (isA<ast::Constant>(lhs) && isA<ast::Variable>(rhs)) {
            varToConstant[as<ast::Variable>(rhs)->getName()] = as<ast::Constant>(lhs);
            continue;
        }

        if (auto* var = as<ast::Variable>(lhs)) {
            VarSet otherVars;
            visit(rhs, [&](const ast::Variable& v) { otherVars.insert(v.getName()); });
            varToOtherVars[var->getName()] = otherVars;
            continue;
        }

        if (auto* var = as<ast::Variable>(rhs)) {
            VarSet otherVars;
            visit(lhs, [&](const ast::Variable& v) { otherVars.insert(v.getName()); });
            varToOtherVars[var->getName()] = otherVars;
            continue;
        }
    }

    // check for bounded inequality i.e. EA < EA2 < EA + Size
    for (auto& p : ineqToUpperLower) {
        // consider this like an equality
        auto& [lower, upper] = p.second;
        if (!lower.empty() && !upper.empty() &&
                std::includes(upper.begin(), upper.end(), lower.begin(), lower.end())) {
            varToOtherVars[p.first] = upper;
        }
    }

    std::unordered_map<AtomIdx, VarSet> atomIdxToGroundedVars;
    for (AtomIdx i = 0; i < atoms.size(); ++i) {
        VarSet groundedVars;
        visit(*atoms[i], [&](const ast::Variable& v) { groundedVars.insert(v.getName()); });
        atomIdxToGroundedVars[i] = groundedVars;
    }

    // #atoms -> variables to join -> plan, cost
    std::map<std::size_t, std::map<AtomSet, PlanTuplesCost>> cache;

    std::unordered_map<AtomIdx, std::map<ArgIdx, std::string>> atomToIdxConstants;

    AtomIdx atomIdx = 0;
    for (auto* atom : atoms) {
        std::string name = getClauseAtomName(*clause, atom, sccAtoms, version, mode);
        std::map<ArgIdx, std::string> idxConstant;

        ArgIdx varIdx = 0;
        for (auto* argument : atom->getArguments()) {
            // if we have a variable and a constraint of the form x = 2 then treat x as 2
            if (auto* var = as<ast::Variable>(argument)) {
                if (varToConstant.count(var->getName())) {
                    argument = varToConstant[var->getName()];
                }
            }

            if (auto* constant = as<ast::Constant>(argument)) {
                std::stringstream ss;
                ss << *translateConstant(*constant);
                std::string constantValue = ss.str();
                idxConstant[varIdx] = constantValue;
            }
            ++varIdx;
        }

        atomToIdxConstants[atomIdx] = idxConstant;

        // start by storing the access cost for each individual relation
        std::vector<AtomIdx> empty;
        bool isRecursive = recursiveInCurrentStratum.count(atomIdx) > 0;
        std::size_t tuples = getRelationSize(isRecursive, name, empty, idxConstant);
        double cost = static_cast<double>(tuples * atom->getArity());
        AtomSet singleton = {atomIdx};
        std::vector<AtomIdx> plan = {atomIdx};
        cache[1].insert(std::make_pair(singleton, PlanTuplesCost(plan, tuples, cost)));
        ++atomIdx;
    }

    // do selinger's algorithm
    auto N = atoms.size();
    for (std::size_t K = 2; K <= N; ++K) {
        // for each K sized subset
        for (auto& subset : getSubsets(N, K)) {
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

                // lookup the cost in the cache
                auto& planTuplesCost = cache[K - 1].at(smallerSubset);
                auto& oldPlan = planTuplesCost.plan;
                auto oldTuples = planTuplesCost.tuples;
                auto oldCost = planTuplesCost.cost;

                // compute the grounded variables from the subset
                VarSet groundedVariablesFromSubset;
                for (auto idx : smallerSubset) {
                    auto& varsGroundedByAtom = atomIdxToGroundedVars[idx];
                    groundedVariablesFromSubset.insert(varsGroundedByAtom.begin(), varsGroundedByAtom.end());
                }

                // compute new cost
                AtomIdx atomIdx = subset[i];
                auto* atom = atoms[atomIdx];
                std::vector<ArgIdx> joinColumns;
                const auto& args = atom->getArguments();
                std::size_t numBound = 0;
                for (ArgIdx argIdx = 0; argIdx < args.size(); ++argIdx) {
                    auto* arg = args[argIdx];
                    // if we have a constant or var = constant then we ignore
                    if (atomToIdxConstants[atomIdx].count(argIdx) > 0) {
                        ++numBound;
                        continue;
                    }

                    // unnamed variable i.e. _
                    if (isA<ast::UnnamedVariable>(arg)) {
                        ++numBound;
                        continue;
                    }

                    if (auto* var = as<ast::Variable>(arg)) {
                        // free variable so we can't join on it
                        if (varToOtherVars.count(var->getName()) > 0) {
                            auto& dependentVars = varToOtherVars.at(var->getName());
                            if (std::includes(groundedVariablesFromSubset.begin(),
                                        groundedVariablesFromSubset.end(), dependentVars.begin(),
                                        dependentVars.end())) {
                                joinColumns.push_back(argIdx);
                                ++numBound;
                                continue;
                            }
                        }

                        // direct match on variable
                        if (groundedVariablesFromSubset.count(var->getName()) > 0) {
                            joinColumns.push_back(argIdx);
                            ++numBound;
                            continue;
                        }
                    }
                }

                bool isRecursive = recursiveInCurrentStratum.count(atomIdx) > 0;
                std::vector<ArgIdx> empty;
                double expectedTuples = 0;

                if (numBound == atom->getArity()) {
                    expectedTuples = 1;
                } else {
                    auto relSizeWithConstants = getRelationSize(isRecursive,
                            getClauseAtomName(*clause, atom, sccAtoms, version, mode), empty,
                            atomToIdxConstants[atomIdx]);

                    if (joinColumns.empty()) {
                        expectedTuples = static_cast<double>(relSizeWithConstants);
                    } else {
                        auto uniqueKeys = getRelationSize(isRecursive,
                                getClauseAtomName(*clause, atom, sccAtoms, version, mode), joinColumns,
                                atomToIdxConstants[atomIdx]);

                        bool normalize = (uniqueKeys > 0);
                        expectedTuples =
                                static_cast<double>(relSizeWithConstants) / (normalize ? uniqueKeys : 1);
                    }
                }

                // calculate new number of tuples
                std::size_t newTuples = static_cast<std::size_t>(oldTuples * expectedTuples);

                // calculate new cost
                double newCost = oldCost + newTuples * atom->getArity();

                // calculate new plan
                std::vector<AtomIdx> newPlan(oldPlan.begin(), oldPlan.end());
                newPlan.push_back(atomIdx);

                // if no plan then insert it
                AtomSet currentSet(subset.begin(), subset.end());
                if (cache[K].count(currentSet) == 0) {
                    cache[K].insert(std::make_pair(currentSet, PlanTuplesCost(newPlan, newTuples, newCost)));
                }
                // if we have a lower cost
                else if (cache[K].at(currentSet).cost >= newCost) {
                    cache[K].erase(currentSet);
                    cache[K].insert(std::make_pair(currentSet, PlanTuplesCost(newPlan, newTuples, newCost)));
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

Own<ram::Expression> SelingerProfileSipsMetric::translateConstant(const ast::Constant& constant) const {
    if (auto strConstant = as<ast::StringConstant>(constant)) {
        return mk<ram::StringConstant>(strConstant->getConstant());
    } else if (isA<ast::NilConstant>(&constant)) {
        return mk<ram::SignedConstant>(0);
    } else if (auto* numConstant = as<ast::NumericConstant>(constant)) {
        switch (polyAnalysis->getInferredType(*numConstant)) {
            case ast::NumericConstant::Type::Int:
                return mk<ram::SignedConstant>(RamSignedFromString(numConstant->getConstant(), nullptr, 0));
            case ast::NumericConstant::Type::Uint:
                return mk<ram::UnsignedConstant>(
                        RamUnsignedFromString(numConstant->getConstant(), nullptr, 0));
            case ast::NumericConstant::Type::Float:
                return mk<ram::FloatConstant>(RamFloatFromString(numConstant->getConstant()));
        }
    }
    fatal("unaccounted-for constant");
}

std::string SelingerProfileSipsMetric::getClauseAtomName(const ast::Clause& clause, const ast::Atom* atom,
        const std::vector<ast::Atom*>& sccAtoms, std::size_t version, ast2ram::TranslationMode mode) const {
    using namespace souffle::ast2ram;

    bool isRecursive = !sccAtoms.empty();

    if (isA<ast::SubsumptiveClause>(clause)) {
        // find the dominated / dominating heads
        const auto& body = clause.getBodyLiterals();
        auto dominatedHeadAtom = dynamic_cast<const ast::Atom*>(body[0]);
        auto dominatingHeadAtom = dynamic_cast<const ast::Atom*>(body[1]);

        if (clause.getHead() == atom) {
            if (mode == SubsumeDeleteCurrentDelta || mode == SubsumeDeleteCurrentCurrent) {
                return getDeleteRelationName(atom->getQualifiedName());
            }
            return getRejectRelationName(atom->getQualifiedName());
        }

        if (dominatedHeadAtom == atom) {
            if (mode == SubsumeDeleteCurrentDelta || mode == SubsumeDeleteCurrentCurrent) {
                return getConcreteRelationName(atom->getQualifiedName());
            }
            return getNewRelationName(atom->getQualifiedName());
        }

        if (dominatingHeadAtom == atom) {
            switch (mode) {
                case SubsumeRejectNewCurrent:
                case SubsumeDeleteCurrentCurrent: return getConcreteRelationName(atom->getQualifiedName());
                case SubsumeDeleteCurrentDelta: return getDeltaRelationName(atom->getQualifiedName());
                default: return getNewRelationName(atom->getQualifiedName());
            }
        }

        if (isRecursive) {
            if (sccAtoms.at(version + 1) == atom) {
                return getDeltaRelationName(atom->getQualifiedName());
            }
        }
    }

    if (!isRecursive) {
        return getConcreteRelationName(atom->getQualifiedName());
    }
    if (clause.getHead() == atom) {
        return getNewRelationName(atom->getQualifiedName());
    }
    if (sccAtoms.at(version) == atom) {
        return getDeltaRelationName(atom->getQualifiedName());
    }
    return getConcreteRelationName(atom->getQualifiedName());
}

const ast::PowerSet& SelingerProfileSipsMetric::getSubsets(std::size_t N, std::size_t K) const {
    if (cache.count({N, K})) {
        return cache.at({N, K});
    }
    // this powerset represents all possible subsets of cardinality K of the set {1,...,N}
    ast::PowerSet res;

    // generate the next permutation of the bitmask
    std::vector<std::size_t> cur;
    cur.reserve(K);

    // use bitmask for subset generation
    std::string bitmask(K, 1);  // K leading 1's
    bitmask.resize(N, 0);       // N-K trailing 0's

    // generate the combination while there are combinations to go
    do {
        cur.clear();

        // construct the subset using the set bits in the bitmask
        for (std::size_t i = 0; i < N; ++i)  // [0..N-1] integers
        {
            if (bitmask[i]) {
                cur.push_back(i);
            }
        }
        res.push_back(cur);
    } while (std::prev_permutation(bitmask.begin(), bitmask.end()));

    cache[std::make_pair(N, K)] = res;
    return cache.at({N, K});
}

/** Create a SIPS metric based on a given heuristic. */
std::unique_ptr<SipsMetric> SipsMetric::create(const std::string& heuristic, const TranslationUnit& tu) {
    if (Global::config().has("auto-schedule")) {
        return mk<SelingerProfileSipsMetric>(tu);
    } else if (heuristic == "strict")
        return mk<StrictSips>();
    else if (heuristic == "all-bound")
        return mk<AllBoundSips>();
    else if (heuristic == "naive")
        return mk<NaiveSips>();
    else if (heuristic == "max-bound")
        return mk<MaxBoundSips>();
    else if (heuristic == "max-ratio")
        return mk<MaxRatioSips>();
    else if (heuristic == "least-free")
        return mk<LeastFreeSips>();
    else if (heuristic == "least-free-vars")
        return mk<LeastFreeVarsSips>();
    else if (heuristic == "input")
        return mk<InputSips>(tu.getProgram(), tu.getAnalysis<analysis::IOTypeAnalysis>());

    // default is all-bound
    return create("all-bound", tu);
}

std::vector<double> StrictSips::evaluateCosts(
        const std::vector<Atom*> atoms, const BindingStore& /* bindingStore */) const {
    // Goal: Always choose the left-most atom
    std::vector<double> cost;
    for (const auto* atom : atoms) {
        cost.push_back(atom == nullptr ? std::numeric_limits<double>::max() : 0);
    }
    assert(atoms.size() == cost.size() && "each atom should have exactly one cost");
    return cost;
}

std::vector<double> AllBoundSips::evaluateCosts(
        const std::vector<Atom*> atoms, const BindingStore& bindingStore) const {
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

std::vector<double> NaiveSips::evaluateCosts(
        const std::vector<Atom*> atoms, const BindingStore& bindingStore) const {
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

std::vector<double> MaxBoundSips::evaluateCosts(
        const std::vector<Atom*> atoms, const BindingStore& bindingStore) const {
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

std::vector<double> MaxRatioSips::evaluateCosts(
        const std::vector<Atom*> atoms, const BindingStore& bindingStore) const {
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

std::vector<double> LeastFreeSips::evaluateCosts(
        const std::vector<Atom*> atoms, const BindingStore& bindingStore) const {
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

std::vector<double> LeastFreeVarsSips::evaluateCosts(
        const std::vector<Atom*> atoms, const BindingStore& bindingStore) const {
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

std::vector<double> InputSips::evaluateCosts(
        const std::vector<Atom*> atoms, const BindingStore& bindingStore) const {
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

}  // namespace souffle::ast
