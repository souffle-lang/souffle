/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file BTreeDeleteIndex.cpp
 *
 * Interpreter btree-deletion index with generic interface.
 *
 ***********************************************************************/

#include "interpreter/Relation.h"
#include "ram/Relation.h"
#include "ram/analysis/Index.h"
#include "souffle/utility/MiscUtil.h"

namespace souffle::interpreter {

#define CREATE_BTREE_DELETE_REL(Structure, Arity, AuxiliaryArity, ...)                       \
    if (id.getArity() == Arity && id.getAuxiliaryArity() == AuxiliaryArity) {                \
        return mk<BtreeDeleteRelation<Arity, AuxiliaryArity>>(id.getName(), indexSelection); \
    }

Own<RelationWrapper> createBTreeDeleteRelation(
        const ram::Relation& id, const ram::analysis::IndexCluster& indexSelection) {
    FOR_EACH_BTREE_DELETE(CREATE_BTREE_DELETE_REL);
    fatal("Requested arity not yet supported. Feel free to add it.");
}

}  // namespace souffle::interpreter
