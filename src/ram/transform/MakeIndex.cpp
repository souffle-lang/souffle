/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file MakeIndex.cpp
 *
 ***********************************************************************/

#include "ram/transform/MakeIndex.h"
#include "BinaryConstraintOps.h"
#include "FunctorOps.h"
#include "RamTypes.h"
#include "RelationTag.h"
#include "ram/RamCondition.h"
#include "ram/RamExpression.h"
#include "ram/RamNode.h"
#include "ram/RamOperation.h"
#include "ram/RamProgram.h"
#include "ram/RamRelation.h"
#include "ram/RamStatement.h"
#include "ram/RamUtils.h"
#include "ram/RamVisitor.h"
#include "utility/ContainerUtil.h"
#include "utility/MiscUtil.h"
#include <algorithm>
#include <cstddef>
#include <functional>
#include <tuple>
#include <utility>
#include <vector>

namespace souffle {

using ExpressionPair = std::pair<std::unique_ptr<RamExpression>, std::unique_ptr<RamExpression>>;

ExpressionPair MakeIndexTransformer::getSignedExpressionPair(
        const RamConstraint* binRelOp, size_t& element, int identifier) {
    if (isLessEqualSigned(binRelOp->getOperator())) {
        // Tuple[level, element] <= <expr>
        if (const auto* lhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getLHS())) {
            const RamExpression* rhs = &binRelOp->getRHS();
            if (lhs->getTupleId() == identifier && rla->getLevel(rhs) < identifier) {
                element = lhs->getElement();
                return {std::make_unique<RamUndefValue>(), clone(rhs)};
            }
        }
        // <expr> <= Tuple[level, element]
        if (const auto* rhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getRHS())) {
            const RamExpression* lhs = &binRelOp->getLHS();
            if (rhs->getTupleId() == identifier && rla->getLevel(lhs) < identifier) {
                element = rhs->getElement();
                return {clone(lhs), std::make_unique<RamUndefValue>()};
            }
        }
    }

    if (isGreaterEqualSigned(binRelOp->getOperator())) {
        // Tuple[level, element] >= <expr>
        if (const auto* lhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getLHS())) {
            const RamExpression* rhs = &binRelOp->getRHS();
            if (lhs->getTupleId() == identifier && rla->getLevel(rhs) < identifier) {
                element = lhs->getElement();
                return {clone(rhs), std::make_unique<RamUndefValue>()};
            }
        }
        // <expr> >= Tuple[level, element]
        if (const auto* rhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getRHS())) {
            const RamExpression* lhs = &binRelOp->getLHS();
            if (rhs->getTupleId() == identifier && rla->getLevel(lhs) < identifier) {
                element = rhs->getElement();
                return {std::make_unique<RamUndefValue>(), clone(lhs)};
            }
        }
    }
    if (isLessThanSigned(binRelOp->getOperator())) {
        // Tuple[level, element] < <expr>
        // is equivalent to
        // Tuple[level, element] <= <expr> - 1
        if (const auto* lhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getLHS())) {
            const RamExpression* rhs = &binRelOp->getRHS();
            if (lhs->getTupleId() == identifier && rla->getLevel(rhs) < identifier) {
                element = lhs->getElement();
                std::vector<std::unique_ptr<RamExpression>> expressions;
                expressions.push_back(std::unique_ptr<RamExpression>(rhs->clone()));
                expressions.push_back(std::make_unique<RamSignedConstant>(RamDomain(1)));

                return {std::make_unique<RamUndefValue>(),
                        std::make_unique<RamIntrinsicOperator>(FunctorOp::SUB, std::move(expressions))};
            }
        }
        // <expr> < Tuple[level, element]
        // is equivalent to
        // <expr> + 1 <= Tuple[level, element]
        if (const auto* rhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getRHS())) {
            const RamExpression* lhs = &binRelOp->getLHS();
            if (rhs->getTupleId() == identifier && rla->getLevel(lhs) < identifier) {
                element = rhs->getElement();
                std::vector<std::unique_ptr<RamExpression>> expressions;
                expressions.push_back(souffle::clone(lhs));
                expressions.push_back(std::make_unique<RamSignedConstant>(RamDomain(1)));

                return {std::make_unique<RamIntrinsicOperator>(FunctorOp::ADD, std::move(expressions)),
                        std::make_unique<RamUndefValue>()};
            }
        }
    }

    if (isGreaterThanSigned(binRelOp->getOperator())) {
        // Tuple[level, element] > <expr>
        // is equivalent to
        // Tuple[level, element] >= <expr> + 1
        if (const auto* lhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getLHS())) {
            const RamExpression* rhs = &binRelOp->getRHS();
            if (lhs->getTupleId() == identifier && rla->getLevel(rhs) < identifier) {
                element = lhs->getElement();
                std::vector<std::unique_ptr<RamExpression>> expressions;
                expressions.push_back(std::unique_ptr<RamExpression>(rhs->clone()));
                expressions.push_back(std::make_unique<RamSignedConstant>(RamDomain(1)));

                return {std::make_unique<RamIntrinsicOperator>(FunctorOp::ADD, std::move(expressions)),
                        std::make_unique<RamUndefValue>()};
            }
        }
        // <expr> > Tuple[level, element]
        // is equivalent to
        // <expr> - 1 >= Tuple[level, element]
        if (const auto* rhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getRHS())) {
            const RamExpression* lhs = &binRelOp->getLHS();
            if (rhs->getTupleId() == identifier && rla->getLevel(lhs) < identifier) {
                element = rhs->getElement();
                std::vector<std::unique_ptr<RamExpression>> expressions;
                expressions.push_back(std::unique_ptr<RamExpression>(lhs->clone()));
                expressions.push_back(std::make_unique<RamSignedConstant>(RamDomain(1)));

                return {std::make_unique<RamUndefValue>(),
                        std::make_unique<RamIntrinsicOperator>(FunctorOp::SUB, std::move(expressions))};
            }
        }
    }
    return {std::make_unique<RamUndefValue>(), std::make_unique<RamUndefValue>()};
}

ExpressionPair MakeIndexTransformer::getUnsignedExpressionPair(
        const RamConstraint* binRelOp, size_t& element, int identifier) {
    if (isLessEqualUnsigned(binRelOp->getOperator())) {
        // Tuple[level, element] <= <expr>
        if (const auto* lhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getLHS())) {
            const RamExpression* rhs = &binRelOp->getRHS();
            if (lhs->getTupleId() == identifier && rla->getLevel(rhs) < identifier) {
                element = lhs->getElement();
                return {std::make_unique<RamUndefValue>(), clone(rhs)};
            }
        }
        // <expr> <= Tuple[level, element]
        if (const auto* rhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getRHS())) {
            const RamExpression* lhs = &binRelOp->getLHS();
            if (rhs->getTupleId() == identifier && rla->getLevel(lhs) < identifier) {
                element = rhs->getElement();
                return {clone(lhs), std::make_unique<RamUndefValue>()};
            }
        }
    }
    if (isGreaterEqualUnsigned(binRelOp->getOperator())) {
        // Tuple[level, element] >= <expr>
        if (const auto* lhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getLHS())) {
            const RamExpression* rhs = &binRelOp->getRHS();
            if (lhs->getTupleId() == identifier && rla->getLevel(rhs) < identifier) {
                element = lhs->getElement();
                return {clone(rhs), std::make_unique<RamUndefValue>()};
            }
        }
        // <expr> >= Tuple[level, element]
        if (const auto* rhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getRHS())) {
            const RamExpression* lhs = &binRelOp->getLHS();
            if (rhs->getTupleId() == identifier && rla->getLevel(lhs) < identifier) {
                element = rhs->getElement();
                return {std::make_unique<RamUndefValue>(), clone(lhs)};
            }
        }
    }
    return {std::make_unique<RamUndefValue>(), std::make_unique<RamUndefValue>()};
}

ExpressionPair MakeIndexTransformer::getFloatExpressionPair(
        const RamConstraint* binRelOp, size_t& element, int identifier) {
    if (isLessEqualFloat(binRelOp->getOperator())) {
        // Tuple[level, element] <= <expr>
        if (const auto* lhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getLHS())) {
            const RamExpression* rhs = &binRelOp->getRHS();
            if (lhs->getTupleId() == identifier && rla->getLevel(rhs) < identifier) {
                element = lhs->getElement();
                return {std::make_unique<RamUndefValue>(), clone(rhs)};
            }
        }
        // <expr> <= Tuple[level, element]
        if (const auto* rhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getRHS())) {
            const RamExpression* lhs = &binRelOp->getLHS();
            if (rhs->getTupleId() == identifier && rla->getLevel(lhs) < identifier) {
                element = rhs->getElement();
                return {clone(lhs), std::make_unique<RamUndefValue>()};
            }
        }
    }

    if (isGreaterEqualFloat(binRelOp->getOperator())) {
        // Tuple[level, element] >= <expr>
        if (const auto* lhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getLHS())) {
            const RamExpression* rhs = &binRelOp->getRHS();
            if (lhs->getTupleId() == identifier && rla->getLevel(rhs) < identifier) {
                element = lhs->getElement();
                return {clone(rhs), std::make_unique<RamUndefValue>()};
            }
        }
        // <expr> >= Tuple[level, element]
        if (const auto* rhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getRHS())) {
            const RamExpression* lhs = &binRelOp->getLHS();
            if (rhs->getTupleId() == identifier && rla->getLevel(lhs) < identifier) {
                element = rhs->getElement();
                return {std::make_unique<RamUndefValue>(), clone(lhs)};
            }
        }
    }
    return {std::make_unique<RamUndefValue>(), std::make_unique<RamUndefValue>()};
}

// Retrieves the <expr1> <= Tuple[level, element] <= <expr2> part of the constraint as a pair { <expr1>,
// <expr2> }
ExpressionPair MakeIndexTransformer::getLowerUpperExpression(
        RamCondition* c, size_t& element, int identifier) {
    if (auto* binRelOp = dynamic_cast<RamConstraint*>(c)) {
        // TODO: FIXME: how does this interact w/ `FEQ`?

        if (isEqConstraint(binRelOp->getOperator())) {
            if (const auto* lhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getLHS())) {
                const RamExpression* rhs = &binRelOp->getRHS();
                if (lhs->getTupleId() == identifier && rla->getLevel(rhs) < identifier) {
                    element = lhs->getElement();
                    return {clone(rhs), clone(rhs)};
                }
            }
            if (const auto* rhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getRHS())) {
                const RamExpression* lhs = &binRelOp->getLHS();
                if (rhs->getTupleId() == identifier && rla->getLevel(lhs) < identifier) {
                    element = rhs->getElement();
                    return {clone(lhs), clone(lhs)};
                }
            }
        } else if (isIneqSigned(binRelOp->getOperator())) {
            return getSignedExpressionPair(binRelOp, element, identifier);
        } else if (isIneqUnsigned(binRelOp->getOperator())) {
            return getUnsignedExpressionPair(binRelOp, element, identifier);
        } else if (isIneqFloat(binRelOp->getOperator())) {
            return getFloatExpressionPair(binRelOp, element, identifier);
        }
    }
    return {std::make_unique<RamUndefValue>(), std::make_unique<RamUndefValue>()};
}

std::unique_ptr<RamCondition> MakeIndexTransformer::constructPattern(
        const std::vector<std::string>& attributeTypes, RamPattern& queryPattern, bool& indexable,
        std::vector<std::unique_ptr<RamCondition>> conditionList, int identifier) {
    // Remaining conditions which cannot be handled by an index
    std::unique_ptr<RamCondition> condition;
    auto addCondition = [&](std::unique_ptr<RamCondition> c) {
        if (condition != nullptr) {
            condition = std::make_unique<RamConjunction>(std::move(condition), std::move(c));
        } else {
            condition = std::move(c);
        }
    };

    // Build query pattern and remaining condition
    for (auto& cond : conditionList) {
        size_t element = 0;
        std::unique_ptr<RamExpression> lowerExpression;
        std::unique_ptr<RamExpression> upperExpression;
        std::tie(lowerExpression, upperExpression) = getLowerUpperExpression(cond.get(), element, identifier);
        auto type = attributeTypes[element];

        // we have new bounds if at least one is defined
        if (!isRamUndefValue(lowerExpression.get()) || !isRamUndefValue(upperExpression.get())) {
            // if no previous bounds are set then just assign them, consider both bounds to be set (but not
            // necessarily defined) in all remaining cases
            indexable = true;
            if (isRamUndefValue(queryPattern.first[element].get()) &&
                    isRamUndefValue(queryPattern.second[element].get())) {
                queryPattern.first[element] = std::move(lowerExpression);
                queryPattern.second[element] = std::move(upperExpression);
                // if lower bound is undefined and we have a new lower bound then assign it
            } else if (isRamUndefValue(queryPattern.first[element].get()) &&
                       !isRamUndefValue(lowerExpression.get()) && isRamUndefValue(upperExpression.get())) {
                queryPattern.first[element] = std::move(lowerExpression);
                // if upper bound is undefined and we have a new upper bound then assign it
            } else if (isRamUndefValue(queryPattern.second[element].get()) &&
                       isRamUndefValue(lowerExpression.get()) && !isRamUndefValue(upperExpression.get())) {
                queryPattern.second[element] = std::move(upperExpression);
                // if both bounds are defined ...
                // and equal then we have a previous equality constraint i.e. Tuple[level, element] = <expr1>
            } else if (!isRamUndefValue(queryPattern.first[element].get()) &&
                       !isRamUndefValue(queryPattern.second[element].get()) &&
                       (*(queryPattern.first[element]) == *(queryPattern.second[element]))) {
                // new equality constraint i.e. Tuple[level, element] = <expr2>
                // simply hoist <expr1> = <expr2> to the outer loop
                if (!isRamUndefValue(lowerExpression.get()) && !isRamUndefValue(upperExpression.get())) {
                    // FIXME: `FEQ` handling; need to know if the expr is a float exp or not
                    addCondition(std::make_unique<RamConstraint>(getEqConstraint(type),
                            souffle::clone(queryPattern.first[element]), std::move(lowerExpression)));
                }
                // new lower bound i.e. Tuple[level, element] >= <expr2>
                // we need to hoist <expr1> >= <expr2> to the outer loop
                else if (!isRamUndefValue(lowerExpression.get()) && isRamUndefValue(upperExpression.get())) {
                    addCondition(std::make_unique<RamConstraint>(getGreaterEqualConstraint(type),
                            souffle::clone(queryPattern.first[element]), std::move(lowerExpression)));
                }
                // new upper bound i.e. Tuple[level, element] <= <expr2>
                // we need to hoist <expr1> <= <expr2> to the outer loop
                else if (isRamUndefValue(lowerExpression.get()) && !isRamUndefValue(upperExpression.get())) {
                    addCondition(std::make_unique<RamConstraint>(getLessEqualConstraint(type),
                            souffle::clone(queryPattern.first[element]), std::move(upperExpression)));
                }
                // if either bound is defined but they aren't equal we must consider the cases for updating
                // them note that at this point we know that if we have a lower/upper bound it can't be the
                // first one
            } else if (!isRamUndefValue(queryPattern.first[element].get()) ||
                       !isRamUndefValue(queryPattern.second[element].get())) {
                // if we have a new equality constraint and previous inequality constraints
                if (!isRamUndefValue(lowerExpression.get()) && !isRamUndefValue(upperExpression.get()) &&
                        *lowerExpression == *upperExpression) {
                    // if Tuple[level, element] >= <expr1> and we see Tuple[level, element] = <expr2>
                    // need to hoist <expr2> >= <expr1> to the outer loop
                    if (!isRamUndefValue(queryPattern.first[element].get())) {
                        addCondition(std::make_unique<RamConstraint>(getGreaterEqualConstraint(type),
                                souffle::clone(lowerExpression), std::move(queryPattern.first[element])));
                    }
                    // if Tuple[level, element] <= <expr1> and we see Tuple[level, element] = <expr2>
                    // need to hoist <expr2> <= <expr1> to the outer loop
                    if (!isRamUndefValue(queryPattern.second[element].get())) {
                        addCondition(std::make_unique<RamConstraint>(getLessEqualConstraint(type),
                                souffle::clone(upperExpression), std::move(queryPattern.second[element])));
                    }
                    // finally replace bounds with equality constraint
                    queryPattern.first[element] = std::move(lowerExpression);
                    queryPattern.second[element] = std::move(upperExpression);
                    // if we have a new lower bound
                } else if (!isRamUndefValue(lowerExpression.get())) {
                    // we want the tightest lower bound so we take the max
                    std::vector<std::unique_ptr<RamExpression>> maxArguments;
                    maxArguments.push_back(std::move(queryPattern.first[element]));
                    maxArguments.push_back(std::move(lowerExpression));

                    queryPattern.first[element] =
                            std::make_unique<RamIntrinsicOperator>(getMaxOp(type), std::move(maxArguments));
                    // if we have a new upper bound
                } else if (!isRamUndefValue(upperExpression.get())) {
                    // we want the tightest upper bound so we take the min
                    std::vector<std::unique_ptr<RamExpression>> minArguments;
                    minArguments.push_back(std::move(queryPattern.second[element]));
                    minArguments.push_back(std::move(upperExpression));

                    queryPattern.second[element] =
                            std::make_unique<RamIntrinsicOperator>(getMinOp(type), std::move(minArguments));
                }
            }
        } else {
            addCondition(std::move(cond));
        }
    }

    // Avoid null-pointers for condition and query pattern
    if (condition == nullptr) {
        condition = std::make_unique<RamTrue>();
    }

    return condition;
}

std::unique_ptr<RamOperation> MakeIndexTransformer::rewriteAggregate(const RamAggregate* agg) {
    if (dynamic_cast<const RamTrue*>(&agg->getCondition()) == nullptr) {
        const RamRelation& rel = agg->getRelation();
        int identifier = agg->getTupleId();
        RamPattern queryPattern;
        for (unsigned int i = 0; i < rel.getArity(); ++i) {
            queryPattern.first.push_back(std::make_unique<RamUndefValue>());
            queryPattern.second.push_back(std::make_unique<RamUndefValue>());
        }

        bool indexable = false;
        std::unique_ptr<RamCondition> condition = constructPattern(rel.getAttributeTypes(), queryPattern,
                indexable, toConjunctionList(&agg->getCondition()), identifier);
        if (indexable) {
            return std::make_unique<RamIndexAggregate>(souffle::clone(&agg->getOperation()),
                    agg->getFunction(), std::make_unique<RamRelationReference>(&rel),
                    souffle::clone(&agg->getExpression()), std::move(condition), std::move(queryPattern),
                    agg->getTupleId());
        }
    }
    return nullptr;
}

std::unique_ptr<RamOperation> MakeIndexTransformer::rewriteScan(const RamScan* scan) {
    if (const auto* filter = dynamic_cast<const RamFilter*>(&scan->getOperation())) {
        const RamRelation& rel = scan->getRelation();
        const int identifier = scan->getTupleId();
        RamPattern queryPattern;
        for (unsigned int i = 0; i < rel.getArity(); ++i) {
            queryPattern.first.push_back(std::make_unique<RamUndefValue>());
            queryPattern.second.push_back(std::make_unique<RamUndefValue>());
        }

        bool indexable = false;
        std::unique_ptr<RamCondition> condition = constructPattern(rel.getAttributeTypes(), queryPattern,
                indexable, toConjunctionList(&filter->getCondition()), identifier);
        if (indexable) {
            std::unique_ptr<RamOperation> op = souffle::clone(&filter->getOperation());
            if (!isRamTrue(condition.get())) {
                op = std::make_unique<RamFilter>(std::move(condition), std::move(op));
            }
            return std::make_unique<RamIndexScan>(std::make_unique<RamRelationReference>(&rel), identifier,
                    std::move(queryPattern), std::move(op), scan->getProfileText());
        }
    }
    return nullptr;
}

std::unique_ptr<RamOperation> MakeIndexTransformer::rewriteIndexScan(const RamIndexScan* iscan) {
    if (const auto* filter = dynamic_cast<const RamFilter*>(&iscan->getOperation())) {
        const RamRelation& rel = iscan->getRelation();
        const int identifier = iscan->getTupleId();

        RamPattern strengthenedPattern;
        strengthenedPattern.first = clone(iscan->getRangePattern().first);
        strengthenedPattern.second = clone(iscan->getRangePattern().second);

        bool indexable = false;
        // strengthen the pattern with construct pattern
        std::unique_ptr<RamCondition> condition = constructPattern(rel.getAttributeTypes(),
                strengthenedPattern, indexable, toConjunctionList(&filter->getCondition()), identifier);

        if (indexable) {
            // Merge Index Pattern here

            std::unique_ptr<RamOperation> op = souffle::clone(&filter->getOperation());
            if (!isRamTrue(condition.get())) {
                op = std::make_unique<RamFilter>(std::move(condition), std::move(op));
            }
            return std::make_unique<RamIndexScan>(std::make_unique<RamRelationReference>(&rel), identifier,
                    std::move(strengthenedPattern), std::move(op), iscan->getProfileText());
        }
    }
    return nullptr;
}

bool MakeIndexTransformer::makeIndex(RamProgram& program) {
    bool changed = false;
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> scanRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (const RamScan* scan = dynamic_cast<RamScan*>(node.get())) {
                if (scan->getRelation().getRepresentation() != RelationRepresentation::INFO) {
                    if (std::unique_ptr<RamOperation> op = rewriteScan(scan)) {
                        changed = true;
                        node = std::move(op);
                    }
                }
            } else if (const RamIndexScan* iscan = dynamic_cast<RamIndexScan*>(node.get())) {
                if (std::unique_ptr<RamOperation> op = rewriteIndexScan(iscan)) {
                    changed = true;
                    node = std::move(op);
                }
            } else if (const RamAggregate* agg = dynamic_cast<RamAggregate*>(node.get())) {
                if (std::unique_ptr<RamOperation> op = rewriteAggregate(agg)) {
                    changed = true;
                    node = std::move(op);
                }
            }
            node->apply(makeLambdaRamMapper(scanRewriter));
            return node;
        };
        const_cast<RamQuery*>(&query)->apply(makeLambdaRamMapper(scanRewriter));
    });
    return changed;
}

}  // end of namespace souffle
