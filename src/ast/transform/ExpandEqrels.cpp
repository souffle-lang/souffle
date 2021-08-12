/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ExpandEqrels.cpp
 *
 ***********************************************************************/

#include "ast/transform/ExpandEqrels.h"
#include "AggregateOp.h"
#include "RelationTag.h"
#include "ast/Aggregator.h"
#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/BinaryConstraint.h"
#include "ast/BooleanConstraint.h"
#include "ast/Clause.h"
#include "ast/Constant.h"
#include "ast/Constraint.h"
#include "ast/Functor.h"
#include "ast/IntrinsicFunctor.h"
#include "ast/Literal.h"
#include "ast/Negation.h"
#include "ast/Node.h"
#include "ast/NumericConstant.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/RecordInit.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/TypeCast.h"
#include "ast/UnnamedVariable.h"
#include "ast/UserDefinedFunctor.h"
#include "ast/Variable.h"
#include "ast/analysis/PolymorphicObjects.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ast::transform {

bool ExpandEqrelsTransformer::transform(TranslationUnit& translationUnit) {
    bool changed = false;
    Program& program = translationUnit.getProgram();

    for (auto* relation : program.getRelations()) {
        // Only concerned with eqrel relations
        if (relation->getRepresentation() != RelationRepresentation::EQREL) {
            continue;
        }
        const auto& relName = relation->getQualifiedName();
        changed = true;

        // Remove eqrel representation
        relation->setRepresentation(RelationRepresentation::BTREE);

        // (1) Transitivity
        // A(x,z) :- A(x,y), A(y,z).
        auto transitiveClauseHead = mk<Atom>(relName);
        transitiveClauseHead->addArgument(mk<Variable>("x"));
        transitiveClauseHead->addArgument(mk<Variable>("z"));

        auto transitiveClause = mk<Clause>(std::move(transitiveClauseHead));

        auto transitiveClauseLeftAtom = mk<Atom>(relName);
        transitiveClauseLeftAtom->addArgument(mk<Variable>("x"));
        transitiveClauseLeftAtom->addArgument(mk<Variable>("y"));
        transitiveClause->addToBody(std::move(transitiveClauseLeftAtom));

        auto transitiveClauseRightAtom = mk<Atom>(relName);
        transitiveClauseRightAtom->addArgument(mk<Variable>("y"));
        transitiveClauseRightAtom->addArgument(mk<Variable>("z"));
        transitiveClause->addToBody(std::move(transitiveClauseRightAtom));

        program.addClause(std::move(transitiveClause));

        // (2) Symmetry
        // A(x,y) :- A(y,x).
        auto symmetryClauseHead = mk<Atom>(relName);
        symmetryClauseHead->addArgument(mk<Variable>("x"));
        symmetryClauseHead->addArgument(mk<Variable>("y"));

        auto symmetryClause = mk<Clause>(std::move(symmetryClauseHead));

        auto symmetryClauseBodyAtom = mk<Atom>(relName);
        symmetryClauseBodyAtom->addArgument(mk<Variable>("y"));
        symmetryClauseBodyAtom->addArgument(mk<Variable>("x"));
        symmetryClause->addToBody(std::move(symmetryClauseBodyAtom));

        program.addClause(std::move(symmetryClause));

        // (3) Reflexivity
        // A(x,x) :- A(x,_).
        auto reflexiveClauseHead = mk<Atom>(relName);
        reflexiveClauseHead->addArgument(mk<Variable>("x"));
        reflexiveClauseHead->addArgument(mk<Variable>("x"));

        auto reflexiveClause = mk<Clause>(std::move(reflexiveClauseHead));

        auto reflexiveClauseBodyAtom = mk<Atom>(relName);
        reflexiveClauseBodyAtom->addArgument(mk<Variable>("x"));
        reflexiveClauseBodyAtom->addArgument(mk<UnnamedVariable>());
        reflexiveClause->addToBody(std::move(reflexiveClauseBodyAtom));

        program.addClause(std::move(reflexiveClause));
    }

    return changed;
}

}  // namespace souffle::ast::transform
