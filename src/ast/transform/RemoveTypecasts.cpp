/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RemoveTypecasts.cpp
 *
 ***********************************************************************/

#include "ast/transform/RemoveTypecasts.h"
#include "ast/AstAbstract.h"
#include "ast/AstArgument.h"
#include "ast/AstNode.h"
#include "ast/AstProgram.h"
#include "ast/AstTranslationUnit.h"
#include "souffle/utility/MiscUtil.h"
#include <memory>

namespace souffle {

bool RemoveTypecastsTransformer::transform(AstTranslationUnit& translationUnit) {
    struct TypecastRemover : public AstNodeMapper {
        mutable bool changed{false};

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            // remove sub-typecasts first
            node->apply(*this);

            // if current node is a typecast, replace with the value directly
            if (auto* cast = dynamic_cast<AstTypeCast*>(node.get())) {
                changed = true;
                return souffle::clone(cast->getValue());
            }

            // otherwise, return the original node
            return node;
        }
    };

    TypecastRemover update;
    translationUnit.getProgram()->apply(update);

    return update.changed;
}

}  // end of namespace souffle
