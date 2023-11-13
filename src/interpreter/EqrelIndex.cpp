/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file EqrelIndex.cpp
 *
 * Interpreter index with generic interface.
 *
 ***********************************************************************/

#include "interpreter/Relation.h"
#include "ram/Relation.h"
#include "ram/analysis/Index.h"

namespace souffle::interpreter {

Own<RelationWrapper> createEqrelRelation(
        const ram::Relation& id, const ram::analysis::IndexCluster& indexSelection) {
    assert(id.getArity() == 2 && "Eqivalence relation must have arity size 2.");
    assert(id.getAuxiliaryArity() == 0 && "Equivalence relation must have auxiliary arity size 0.");
    return mk<EqrelRelation>(id.getName(), indexSelection);
}

}  // namespace souffle::interpreter
