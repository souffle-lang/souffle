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
#include "ast/NumericConstant.h"
#include "ast/Program.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace souffle::ast::transform {

bool RemoveBooleanConstraintsTransformer::transform(TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();

    // If any boolean constraints exist, they will be removed
    bool changed = false;
    visit(program, [&](const BooleanConstraint&) { changed = true; });

    // Remove true and false constant literals from all aggregators
    struct removeBools : public NodeMapper {
        Own<Node> operator()(Own<Node> node) const override {
            // Remove them from child nodes
            node->apply(*this);

            if (auto* aggr = as<Aggregator>(node)) {
                bool containsTrue = false;
                bool containsFalse = false;

                // Check if aggregator body contains booleans.
                for (Literal* lit : aggr->getBodyLiterals()) {
                    if (auto* bc = as<BooleanConstraint>(lit)) {
                        if (bc->isTrue()) {
                            containsTrue = true;
                        } else {
                            containsFalse = true;
                        }
                    }
                }

                // Only keep literals that aren't boolean constraints
                if (containsFalse || containsTrue) {
                    auto replacementAggregator = clone(aggr);
                    VecOwn<Literal> newBody;

                    bool isEmpty = true;

                    // Don't bother copying over body literals if any are false
                    if (!containsFalse) {
                        for (Literal* lit : aggr->getBodyLiterals()) {
                            // Don't add in boolean constraints
                            if (!isA<BooleanConstraint>(lit)) {
                                isEmpty = false;
                                newBody.push_back(clone(lit));
                            }
                        }

                        // If the body is still empty and the original body contains true add it now.
                        if (containsTrue && isEmpty) {
                            newBody.push_back(mk<BinaryConstraint>(
                                    BinaryConstraintOp::EQ, mk<NumericConstant>(1), mk<NumericConstant>(1)));

                            isEmpty = false;
                        }
                    }

                    if (containsFalse || isEmpty) {
                        // Empty aggregator body!
                        // Not currently handled, so add in a false literal in the body
                        // E.g. max x : { } =becomes=> max 1 : {0 = 1}
                        newBody.push_back(mk<BinaryConstraint>(
                                BinaryConstraintOp::EQ, mk<NumericConstant>(0), mk<NumericConstant>(1)));
                    }

                    replacementAggregator->setBodyLiterals(std::move(newBody));
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
    for (Relation* rel : program.getRelations()) {
        for (auto&& clause : program.getClauses(*rel)) {
            bool containsTrue = false;
            bool containsFalse = false;

            for (Literal* lit : clause->getBodyLiterals()) {
                if (auto* bc = as<BooleanConstraint>(lit)) {
                    bc->isTrue() ? containsTrue = true : containsFalse = true;
                }
            }

            if (containsFalse) {
                // Clause will always fail
                program.removeClause(*clause);
            } else if (containsTrue) {
                auto replacementClause = Own<Clause>(clause->cloneHead());

                // Only keep non-'true' literals
                for (Literal* lit : clause->getBodyLiterals()) {
                    if (!isA<BooleanConstraint>(lit)) {
                        replacementClause->addToBody(clone(lit));
                    }
                }

                program.removeClause(*clause);
                program.addClause(std::move(replacementClause));
            }
        }
    }

    return changed;
}

}  // namespace souffle::ast::transform
