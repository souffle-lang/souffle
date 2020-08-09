/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RemoveBooleanConstraints.cpp
 *
 ***********************************************************************/

#include "ast/transform/RemoveBooleanConstraints.h"
#include "ast/Aggregator.h"
#include "ast/BinaryConstraint.h"
#include "ast/BooleanConstraint.h"
#include "ast/Clause.h"
#include "ast/Literal.h"
#include "ast/Node.h"
#include "ast/NodeMapper.h"
#include "ast/NumericConstant.h"
#include "ast/Program.h"
#include "ast/TranslationUnit.h"
#include "ast/Utils.h"
#include "ast/Visitor.h"
#include "parser/BinaryConstraintOps.h"
#include "utility/MiscUtil.h"
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace souffle {
class AstRelation;

bool RemoveBooleanConstraintsTransformer::transform(AstTranslationUnit& translationUnit) {
    AstProgram& program = *translationUnit.getProgram();

    // If any boolean constraints exist, they will be removed
    bool changed = false;
    visitDepthFirst(program, [&](const AstBooleanConstraint&) { changed = true; });

    // Remove true and false constant literals from all aggregators
    struct removeBools : public AstNodeMapper {
        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            // Remove them from child nodes
            node->apply(*this);

            if (auto* aggr = dynamic_cast<AstAggregator*>(node.get())) {
                bool containsTrue = false;
                bool containsFalse = false;

                // Check if aggregator body contains booleans.
                for (AstLiteral* lit : aggr->getBodyLiterals()) {
                    if (auto* bc = dynamic_cast<AstBooleanConstraint*>(lit)) {
                        if (bc->isTrue()) {
                            containsTrue = true;
                        } else {
                            containsFalse = true;
                        }
                    }
                }

                // Only keep literals that aren't boolean constraints
                if (containsFalse || containsTrue) {
                    auto replacementAggregator = souffle::clone(aggr);
                    std::vector<std::unique_ptr<AstLiteral>> newBody;

                    bool isEmpty = true;

                    // Don't bother copying over body literals if any are false
                    if (!containsFalse) {
                        for (AstLiteral* lit : aggr->getBodyLiterals()) {
                            // Don't add in boolean constraints
                            if (dynamic_cast<AstBooleanConstraint*>(lit) == nullptr) {
                                isEmpty = false;
                                newBody.push_back(souffle::clone(lit));
                            }
                        }

                        // If the body is still empty and the original body contains true add it now.
                        if (containsTrue && isEmpty) {
                            newBody.push_back(std::make_unique<AstBinaryConstraint>(BinaryConstraintOp::EQ,
                                    std::make_unique<AstNumericConstant>(1),
                                    std::make_unique<AstNumericConstant>(1)));

                            isEmpty = false;
                        }
                    }

                    if (containsFalse || isEmpty) {
                        // Empty aggregator body!
                        // Not currently handled, so add in a false literal in the body
                        // E.g. max x : { } =becomes=> max 1 : {0 = 1}
                        newBody.push_back(std::make_unique<AstBinaryConstraint>(BinaryConstraintOp::EQ,
                                std::make_unique<AstNumericConstant>(0),
                                std::make_unique<AstNumericConstant>(1)));
                    }

                    replacementAggregator->setBody(std::move(newBody));
                    return replacementAggregator;
                }
            }

            // no false or true, so return the original node
            return node;
        }
    };

    removeBools update;
    program.apply(update);

    // Remove true and false constant literals from all clauses
    for (AstRelation* rel : program.getRelations()) {
        for (AstClause* clause : getClauses(program, *rel)) {
            bool containsTrue = false;
            bool containsFalse = false;

            for (AstLiteral* lit : clause->getBodyLiterals()) {
                if (auto* bc = dynamic_cast<AstBooleanConstraint*>(lit)) {
                    bc->isTrue() ? containsTrue = true : containsFalse = true;
                }
            }

            if (containsFalse) {
                // Clause will always fail
                program.removeClause(clause);
            } else if (containsTrue) {
                auto replacementClause = std::unique_ptr<AstClause>(cloneHead(clause));

                // Only keep non-'true' literals
                for (AstLiteral* lit : clause->getBodyLiterals()) {
                    if (dynamic_cast<AstBooleanConstraint*>(lit) == nullptr) {
                        replacementClause->addToBody(souffle::clone(lit));
                    }
                }

                program.removeClause(clause);
                program.addClause(std::move(replacementClause));
            }
        }
    }

    return changed;
}

}  // end of namespace souffle
