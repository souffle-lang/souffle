/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Scan.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Operation.h"
#include "ram/Relation.h"
#include "ram/RelationOperation.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <iosfwd>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

namespace souffle::ram {

/**
 * @class Scan
 * @brief Iterate all tuples of a relation
 *
 * The following example iterates over all tuples
 * in the set A:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *   FOR t0 IN A
 *     ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class Scan : public RelationOperation {
public:
    Scan(std::string rel, std::size_t ident, Own<Operation> nested, std::string profileText = "")
            : Scan(NK_Scan, std::move(rel), ident, std::move(nested), std::move(profileText)) {}

    Scan* cloning() const override {
        return new Scan(NK_Scan, relation, getTupleId(), clone(getOperation()), getProfileText());
    }

    static bool classof(const Node* n) {
        const NodeKind kind = n->getKind();
        return (kind >= NK_Scan && kind < NK_LastScan);
    }

protected:
    Scan(NodeKind kind, std::string rel, std::size_t ident, Own<Operation> nested,
            std::string profileText = "")
            : RelationOperation(kind, std::move(rel), ident, std::move(nested), std::move(profileText)) {
        assert(kind >= NK_Scan && kind < NK_LastScan);
    }

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "FOR t" << getTupleId();
        os << " IN " << relation << std::endl;
        RelationOperation::print(os, tabpos + 1);
    }
};

}  // namespace souffle::ram
