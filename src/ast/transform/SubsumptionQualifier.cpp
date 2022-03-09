/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SubsumptionQualifier.cpp
 *
 ***********************************************************************/

#include "ast/transform/SubsumptionQualifier.h"
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
#include "ast/analysis/typesystem/PolymorphicObjects.h"
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

bool SubsumptionQualifierTransformer::transform(TranslationUnit& translationUnit) {
    bool changed = false;
    Program& program = translationUnit.getProgram();

    for (auto* relation : program.getRelations()) {
        // Only concerned with default relations
        if (relation->getRepresentation() != RelationRepresentation::DEFAULT) {
            continue;
        }
        bool hasSubsumptiveRule = visitExists(program, [&](const ast::SubsumptiveClause& sClause) {
            return sClause.getHead()->getQualifiedName() == relation->getQualifiedName();
        });

        // rewrite relation representation
        if (relation->getRepresentation() == RelationRepresentation::DEFAULT && hasSubsumptiveRule) {
            relation->setRepresentation(RelationRepresentation::BTREE_DELETE);
        }
    }
    return changed;
}

}  // namespace souffle::ast::transform
