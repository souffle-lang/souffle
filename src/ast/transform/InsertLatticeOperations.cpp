/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2023 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file InsertLatticeOperations.cpp
 *
 * Implements AST transformation related to the support of lattices
 *
 ***********************************************************************/

#include "ast/transform/InsertLatticeOperations.h"
#include "ast/Atom.h"
#include "ast/BinaryConstraint.h"
#include "ast/Clause.h"
#include "ast/Constraint.h"
#include "ast/Negation.h"
#include "ast/QualifiedName.h"
#include "ast/UnnamedVariable.h"
#include "ast/UserDefinedFunctor.h"
#include "ast/Variable.h"
#include "ast/analysis/Ground.h"
#include "ast/utility/Visitor.h"
#include <utility>

namespace souffle::ast::transform {

/*
 * If a non-negated body atom has a lattice argument that is grounded by other atoms and constraints,
 * e.g. `R(a, b, $B(x))`, then:
 * - Given that `$B(x)` is a lattice argument:
 *    `R(a, b, $B(x))` holds <=> the current value `lat` in `R(a, b, lat)` is greater or equal to `$B(x)` in
 * the lattice Consequently, we must translate the atom `R(a, ,b $B(x))` into `R(a, b, lat), GLB(lat, $B(x)) =
 * $B(x)` or `R(a, b, lat), LEQ($B(x), lat) != 0`.
 */
struct ReplaceGroundedLatticeArguments : public NodeMapper {
    ReplaceGroundedLatticeArguments(std::map<const ast::Argument*, const ast::Lattice*>& toReplace,
            VecOwn<ast::Literal>& newConstraints)
            : toReplace(toReplace), newConstraints(newConstraints) {}

    Own<Node> operator()(Own<Node> node) const override {
        if (auto arg = as<ast::Argument>(node)) {
            if (toReplace.count(arg) > 0) {
                Point p = arg->getSrcLoc().start;
                std::string lat = "<lattice" + std::to_string(p.line) + "_" + std::to_string(p.column) + ">";
                // add Glb constraint
                assert(toReplace.count(arg) > 0);
                const ast::Lattice* lattice = toReplace.at(arg);
                assert(lattice->hasGlb());
                auto glbName = as<UserDefinedFunctor>(lattice->getGlb())->getName();
                VecOwn<Argument> args;
                args.push_back(mk<Variable>(lat, arg->getSrcLoc()));
                args.push_back(clone(arg));
                auto glb = mk<UserDefinedFunctor>(glbName, std::move(args), arg->getSrcLoc());
                newConstraints.push_back(mk<BinaryConstraint>(
                        BinaryConstraintOp::EQ, std::move(glb), clone(arg), arg->getSrcLoc()));
                return mk<Variable>(lat, arg->getSrcLoc());
            } else {
                return node;
            }
        }
        node->apply(*this);
        return node;
    }

private:
    std::map<const ast::Argument*, const ast::Lattice*> toReplace;
    VecOwn<ast::Literal>& newConstraints;
};

struct varInfo {
    const ast::Lattice* lattice;
    // lattice variables in body atoms
    std::set<const ast::Variable*> variables;
    // set of newly introduced variable names
    std::set<std::string> glbs;
};

/*
 * If a non-negated body atom has a lattice argument `x` that is not grounded,
 * e.g. `R(a, b, x)`:
 * Then, we gather all occurences of `x` that appear as lattice arguments,
 * replace these occurences with distinct variable names `x_1`, `x_2`, ... `x_n`
 * and we compute the GLB of all these occurences and store in variable `x`,
 * and finally add the constraint `x != BOTTOM`.
 * If the variable `x` is used in the atom head, it can remain unchanged.
 * If some literals have constraints on `x`, e.g. `@myfunctor(x) > 0`:
 * This means we would need to find the greatest lower bound of `x_1`, `x_2`, ... `x_n`
 * verifying the constraints, which is not possible => the clause cannot be translated.
 */
struct ReplaceUngroundedLatticeArguments : public NodeMapper {
    ReplaceUngroundedLatticeArguments(varInfo& infos) : infos(infos) {}

    Own<Node> operator()(Own<Node> node) const override {
        if (auto arg = as<ast::Variable>(node)) {
            if (infos.variables.count(arg) > 0) {
                std::string varName;
                varName = arg->getName() + "_<lattice" + std::to_string(infos.glbs.size()) + ">";
                infos.glbs.insert(varName);
                return mk<Variable>(varName, arg->getSrcLoc());
            }
        }
        node->apply(*this);
        return node;
    }

private:
    varInfo& infos;
};

/*
 * If a negated body atom has lattice argument `arg1, ..., argN`,
 * e.g. `!R(a, b, arg1, arg2)`, then
 * - the arguments `arg1`, ... `argN` must be bound by other atoms
 * - We must translate the negated atom `!R(a, b, arg1, arg2)`
 *   into the disjunction :
 * (
 *  !R(a, b, _, _) ;
 *  R(a, b, lat1, _), GLB(lat1, arg1) = BOTTOM;
 *  R(a, b, _, lat2), GLB(lat2, arg2) = BOTTOM
 * )
 */

RuleBody LatticeTransformer::translateNegatedAtom(ast::Atom& atom) {
    auto args = atom.getArguments();
    auto range = latticeAttributes.equal_range(atom.getQualifiedName());

    std::map<std::size_t, const QualifiedName> latticeIndexes;
    for (auto it = range.first; it != range.second; it++) {
        latticeIndexes.insert(it->second);
    }

    std::size_t arity = atom.getArity();
    VecOwn<Argument> negatedArgs;
    for (std::size_t i = 0; i < arity; i++) {
        if (latticeIndexes.count(i)) {
            negatedArgs.push_back(mk<UnnamedVariable>());
        } else {
            negatedArgs.push_back(clone(args[i]));
        }
    }
    auto negated = mk<Atom>(atom.getQualifiedName(), std::move(negatedArgs), atom.getSrcLoc());
    RuleBody body = RuleBody::atom(std::move(negated)).negated();

    // create one disjunct per argument
    for (const auto& [index, type] : latticeIndexes) {
        const ast::Lattice* lattice = lattices.at(type);
        VecOwn<Argument> arguments;
        const auto& arg = args[index];
        if (isA<UnnamedVariable>(arg)) {
            continue;
        }
        const auto& sloc = arg->getSrcLoc();
        std::string lat =
                "<lattice" + std::to_string(sloc.start.line) + "_" + std::to_string(sloc.start.column) + ">";
        for (std::size_t i = 0; i < arity; i++) {
            if (latticeIndexes.count(i)) {
                if (i == index) {
                    arguments.push_back(mk<Variable>(lat));
                } else {
                    arguments.push_back(mk<UnnamedVariable>());
                }
            } else {
                arguments.push_back(clone(args[i]));
            }
        }
        VecOwn<Argument> glbArgs;
        glbArgs.push_back(mk<Variable>(lat, sloc));
        glbArgs.push_back(clone(arg));
        auto glbName = as<UserDefinedFunctor>(lattice->getGlb())->getName();
        auto glb = mk<UserDefinedFunctor>(glbName, std::move(glbArgs), sloc);
        auto constraint = mk<BinaryConstraint>(
                BinaryConstraintOp::EQ, std::move(glb), clone(lattice->getBottom()), sloc);
        auto conjunct =
                RuleBody::atom(mk<Atom>(atom.getQualifiedName(), std::move(arguments), atom.getSrcLoc()));
        conjunct.conjunct(RuleBody::constraint(std::move(constraint)));
        body.disjunct(std::move(conjunct));
    }

    return body;
}

bool LatticeTransformer::translateClause(
        TranslationUnit& translationUnit, ErrorReport& report, ast::Clause* clause) {
    bool changed = false;

    // Set of atoms that are negated in the clause body and contain lattice arguments
    std::set<const ast::Atom*> negatedLatticeAtoms;
    // set of lattice arguments that are grounded, and their corresponding lattice
    std::map<const ast::Argument*, const ast::Lattice*> groundedLatticeArguments;
    // stores information on ungrounded lattice variable names
    std::map<std::string, varInfo> ungroundedLatticeArguments;

    // Compute grounded/ungrounded arguments
    auto isGrounded = analysis::getGroundedTerms(translationUnit, *clause, true);

    // Identify which body atoms are negated
    std::set<const ast::Atom*> negated;
    visit(clause->getBodyLiterals(), [&](const Negation& negation) { negated.insert(negation.getAtom()); });

    // Identify lattice arguments in body atoms
    visit(clause->getBodyLiterals(), [&](const Atom& atom) {
        auto range = latticeAttributes.equal_range(atom.getQualifiedName());
        auto args = atom.getArguments();
        for (auto it = range.first; it != range.second; it++) {
            const ast::Argument* arg = args[it->second.first];
            assert(lattices.count(it->second.second) > 0);
            const ast::Lattice* lattice = lattices.at(it->second.second);
            if (isA<UnnamedVariable>(arg)) {
                // nothing to do
            } else if (negated.count(&atom)) {
                negatedLatticeAtoms.insert(&atom);
            } else if (isGrounded[arg]) {
                groundedLatticeArguments.insert(std::make_pair(arg, lattice));
            } else if (const auto* var = as<Variable>(arg)) {
                ungroundedLatticeArguments[var->getName()].variables.insert(var);
                ungroundedLatticeArguments[var->getName()].lattice = lattice;
            } else {
                report.addError("Lattice argument is not grounded", arg->getSrcLoc());
            }
        }
    });

    VecOwn<ast::Literal> constraints;

    // Update grounded lattice arguments
    ReplaceGroundedLatticeArguments update(groundedLatticeArguments, constraints);
    for (auto* literal : clause->getBodyLiterals()) {
        literal->apply(update);
    }

    // Update ungrounded lattice arguments
    for (auto& it : ungroundedLatticeArguments) {
        auto& infos = it.second;
        auto& name = it.first;
        if (infos.variables.size() > 1) {
            changed = true;

            // First, we must make sure that the ungrounded variables are only used as lattice arguments
            visit(clause->getBodyLiterals(), [&](const Variable& variable) {
                if (variable.getName() == name && !infos.variables.count(&variable)) {
                    report.addError("Ungrounded lattice variable cannot be used in other literals",
                            variable.getSrcLoc());
                }
            });

            ReplaceUngroundedLatticeArguments update(infos);
            for (auto* literal : clause->getBodyLiterals()) {
                literal->apply(update);
            }
            // create new constraints
            Own<Argument> glb;
            const auto& lattice = infos.lattice;
            assert(lattice->hasGlb());
            auto glbName = as<UserDefinedFunctor>(lattice->getGlb())->getName();
            for (const std::string& name : infos.glbs) {
                if (!glb) {
                    // first element
                    glb = mk<Variable>(name, clause->getSrcLoc());
                } else {
                    VecOwn<Argument> args;
                    args.push_back(mk<Variable>(name, clause->getSrcLoc()));
                    args.push_back(std::move(glb));
                    glb = mk<UserDefinedFunctor>(glbName, std::move(args), clause->getSrcLoc());
                }
            }
            // x = GLB(...)
            constraints.push_back(mk<BinaryConstraint>(BinaryConstraintOp::EQ,
                    mk<Variable>(name, clause->getSrcLoc()), std::move(glb), clause->getSrcLoc()));
            // x != BOTTOM
            constraints.push_back(
                    mk<BinaryConstraint>(BinaryConstraintOp::NE, mk<Variable>(name, clause->getSrcLoc()),
                            clone(lattice->getBottom()), clause->getSrcLoc()));
        }
    }
    clause->addToBody(std::move(constraints));

    // updated negated atoms containing lattice arguments
    if (!negatedLatticeAtoms.empty()) {
        RuleBody body = RuleBody::getTrue();
        for (const auto* lit : clause->getBodyLiterals()) {
            if (const auto& neg = as<Negation>(lit)) {
                body.conjunct(translateNegatedAtom(*neg->getAtom()));
            } else if (const auto& atom = as<Atom>(lit)) {
                body.conjunct(RuleBody::atom(clone(atom)));
            } else if (const auto& cst = as<Constraint>(lit)) {
                body.conjunct(RuleBody::constraint(clone(cst)));
            } else {
                assert(false && "unreachable");
            }
        }
        VecOwn<Clause> clauses = body.toClauseBodies();
        for (auto& c : clauses) {
            c->setHead(clone(clause->getHead()));
        }
        translationUnit.getProgram().addClauses(std::move(clauses));
        translationUnit.getProgram().removeClause(*clause);
    }
    return changed;
}

bool LatticeTransformer::transform(TranslationUnit& translationUnit) {
    bool changed = false;
    Program& program = translationUnit.getProgram();
    ErrorReport& report = translationUnit.getErrorReport();

    if (program.getLattices().empty()) {
        return changed;
    }

    // populates map type name -> lattice
    for (const ast::Lattice* lattice : program.getLattices()) {
        // We ignore lattices lacking a definition for Glb and Bottom
        if (lattice->hasGlb() && lattice->hasBottom()) {
            lattices.emplace(lattice->getQualifiedName(), lattice);
        }
    }

    for (const Relation* rel : program.getRelations()) {
        const auto attributes = rel->getAttributes();
        for (std::size_t i = 0; i < attributes.size(); i++) {
            if (attributes[i]->getIsLattice()) {
                const auto type = attributes[i]->getTypeName();
                if (lattices.count(type)) {
                    latticeAttributes.emplace(
                            std::make_pair(rel->getQualifiedName(), std::make_pair(i, type)));
                }
            }
        }
    }
    if (latticeAttributes.empty()) {
        return false;
    }

    for (Clause* clause : program.getClauses()) {
        changed |= translateClause(translationUnit, report, clause);
    }

    return changed;
}

}  // namespace souffle::ast::transform
