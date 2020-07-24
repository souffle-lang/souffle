/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file NormaliseConstraints.cpp
 *
 ***********************************************************************/

#include "ast/transform/NormaliseConstraints.h"
#include "ast/AstAbstract.h"
#include "ast/AstArgument.h"
#include "ast/AstClause.h"
#include "ast/AstLiteral.h"
#include "ast/AstNode.h"
#include "ast/AstProgram.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstUtils.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <vector>

namespace souffle {
class AstRelation;

bool NormaliseConstraintsTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;

    // set a prefix for variables bound by magic-set for identification later
    // prepended by + to avoid conflict with user-defined variables
    static constexpr const char* boundPrefix = "+abdul";

    AstProgram& program = *translationUnit.getProgram();

    /* Create a node mapper that recursively replaces all constants and underscores
     * with named variables.
     *
     * The mapper keeps track of constraints that should be added to the original
     * clause it is being applied on in a given constraint set.
     */
    struct constraintNormaliser : public AstNodeMapper {
        std::set<AstBinaryConstraint*>& constraints;
        mutable int changeCount;

        constraintNormaliser(std::set<AstBinaryConstraint*>& constraints, int changeCount)
                : constraints(constraints), changeCount(changeCount) {}

        bool hasChanged() const {
            return changeCount > 0;
        }

        int getChangeCount() const {
            return changeCount;
        }

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            if (auto* stringConstant = dynamic_cast<AstStringConstant*>(node.get())) {
                // string constant found
                changeCount++;

                // create new variable name (with appropriate suffix)
                std::string constantValue = stringConstant->getConstant();
                std::stringstream newVariableName;
                newVariableName << boundPrefix << changeCount << "_" << constantValue << "_s";

                // create new constraint (+abdulX = constant)
                auto newVariable = std::make_unique<AstVariable>(newVariableName.str());
                constraints.insert(new AstBinaryConstraint(
                        BinaryConstraintOp::EQ, souffle::clone(newVariable), souffle::clone(stringConstant)));

                // update constant to be the variable created
                return newVariable;
            } else if (auto* numberConstant = dynamic_cast<AstNumericConstant*>(node.get())) {
                // number constant found
                changeCount++;

                // create new variable name (with appropriate suffix)
                std::stringstream newVariableName;
                newVariableName << boundPrefix << changeCount << "_" << numberConstant->getConstant() << "_n";

                assert(numberConstant->getType() && "numeric constant hasn't been poly-constrained");
                auto opEq = *numberConstant->getType() == AstNumericConstant::Type::Float
                                    ? BinaryConstraintOp::FEQ
                                    : BinaryConstraintOp::EQ;

                // create new constraint (+abdulX = constant)
                auto newVariable = std::make_unique<AstVariable>(newVariableName.str());
                constraints.insert(new AstBinaryConstraint(
                        opEq, souffle::clone(newVariable), souffle::clone(numberConstant)));

                // update constant to be the variable created
                return newVariable;
            } else if (dynamic_cast<AstUnnamedVariable*>(node.get()) != nullptr) {
                // underscore found
                changeCount++;

                // create new variable name
                std::stringstream newVariableName;
                newVariableName << "+underscore" << changeCount;

                return std::make_unique<AstVariable>(newVariableName.str());
            }

            node->apply(*this);
            return node;
        }
    };

    int changeCount = 0;  // number of constants and underscores seen so far

    // apply the change to all clauses in the program
    for (AstRelation* rel : program.getRelations()) {
        for (AstClause* clause : getClauses(program, *rel)) {
            if (isFact(*clause)) {
                continue;  // don't normalise facts
            }

            std::set<AstBinaryConstraint*> constraints;
            constraintNormaliser update(constraints, changeCount);
            clause->apply(update);

            changeCount = update.getChangeCount();
            changed = changed || update.hasChanged();

            for (AstBinaryConstraint* constraint : constraints) {
                clause->addToBody(std::unique_ptr<AstBinaryConstraint>(constraint));
            }
        }
    }

    return changed;
}

}  // end of namespace souffle
