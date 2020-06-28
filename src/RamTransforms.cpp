/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamTransforms.cpp
 *
 * Implementation of RAM transformation passes.
 *
 ***********************************************************************/

#include "RamTransforms.h"
#include "BinaryConstraintOps.h"
#include "FunctorOps.h"
#include "RamComplexityAnalysis.h"
#include "RamCondition.h"
#include "RamExpression.h"
#include "RamNode.h"
#include "RamOperation.h"
#include "RamProgram.h"
#include "RamRelation.h"
#include "RamStatement.h"
#include "RamTypes.h"
#include "RamUtils.h"
#include "RamVisitor.h"
#include "RelationTag.h"
#include "utility/ContainerUtil.h"
#include "utility/MiscUtil.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

namespace souffle {

bool ExpandFilterTransformer::expandFilters(RamProgram& program) {
    bool changed = false;
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> filterRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (const RamFilter* filter = dynamic_cast<RamFilter*>(node.get())) {
                const RamCondition* condition = &filter->getCondition();
                std::vector<std::unique_ptr<RamCondition>> conditionList = toConjunctionList(condition);
                if (conditionList.size() > 1) {
                    changed = true;
                    std::vector<std::unique_ptr<RamFilter>> filters;
                    for (auto& cond : conditionList) {
                        auto tempCond = cond->clone();
                        if (filters.empty()) {
                            filters.emplace_back(
                                    std::make_unique<RamFilter>(std::unique_ptr<RamCondition>(tempCond),
                                            std::unique_ptr<RamOperation>(filter->getOperation().clone())));
                        } else {
                            filters.emplace_back(std::make_unique<RamFilter>(
                                    std::unique_ptr<RamCondition>(tempCond), std::move(filters.back())));
                        }
                    }
                    node = std::move(filters.back());
                }
            }
            node->apply(makeLambdaRamMapper(filterRewriter));
            return node;
        };
        const_cast<RamQuery*>(&query)->apply(makeLambdaRamMapper(filterRewriter));
    });
    return changed;
}

bool ReorderConditionsTransformer::reorderConditions(RamProgram& program) {
    bool changed = false;
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> filterRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (const RamFilter* filter = dynamic_cast<RamFilter*>(node.get())) {
                const RamCondition* condition = &filter->getCondition();
                std::vector<std::unique_ptr<RamCondition>> sortedConds;
                std::vector<std::unique_ptr<RamCondition>> condList = toConjunctionList(condition);
                for (auto& cond : condList) {
                    sortedConds.emplace_back(cond->clone());
                }
                std::sort(sortedConds.begin(), sortedConds.end(),
                        [&](std::unique_ptr<RamCondition>& a, std::unique_ptr<RamCondition>& b) {
                            return rca->getComplexity(a.get()) < rca->getComplexity(b.get());
                        });

                if (!std::equal(sortedConds.begin(), sortedConds.end(), condList.begin(),
                            [](std::unique_ptr<RamCondition>& a, std::unique_ptr<RamCondition>& b) {
                                return *a == *b;
                            })) {
                    changed = true;
                    node = std::make_unique<RamFilter>(
                            std::unique_ptr<RamCondition>(toCondition(sortedConds)),
                            std::unique_ptr<RamOperation>(filter->getOperation().clone()));
                }
            }
            node->apply(makeLambdaRamMapper(filterRewriter));
            return node;
        };
        const_cast<RamQuery*>(&query)->apply(makeLambdaRamMapper(filterRewriter));
    });
    return changed;
}

bool CollapseFiltersTransformer::collapseFilters(RamProgram& program) {
    bool changed = false;
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> filterRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (const RamFilter* filter = dynamic_cast<RamFilter*>(node.get())) {
                // true if two consecutive filters in loop nest found
                bool canCollapse = false;

                // storing conditions for collapsing
                std::vector<std::unique_ptr<RamCondition>> conditions;

                const RamFilter* prevFilter = filter;
                conditions.emplace_back(filter->getCondition().clone());
                while (auto* nextFilter = dynamic_cast<RamFilter*>(&prevFilter->getOperation())) {
                    canCollapse = true;
                    conditions.emplace_back(nextFilter->getCondition().clone());
                    prevFilter = nextFilter;
                }

                if (canCollapse) {
                    changed = true;
                    node = std::make_unique<RamFilter>(toCondition(conditions),
                            std::unique_ptr<RamOperation>(
                                    dynamic_cast<RamOperation*>(prevFilter->getOperation().clone())),
                            prevFilter->getProfileText());
                }
            }
            node->apply(makeLambdaRamMapper(filterRewriter));
            return node;
        };
        const_cast<RamQuery*>(&query)->apply(makeLambdaRamMapper(filterRewriter));
    });
    return changed;
}

bool EliminateDuplicatesTransformer::eliminateDuplicates(RamProgram& program) {
    bool changed = false;
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> filterRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (const RamFilter* filter = dynamic_cast<RamFilter*>(node.get())) {
                const RamCondition* condition = &filter->getCondition();
                std::vector<std::unique_ptr<RamCondition>> conds = toConjunctionList(condition);
                bool eliminatedDuplicate = false;
                for (std::size_t i = 0; i < conds.size(); i++) {
                    for (std::size_t j = i + 1; j < conds.size(); j++) {
                        if (*conds[i] == *conds[j]) {
                            conds.erase(conds.begin() + j);
                            i = -1;
                            eliminatedDuplicate = true;
                            break;
                        }
                    }
                }
                if (eliminatedDuplicate) {
                    changed = true;
                    node = std::make_unique<RamFilter>(std::unique_ptr<RamCondition>(toCondition(conds)),
                            std::unique_ptr<RamOperation>(filter->getOperation().clone()));
                }
            }
            node->apply(makeLambdaRamMapper(filterRewriter));
            return node;
        };
        const_cast<RamQuery*>(&query)->apply(makeLambdaRamMapper(filterRewriter));
    });
    return changed;
}
bool HoistConditionsTransformer::hoistConditions(RamProgram& program) {
    bool changed = false;

    // helper for collecting conditions from filter operations
    auto addCondition = [](std::unique_ptr<RamCondition> condition,
                                RamCondition* c) -> std::unique_ptr<RamCondition> {
        if (condition == nullptr) {
            return std::unique_ptr<RamCondition>(c);
        } else {
            return std::make_unique<RamConjunction>(std::move(condition), std::unique_ptr<RamCondition>(c));
        }
    };

    // hoist conditions to the most outer scope if they
    // don't depend on RamTupleOperations
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::unique_ptr<RamCondition> newCondition;
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> filterRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (auto* filter = dynamic_cast<RamFilter*>(node.get())) {
                const RamCondition& condition = filter->getCondition();
                // if filter condition is independent of any RamTupleOperation,
                // delete the filter operation and collect condition
                if (rla->getLevel(&condition) == -1) {
                    changed = true;
                    newCondition = addCondition(std::move(newCondition), condition.clone());
                    node->apply(makeLambdaRamMapper(filterRewriter));
                    return std::unique_ptr<RamOperation>(filter->getOperation().clone());
                }
            }
            node->apply(makeLambdaRamMapper(filterRewriter));
            return node;
        };
        auto* mQuery = const_cast<RamQuery*>(&query);
        mQuery->apply(makeLambdaRamMapper(filterRewriter));
        if (newCondition != nullptr) {
            // insert new filter operation at outer-most level of the query
            changed = true;
            auto* nestedOp = const_cast<RamOperation*>(&mQuery->getOperation());
            mQuery->rewrite(nestedOp, std::make_unique<RamFilter>(std::move(newCondition),
                                              std::unique_ptr<RamOperation>(nestedOp->clone())));
        }
    });

    // hoist conditions for each RamTupleOperation operation
    visitDepthFirst(program, [&](const RamTupleOperation& search) {
        std::unique_ptr<RamCondition> newCondition;
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> filterRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (auto* filter = dynamic_cast<RamFilter*>(node.get())) {
                const RamCondition& condition = filter->getCondition();
                // if filter condition matches level of RamTupleOperation,
                // delete the filter operation and collect condition
                if (rla->getLevel(&condition) == search.getTupleId()) {
                    changed = true;
                    newCondition = addCondition(std::move(newCondition), condition.clone());
                    node->apply(makeLambdaRamMapper(filterRewriter));
                    return std::unique_ptr<RamOperation>(filter->getOperation().clone());
                }
            }
            node->apply(makeLambdaRamMapper(filterRewriter));
            return node;
        };
        auto* tupleOp = const_cast<RamTupleOperation*>(&search);
        tupleOp->apply(makeLambdaRamMapper(filterRewriter));
        if (newCondition != nullptr) {
            // insert new filter operation after the search operation
            changed = true;
            tupleOp->rewrite(&tupleOp->getOperation(),
                    std::make_unique<RamFilter>(std::move(newCondition),
                            std::unique_ptr<RamOperation>(tupleOp->getOperation().clone())));
        }
    });
    return changed;
}

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
                expressions.push_back(std::unique_ptr<RamExpression>(lhs->clone()));
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
    if (isLessThanUnsigned(binRelOp->getOperator())) {
        // Tuple[level, element] < <expr>
        // is equivalent to
        // Tuple[level, element] <= <expr> - 1
        if (const auto* lhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getLHS())) {
            const RamExpression* rhs = &binRelOp->getRHS();
            if (dynamic_cast<const RamConstant*>(rhs)) {
                if (lhs->getTupleId() == identifier && rla->getLevel(rhs) < identifier) {
                    element = lhs->getElement();
                    std::vector<std::unique_ptr<RamExpression>> expressions;
                    expressions.push_back(std::unique_ptr<RamExpression>(rhs->clone()));
                    expressions.push_back(std::make_unique<RamUnsignedConstant>(RamUnsigned(1)));

                    return {std::make_unique<RamUndefValue>(),
                            std::make_unique<RamIntrinsicOperator>(FunctorOp::USUB, std::move(expressions))};
                }
            }
        }
        // <expr> < Tuple[level, element]
        // is equivalent to
        // <expr> + 1 <= Tuple[level, element]
        if (const auto* rhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getRHS())) {
            const RamExpression* lhs = &binRelOp->getLHS();
            if (dynamic_cast<const RamConstant*>(lhs)) {
                if (rhs->getTupleId() == identifier && rla->getLevel(lhs) < identifier) {
                    element = rhs->getElement();
                    std::vector<std::unique_ptr<RamExpression>> expressions;
                    expressions.push_back(std::unique_ptr<RamExpression>(lhs->clone()));
                    expressions.push_back(std::make_unique<RamUnsignedConstant>(RamUnsigned(1)));

                    return {std::make_unique<RamIntrinsicOperator>(FunctorOp::UADD, std::move(expressions)),
                            std::make_unique<RamUndefValue>()};
                }
            }
        }
    }

    if (isGreaterThanUnsigned(binRelOp->getOperator())) {
        // Tuple[level, element] > <expr>
        // is equivalent to
        // Tuple[level, element] >= <expr> + 1
        if (const auto* lhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getLHS())) {
            const RamExpression* rhs = &binRelOp->getRHS();
            if (dynamic_cast<const RamConstant*>(rhs)) {
                if (lhs->getTupleId() == identifier && rla->getLevel(rhs) < identifier) {
                    element = lhs->getElement();
                    std::vector<std::unique_ptr<RamExpression>> expressions;
                    expressions.push_back(std::unique_ptr<RamExpression>(rhs->clone()));
                    expressions.push_back(std::make_unique<RamUnsignedConstant>(RamUnsigned(1)));

                    return {std::make_unique<RamIntrinsicOperator>(FunctorOp::UADD, std::move(expressions)),
                            std::make_unique<RamUndefValue>()};
                }
            }
        }
        // <expr> > Tuple[level, element]
        // is equivalent to
        // <expr> - 1 >= Tuple[level, element]
        if (const auto* rhs = dynamic_cast<const RamTupleElement*>(&binRelOp->getRHS())) {
            const RamExpression* lhs = &binRelOp->getLHS();
            if (dynamic_cast<const RamConstant*>(lhs)) {
                if (rhs->getTupleId() == identifier && rla->getLevel(lhs) < identifier) {
                    element = rhs->getElement();
                    std::vector<std::unique_ptr<RamExpression>> expressions;
                    expressions.push_back(std::unique_ptr<RamExpression>(lhs->clone()));
                    expressions.push_back(std::make_unique<RamUnsignedConstant>(RamUnsigned(1)));

                    return {std::make_unique<RamUndefValue>(),
                            std::make_unique<RamIntrinsicOperator>(FunctorOp::USUB, std::move(expressions))};
                }
            }
        }
    }
    return {std::make_unique<RamUndefValue>(), std::make_unique<RamUndefValue>()};
}

ExpressionPair MakeIndexTransformer::getFloatExpressionPair(
        const RamConstraint* binRelOp, size_t& element, int identifier) {
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

std::unique_ptr<RamCondition> MakeIndexTransformer::constructPattern(RamPattern& queryPattern,
        bool& indexable, std::vector<std::unique_ptr<RamCondition>> conditionList, int identifier) {
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
                    addCondition(std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
                            std::unique_ptr<RamExpression>(queryPattern.first[element]->clone()),
                            std::move(lowerExpression)));
                }
                // new lower bound i.e. Tuple[level, element] >= <expr2>
                // we need to hoist <expr1> >= <expr2> to the outer loop
                else if (!isRamUndefValue(lowerExpression.get()) && isRamUndefValue(upperExpression.get())) {
                    addCondition(std::make_unique<RamConstraint>(BinaryConstraintOp::GE,
                            std::unique_ptr<RamExpression>(queryPattern.first[element]->clone()),
                            std::move(lowerExpression)));
                }
                // new upper bound i.e. Tuple[level, element] <= <expr2>
                // we need to hoist <expr1> <= <expr2> to the outer loop
                else if (isRamUndefValue(lowerExpression.get()) && !isRamUndefValue(upperExpression.get())) {
                    addCondition(std::make_unique<RamConstraint>(BinaryConstraintOp::LE,
                            std::unique_ptr<RamExpression>(queryPattern.first[element]->clone()),
                            std::move(upperExpression)));
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
                        addCondition(std::make_unique<RamConstraint>(BinaryConstraintOp::GE,
                                std::unique_ptr<RamExpression>(lowerExpression->clone()),
                                std::move(queryPattern.first[element])));
                    }
                    // if Tuple[level, element] <= <expr1> and we see Tuple[level, element] = <expr2>
                    // need to hoist <expr2> <= <expr1> to the outer loop
                    if (!isRamUndefValue(queryPattern.second[element].get())) {
                        addCondition(std::make_unique<RamConstraint>(BinaryConstraintOp::LE,
                                std::unique_ptr<RamExpression>(upperExpression->clone()),
                                std::move(queryPattern.second[element])));
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
                            std::make_unique<RamIntrinsicOperator>(FunctorOp::MAX, std::move(maxArguments));
                    // if we have a new upper bound
                } else if (!isRamUndefValue(upperExpression.get())) {
                    // we want the tightest upper bound so we take the min
                    std::vector<std::unique_ptr<RamExpression>> minArguments;
                    minArguments.push_back(std::move(queryPattern.second[element]));
                    minArguments.push_back(std::move(upperExpression));

                    queryPattern.second[element] =
                            std::make_unique<RamIntrinsicOperator>(FunctorOp::MIN, std::move(minArguments));
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
        std::unique_ptr<RamCondition> condition = constructPattern(
                queryPattern, indexable, toConjunctionList(&agg->getCondition()), identifier);
        if (indexable) {
            return std::make_unique<RamIndexAggregate>(
                    std::unique_ptr<RamOperation>(agg->getOperation().clone()), agg->getFunction(),
                    std::make_unique<RamRelationReference>(&rel),
                    std::unique_ptr<RamExpression>(agg->getExpression().clone()), std::move(condition),
                    std::move(queryPattern), agg->getTupleId());
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
        std::unique_ptr<RamCondition> condition = constructPattern(
                queryPattern, indexable, toConjunctionList(&filter->getCondition()), identifier);
        if (indexable) {
            std::unique_ptr<RamOperation> op = std::unique_ptr<RamOperation>(filter->getOperation().clone());
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
        std::unique_ptr<RamCondition> condition = constructPattern(
                strengthenedPattern, indexable, toConjunctionList(&filter->getCondition()), identifier);

        if (indexable) {
            // Merge Index Pattern here
            std::unique_ptr<RamOperation> op = std::unique_ptr<RamOperation>(filter->getOperation().clone());
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

bool IndexedInequalityTransformer::transformIndexToFilter(RamProgram& program) {
    bool changed = false;

    // helper for collecting conditions from filter operations
    auto addCondition = [](std::unique_ptr<RamCondition> condition,
                                RamCondition* c) -> std::unique_ptr<RamCondition> {
        if (condition == nullptr) {
            return std::unique_ptr<RamCondition>(c);
        } else {
            return std::make_unique<RamConjunction>(std::move(condition), std::unique_ptr<RamCondition>(c));
        }
    };

    visitDepthFirst(program, [&](const RamQuery& query) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> indexToFilterRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            // find a RamIndexOperation
            if (const RamIndexOperation* indexOperation = dynamic_cast<RamIndexOperation*>(node.get())) {
                auto indexSelection = idxAnalysis->getIndexes(indexOperation->getRelation());
                auto attributesToDischarge = indexSelection.getAttributesToDischarge(
                        idxAnalysis->getSearchSignature(indexOperation), indexOperation->getRelation());

                auto pattern = indexOperation->getRangePattern();
                std::unique_ptr<RamCondition> condition;
                RamPattern updatedPattern;

                for (RamExpression* p : indexOperation->getRangePattern().first) {
                    updatedPattern.first.emplace_back(p->clone());
                }
                for (RamExpression* p : indexOperation->getRangePattern().second) {
                    updatedPattern.second.emplace_back(p->clone());
                }
                for (auto i : attributesToDischarge) {
                    // move constraints out of the indexed inequality and into a conjuction
                    std::unique_ptr<RamConstraint> lowerBound;
                    std::unique_ptr<RamConstraint> upperBound;
                    changed = true;

                    if (!isRamUndefValue(pattern.first[i])) {
                        lowerBound = std::make_unique<RamConstraint>(BinaryConstraintOp::GE,
                                std::make_unique<RamTupleElement>(indexOperation->getTupleId(), i),
                                std::unique_ptr<RamExpression>(pattern.first[i]->clone()));
                        condition = addCondition(std::move(condition), lowerBound->clone());
                    }

                    if (!isRamUndefValue(pattern.second[i])) {
                        upperBound = std::make_unique<RamConstraint>(BinaryConstraintOp::LE,
                                std::make_unique<RamTupleElement>(indexOperation->getTupleId(), i),
                                std::unique_ptr<RamExpression>(pattern.second[i]->clone()));
                        condition = addCondition(std::move(condition), upperBound->clone());
                    }

                    // reset the bounds
                    updatedPattern.first[i] = std::make_unique<RamUndefValue>();
                    updatedPattern.second[i] = std::make_unique<RamUndefValue>();
                }

                if (condition) {
                    auto nestedOp = std::unique_ptr<RamOperation>(indexOperation->getOperation().clone());
                    auto filter = std::make_unique<RamFilter>(std::move(condition), std::move(nestedOp));

                    // need to rewrite the node with the same index operation
                    if (const RamIndexScan* iscan = dynamic_cast<RamIndexScan*>(node.get())) {
                        node = std::make_unique<RamIndexScan>(
                                std::make_unique<RamRelationReference>(&iscan->getRelation()),
                                iscan->getTupleId(), std::move(updatedPattern), std::move(filter),
                                iscan->getProfileText());
                    } else if (const RamParallelIndexScan* pscan =
                                       dynamic_cast<RamParallelIndexScan*>(node.get())) {
                        node = std::make_unique<RamParallelIndexScan>(
                                std::make_unique<RamRelationReference>(&pscan->getRelation()),
                                pscan->getTupleId(), std::move(updatedPattern), std::move(filter),
                                pscan->getProfileText());
                    } else if (const RamIndexChoice* ichoice = dynamic_cast<RamIndexChoice*>(node.get())) {
                        node = std::make_unique<RamIndexChoice>(
                                std::make_unique<RamRelationReference>(&ichoice->getRelation()),
                                ichoice->getTupleId(),
                                std::unique_ptr<RamCondition>(ichoice->getCondition().clone()),
                                std::move(updatedPattern), std::move(filter), ichoice->getProfileText());

                    } else if (const RamIndexAggregate* iagg = dynamic_cast<RamIndexAggregate*>(node.get())) {
                        node = std::make_unique<RamIndexAggregate>(std::move(filter), iagg->getFunction(),
                                std::make_unique<RamRelationReference>(&iagg->getRelation()),
                                std::unique_ptr<RamExpression>(iagg->getExpression().clone()),
                                std::unique_ptr<RamCondition>(iagg->getCondition().clone()),
                                std::move(updatedPattern), iagg->getTupleId());
                    } else {
                        fatal("New RamIndexOperation subclass found but not supported while making index.");
                    }
                }
            }
            node->apply(makeLambdaRamMapper(indexToFilterRewriter));
            return node;
        };
        const_cast<RamQuery*>(&query)->apply(makeLambdaRamMapper(indexToFilterRewriter));
    });

    visitDepthFirst(program, [&](const RamQuery& query) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> removeEmptyIndexRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            // find an IndexOperation
            if (const RamIndexOperation* indexOperation = dynamic_cast<RamIndexOperation*>(node.get())) {
                auto pattern = indexOperation->getRangePattern();
                size_t length = pattern.first.size();
                bool foundRealIndexableOperation = false;

                for (size_t i = 0; i < length; ++i) {
                    // if both bounds are undefined we don't have a box query
                    if (isRamUndefValue(pattern.first[i]) && isRamUndefValue(pattern.second[i])) {
                        continue;
                    }
                    // if lower and upper bounds are equal its also not a box query
                    if (*(pattern.first[i]) == *(pattern.second[i])) {
                        foundRealIndexableOperation = true;
                        break;
                    } else {
                        foundRealIndexableOperation = true;
                        break;
                    }
                }
                if (!foundRealIndexableOperation) {
                    // need to rewrite the node with a semantically equivalent operation to get rid of the
                    // index operation i.e. RamIndexScan with no indexable attributes -> RamScan
                    if (const RamIndexScan* iscan = dynamic_cast<RamIndexScan*>(node.get())) {
                        node = std::make_unique<RamScan>(
                                std::make_unique<RamRelationReference>(&iscan->getRelation()),
                                iscan->getTupleId(),
                                std::unique_ptr<RamOperation>(iscan->getOperation().clone()),
                                iscan->getProfileText());
                    } else if (const RamParallelIndexScan* pscan =
                                       dynamic_cast<RamParallelIndexScan*>(node.get())) {
                        node = std::make_unique<RamParallelScan>(
                                std::make_unique<RamRelationReference>(&pscan->getRelation()),
                                pscan->getTupleId(),
                                std::unique_ptr<RamOperation>(pscan->getOperation().clone()),
                                pscan->getProfileText());
                    } else if (const RamIndexChoice* ichoice = dynamic_cast<RamIndexChoice*>(node.get())) {
                        node = std::make_unique<RamChoice>(
                                std::make_unique<RamRelationReference>(&ichoice->getRelation()),
                                ichoice->getTupleId(),
                                std::unique_ptr<RamCondition>(ichoice->getCondition().clone()),
                                std::unique_ptr<RamOperation>(ichoice->getOperation().clone()),
                                ichoice->getProfileText());
                    } else if (const RamIndexAggregate* iagg = dynamic_cast<RamIndexAggregate*>(node.get())) {
                        node = std::make_unique<RamAggregate>(
                                std::unique_ptr<RamOperation>(iagg->getOperation().clone()),
                                iagg->getFunction(),
                                std::make_unique<RamRelationReference>(&iagg->getRelation()),
                                std::unique_ptr<RamExpression>(iagg->getExpression().clone()),
                                std::unique_ptr<RamCondition>(iagg->getCondition().clone()),
                                iagg->getTupleId());
                    } else {
                        fatal("New RamIndexOperation subclass found but not supported while transforming "
                              "index.");
                    }
                }
            }
            node->apply(makeLambdaRamMapper(removeEmptyIndexRewriter));
            return node;
        };
        const_cast<RamQuery*>(&query)->apply(makeLambdaRamMapper(removeEmptyIndexRewriter));
    });
    return changed;
}

bool ReorderFilterBreak::reorderFilterBreak(RamProgram& program) {
    bool changed = false;
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> filterRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            // find filter-break nesting
            if (const RamFilter* filter = dynamic_cast<RamFilter*>(node.get())) {
                if (const RamBreak* br = dynamic_cast<RamBreak*>(&filter->getOperation())) {
                    changed = true;
                    // convert to break-filter nesting
                    node = std::make_unique<RamBreak>(
                            std::unique_ptr<RamCondition>(br->getCondition().clone()),
                            std::make_unique<RamFilter>(
                                    std::unique_ptr<RamCondition>(filter->getCondition().clone()),
                                    std::unique_ptr<RamOperation>(br->getOperation().clone())));
                }
            }
            node->apply(makeLambdaRamMapper(filterRewriter));
            return node;
        };
        const_cast<RamQuery*>(&query)->apply(makeLambdaRamMapper(filterRewriter));
    });
    return changed;
}

std::unique_ptr<RamOperation> IfConversionTransformer::rewriteIndexScan(const RamIndexScan* indexScan) {
    // check whether tuple is used in subsequent operations
    bool tupleNotUsed = true;
    visitDepthFirst(*indexScan, [&](const RamTupleElement& element) {
        if (element.getTupleId() == indexScan->getTupleId()) {
            tupleNotUsed = false;
        }
    });

    // if not used, transform the IndexScan operation to an existence check
    if (tupleNotUsed) {
        // replace IndexScan with an Filter/Existence check
        std::vector<std::unique_ptr<RamExpression>> newValues;

        size_t arity = indexScan->getRangePattern().first.size();
        for (size_t i = 0; i < arity; ++i) {
            if (*indexScan->getRangePattern().first[i] != *indexScan->getRangePattern().second[i]) {
                return nullptr;
            }
        }

        for (auto& cur : indexScan->getRangePattern().second) {
            RamExpression* val = nullptr;
            if (cur != nullptr) {
                val = cur->clone();
            }
            newValues.emplace_back(val);
        }

        // check if there is a break statement nested in the Scan - if so, remove it
        RamOperation* newOp;
        if (const auto* breakOp = dynamic_cast<const RamBreak*>(&indexScan->getOperation())) {
            newOp = breakOp->getOperation().clone();
        } else {
            newOp = indexScan->getOperation().clone();
        }

        return std::make_unique<RamFilter>(
                std::make_unique<RamExistenceCheck>(
                        std::make_unique<RamRelationReference>(&indexScan->getRelation()),
                        std::move(newValues)),
                std::unique_ptr<RamOperation>(newOp), indexScan->getProfileText());
    }
    return nullptr;
}

bool IfConversionTransformer::convertIndexScans(RamProgram& program) {
    bool changed = false;
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> scanRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (const RamIndexScan* scan = dynamic_cast<RamIndexScan*>(node.get())) {
                if (std::unique_ptr<RamOperation> op = rewriteIndexScan(scan)) {
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

std::unique_ptr<RamOperation> ChoiceConversionTransformer::rewriteScan(const RamScan* scan) {
    bool transformTuple = false;

    // Check that RamFilter follows the Scan in the loop nest
    if (const auto* filter = dynamic_cast<const RamFilter*>(&scan->getOperation())) {
        // Check that the Filter uses the identifier in the Scan
        if (rla->getLevel(&filter->getCondition()) == scan->getTupleId()) {
            transformTuple = true;

            // Check that the filter is not referred to after
            const auto* nextNode = dynamic_cast<const RamNode*>(&filter->getOperation());

            visitDepthFirst(*nextNode, [&](const RamTupleElement& element) {
                if (element.getTupleId() == scan->getTupleId()) {
                    transformTuple = false;
                }
            });
        }
    }

    // Convert the Scan/If pair into a Choice
    if (transformTuple) {
        std::vector<std::unique_ptr<RamExpression>> newValues;
        const auto* filter = dynamic_cast<const RamFilter*>(&scan->getOperation());
        const int identifier = scan->getTupleId();

        return std::make_unique<RamChoice>(std::make_unique<RamRelationReference>(&scan->getRelation()),
                identifier, std::unique_ptr<RamCondition>(filter->getCondition().clone()),
                std::unique_ptr<RamOperation>(scan->getOperation().clone()), scan->getProfileText());
    }
    return nullptr;
}

std::unique_ptr<RamOperation> ChoiceConversionTransformer::rewriteIndexScan(const RamIndexScan* indexScan) {
    bool transformTuple = false;

    // Check that RamFilter follows the IndexScan in the loop nest
    if (const auto* filter = dynamic_cast<const RamFilter*>(&indexScan->getOperation())) {
        // Check that the Filter uses the identifier in the IndexScan
        if (rla->getLevel(&filter->getCondition()) == indexScan->getTupleId()) {
            transformTuple = true;

            // Check that the filter is not referred to after
            const auto* nextNode = dynamic_cast<const RamNode*>(&filter->getOperation());

            visitDepthFirst(*nextNode, [&](const RamTupleElement& element) {
                if (element.getTupleId() == indexScan->getTupleId()) {
                    transformTuple = false;
                }
            });
        }
    }

    // Convert the IndexScan/If pair into an IndexChoice
    if (transformTuple) {
        RamPattern newValues;
        const auto* filter = dynamic_cast<const RamFilter*>(&indexScan->getOperation());
        const int identifier = indexScan->getTupleId();
        const RamRelation& rel = indexScan->getRelation();

        for (auto& cur : indexScan->getRangePattern().first) {
            RamExpression* val = nullptr;
            if (cur != nullptr) {
                val = cur->clone();
            }
            newValues.first.emplace_back(val);
        }
        for (auto& cur : indexScan->getRangePattern().second) {
            RamExpression* val = nullptr;
            if (cur != nullptr) {
                val = cur->clone();
            }
            newValues.second.emplace_back(val);
        }

        return std::make_unique<RamIndexChoice>(std::make_unique<RamRelationReference>(&rel), identifier,
                std::unique_ptr<RamCondition>(filter->getCondition().clone()), std::move(newValues),
                std::unique_ptr<RamOperation>(filter->getOperation().clone()), indexScan->getProfileText());
    }
    return nullptr;
}

bool ChoiceConversionTransformer::convertScans(RamProgram& program) {
    bool changed = false;
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> scanRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (const RamScan* scan = dynamic_cast<RamScan*>(node.get())) {
                if (std::unique_ptr<RamOperation> op = rewriteScan(scan)) {
                    changed = true;
                    node = std::move(op);
                }
            } else if (const RamIndexScan* indexScan = dynamic_cast<RamIndexScan*>(node.get())) {
                if (std::unique_ptr<RamOperation> op = rewriteIndexScan(indexScan)) {
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

bool TupleIdTransformer::reorderOperations(RamProgram& program) {
    bool changed = false;
    visitDepthFirst(program, [&](const RamQuery& query) {
        // Maps old tupleIds to new tupleIds
        std::map<int, int> reorder;
        int ctr = 0;

        visitDepthFirst(query, [&](const RamTupleOperation& search) {
            if (ctr != search.getTupleId()) {
                changed = true;
            }
            reorder[search.getTupleId()] = ctr;
            const_cast<RamTupleOperation*>(&search)->setTupleId(ctr);
            ctr++;
        });

        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> elementRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (auto* element = dynamic_cast<RamTupleElement*>(node.get())) {
                if (reorder[element->getTupleId()] != element->getTupleId()) {
                    changed = true;
                    node = std::make_unique<RamTupleElement>(
                            reorder[element->getTupleId()], element->getElement());
                }
            }
            node->apply(makeLambdaRamMapper(elementRewriter));
            return node;
        };
        const_cast<RamQuery*>(&query)->apply(makeLambdaRamMapper(elementRewriter));
    });

    return changed;
}

bool HoistAggregateTransformer::hoistAggregate(RamProgram& program) {
    bool changed = false;

    // There are two cases: aggregates that have no data-dependencies on
    // other RAM operations, and aggregates that have data-dependencies.
    // A rewriter has two tasks: (1) identify a single aggregate that
    // can be hoisted and (2) insert it at the outermost level.
    // We assume all RamOperations are renumbered for this transformation.

    // Hoist a single aggregate to an outer scope that is data-independent.
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::unique_ptr<RamNestedOperation> newAgg;
        bool priorTupleOp = false;
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> aggRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (nullptr != dynamic_cast<RamAggregate*>(node.get())) {
                auto* tupleOp = dynamic_cast<RamTupleOperation*>(node.get());
                assert(tupleOp != nullptr && "aggregate conversion to tuple operation failed");
                if (rla->getLevel(tupleOp) == -1 && !priorTupleOp) {
                    changed = true;
                    newAgg = std::unique_ptr<RamNestedOperation>(
                            dynamic_cast<RamNestedOperation*>(tupleOp->clone()));
                    assert(newAgg != nullptr && "failed to make a clone");
                    return std::unique_ptr<RamOperation>(tupleOp->getOperation().clone());
                }
            } else if (nullptr != dynamic_cast<RamTupleOperation*>(node.get())) {
                // tuple operation that is a non-aggregate
                priorTupleOp = true;
            }
            node->apply(makeLambdaRamMapper(aggRewriter));
            return node;
        };
        const_cast<RamQuery*>(&query)->apply(makeLambdaRamMapper(aggRewriter));
        if (newAgg != nullptr) {
            newAgg->rewrite(
                    &newAgg->getOperation(), std::unique_ptr<RamOperation>(query.getOperation().clone()));
            const_cast<RamQuery*>(&query)->rewrite(&query.getOperation(), std::move(newAgg));
        }
    });

    // hoist a single aggregate to an outer scope that is data-dependent on a prior operation.
    visitDepthFirst(program, [&](const RamQuery& query) {
        int newLevel = -1;
        std::unique_ptr<RamNestedOperation> newAgg;
        int priorOpLevel = -1;

        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> aggRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (nullptr != dynamic_cast<RamAbstractAggregate*>(node.get())) {
                auto* tupleOp = dynamic_cast<RamTupleOperation*>(node.get());
                assert(tupleOp != nullptr && "aggregate conversion to nested operation failed");
                int dataDepLevel = rla->getLevel(tupleOp);
                if (dataDepLevel != -1 && dataDepLevel < tupleOp->getTupleId() - 1) {
                    // If all tuple ops between the data-dependence level and agg
                    // are aggregates, then we do not hoist, i.e., we would
                    // continuously swap their positions.
                    if (dataDepLevel != priorOpLevel) {
                        changed = true;
                        newLevel = dataDepLevel;
                        newAgg = std::unique_ptr<RamNestedOperation>(
                                dynamic_cast<RamNestedOperation*>(tupleOp->clone()));
                        assert(newAgg != nullptr && "failed to make a clone");
                        return std::unique_ptr<RamOperation>(tupleOp->getOperation().clone());
                    }
                }
            } else if (const RamTupleOperation* tupleOp = dynamic_cast<RamTupleOperation*>(node.get())) {
                priorOpLevel = tupleOp->getTupleId();
            }
            node->apply(makeLambdaRamMapper(aggRewriter));
            if (auto* search = dynamic_cast<RamTupleOperation*>(node.get())) {
                if (newAgg != nullptr && search->getTupleId() == newLevel) {
                    newAgg->rewrite(&newAgg->getOperation(),
                            std::unique_ptr<RamOperation>(search->getOperation().clone()));
                    search->rewrite(&search->getOperation(), std::move(newAgg));
                }
            }
            return node;
        };
        const_cast<RamQuery*>(&query)->apply(makeLambdaRamMapper(aggRewriter));
    });
    return changed;
}  // namespace souffle

bool ParallelTransformer::parallelizeOperations(RamProgram& program) {
    bool changed = false;

    // parallelize the most outer loop only
    // most outer loops can be scan/choice/indexScan/indexChoice
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> parallelRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (const RamScan* scan = dynamic_cast<RamScan*>(node.get())) {
                if (scan->getTupleId() == 0 && scan->getRelation().getArity() > 0) {
                    if (nullptr == dynamic_cast<RamProject*>(&scan->getOperation())) {
                        changed = true;
                        return std::make_unique<RamParallelScan>(
                                std::make_unique<RamRelationReference>(&scan->getRelation()),
                                scan->getTupleId(),
                                std::unique_ptr<RamOperation>(scan->getOperation().clone()),
                                scan->getProfileText());
                    }
                }
            } else if (const RamChoice* choice = dynamic_cast<RamChoice*>(node.get())) {
                if (choice->getTupleId() == 0) {
                    changed = true;
                    return std::make_unique<RamParallelChoice>(
                            std::make_unique<RamRelationReference>(&choice->getRelation()),
                            choice->getTupleId(),
                            std::unique_ptr<RamCondition>(choice->getCondition().clone()),
                            std::unique_ptr<RamOperation>(choice->getOperation().clone()),
                            choice->getProfileText());
                }
            } else if (const RamIndexScan* indexScan = dynamic_cast<RamIndexScan*>(node.get())) {
                if (indexScan->getTupleId() == 0) {
                    changed = true;
                    const RamRelation& rel = indexScan->getRelation();
                    RamPattern queryPattern = clone(indexScan->getRangePattern());
                    return std::make_unique<RamParallelIndexScan>(
                            std::make_unique<RamRelationReference>(&rel), indexScan->getTupleId(),
                            std::move(queryPattern),
                            std::unique_ptr<RamOperation>(indexScan->getOperation().clone()),
                            indexScan->getProfileText());
                }
            } else if (const RamIndexChoice* indexChoice = dynamic_cast<RamIndexChoice*>(node.get())) {
                if (indexChoice->getTupleId() == 0) {
                    changed = true;
                    const RamRelation& rel = indexChoice->getRelation();
                    RamPattern queryPattern = clone(indexChoice->getRangePattern());
                    return std::make_unique<RamParallelIndexChoice>(
                            std::make_unique<RamRelationReference>(&rel), indexChoice->getTupleId(),
                            std::unique_ptr<RamCondition>(indexChoice->getCondition().clone()),
                            std::move(queryPattern),
                            std::unique_ptr<RamOperation>(indexChoice->getOperation().clone()),
                            indexChoice->getProfileText());
                }
            }
            node->apply(makeLambdaRamMapper(parallelRewriter));
            return node;
        };
        const_cast<RamQuery*>(&query)->apply(makeLambdaRamMapper(parallelRewriter));
    });
    return changed;
}

}  // end of namespace souffle
