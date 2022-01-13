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

// Specialized Eqrel constructor, ignore index analysis and use only one index.
template <>
Relation<2, Eqrel>::Relation(
        std::size_t auxiliaryArity, const std::string& name, const ram::analysis::IndexCluster& /* unused */)
        : RelationWrapper(Arity, auxiliaryArity, name) {
    indexes.push_back(mk<Index>(ram::analysis::LexOrder{0, 1}));
    main = indexes[0].get();
}

Own<RelationWrapper> createEqrelRelation(
        const ram::Relation& id, const ram::analysis::IndexCluster& indexSelection) {
    assert(id.getArity() == 2 && "Eqivalence relation must have arity size 2.");
    return mk<EqrelRelation>(id.getAuxiliaryArity(), id.getName(), indexSelection);
}

}  // namespace souffle::interpreter
