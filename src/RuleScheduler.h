/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file QueryOptimizer.h
 *
 * A set of generic utilities to optimize queries.
 *
 ***********************************************************************/

#pragma once

#include "RamRelationStats.h"
#include "Util.h"

#include <cmath>
#include <vector>

namespace souffle {

namespace scheduler {

using namespace std;

// ######################################################################
//                        Scheduler Framework
// ######################################################################

/** The type utilized to reference variables */
typedef int Var;

/**
 * A type to model atom arguments within scheduling problems.
 */
class Argument {
    /** The type of argument */
    enum Kind {
        Variable,    // < Named variables, require to expose and ID
        Constant,    // < constants, no ID
        UnnamedVar,  // < underscore, no ID
        Other        // < others, no ID
    };

    /** The kind of argument represented */
    Kind kind;

    /** The ID of the represented variable or 0 if not a variable */
    Var id;

    /** A private constructor to be utilized by factories */
    Argument(Kind kind, Var id = 0) : kind(kind), id(id) {}

public:
    /** A factory creating a variable */
    static Argument createVar(Var id) {
        return Argument(Variable, id);
    }

    /** A factory creating a constant */
    static Argument createConst() {
        return Argument(Constant);
    }

    /** A factory creating an underscore */
    static Argument createUnderscore() {
        return Argument(UnnamedVar);
    }

    /** A factory creating an unknown parameter */
    static Argument createOther() {
        return Argument(Other);
    }

    /** An implicit converter to variables for easy of use */
    operator Var() const {
        assert(isVariable());
        return id;
    }

    /** Determines whether this is a variable */
    bool isVariable() const {
        return kind == Variable;
    }

    /** Determines whether this is a constant */
    bool isConstant() const {
        return kind == Constant;
    }

    /** Determines whether this is an underscore */
    bool isUnderscore() const {
        return kind == UnnamedVar;
    }

    /** Adds support for equality checks for arguments */
    bool operator==(const Argument& other) const {
        return kind == other.kind && id == other.id;
    }

    /** Adds support for inequality checks for arguments */
    bool operator!=(const Argument& other) const {
        return !(*this == other);
    }

    void print(std::ostream& out) const {
        if (isVariable()) {
            out << id;
        } else if (isConstant()) {
            out << "c";
        } else if (isUnderscore()) {
            out << "_";
        } else {
            out << "?";
        }
    }

    /** Enables arguments to be printed */
    friend std::ostream& operator<<(std::ostream& out, const Argument& arg) {
        arg.print(out);
        return out;
    }
};

/**
 * The base class for atoms to model the input of scheduling problems.
 */
struct Atom {
    /** An ID of this atom, making it unique. */
    int id;

    /** The list of arguments of this atom */
    std::vector<Argument> args;

public:
    Atom(int id, const std::vector<Argument>& args) : id(id), args(args) {}

    virtual ~Atom() = default;

    /** Obtains the identifier of this atom */
    int getID() const {
        return id;
    }

    /** Obtains the list of (abstracted) arguments exposed by this atom. */
    const std::vector<Argument>& getArguments() const {
        return args;
    }

    /** Obtains the number of constant arguments */
    unsigned getNumConstants() const {
        unsigned count = 0;
        for (const auto& cur : args) {
            if (cur.isConstant()) {
                count++;
            }
        }
        return count;
    }

    /** Obtains a list of the variable arguments */
    std::vector<Var> getVariables() const {
        std::vector<Var> res;
        for (const Argument& cur : args) {
            if (cur.isVariable()) {
                Var v = cur;
                if (!contains(res, v)) {
                    res.push_back(v);
                }
            }
        }
        return res;
    }

    /** Obtains the arity of the targeted relation. */
    unsigned getArity() const {
        return args.size();
    }

    bool operator==(const Atom& other) const {
        return id == other.id;
    }

    bool operator<(const Atom& other) const {
        return id < other.id;
    }

    virtual void print(std::ostream& out) const {
        out << "<" << id << ">"
            << "( " << args << " )";
    }

    friend std::ostream& operator<<(std::ostream& out, const Atom& atom) {
        atom.print(out);
        return out;
    }
};

/**
 * The class summarizing a scheduling problem to be solved
 * utilizing a given cost model.
 */
template <typename CostModel>
class Problem {
public:
    /** The type of atom to be utilized */
    typedef typename CostModel::atom_type atom_type;

    /** The type of state to be associated to execution plans */
    typedef typename CostModel::state_type state_type;

    /** The type of an execution plan */
    typedef typename std::vector<atom_type> plan;

private:
    /** The atoms to be scheduled */
    std::vector<atom_type> atoms;

public:
    /** A constructor for scheduling problems */
    Problem(const std::vector<atom_type>& atoms = std::vector<atom_type>()) : atoms(atoms) {}

    /** Adds another atom to the list of atoms to be scheduled */
    void addAtom(const atom_type& atom) {
        atoms.push_back(atom);
    }

    /** Obtains the list of atoms to be scheduled */
    const std::vector<atom_type>& getAtoms() const {
        return atoms;
    }

    /**
     * Obtains a cost-optimal schedule of the atoms to be scheduled
     * based on the underlying cost model.
     */
    plan solve(bool debug = false) const {
        // create an instance of the cost model
        CostModel model;

        // shortcut
        if (atoms.size() < 2) {
            return atoms;  // not much to plan there
        }

        // print some debugging
        if (debug) {
            std::cout << "Processing Problem: " << atoms << "\n";

            // list of memorized results
        }
        map<int, map<plan, state_type>> cache;

        // initialize with empty plan
        cache[0][plan()] = model.getInitState();

        // compute recursive steps
        for (unsigned n = 1; n <= atoms.size(); n++) {
            for (const auto& cur : cache[n - 1]) {
                const plan& subPlan = cur.first;
                const state_type& inState = cur.second;

                for (const auto& cur : atoms) {
                    // only add non-covered steps
                    if (contains(subPlan, cur)) {
                        continue;
                    }

                    // extend plan
                    plan plan = subPlan;
                    plan.push_back(cur);

                    // compute new state and register it
                    cache[n][plan] = model.applyTo(inState, cur);
                }
            }
        }

        // get best full plan
        state_type best = cache[atoms.size()][atoms];
        plan bestPlan = atoms;
        for (const auto& cur : cache[atoms.size()]) {
            if (cur.second.getCost() < best.getCost()) {
                best = cur.second;
                bestPlan = cur.first;
            }
        }

        if (debug) {
            std::cout << "Results:\n"
                      << join(cache, "\n",
                                 [](std::ostream& out, const std::pair<int, map<plan, state_type>>& cur) {
                                     out << cur.first << ":\n\t"
                                         << join(cur.second, "\n\t",
                                                    [](std::ostream& out,
                                                            const std::pair<plan, state_type>& cur) {
                                                        out << "[" << join(cur.first, ",",
                                                                              [](std::ostream& out,
                                                                                      const atom_type& atom) {
                                                                                  out << atom.getID();
                                                                              })
                                                            << "] => " << cur.second;
                                                    })
                                         << "\n";
                                 })
                      << "\n";
            std::cout << "Solution: " << bestPlan << "\n";
        }

        // return best plan
        return bestPlan;
    }

    /** Enables problems to be printed. */
    void print(std::ostream& out) const {
        out << "{ " << join(atoms, ", ") << " }";
    }

    friend std::ostream& operator<<(std::ostream& out, const Problem& p) {
        p.print(out);
        return out;
    }
};

// ######################################################################
//                           Cost Models
// ######################################################################

// ----------------------------------------------------------------------
//                       Abstract Cost Model
// ----------------------------------------------------------------------

typedef double Cost;

/**
 * The base class for the state associated to a problem after executing
 * a (partial) plan.
 */
class State {
    /** The cost for reaching this state */
    Cost cost;

public:
    State() : cost(0) {}

    virtual ~State() = default;

    Cost getCost() const {
        return cost;
    }

    void setCost(Cost cost) {
        this->cost = cost;
    }

    void incCost(Cost inc) {
        cost += inc;
    }

    virtual void print(std::ostream& out) const = 0;

    friend std::ostream& operator<<(std::ostream& out, const State& s) {
        s.print(out);
        return out;
    }
};

/**
 * A abstract base class for a cost model.
 */
template <typename Atom, typename State>
struct cost_model {
    /** The type of atom to be required within the input problem. */
    typedef Atom atom_type;

    /** The type of state to be associated to plans by this model. */
    typedef State state_type;

    virtual ~cost_model() = default;

    /** A factory for creating the state to be associated to the empty plan */
    virtual State getInitState() const {
        return State();
    }

    /**
     * Obtains the state reached when appending the given atom to a plan
     * resulting in the given state.
     */
    virtual State applyTo(const State& state, const Atom& atom) const = 0;
};

// ----------------------------------------------------------------------
//                           Max Binding
// ----------------------------------------------------------------------

/**
 * A model tracing the list of bound variables and estimating
 * the cost of joins by assuming a fixed selectivity of bound values.
 */
class BindingState : public State {
    /** The set of bound variables */
    std::set<Var> bound;

public:
    void bind(const Var& var) {
        bound.insert(var);
    }

    bool isBound(const Var& var) const {
        return bound.find(var) != bound.end();
    }

    void print(std::ostream& out) const override {
        out << "State(" << getCost() << "," << bound << ")";
    }
};

/**
 * The cost model estimating the cost of execution plans
 * based on the number of bound/unbound variables in each step.
 *
 * This model has turned out to be sub-optimal when facing
 * chains of atoms. It is here as a reference and not the
 * default.
 */
struct MaxBindingModel : public cost_model<Atom, BindingState> {
    BindingState applyTo(const BindingState& state, const Atom& atom) const override {
        BindingState res = state;

        // adding this relation costs 1 unit
        Cost cost = 1;

        // unless there are variables pre-bound
        for (const Var& var : atom.getVariables()) {
            if (state.isBound(var)) {
                cost *= 0.5;
            }
            res.bind(var);
        }

        // increment costs
        res.incCost(cost);

        // that's it
        return res;
    }
};

// ----------------------------------------------------------------------
//                    Simple Computational Cost Model
// ----------------------------------------------------------------------

/**
 * An extended version of the atom utilized by the simple computational cost
 * model which required the cardinality of the targeted relation.
 */
class SimpleComputationalCostAtom : public Atom {
    std::size_t cardinality;

public:
    SimpleComputationalCostAtom(int id, const std::vector<Argument>& args, std::size_t card)
            : Atom(id, args), cardinality(card) {}

    unsigned getCardinality() const {
        return cardinality;
    }

    void print(std::ostream& out) const override {
        out << "<" << getID() << ">|" << cardinality << "|( " << getArguments() << " )";
    }
};

/**
 * The state associated to the simple computational cost model maintaining
 * a set of bound variables and an estimation for the number of innermost
 * loop iterations.
 */
class SimpleComputationalCostState : public State {
    /** The set of bound variables at this state */
    std::set<Var> bound;

    /** The estimated number of innermost iterations at this state */
    uint64_t innermostIterations;

public:
    SimpleComputationalCostState() : innermostIterations(1) {}

    void bind(const Var& var) {
        bound.insert(var);
    }

    bool isBound(const Var& var) const {
        return bound.find(var) != bound.end();
    }

    uint64_t getInnermostIterations() const {
        return innermostIterations;
    }

    void setInnermostIterations(uint64_t val) {
        innermostIterations = val;
    }

    void print(std::ostream& out) const override {
        out << "State(" << getCost() << "," << bound << "," << innermostIterations << ")";
    }
};

/**
 * The simple computational model is estimating the number of loop iterations
 * on each level of the computation as well as the costs for each iteration.
 */
struct SimpleComputationalCostModel
        : public cost_model<SimpleComputationalCostAtom, SimpleComputationalCostState> {
    SimpleComputationalCostState applyTo(const SimpleComputationalCostState& state,
            const SimpleComputationalCostAtom& atom) const override {
        SimpleComputationalCostState res = state;

        // keep a record on bound attributes
        bool someAttributsBound = atom.getNumConstants() > 0;

        // compute iterations per application
        Cost numIterations = atom.getCardinality();
        if (numIterations > 0) {
            for (const Argument& arg : atom.getArguments()) {
                if (arg.isConstant()) {
                    numIterations *= 0.001;  // TODO: improve this value
                    someAttributsBound = true;
                } else if (arg.isVariable()) {
                    if (state.isBound(arg)) {
                        numIterations *= 0.001;  // TODO: improve this value
                        someAttributsBound = true;
                    } else {
                        res.bind(arg);
                    }
                }
            }

            // make sure number of iterations is not getting to small
            if (numIterations < 1) {
                numIterations = 1;
            }
        }

        // compute full computation costs per iteration
        Cost costPerCall = (someAttributsBound) ? log(atom.getCardinality()) : 1;

        // increase total costs
        res.incCost(costPerCall * state.getInnermostIterations());

        // increase number of innermost iterations
        res.setInnermostIterations(res.getInnermostIterations() * numIterations);

        // that's it
        return res;
    }
};

// ----------------------------------------------------------------------
//                     Computational Cost Model
// ----------------------------------------------------------------------

/**
 * An extended version of the atom utilized by the computational cost
 * model which is depending on additional statistical information.
 */
class ComputeCostAtom : public Atom {
    RamRelationStats stats;

public:
    ComputeCostAtom(int id, const std::vector<Argument>& args, const RamRelationStats& stats)
            : Atom(id, args), stats(stats) {
        assert(args.size() == stats.getArity());
    }

    const RamRelationStats& getRelationStats() const {
        return stats;
    }

    unsigned getCardinality() const {
        return stats.getCardinality();
    }

    void print(std::ostream& out) const override {
        out << "<" << getID() << ">|" << getCardinality() << "," << getRelationStats() << "|( "
            << getArguments() << " )";
    }
};

/**
 * The state associated to the computational cost model maintaining
 * an estimate to the multiplicity of bound variables and an estimate
 * of innermost loop iterations.
 */
class ComputeCostState : public State {
    /**
     * An estimate for the multiplicity of variables -- hence, to how
     * many different values those variables may be bound at the given
     * state.
     */
    std::map<Var, uint32_t> multiplicity;

    /**
     * An estimate for the number of innermost iterations.
     */
    uint64_t innermostIterations;

public:
    ComputeCostState() : innermostIterations(1) {}

    void bind(const Var& var, uint32_t mult) {
        multiplicity[var] = mult;
    }

    bool isBound(const Var& var) const {
        return multiplicity.find(var) != multiplicity.end();
    }

    uint32_t getMultiplicity(const Var& var) const {
        assert(isBound(var));
        return multiplicity.find(var)->second;
    }

    uint64_t getInnermostIterations() const {
        return innermostIterations;
    }

    void setInnermostIterations(uint64_t val) {
        innermostIterations = val;
    }

    void print(std::ostream& out) const override {
        out << "State(" << getCost() << "," << multiplicity << "," << innermostIterations << ")";
    }
};

/**
 * The compute cost model tries to estimate the costs for computing
 * a plan by maintaining estimates for the number of different values
 * variables may be bound to at various stages of the execution of a
 * plan.
 *
 * Note: at its current state -- which might still provide improvement
 * potential since not thoroughly investigated, this model turned out
 * to be inferior to the simple cost model.
 */
struct ComputeCostModel : public cost_model<ComputeCostAtom, ComputeCostState> {
    ComputeCostState applyTo(const ComputeCostState& state, const ComputeCostAtom& atom) const override {
        ComputeCostState res = state;

        const RamRelationStats& stats = atom.getRelationStats();
        const std::vector<Argument>& args = atom.getArguments();

        bool constraint = false;

        // multiplier factor of this loop operation
        auto f = stats.getCardinality();
        for (unsigned i = 0; i < args.size(); i++) {
            const Argument& cur = args[i];
            if (cur.isConstant()) {
                f = min(f, stats.getEstimatedCardinality(i));
                constraint = true;
            } else if (cur.isVariable()) {
                if (state.isBound(cur)) {
                    f = min(f, stats.getEstimatedCardinality(i));
                } else {
                    res.bind(cur, 1);
                }
                constraint = true;
            }
        }

        // compute number of nested iterations
        uint64_t numIterations = (uint64_t)f;
        if (numIterations <= 0) {
            numIterations = 1;
        }

        // costs per iteration
        double iterCost = (!constraint) ? 1 : log(stats.getCardinality());

        // add to cost
        res.incCost(state.getInnermostIterations() * iterCost);

        // increment nested iterators
        res.setInnermostIterations(state.getInnermostIterations() * f);

        // that's it
        return res;
    }
};

// ----------------------------------------------------------------------
//                          Log Cost Model
// ----------------------------------------------------------------------

/**
 * A model computing the cost of a schedule based on the formula:
 *
 *          C = prod_{ a in schedule } ( n_a^(f_a/m_a) )
 *
 * where
 *      C   ... the costs,
 *      a   ... atoms in a schedule,
 *      n_a ... cardinality of relation referenced by atom a
 *      f_a ... free arguments of atom a
 *      m_a ... arity of relation referenced by atom a
 *
 */
struct LogCostModel : public cost_model<SimpleComputationalCostAtom, BindingState> {
    BindingState getInitState() const override {
        BindingState res;
        res.setCost(1);
        return res;
    }

    BindingState applyTo(const BindingState& state, const SimpleComputationalCostAtom& atom) const override {
        BindingState res = state;

        // compute the costs of this step
        double step = pow((double)atom.getCardinality(), (double)1 / (double)atom.getArity());
        double cost = 1;

        for (const auto& cur : atom.getArguments()) {
            if (cur.isConstant()) {
                // nothing here
            } else if (cur.isVariable()) {
                if (!state.isBound(cur)) {
                    cost *= step;
                    res.bind(cur);
                }
            } else {
                // unbound value => increases cost
                cost *= step;
            }
        }

        // update costs
        res.setCost(state.getCost() * cost);

        // that's it
        return res;
    }
};

}  // end of namespace scheduler
}  // end of namespace souffle
