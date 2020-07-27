/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file PolymorphicObjects.cpp
 *
 ***********************************************************************/

#include "ast/transform/PolymorphicObjects.h"
#include "AggregateOp.h"
#include "BinaryConstraintOps.h"
#include "RamTypes.h"
#include "ast/AstArgument.h"
#include "ast/AstLiteral.h"
#include "ast/AstNode.h"
#include "ast/AstProgram.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstUtils.h"
#include "ast/TypeSystem.h"
#include "ast/analysis/AstType.h"
#include "utility/ContainerUtil.h"
#include "utility/FunctionalUtil.h"
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace souffle {
class AstArgument;
class ErrorReport;

bool PolymorphicObjectsTransformer::transform(AstTranslationUnit& translationUnit) {
    struct TypeRewriter : public AstNodeMapper {
        mutable bool changed{false};
        const TypeAnalysis& typeAnalysis;
        ErrorReport& report;

        TypeRewriter(const TypeAnalysis& typeAnalysis, ErrorReport& report)
                : typeAnalysis(typeAnalysis), report(report) {}

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            // Utility lambdas to determine if all args are of the same type.
            auto isFloat = [&](const AstArgument* argument) {
                return isOfKind(typeAnalysis.getTypes(argument), TypeAttribute::Float);
            };
            auto isUnsigned = [&](const AstArgument* argument) {
                return isOfKind(typeAnalysis.getTypes(argument), TypeAttribute::Unsigned);
            };
            auto isSymbol = [&](const AstArgument* argument) {
                return isOfKind(typeAnalysis.getTypes(argument), TypeAttribute::Symbol);
            };

            // rewrite sub-expressions first
            node->apply(*this);

            // It's possible that at this stage we get an undeclared clause.
            // In this case types can't be assigned to it, and the procedure getTypes can fail
            try {
                // Handle numeric constant
                if (auto* numericConstant = dynamic_cast<AstNumericConstant*>(node.get())) {
                    // Check if there is no value yet.
                    if (!numericConstant->getType().has_value()) {
                        TypeSet types = typeAnalysis.getTypes(numericConstant);

                        auto hasOfKind = [&](TypeAttribute kind) -> bool {
                            return any_of(types, [&](const Type& type) { return isOfKind(type, kind); });
                        };
                        if (hasOfKind(TypeAttribute::Signed)) {
                            numericConstant->setType(AstNumericConstant::Type::Int);
                            changed = true;
                        } else if (hasOfKind(TypeAttribute::Unsigned)) {
                            numericConstant->setType(AstNumericConstant::Type::Uint);
                            changed = true;
                        } else if (hasOfKind(TypeAttribute::Float)) {
                            numericConstant->setType(AstNumericConstant::Type::Float);
                            changed = true;
                        }
                    }
                }

                // Handle functor
                auto* functor = as<AstIntrinsicFunctor>(node);
                if (functor && !functor->getFunctionInfo()) {
                    // any valid candidate will do. pick the first.
                    auto candidates = validOverloads(typeAnalysis, *functor);
                    if (!candidates.empty()) {
                        functor->setFunctionInfo(candidates.front().get());
                        changed = true;
                    }
                }

                // Handle binary constraint
                if (auto* binaryConstraint = dynamic_cast<AstBinaryConstraint*>(node.get())) {
                    if (isOverloaded(binaryConstraint->getOperator())) {
                        // Get arguments
                        auto* leftArg = binaryConstraint->getLHS();
                        auto* rightArg = binaryConstraint->getRHS();
                        auto oldOp = binaryConstraint->getOperator();

                        // Both args must be of the same type
                        if (isFloat(leftArg) && isFloat(rightArg)) {
                            binaryConstraint->setOperator(convertOverloadedConstraint(
                                    binaryConstraint->getOperator(), TypeAttribute::Float));
                        } else if (isUnsigned(leftArg) && isUnsigned(rightArg)) {
                            binaryConstraint->setOperator(convertOverloadedConstraint(
                                    binaryConstraint->getOperator(), TypeAttribute::Unsigned));
                        } else if (isSymbol(leftArg) && isSymbol(rightArg)) {
                            binaryConstraint->setOperator(convertOverloadedConstraint(
                                    binaryConstraint->getOperator(), TypeAttribute::Symbol));
                        }

                        changed |= binaryConstraint->getOperator() != oldOp;
                    }
                }

                if (auto* aggregator = dynamic_cast<AstAggregator*>(node.get())) {
                    if (isOverloadedAggregator(aggregator->getOperator())) {
                        auto* targetExpression = aggregator->getTargetExpression();
                        auto oldOp = aggregator->getOperator();

                        if (isFloat(targetExpression)) {
                            aggregator->setOperator(convertOverloadedAggregator(
                                    aggregator->getOperator(), TypeAttribute::Float));
                        } else if (isUnsigned(targetExpression)) {
                            aggregator->setOperator(convertOverloadedAggregator(
                                    aggregator->getOperator(), TypeAttribute::Unsigned));
                        }

                        changed |= aggregator->getOperator() != oldOp;
                    }
                }
            } catch (std::out_of_range&) {
                // No types to convert in undeclared clauses
            }

            return node;
        }
    };
    const TypeAnalysis& typeAnalysis = *translationUnit.getAnalysis<TypeAnalysis>();
    TypeRewriter update(typeAnalysis, translationUnit.getErrorReport());
    translationUnit.getProgram()->apply(update);
    return update.changed;
}

}  // end of namespace souffle
