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
#include "FunctorOps.h"
#include "Global.h"
#include "RelationTag.h"
#include "ram/Condition.h"
#include "ram/Constraint.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Program.h"
#include "ram/Relation.h"
#include "ram/Statement.h"
#include "ram/utility/NodeMapper.h"
#include "ram/utility/Utils.h"
#include "ram/utility/Visitor.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/RamTypes.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

namespace souffle::ram::transform {

using ExpressionPair = std::pair<Own<Expression>, Own<Expression>>;

ExpressionPair MakeIndexTransformer::getExpressionPair(
        const Constraint* binRelOp, std::size_t& element, const std::optional<std::size_t>& identifier) {
    if (isLessEqual(binRelOp->getOperator())) {
        // Tuple[level, element] <= <expr>
        if (const auto* lhs = as<TupleElement>(binRelOp->getLHS())) {
            const Expression* rhs = &binRelOp->getRHS();
            if (lhs->getTupleId() == identifier && rla->getLevel(rhs) < identifier) {
                element = lhs->getElement();
                return {mk<UndefValue>(), clone(rhs)};
            }
        }
        // <expr> <= Tuple[level, element]
        if (const auto* rhs = as<TupleElement>(binRelOp->getRHS())) {
            const Expression* lhs = &binRelOp->getLHS();
            if (rhs->getTupleId() == identifier && rla->getLevel(lhs) < identifier) {
                element = rhs->getElement();
                return {clone(lhs), mk<UndefValue>()};
            }
        }
    }

    if (isGreaterEqual(binRelOp->getOperator())) {
        // Tuple[level, element] >= <expr>
        if (const auto* lhs = as<TupleElement>(binRelOp->getLHS())) {
            const Expression* rhs = &binRelOp->getRHS();
            if (lhs->getTupleId() == identifier && rla->getLevel(rhs) < identifier) {
                element = lhs->getElement();
                return {clone(rhs), mk<UndefValue>()};
            }
        }
        // <expr> >= Tuple[level, element]
        if (const auto* rhs = as<TupleElement>(binRelOp->getRHS())) {
            const Expression* lhs = &binRelOp->getLHS();
            if (rhs->getTupleId() == identifier && rla->getLevel(lhs) < identifier) {
                element = rhs->getElement();
                return {mk<UndefValue>(), clone(lhs)};
            }
        }
    }
    return {mk<UndefValue>(), mk<UndefValue>()};
}

// Retrieves the <expr1> <= Tuple[level, element] <= <expr2> part of the constraint as a pair { <expr1>,
// <expr2> }
ExpressionPair MakeIndexTransformer::getLowerUpperExpression(Condition* c, std::size_t& element,
        const std::optional<std::size_t>& identifier, RelationRepresentation rep) {
    if (auto* binRelOp = as<Constraint>(c)) {
        bool interpreter = !Global::config().has("compile") && !Global::config().has("dl-program") &&
                           !Global::config().has("generate") && !Global::config().has("swig");
        bool provenance = rep == RelationRepresentation::PROVENANCE;
        bool btree = (rep == RelationRepresentation::BTREE || rep == RelationRepresentation::DEFAULT ||
                      rep == RelationRepresentation::BTREE_DELETE);
        auto op = binRelOp->getOperator();

        // don't index FEQ in interpreter mode
        if (op == BinaryConstraintOp::FEQ && interpreter) {
            return {mk<UndefValue>(), mk<UndefValue>()};
        }
        // don't index any inequalities that aren't signed
        if (isIneqConstraint(op) && !isSignedInequalityConstraint(op) && interpreter) {
            return {mk<UndefValue>(), mk<UndefValue>()};
        }
        // don't index inequalities for provenance
        if (isIneqConstraint(op) && provenance) {
            return {mk<UndefValue>(), mk<UndefValue>()};
        }
        // don't index inequalities if we aren't using a BTREE
        if (isIneqConstraint(op) && !btree) {
            return {mk<UndefValue>(), mk<UndefValue>()};
        }

        if (isEqConstraint(op)) {
            if (const auto* lhs = as<TupleElement>(binRelOp->getLHS())) {
                const Expression* rhs = &binRelOp->getRHS();
                if (lhs->getTupleId() == identifier && rla->getLevel(rhs) < identifier) {
                    element = lhs->getElement();
                    return {clone(rhs), clone(rhs)};
                }
            }
            if (const auto* rhs = as<TupleElement>(binRelOp->getRHS())) {
                const Expression* lhs = &binRelOp->getLHS();
                if (rhs->getTupleId() == identifier && rla->getLevel(lhs) < identifier) {
                    element = rhs->getElement();
                    return {clone(lhs), clone(lhs)};
                }
            }
        }

        if (isWeakIneqConstraint(op)) {
            return getExpressionPair(binRelOp, element, identifier);
        }
    }
    return {mk<UndefValue>(), mk<UndefValue>()};
}

Own<Condition> MakeIndexTransformer::constructPattern(const std::vector<std::string>& attributeTypes,
        RamPattern& queryPattern, bool& indexable, VecOwn<Condition> conditionList, std::size_t identifier,
        RelationRepresentation rep) {
    // Remaining conditions which cannot be handled by an index
    Own<Condition> condition;
    auto addCondition = [&](Own<Condition> c) {
        if (condition != nullptr) {
            condition = mk<Conjunction>(std::move(condition), std::move(c));
        } else {
            condition = std::move(c);
        }
    };

    // transform condition list so that every strict inequality becomes a weak inequality + filter
    // e.g. Tuple[level, element] < <expr> --> Tuple[level, element] <= <expr> and Tuple[level, element] !=
    // <expr>
    std::vector<Own<Condition>> toAppend;
    auto it = conditionList.begin();
    while (it != conditionList.end()) {
        auto* binRelOp = as<Constraint>(*it);
        if (binRelOp == nullptr) {
            ++it;
            continue;
        }

        bool transformable = false;

        if (isStrictIneqConstraint(binRelOp->getOperator())) {
            if (const auto* lhs = as<TupleElement>(binRelOp->getLHS())) {
                const Expression* rhs = &binRelOp->getRHS();
                if (lhs->getTupleId() == identifier && rla->getLevel(rhs) < identifier) {
                    transformable = true;
                }
            }
            if (const auto* rhs = as<TupleElement>(binRelOp->getRHS())) {
                const Expression* lhs = &binRelOp->getLHS();
                if (rhs->getTupleId() == identifier && rla->getLevel(lhs) < identifier) {
                    transformable = true;
                }
            }
        }

        if (transformable) {
            // append the weak version of inequality
            toAppend.emplace_back(mk<Constraint>(convertStrictToWeakIneqConstraint(binRelOp->getOperator()),
                    clone(binRelOp->getLHS()), clone(binRelOp->getRHS())));
            // append the != constraint
            toAppend.emplace_back(mk<Constraint>(convertStrictToNotEqualConstraint(binRelOp->getOperator()),
                    clone(binRelOp->getLHS()), clone(binRelOp->getRHS())));

            // remove the strict version of inequality
            it = conditionList.erase(it);
        } else {
            ++it;
        }
    }

    std::transform(toAppend.begin(), toAppend.end(), std::back_inserter(conditionList),
            [](const Own<Condition>& cond) { return clone(cond); });

    // Define a comparator which orders all of the conditions nicely
    // 1. Equalities come before inequalities
    // 2. Conditions are ordered by the index of the constraint i.e. t0.0 comes before t0.1
    auto cmp = [&](auto& c1, auto& c2) -> bool {
        auto* cond1 = as<Constraint>(c1);
        auto* cond2 = as<Constraint>(c2);
        // place non-conditions at the end
        if (!cond1 && !cond2) {
            return c1.get() < c2.get();
        }
        if (cond1 && !cond2) {
            return true;
        }

        if (!cond1 && cond2) {
            return false;
        }
        // if it's not indexable place the condition at the end
        bool lhsIndexable = isIndexableConstraint(cond1->getOperator());
        bool rhsIndexable = isIndexableConstraint(cond2->getOperator());

        if (!lhsIndexable && !rhsIndexable) {
            return c1.get() < c2.get();
        }
        if (lhsIndexable && !rhsIndexable) {
            return true;
        }
        if (!lhsIndexable && rhsIndexable) {
            return false;
        }

        // eq before ineq otherwise we compare the attribute of the involved relation
        if (isEqConstraint(cond1->getOperator()) && isIneqConstraint(cond2->getOperator())) {
            return true;
        }
        if (isIneqConstraint(cond1->getOperator()) && isEqConstraint(cond2->getOperator())) {
            return false;
        }

        std::size_t attr1 = 0;
        std::size_t attr2 = 0;
        const auto p1 = getExpressionPair(cond1, attr1, identifier);
        const auto p2 = getExpressionPair(cond2, attr2, identifier);

        // check if the constraint is of the right form
        bool lhsUndefined = isUndefValue(p1.first.get()) && isUndefValue(p1.second.get());
        bool rhsUndefined = isUndefValue(p2.first.get()) && isUndefValue(p2.second.get());

        if (lhsUndefined && rhsUndefined) {
            return c1.get() < c2.get();
        }

        if (!lhsUndefined && rhsUndefined) {
            return true;
        }

        if (lhsUndefined && !rhsUndefined) {
            return false;
        }

        // at this point we are guaranteed that attr1 and attr2 are set
        return attr1 < attr2;
    };

    std::sort(conditionList.begin(), conditionList.end(), cmp);

    // Build query pattern and remaining condition
    bool seenInequality = false;

    std::size_t arity = queryPattern.first.size();
    for (std::size_t i = 0; i < arity; ++i) {
        // ignore attributes with no constraints
        if (isUndefValue(queryPattern.first[i].get()) && isUndefValue(queryPattern.second[i].get())) {
            continue;
        }
        // found an inequality
        if (*queryPattern.first[i] != *queryPattern.second[i]) {
            seenInequality = true;
            break;
        }
    }

    for (auto& cond : conditionList) {
        std::size_t element = 0;
        Own<Expression> lowerExpression;
        Own<Expression> upperExpression;
        std::tie(lowerExpression, upperExpression) =
                getLowerUpperExpression(cond.get(), element, identifier, rep);

        // we have new bounds if at least one is defined
        if (!isUndefValue(lowerExpression.get()) || !isUndefValue(upperExpression.get())) {
            // if no previous bounds are set then just assign them, consider both bounds to be set (but not
            // necessarily defined) in all remaining cases

            bool firstLowerBound = isUndefValue(queryPattern.first[element].get());
            bool firstUpperBound = isUndefValue(queryPattern.second[element].get());
            bool firstConstraint = firstLowerBound && firstUpperBound;

            bool newLowerBound = !isUndefValue(lowerExpression.get());
            bool newUpperBound = !isUndefValue(upperExpression.get());

            bool equality = (*lowerExpression == *upperExpression);
            bool inequality = !equality;

            auto& lowerBound = queryPattern.first[element];
            auto& upperBound = queryPattern.second[element];

            // don't permit multiple inequalities
            // TODO: @SamArch27 invariant that we have at most one indexed inequality per relation
            if (firstConstraint && inequality && seenInequality) {
                addCondition(std::move(cond));
                continue;
            }

            auto type = attributeTypes[element];
            indexable = true;
            if (firstConstraint) {
                // equality
                lowerBound = std::move(lowerExpression);
                upperBound = std::move(upperExpression);

                // seen inequality
                if (inequality) {
                    seenInequality = true;
                }

                // if lower bound is undefined and we have a new lower bound then assign it
            } else if (firstLowerBound && newLowerBound && !newUpperBound) {
                lowerBound = std::move(lowerExpression);
                // if upper bound is undefined and we have a new upper bound then assign it
            } else if (firstUpperBound && !newLowerBound && newUpperBound) {
                upperBound = std::move(upperExpression);
                // if both bounds are defined ...
                // and equal then we have a previous equality constraint i.e. Tuple[level, element] = <expr1>
            } else if (!firstLowerBound && !firstUpperBound && (*(lowerBound) == *(upperBound))) {
                // new equality constraint i.e. Tuple[level, element] = <expr2>
                // simply hoist <expr1> = <expr2> to the outer loop
                if (newLowerBound && newUpperBound) {
                    addCondition(mk<Constraint>(
                            getEqConstraint(type), clone(lowerBound), std::move(lowerExpression)));
                }
                // new lower bound i.e. Tuple[level, element] >= <expr2>
                // we need to hoist <expr1> >= <expr2> to the outer loop
                else if (newLowerBound && !newUpperBound) {
                    addCondition(mk<Constraint>(
                            getGreaterEqualConstraint(type), clone(lowerBound), std::move(lowerExpression)));
                }
                // new upper bound i.e. Tuple[level, element] <= <expr2>
                // we need to hoist <expr1> <= <expr2> to the outer loop
                else if (!newLowerBound && newUpperBound) {
                    addCondition(mk<Constraint>(
                            getLessEqualConstraint(type), clone(lowerBound), std::move(upperExpression)));
                }
                // if either bound is defined but they aren't equal we must consider the cases for updating
                // them note that at this point we know that if we have a lower/upper bound it can't be the
                // first one
            } else if (!firstLowerBound || !firstUpperBound) {
                // if we have a new equality constraint and previous inequality constraints
                if (newLowerBound && newUpperBound && *lowerExpression == *upperExpression) {
                    // if Tuple[level, element] >= <expr1> and we see Tuple[level, element] = <expr2>
                    // need to hoist <expr2> >= <expr1> to the outer loop
                    if (!firstLowerBound) {
                        addCondition(mk<Constraint>(getGreaterEqualConstraint(type), clone(lowerExpression),
                                std::move(lowerBound)));
                    }
                    // if Tuple[level, element] <= <expr1> and we see Tuple[level, element] = <expr2>
                    // need to hoist <expr2> <= <expr1> to the outer loop
                    if (!firstUpperBound) {
                        addCondition(mk<Constraint>(
                                getLessEqualConstraint(type), clone(upperExpression), std::move(upperBound)));
                    }
                    // finally replace bounds with equality constraint
                    lowerBound = std::move(lowerExpression);
                    upperBound = std::move(upperExpression);
                    // if we have a new lower bound
                } else if (newLowerBound) {
                    // we want the tightest lower bound so we take the max
                    VecOwn<Expression> maxArguments;
                    maxArguments.push_back(std::move(lowerBound));
                    maxArguments.push_back(std::move(lowerExpression));

                    lowerBound = mk<IntrinsicOperator>(getMaxOp(type), std::move(maxArguments));
                    // if we have a new upper bound
                } else if (newUpperBound) {
                    // we want the tightest upper bound so we take the min
                    VecOwn<Expression> minArguments;
                    minArguments.push_back(std::move(upperBound));
                    minArguments.push_back(std::move(upperExpression));

                    upperBound = mk<IntrinsicOperator>(getMinOp(type), std::move(minArguments));
                }
            }
        } else {
            addCondition(std::move(cond));
        }
    }

    // Avoid null-pointers for condition and query pattern
    if (condition == nullptr) {
        condition = mk<True>();
    }
    return condition;
}

Own<Operation> MakeIndexTransformer::rewriteAggregate(const Aggregate* agg) {
    if (!isA<True>(agg->getCondition())) {
        const Relation& rel = relAnalysis->lookup(agg->getRelation());
        std::size_t identifier = agg->getTupleId();
        RamPattern queryPattern;
        for (std::size_t i = 0; i < rel.getArity(); ++i) {
            queryPattern.first.push_back(mk<UndefValue>());
            queryPattern.second.push_back(mk<UndefValue>());
        }

        bool indexable = false;
        Own<Condition> condition = constructPattern(rel.getAttributeTypes(), queryPattern, indexable,
                toConjunctionList(&agg->getCondition()), identifier, rel.getRepresentation());
        if (indexable) {
            return mk<IndexAggregate>(clone(agg->getOperation()), agg->getFunction(), agg->getRelation(),
                    clone(agg->getExpression()), std::move(condition), std::move(queryPattern),
                    agg->getTupleId());
        }
    }
    return nullptr;
}

Own<Operation> MakeIndexTransformer::rewriteScan(const Scan* scan) {
    if (const auto* filter = as<Filter>(scan->getOperation())) {
        const Relation& rel = relAnalysis->lookup(scan->getRelation());
        const std::size_t identifier = scan->getTupleId();
        RamPattern queryPattern;
        for (std::size_t i = 0; i < rel.getArity(); ++i) {
            queryPattern.first.push_back(mk<UndefValue>());
            queryPattern.second.push_back(mk<UndefValue>());
        }

        bool indexable = false;
        Own<Condition> condition = constructPattern(rel.getAttributeTypes(), queryPattern, indexable,
                toConjunctionList(&filter->getCondition()), identifier, rel.getRepresentation());
        if (indexable) {
            Own<Operation> op = clone(filter->getOperation());
            if (!isTrue(condition.get())) {
                op = mk<Filter>(std::move(condition), std::move(op));
            }
            return mk<IndexScan>(scan->getRelation(), identifier, std::move(queryPattern), std::move(op),
                    scan->getProfileText());
        }
    }
    return nullptr;
}

Own<Operation> MakeIndexTransformer::rewriteIndexScan(const IndexScan* iscan) {
    if (const auto* filter = as<Filter>(iscan->getOperation())) {
        const Relation& rel = relAnalysis->lookup(iscan->getRelation());
        const std::size_t identifier = iscan->getTupleId();

        RamPattern strengthenedPattern;
        strengthenedPattern.first = clone(iscan->getRangePattern().first);
        strengthenedPattern.second = clone(iscan->getRangePattern().second);

        bool indexable = false;
        // strengthen the pattern with construct pattern
        Own<Condition> condition = constructPattern(rel.getAttributeTypes(), strengthenedPattern, indexable,
                toConjunctionList(&filter->getCondition()), identifier, rel.getRepresentation());

        if (indexable) {
            // Merge Index Pattern here

            Own<Operation> op = clone(filter->getOperation());
            if (!isTrue(condition.get())) {
                op = mk<Filter>(std::move(condition), std::move(op));
            }
            return mk<IndexScan>(iscan->getRelation(), identifier, std::move(strengthenedPattern),
                    std::move(op), iscan->getProfileText());
        }
    }
    return nullptr;
}

bool MakeIndexTransformer::makeIndex(Program& program) {
    bool changed = false;
    forEachQueryMap(program, [&](auto&& go, Own<Node> node) -> Own<Node> {
        if (const Scan* scan = as<Scan>(node)) {
            const Relation& rel = relAnalysis->lookup(scan->getRelation());
            if (rel.getRepresentation() != RelationRepresentation::INFO) {
                if (auto op = rewriteScan(scan)) {
                    changed = true;
                    node = std::move(op);
                }
            }
        } else if (const IndexScan* iscan = as<IndexScan>(node)) {
            if (auto op = rewriteIndexScan(iscan)) {
                changed = true;
                node = std::move(op);
            }
        } else if (const Aggregate* agg = as<Aggregate>(node)) {
            if (auto op = rewriteAggregate(agg)) {
                changed = true;
                node = std::move(op);
            }
        }

        node->apply(go);
        return node;
    });
    return changed;
}

}  // namespace souffle::ram::transform
