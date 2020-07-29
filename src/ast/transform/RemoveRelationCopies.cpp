/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
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
#include "ast/Abstract.h"
#include "ast/Argument.h"
#include "ast/Clause.h"
#include "ast/Literal.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/Utils.h"
#include "ast/Visitor.h"
#include "ast/analysis/IOType.h"
#include "utility/ContainerUtil.h"
#include <algorithm>
#include <cassert>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace souffle {

bool RemoveRelationCopiesTransformer::removeRelationCopies(AstTranslationUnit& translationUnit) {
    using alias_map = std::map<AstQualifiedName, AstQualifiedName>;

    // collect aliases
    alias_map isDirectAliasOf;

    auto* ioType = translationUnit.getAnalysis<IOType>();

    AstProgram& program = *translationUnit.getProgram();

    // search for relations only defined by a single rule ..
    for (AstRelation* rel : program.getRelations()) {
        const auto& clauses = getClauses(program, *rel);
        if (!ioType->isIO(rel) && clauses.size() == 1u) {
            // .. of shape r(x,y,..) :- s(x,y,..)
            AstClause* cl = clauses[0];
            std::vector<AstAtom*> bodyAtoms = getBodyLiterals<AstAtom>(*cl);
            if (!isFact(*cl) && cl->getBodyLiterals().size() == 1u && bodyAtoms.size() == 1u) {
                AstAtom* atom = bodyAtoms[0];
                if (equal_targets(cl->getHead()->getArguments(), atom->getArguments())) {
                    // Requirements:
                    // 1) (checked) It is a rule with exactly one body.
                    // 3) (checked) The body consists of an atom.
                    // 4) (checked) The atom's arguments must be identical to the rule's head.
                    // 5) (pending) The rules's head must consist only of either:
                    //  5a) Variables
                    //  5b) Records unpacked into variables
                    // 6) (pending) Each variable must have a distinct name.
                    bool onlyDistinctHeadVars = true;
                    std::set<std::string> headVars;

                    auto args = cl->getHead()->getArguments();
                    while (onlyDistinctHeadVars && !args.empty()) {
                        const auto cur = args.back();
                        args.pop_back();

                        if (auto var = dynamic_cast<const AstVariable*>(cur)) {
                            onlyDistinctHeadVars &= headVars.insert(var->getName()).second;
                        } else if (auto init = dynamic_cast<const AstRecordInit*>(cur)) {
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
    std::set<AstQualifiedName> cycle_reps;

    for (std::pair<AstQualifiedName, AstQualifiedName> cur : isDirectAliasOf) {
        // compute replacement

        std::set<AstQualifiedName> visited;
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

    // replace usage of relations according to alias map
    visitDepthFirst(program, [&](const AstAtom& atom) {
        auto pos = isAliasOf.find(atom.getQualifiedName());
        if (pos != isAliasOf.end()) {
            const_cast<AstAtom&>(atom).setQualifiedName(pos->second);
        }
    });

    // break remaining cycles
    for (const auto& rep : cycle_reps) {
        const auto& rel = *getRelation(program, rep);
        const auto& clauses = getClauses(program, rel);
        assert(clauses.size() == 1u && "unexpected number of clauses in relation");
        program.removeClause(clauses[0]);
    }

    // remove unused relations
    for (const auto& cur : isAliasOf) {
        if (cycle_reps.count(cur.first) == 0u) {
            program.removeRelation(getRelation(program, cur.first)->getQualifiedName());
        }
    }

    return true;
}

}  // end of namespace souffle
