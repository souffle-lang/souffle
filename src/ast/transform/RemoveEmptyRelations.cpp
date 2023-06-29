/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RemoveEmptyRelations.cpp
 *
 ***********************************************************************/

#include "ast/transform/RemoveEmptyRelations.h"
#include "ast/Aggregator.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/Literal.h"
#include "ast/Negation.h"
#include "ast/Program.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/analysis/IOType.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace souffle::ast::transform {

bool RemoveEmptyRelationsTransformer::removeEmptyRelations(TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();
    auto& ioTypes = translationUnit.getAnalysis<analysis::IOTypeAnalysis>();

    std::set<QualifiedName> atoms_in_aggs;
    visitFrontier(program, [&](Aggregator& agg) {
        visit(agg, [&](Atom& atom) { atoms_in_aggs.insert(atom.getQualifiedName()); });
        return true;
    });

    std::set<QualifiedName> emptyRelations;
    bool changed = false;
    for (auto rel : program.getRelations()) {
        if (ioTypes.isInput(rel)) continue;
        if (!program.getClauses(*rel).empty()) continue;
        if (rel->getIsDeltaDebug()) continue;

        emptyRelations.insert(rel->getQualifiedName());

        bool usedInAggregate = contains(atoms_in_aggs, rel->getQualifiedName());
        if (!usedInAggregate && !ioTypes.isOutput(rel)) {
            program.removeRelation(*rel);
            changed = true;
        }
    }

    for (const auto& relName : emptyRelations) {
        changed |= removeEmptyRelationUses(translationUnit, relName);
    }

    return changed;
}

bool RemoveEmptyRelationsTransformer::removeEmptyRelationUses(
        TranslationUnit& translationUnit, const QualifiedName& emptyRelationName) {
    Program& program = translationUnit.getProgram();
    bool changed = false;

    //
    // (1) drop rules from the program that have empty relations in their bodies.
    // (2) drop negations of empty relations

    // clean all clauses
    for (const Clause* cl : program.getClauses()) {
        // check for an atom whose relation is the empty relation

        bool removed = false;
        for (Literal* lit : cl->getBodyLiterals()) {
            if (auto* arg = as<Atom>(lit)) {
                if (arg->getQualifiedName() == emptyRelationName) {
                    program.removeClause(*cl);
                    removed = true;
                    changed = true;
                    break;
                }
            }
        }

        if (!removed) {
            // check whether a negation with empty relations exists

            bool rewrite = false;
            for (Literal* lit : cl->getBodyLiterals()) {
                if (auto* neg = as<Negation>(lit)) {
                    if (neg->getAtom()->getQualifiedName() == emptyRelationName) {
                        rewrite = true;
                        break;
                    }
                }
            }

            if (rewrite) {
                // clone clause without negation for empty relations

                auto res = Own<Clause>(cl->cloneHead());

                for (Literal* lit : cl->getBodyLiterals()) {
                    if (auto* neg = as<Negation>(lit)) {
                        if (neg->getAtom()->getQualifiedName() != emptyRelationName) {
                            res->addToBody(clone(lit));
                        }
                    } else {
                        res->addToBody(clone(lit));
                    }
                }

                program.removeClause(*cl);
                program.addClause(std::move(res));
                changed = true;
            }
        }
    }

    return changed;
}

}  // namespace souffle::ast::transform
