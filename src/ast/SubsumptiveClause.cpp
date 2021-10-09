/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/SubsumptiveClause.h"
#include "ast/Clause.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/NodeMapperFwd.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <ostream>
#include <utility>

namespace souffle::ast {

void SubsumptiveClause::apply(const NodeMapper& map) {
    Clause::apply(map);
    subsumptiveHead = map(std::move(subsumptiveHead));
}

Node::NodeVec SubsumptiveClause::getChildren() const {
    std::vector<const Node*> res = Clause::getChildren();
    res.push_back(subsumptiveHead.get());
    return res;
}

void SubsumptiveClause::print(std::ostream& os) const {
    assert(head != nullptr && "head is null");
    assert(subsumptiveHead != nullptr && "subsumptive head is null");
    os << *head;
    os << " <= ";
    os << *subsumptiveHead;
    if (!bodyLiterals.empty()) {
        os << " :- \n   " << join(bodyLiterals, ",\n   ");
    }
    os << ".";
    if (plan != nullptr) {
        os << *plan;
    }
}

bool SubsumptiveClause::equal(const Node& node) const {
    // may not work; double-check
    const auto& other = asAssert<SubsumptiveClause>(node);
    return Clause::equal(node) && equal_ptr(subsumptiveHead, other.subsumptiveHead);
}

SubsumptiveClause* SubsumptiveClause::cloning() const {
    return new SubsumptiveClause(
            clone(head), clone(subsumptiveHead), clone(bodyLiterals), clone(plan), getSrcLoc());
}

}  // namespace souffle::ast
