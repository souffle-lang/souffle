/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SipsMetric.h
 *
 * Defines the SipsMetric class, which specifies cost functions for atom orderings in a clause.
 *
 ***********************************************************************/

#pragma once

#include "ast2ram/ClauseTranslator.h"
#include "souffle/utility/Types.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

// define the maximum number of atoms for which the
// SelingerProfileSipsMetric will be applied
// The algorithm is exponential in N.
#ifndef SELINGER_METRIC_CUTOFF
#define SELINGER_METRIC_CUTOFF 16
#endif

namespace souffle::ram {
class Expression;
}  // namespace souffle::ram

namespace souffle::ast::analysis {
class IOTypeAnalysis;
class ProfileUseAnalysis;
class PolymorphicObjectsAnalysis;
class SCCGraphAnalysis;
}  // namespace souffle::ast::analysis
namespace souffle::ast {

class Atom;
class BindingStore;
class Clause;
class Constant;
class Program;
class TranslationUnit;

/**
 * Class for SIPS cost-metric functions
 * Each subclass represents a different heuristic used for evaluating
 * the cost of choosing an atom next in the schedule.
 */
class SipsMetric {
public:
    SipsMetric(const TranslationUnit& tu);

    virtual ~SipsMetric() = default;

    /**
     * Determines the new ordering of a clause after the SIPS is applied.
     * @param clause clause to reorder
     * @return the vector of new positions; v[i] = j iff atom j moves to pos i
     */
    virtual std::vector<std::size_t> getReordering(
            const Clause* clause, const std::vector<std::string>& atomNames) const = 0;

    /** Create a SIPS metric based on a given heuristic. */
    static std::unique_ptr<SipsMetric> create(const std::string& heuristic, const TranslationUnit& tu);

protected:
    const ast::Program& program;
    const ast::analysis::SCCGraphAnalysis* sccGraph = nullptr;
};

class SelingerProfileSipsMetric : public SipsMetric {
public:
    SelingerProfileSipsMetric(const TranslationUnit& tu);
    std::vector<std::size_t> getReordering(
            const Clause* clause, const std::vector<std::string>& atomNames) const override;

    // type aliases
    using AtomIdx = std::size_t;
    using AtomSet = std::set<std::size_t>;
    using ArgIdx = std::size_t;

private:
    /* helper struct for Selinger */
    struct PlanTuplesCost {
        std::vector<std::size_t> plan;
        std::vector<double> tuplesPerIteration;
        std::vector<double> costsPerIteration;
    };

    const ast::analysis::PolymorphicObjectsAnalysis* polyAnalysis = nullptr;
    const ast::analysis::ProfileUseAnalysis* profileUseAnalysis = nullptr;
};

class StaticSipsMetric : public SipsMetric {
public:
    StaticSipsMetric(const TranslationUnit& tu) : SipsMetric(tu) {}

    std::vector<std::size_t> getReordering(
            const Clause* clause, const std::vector<std::string>& atomNames) const override;

protected:
    /**
     * Evaluates the cost of choosing each atom next in the current schedule
     * @param atoms atoms to choose from; may be nullptr
     * @param bindingStore the variables already bound to a value
     */
    virtual std::vector<double> evaluateCosts(const std::vector<Atom*> atoms,
            const BindingStore& bindingStore, const std::vector<std::string>& atomNames) const = 0;
};

/** Goal: Always choose the left-most atom */
class StrictSips : public StaticSipsMetric {
public:
    StrictSips(const TranslationUnit& tu) : StaticSipsMetric(tu) {}

protected:
    std::vector<double> evaluateCosts(const std::vector<Atom*> atoms, const BindingStore& bindingStore,
            const std::vector<std::string>& atomNames) const override;
};

/** Goal: Prioritise atoms with all arguments bound */
class AllBoundSips : public StaticSipsMetric {
public:
    AllBoundSips(const TranslationUnit& tu) : StaticSipsMetric(tu) {}

protected:
    std::vector<double> evaluateCosts(const std::vector<Atom*> atoms, const BindingStore& bindingStore,
            const std::vector<std::string>& atomNames) const override;
};

/** Goal: Prioritise (1) all bound, then (2) atoms with at least one bound argument, then (3) left-most */
class NaiveSips : public StaticSipsMetric {
public:
    NaiveSips(const TranslationUnit& tu) : StaticSipsMetric(tu) {}

protected:
    std::vector<double> evaluateCosts(const std::vector<Atom*> atoms, const BindingStore& bindingStore,
            const std::vector<std::string>& atomNames) const override;
};

/** Goal: prioritise (1) all-bound, then (2) max number of bound vars, then (3) left-most */
class MaxBoundSips : public StaticSipsMetric {
public:
    MaxBoundSips(const TranslationUnit& tu) : StaticSipsMetric(tu) {}

protected:
    std::vector<double> evaluateCosts(const std::vector<Atom*> atoms, const BindingStore& bindingStore,
            const std::vector<std::string>& atomNames) const override;
};

/** Goal: prioritise (1) delta, (2) all-bound, then (3) max number of bound vars, then (4) left-most */
class DeltaMaxBoundSips : public StaticSipsMetric {
public:
    DeltaMaxBoundSips(const TranslationUnit& tu) : StaticSipsMetric(tu) {}

protected:
    std::vector<double> evaluateCosts(const std::vector<Atom*> atoms, const BindingStore& bindingStore,
            const std::vector<std::string>& atomNames) const override;
};

/** Goal: prioritise max ratio of bound args */
class MaxRatioSips : public StaticSipsMetric {
public:
    MaxRatioSips(const TranslationUnit& tu) : StaticSipsMetric(tu) {}

protected:
    std::vector<double> evaluateCosts(const std::vector<Atom*> atoms, const BindingStore& bindingStore,
            const std::vector<std::string>& atomNames) const override;
};

/** Goal: choose the atom with the least number of unbound arguments */
class LeastFreeSips : public StaticSipsMetric {
public:
    LeastFreeSips(const TranslationUnit& tu) : StaticSipsMetric(tu) {}

protected:
    std::vector<double> evaluateCosts(const std::vector<Atom*> atoms, const BindingStore& bindingStore,
            const std::vector<std::string>& atomNames) const override;
};

/** Goal: choose the atom with the least amount of unbound variables */
class LeastFreeVarsSips : public StaticSipsMetric {
public:
    LeastFreeVarsSips(const TranslationUnit& tu) : StaticSipsMetric(tu) {}

protected:
    std::vector<double> evaluateCosts(const std::vector<Atom*> atoms, const BindingStore& bindingStore,
            const std::vector<std::string>& atomNames) const override;
};

/** Goal: prioritise (1) all-bound, then (2) input, and then (3) left-most */
class InputSips : public StaticSipsMetric {
public:
    InputSips(const TranslationUnit& tu);

protected:
    std::vector<double> evaluateCosts(const std::vector<Atom*> atoms, const BindingStore& bindingStore,
            const std::vector<std::string>& atomNames) const override;

private:
    const analysis::IOTypeAnalysis& ioTypes;
};

}  // namespace souffle::ast
