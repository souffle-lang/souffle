/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
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
#include "ast/QualifiedName.h"
#include "ast/UnnamedVariable.h"
#include "ast/UserDefinedFunctor.h"
#include "ast/Variable.h"
#include "ast/utility/Visitor.h"
#include <utility>

namespace souffle::ast::transform {

struct ReplaceNonVariableLatticeArguments : public NodeMapper {
    ReplaceNonVariableLatticeArguments(std::map<const ast::Argument*, const ast::Lattice*>& toReplace,
            VecOwn<ast::Literal>& newConstraints)
            : toReplace(toReplace), newConstraints(newConstraints){};

    Own<Node> operator()(Own<Node> node) const override {
        if (auto arg = as<ast::Argument>(node)) {
            if (toReplace.count(arg) > 0) {
                Point p = arg->getSrcLoc().start;
                std::string varName =
                        "<lattice" + std::to_string(p.line) + "_" + std::to_string(p.column) + ">";
                // add Glb constraint
                assert(toReplace.count(arg) > 0);
                const ast::Lattice* lattice = toReplace.at(arg);
                assert(lattice->hasGlb());
                auto glbName = as<UserDefinedFunctor>(lattice->getGlb())->getName();
                VecOwn<Argument> args;
                args.push_back(mk<Variable>(varName, arg->getSrcLoc()));
                args.push_back(clone(arg));
                auto glb = mk<UserDefinedFunctor>(glbName, std::move(args), arg->getSrcLoc());
                newConstraints.push_back(mk<BinaryConstraint>(BinaryConstraintOp::NE, std::move(glb),
                        clone(lattice->getBottom()), arg->getSrcLoc()));
                return mk<Variable>(varName, arg->getSrcLoc());
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
    std::set<const ast::Variable*> latticeVariables;
    std::set<const ast::Variable*> variables;
    std::set<std::string> glbs;
};

struct ReplaceVariableLatticeArguments : public NodeMapper {
    ReplaceVariableLatticeArguments(varInfo& infos) : isHead(false), infos(infos){};

    Own<Node> operator()(Own<Node> node) const override {
        if (auto arg = as<ast::Variable>(node)) {
            if (infos.variables.count(arg) > 0) {
                std::string varName;
                if (infos.latticeVariables.count(arg) > 0) {
                    // lattice variable
                    if (isHead) {
                        varName = arg->getName() + "_<lattice>";
                    } else {
                        varName = arg->getName() + "_<lattice" + std::to_string(infos.glbs.size()) + ">";
                        infos.glbs.insert(varName);
                    }
                } else {
                    // regular variable
                    if (isHead) {
                        varName = arg->getName() + "_<lattice>";
                    } else {
                        varName = arg->getName() + "_<non_lattice>";
                        infos.glbs.insert(varName);
                    }
                }
                return mk<Variable>(varName, arg->getSrcLoc());
            }
        }
        node->apply(*this);
        return node;
    }

    bool isHead;

private:
    varInfo& infos;
};

bool LatticeTransformer::transform(TranslationUnit& translationUnit) {
    bool changed = false;
    Program& program = translationUnit.getProgram();

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

    std::multimap<const QualifiedName, std::pair<std::size_t, const QualifiedName>> latticeAttributes;
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
        std::map<const ast::Argument*, const ast::Lattice*> nonVariablelatticeArguments;
        std::map<std::string, varInfo> infos;

        // step 1 : identify lattice arguments
        visit(clause, [&](const Atom& atom) {
            auto range = latticeAttributes.equal_range(atom.getQualifiedName());
            auto args = atom.getArguments();
            for (auto it = range.first; it != range.second; it++) {
                const ast::Argument* arg = args[it->second.first];
                assert(lattices.count(it->second.second) > 0);
                const ast::Lattice* lattice = lattices.at(it->second.second);
                if (isA<UnnamedVariable>(arg)) {
                    // skip
                } else if (const auto* var = as<Variable>(arg)) {
                    // variableLatticeArguments.insert(std::make_pair(var, lattice));
                    infos[var->getName()].latticeVariables.insert(var);
                    infos[var->getName()].lattice = lattice;
                } else {
                    nonVariablelatticeArguments.insert(std::make_pair(arg, lattice));
                }
            }
        });

        // step 2 : identify where variable lattice arguments appear in body
        visit(clause, [&](const Variable& v) {
            const std::string& name = v.getName();
            if (infos.count(name) > 0) {
                infos[name].variables.insert(&v);
            }
        });

        VecOwn<ast::Literal> constraints;

        ReplaceNonVariableLatticeArguments update(nonVariablelatticeArguments, constraints);
        for (auto* literal : clause->getBodyLiterals()) {
            literal->apply(update);
        }

        for (auto [name, s] : infos) {
            if (s.variables.size() > 1) {
                ReplaceVariableLatticeArguments update(infos.at(name));
                for (auto* literal : clause->getBodyLiterals()) {
                    literal->apply(update);
                }
                update.isHead = true;
                clause->getHead()->apply(update);
                // create new constraints
                Own<Argument> current;
                const auto& lattice = infos.at(name).lattice;
                assert(lattice->hasGlb());
                auto glbName = as<UserDefinedFunctor>(lattice->getGlb())->getName();
                for (const std::string& name : infos.at(name).glbs) {
                    if (!current) {
                        // first element
                        current = mk<Variable>(name, clause->getSrcLoc());
                    } else {
                        VecOwn<Argument> args;
                        args.push_back(mk<Variable>(name, clause->getSrcLoc()));
                        args.push_back(std::move(current));
                        current = mk<UserDefinedFunctor>(glbName, std::move(args), clause->getSrcLoc());
                    }
                }
                constraints.push_back(mk<BinaryConstraint>(BinaryConstraintOp::EQ,
                        mk<Variable>(name + "_<lattice>", clause->getSrcLoc()), std::move(current),
                        clause->getSrcLoc()));
                constraints.push_back(mk<BinaryConstraint>(BinaryConstraintOp::NE,
                        mk<Variable>(name + "_<lattice>", clause->getSrcLoc()), clone(lattice->getBottom()),
                        clause->getSrcLoc()));
            }
        }
        clause->addToBody(std::move(constraints));
    }

    return changed;
}

}  // namespace souffle::ast::transform
