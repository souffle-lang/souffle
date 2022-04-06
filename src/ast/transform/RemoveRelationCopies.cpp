/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RemoveRelationCopies.cpp
 *
 ***********************************************************************/

#include "ast/transform/RemoveRelationCopies.h"
#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/RecordInit.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/Variable.h"
#include "ast/analysis/IOType.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "souffle/utility/ContainerUtil.h"
#include <algorithm>
#include <cassert>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace souffle::ast::transform {

bool RemoveRelationCopiesTransformer::removeRelationCopies(TranslationUnit& translationUnit) {
    using alias_map = std::map<QualifiedName, QualifiedName>;

    // collect aliases
    alias_map isDirectAliasOf;

    auto& ioType = translationUnit.getAnalysis<analysis::IOTypeAnalysis>();

    Program& program = translationUnit.getProgram();

    // search for relations only defined by a single rule ..
    for (Relation* rel : program.getRelations()) {
        // skip relations with functional dependencies
        if (!rel->getFunctionalDependencies().empty()) {
            continue;
        }
        // skip equivalence relations (which automatically create reflexive,
        // symmetric, and transitive tuples)
        if (rel->getRepresentation() == RelationRepresentation::EQREL) {
            continue;
        }
        const auto& clauses = program.getClauses(*rel);
        if (!ioType.isIO(rel) && clauses.size() == 1u) {
            // .. of shape r(x,y,..) :- s(x,y,..)
            Clause* cl = clauses[0];
            std::vector<Atom*> bodyAtoms = getBodyLiterals<Atom>(*cl);
            if (!isFact(*cl) && cl->getBodyLiterals().size() == 1u && bodyAtoms.size() == 1u) {
                Atom* atom = bodyAtoms[0];
                if (equal_targets(cl->getHead()->getArguments(), atom->getArguments())) {
                    // Requirements:
                    // 1) (checked) It is a rule with exactly one body.
                    // 3) (checked) The body consists of an atom.
                    // 4) (checked) The atom's arguments must be identical to the rule's head.
                    // 5) (pending) The rules's head must consist only of either:
                    //  5a) Variables
                    //  5b) Records unpacked into variables
                    // 6) (pending) Each variable must have a distinct name.
                    // 7) (checked?) Head rule cannot have any functional dependency.
                    bool onlyDistinctHeadVars = true;
                    std::set<std::string> headVars;

                    auto args = cl->getHead()->getArguments();
                    while (onlyDistinctHeadVars && !args.empty()) {
                        const auto cur = args.back();
                        args.pop_back();

                        if (auto var = as<ast::Variable>(cur)) {
                            onlyDistinctHeadVars &= headVars.insert(var->getName()).second;
                        } else if (auto init = as<RecordInit>(cur)) {
                            // records are decomposed and their arguments are checked
                            for (auto rec_arg : init->getArguments()) {
                                args.push_back(rec_arg);
                            }
                        } else {
                            onlyDistinctHeadVars = false;
                        }
                    }

                    if (onlyDistinctHeadVars) {
                        // all arguments are either distinct variables or records unpacked into distinct
                        // variables
                        isDirectAliasOf[cl->getHead()->getQualifiedName()] = atom->getQualifiedName();
                    }
                }
            }
        }
    }

    // map each relation to its ultimate alias (could be transitive)
    alias_map isAliasOf;

    // track any copy cycles; cyclic rules are effectively empty
    std::set<QualifiedName> cycle_reps;

    for (std::pair<QualifiedName, QualifiedName> cur : isDirectAliasOf) {
        // compute replacement

        std::set<QualifiedName> visited;
        visited.insert(cur.first);
        visited.insert(cur.second);

        auto pos = isDirectAliasOf.find(cur.second);
        while (pos != isDirectAliasOf.end()) {
            if (visited.count(pos->second) != 0u) {
                cycle_reps.insert(cur.second);
                break;
            }
            cur.second = pos->second;
            pos = isDirectAliasOf.find(cur.second);
        }
        isAliasOf[cur.first] = cur.second;
    }

    if (isAliasOf.empty()) {
        return false;
    }

    // break remaining cycles
    for (const auto& rep : cycle_reps) {
        auto* info = program.getRelationInfo(rep);
        assert(info && info->clauses.size() == 1 && "unexpected number of clauses in relation");
        info->clauses.clear();  // remove the clause for a broken cycle
    }

    // remove unused relations
    for (const auto& cur : isAliasOf) {
        if (!contains(cycle_reps, cur.first)) {
            program.removeRelation(cur.first);
        }
    }

    renameAtoms(program, isAliasOf);
    return true;
}

}  // namespace souffle::ast::transform
