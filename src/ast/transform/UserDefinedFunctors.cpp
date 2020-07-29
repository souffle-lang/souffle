/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file UserDefinedFunctors.cpp
 *
 ***********************************************************************/

#include "ast/transform/UserDefinedFunctors.h"
#include "ErrorReport.h"
#include "RamTypes.h"
#include "ast/Argument.h"
#include "ast/FunctorDeclaration.h"
#include "ast/Node.h"
#include "ast/Program.h"
#include "ast/TranslationUnit.h"
#include "ast/Utils.h"
#include <memory>
#include <vector>

namespace souffle {

bool AstUserDefinedFunctorsTransformer::transform(AstTranslationUnit& translationUnit) {
    struct UserFunctorRewriter : public AstNodeMapper {
        mutable bool changed{false};
        const AstProgram& program;
        ErrorReport& report;

        UserFunctorRewriter(const AstProgram& program, ErrorReport& report)
                : program(program), report(report){};

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            node->apply(*this);

            if (auto* userFunctor = dynamic_cast<AstUserDefinedFunctor*>(node.get())) {
                const AstFunctorDeclaration* functorDeclaration =
                        getFunctorDeclaration(program, userFunctor->getName());

                // Check if the functor has been declared
                if (functorDeclaration == nullptr) {
                    report.addError("User-defined functor hasn't been declared", userFunctor->getSrcLoc());
                    return node;
                }

                // Check arity correctness.
                if (functorDeclaration->getArity() != userFunctor->getArguments().size()) {
                    report.addError("Mismatching number of arguments of functor", userFunctor->getSrcLoc());
                    return node;
                }

                // Set types of functor instance based on its declaration.
                userFunctor->setTypes(
                        functorDeclaration->getArgsTypes(), functorDeclaration->getReturnType());

                changed = true;
            }
            return node;
        }
    };
    UserFunctorRewriter update(*translationUnit.getProgram(), translationUnit.getErrorReport());
    translationUnit.getProgram()->apply(update);
    return update.changed;
}

}  // end of namespace souffle
