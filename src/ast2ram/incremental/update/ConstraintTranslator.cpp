/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ConstraintTranslator.cpp
 *
 ***********************************************************************/

#include "ast2ram/incremental/update/ConstraintTranslator.h"
#include "ast/Atom.h"
#include "ast/TranslationUnit.h"
#include "ast2ram/ValueTranslator.h"
#include "ast2ram/utility/TranslatorContext.h"
#include "ast2ram/utility/Utils.h"
#include "ast2ram/utility/ValueIndex.h"
#include "ram/ExistenceCheck.h"
#include "ram/Negation.h"
#include "ram/UndefValue.h"

namespace souffle::ast2ram::incremental::update {

Own<ram::Condition> ConstraintTranslator::translateConstraint(const ast::Literal* lit) {
    assert(lit != nullptr && "literal should be defined");
    return ConstraintTranslator(context, index)(*lit);
}

Own<ram::Condition> ConstraintTranslator::visit_(type_identity<ast::Negation>, const ast::Negation& neg) {
    // construct the atom and create a negation
    const auto* atom = neg.getAtom();
    VecOwn<ram::Expression> values;

    // actual arguments
    for (const auto* arg : atom->getArguments()) {
        values.push_back(context.translateValue(index, arg));
    }

    // add rule + level number
    values.push_back(mk<ram::UndefValue>());
    values.push_back(mk<ram::UndefValue>());

    return mk<ram::Negation>(
            mk<ram::ExistenceCheck>(getConcreteRelationName(atom->getQualifiedName()), std::move(values)));
}

}  // namespace souffle::ast2ram::incremental::update
