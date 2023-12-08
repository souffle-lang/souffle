/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/SubsumptiveClause.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/NodeMapperFwd.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <ostream>
#include <utility>

namespace souffle::ast {

SubsumptiveClause::SubsumptiveClause(
        Own<Atom> head, VecOwn<Literal> bodyLiterals, Own<ExecutionPlan> plan, SrcLocation loc)
        : Clause(NK_SubsumptiveClause, std::move(head), std::move(bodyLiterals), std::move(plan),
                  std::move(loc)) {}

SubsumptiveClause::SubsumptiveClause(Own<Atom> head, SrcLocation loc)
        : SubsumptiveClause(std::move(head), {}, {}, std::move(loc)) {}

SubsumptiveClause::SubsumptiveClause(QualifiedName name, SrcLocation loc)
        : SubsumptiveClause(mk<Atom>(name), std::move(loc)) {}

void SubsumptiveClause::addToBodyFront(Own<Literal> literal) {
    assert(literal != nullptr);
    bodyLiterals.insert(bodyLiterals.begin(), std::move(literal));
}

void SubsumptiveClause::print(std::ostream& os) const {
    os << *bodyLiterals[0];
    os << " <= ";
    os << *bodyLiterals[1];
    os << " :- \n";
    std::size_t i = 2;
    while (i < bodyLiterals.size() - 1) {
        os << "   " << *bodyLiterals[i++] << ",\n";
    }
    if (i < bodyLiterals.size()) {
        os << "   " << *bodyLiterals[i] << ".";
    }
    if (plan != nullptr) {
        os << *plan;
    }
}

SubsumptiveClause* SubsumptiveClause::cloning() const {
    return new SubsumptiveClause(clone(head), clone(bodyLiterals), clone(plan), getSrcLoc());
}

Clause* SubsumptiveClause::cloneHead() const {
    SubsumptiveClause* myClone = new SubsumptiveClause(clone(head), getSrcLoc());
    if (getExecutionPlan() != nullptr) {
        myClone->setExecutionPlan(clone(getExecutionPlan()));
    }
    return myClone;
}

bool SubsumptiveClause::classof(const Node* n) {
    return n->getKind() == NK_SubsumptiveClause;
}

}  // namespace souffle::ast
