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

#include <memory>
#include <string>
#include <vector>

namespace souffle::ast::analysis {
class IOTypeAnalysis;
class ProfileUseAnalysis;
}  // namespace souffle::ast::analysis
namespace souffle::ast {

class Atom;
class BindingStore;
class Clause;
class Program;
class TranslationUnit;

/**
 * Class for SIPS cost-metric functions
 * Each subclass represents a different heuristic used for evaluating
 * the cost of choosing an atom next in the schedule.
 */
class SipsMetric {
public:
    virtual ~SipsMetric() = default;

    /**
     * Determines the new ordering of a clause after the SIPS is applied.
     * @param clause clause to reorder
     * @return the vector of new positions; v[i] = j iff atom j moves to pos i
     */
    std::vector<std::size_t> getReordering(const Clause* clause) const;

    /** Create a SIPS metric based on a given heuristic. */
    static std::unique_ptr<SipsMetric> create(const std::string& heuristic, const TranslationUnit& tu);

protected:
    /**
     * Evaluates the cost of choosing each atom next in the current schedule
     * @param atoms atoms to choose from; may be nullptr
     * @param bindingStore the variables already bound to a value
     */
    virtual std::vector<double> evaluateCosts(
            const std::vector<Atom*> atoms, const BindingStore& bindingStore) const = 0;
};

/** Goal: Always choose the left-most atom */
class StrictSips : public SipsMetric {
public:
    StrictSips() = default;

protected:
    std::vector<double> evaluateCosts(
            const std::vector<Atom*> atoms, const BindingStore& bindingStore) const override;
};

/** Goal: Prioritise atoms with all arguments bound */
class AllBoundSips : public SipsMetric {
public:
    AllBoundSips() = default;

protected:
    std::vector<double> evaluateCosts(
            const std::vector<Atom*> atoms, const BindingStore& bindingStore) const override;
};

/** Goal: Prioritise (1) all bound, then (2) atoms with at least one bound argument, then (3) left-most */
class NaiveSips : public SipsMetric {
public:
    NaiveSips() = default;

protected:
    std::vector<double> evaluateCosts(
            const std::vector<Atom*> atoms, const BindingStore& bindingStore) const override;
};

/** Goal: prioritise (1) all-bound, then (2) max number of bound vars, then (3) left-most */
class MaxBoundSips : public SipsMetric {
public:
    MaxBoundSips() = default;

protected:
    std::vector<double> evaluateCosts(
            const std::vector<Atom*> atoms, const BindingStore& bindingStore) const override;
};

/** Goal: prioritise max ratio of bound args */
class MaxRatioSips : public SipsMetric {
public:
    MaxRatioSips() = default;

protected:
    std::vector<double> evaluateCosts(
            const std::vector<Atom*> atoms, const BindingStore& bindingStore) const override;
};

/** Goal: choose the atom with the least number of unbound arguments */
class LeastFreeSips : public SipsMetric {
public:
    LeastFreeSips() = default;

protected:
    std::vector<double> evaluateCosts(
            const std::vector<Atom*> atoms, const BindingStore& bindingStore) const override;
};

/** Goal: choose the atom with the least amount of unbound variables */
class LeastFreeVarsSips : public SipsMetric {
public:
    LeastFreeVarsSips() = default;

protected:
    std::vector<double> evaluateCosts(
            const std::vector<Atom*> atoms, const BindingStore& bindingStore) const override;
};

/**
 * Goal: reorder based on the given profiling information
 * Metric: cost(atom_R) = log(|atom_R|) * #free/#args
 *         - exception: propositions are prioritised
 */
class ProfileUseSips : public SipsMetric {
public:
    ProfileUseSips(const analysis::ProfileUseAnalysis& profileUse) : profileUse(profileUse) {}

protected:
    std::vector<double> evaluateCosts(
            const std::vector<Atom*> atoms, const BindingStore& bindingStore) const override;

private:
    const analysis::ProfileUseAnalysis& profileUse;
};

/** Goal: prioritise (1) all-bound, then (2) input, and then (3) left-most */
class InputSips : public SipsMetric {
public:
    InputSips(const Program& program, const analysis::IOTypeAnalysis& ioTypes)
            : program(program), ioTypes(ioTypes) {}

protected:
    std::vector<double> evaluateCosts(
            const std::vector<Atom*> atoms, const BindingStore& bindingStore) const override;

private:
    const Program& program;
    const analysis::IOTypeAnalysis& ioTypes;
};

}  // namespace souffle::ast
