/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamOperation.cpp
 *
 * Implements the operation of a relational algebra query consisting of
 * Search/Scan and a Project operation. The Search/Scan operation traverses
 * a table and/or check for condition of each tuple and/or uses an index.
 *
 ***********************************************************************/

#include "RamOperation.h"
#include "RamCondition.h"
#include "RamIndex.h"
#include "RamRecords.h"
#include "RamRelation.h"

#include <iostream>
#include <list>
#include <memory>
#include <string>

namespace souffle {

/** add condition */
void RamOperation::addCondition(std::unique_ptr<RamCondition> c, RamOperation* root) {
    assert(c->getLevel() == level);

    if (condition) {
        condition = std::unique_ptr<RamCondition>(new RamAnd(std::move(condition), std::move(c)));
    } else {
        condition.swap(c);
    }
}

/** add condition */
void RamSearch::addCondition(std::unique_ptr<RamCondition> c, RamOperation* root) {
    assert(c->getLevel() >= level);

    if (c->getLevel() > level) {
        getNestedOperation()->addCondition(std::move(c), root);
        return;
    }

    // use base-class implementation
    RamOperation::addCondition(std::move(c), root);
}

namespace {

/** get indexable element */
std::unique_ptr<RamValue> getIndexElement(RamCondition* c, size_t& element, size_t level) {
    if (RamBinaryRelation* binRelOp = dynamic_cast<RamBinaryRelation*>(c)) {
        if (binRelOp->getOperator() == BinaryConstraintOp::EQ) {
            if (RamElementAccess* lhs = dynamic_cast<RamElementAccess*>(binRelOp->getLHS())) {
                RamValue* rhs = binRelOp->getRHS();
                if (lhs->getLevel() == level && (rhs->isConstant() || rhs->getLevel() < level)) {
                    element = lhs->getElement();
                    return binRelOp->takeRHS();
                }
            }
            if (RamElementAccess* rhs = dynamic_cast<RamElementAccess*>(binRelOp->getRHS())) {
                RamValue* lhs = binRelOp->getLHS();
                if (rhs->getLevel() == level && (lhs->isConstant() || lhs->getLevel() < level)) {
                    element = rhs->getElement();
                    return binRelOp->takeLHS();
                }
            }
        }
    }
    return std::unique_ptr<RamValue>(nullptr);
}
}  // namespace

/** add condition */
void RamScan::addCondition(std::unique_ptr<RamCondition> c, RamOperation* root) {
    // use condition to narrow scan if possible
    if (c->getLevel() == level) {
        size_t element;
        if (std::unique_ptr<RamValue> value = getIndexElement(c.get(), element, level)) {
            keys |= (1 << element);
            if (queryPattern[element] == nullptr) {
                queryPattern[element] = std::move(value);
            } else {
                std::unique_ptr<RamValue> field(new RamElementAccess(level, element));
                RamSearch::addCondition(std::unique_ptr<RamCondition>(new RamBinaryRelation(
                                                BinaryConstraintOp::EQ, std::move(field), std::move(value))),
                        root);
            }
            return;
        }
    }

    // otherwise: use default handling
    RamSearch::addCondition(std::move(c), root);
}

void RamScan::print(std::ostream& os, int tabpos) const {
    os << times('\t', tabpos);

    if (isPureExistenceCheck()) {
        os << "IF ∃ t" << level << " ∈ " << relation.getName() << " ";
        if (keys != 0) {
            os << "WITH ";
            bool first = true;
            for (size_t i = 0; i < relation.getArity(); i++) {
                if (queryPattern[i] != nullptr) {
                    if (first) {
                        first = false;
                    } else {
                        os << "and ";
                    }
                    os << "t" << level << "." << relation.getArg(i) << "=";
                    queryPattern[i]->print(os);
                    os << " ";
                }
            }
        }
    } else {
        if (keys == 0) {
            os << "SCAN " << relation.getName() << " AS t" << level << " ";
        } else {
            // Keys indicates index search?
            os << "SEARCH " << relation.getName() << " AS t" << level;
            os << " ON INDEX ";
            bool first = true;
            for (size_t i = 0; i < relation.getArity(); i++) {
                if (queryPattern[i] != nullptr) {
                    if (first) {
                        first = false;
                    } else {
                        os << "and ";
                    }
                    os << "t" << level << "." << relation.getArg(i) << "=";
                    queryPattern[i]->print(os);
                    os << " ";
                }
            }
        }
    }
    if (auto condition = getCondition()) {
        os << "WHERE ";
        condition->print(os);
    }

    os << "\n";
    if (getNestedOperation() != nullptr) {
        getNestedOperation()->print(os, tabpos + 1);
    }
}

/*
 * Class Lookup
 */

/** print search */
void RamLookup::print(std::ostream& os, int tabpos) const {
    os << times('\t', tabpos);

    os << "UNPACK env(t" << refLevel << ", i" << refPos << ") INTO t" << getLevel();

    if (auto condition = getCondition()) {
        os << " WHERE ";
        condition->print(os);
    }

    os << " FOR \n";
    getNestedOperation()->print(os, tabpos + 1);
}

/** add condition */
void RamAggregate::addCondition(std::unique_ptr<RamCondition> c, RamOperation* root) {
    // use condition to narrow scan if possible
    if (c->getLevel() == level) {
        size_t element;
        if (std::unique_ptr<RamValue> value = getIndexElement(c.get(), element, level)) {
            if (element > 0 || relation.getName().find("__agg") == std::string::npos) {
                keys |= (1 << element);
                if (pattern[element] == nullptr) {
                    pattern[element] = std::move(value);
                } else {
                    std::unique_ptr<RamValue> field(new RamElementAccess(level, element));
                    RamSearch::addCondition(
                            std::unique_ptr<RamCondition>(new RamBinaryRelation(
                                    BinaryConstraintOp::EQ, std::move(field), std::move(value))),
                            root);
                }
            } else {
                std::unique_ptr<RamValue> field(new RamElementAccess(level, element));
                std::unique_ptr<RamCondition> eq(
                        new RamBinaryRelation(BinaryConstraintOp::EQ, std::move(field), std::move(value)));
                if (condition != nullptr) {
                    condition =
                            std::unique_ptr<RamCondition>(new RamAnd(std::move(condition), std::move(eq)));
                } else {
                    condition.swap(eq);
                }
            }
            return;
        }
    }

    // otherwise: use default handling
    RamSearch::addCondition(std::move(c), root);
}

/** print search */
void RamAggregate::print(std::ostream& os, int tabpos) const {
    os << times('\t', tabpos);

    switch (fun) {
        case MIN:
            os << "MIN ";
            break;
        case MAX:
            os << "MAX ";
            break;
        case COUNT:
            os << "COUNT ";
            break;
        case SUM:
            os << "SUM ";
            break;
    }

    if (fun != COUNT) {
        os << *value << " ";
    }

    os << "AS t" << getLevel() << ".0 IN t" << getLevel() << " ∈ " << relation.getName();
    os << "(" << join(pattern, ",", [&](std::ostream& out, const std::unique_ptr<RamValue>& value) {
        if (!value) {
            out << "_";
        } else {
            out << *value;
        }
    }) << ")";

    if (auto condition = getCondition()) {
        os << " WHERE ";
        condition->print(os);
    }

    os << " FOR \n";
    getNestedOperation()->print(os, tabpos + 1);
}

/*
 * Class Project
 */

/* print projection */
void RamProject::print(std::ostream& os, int tabpos) const {
    const std::string tabs(tabpos, '\t');

    // support table-less options
    if (auto condition = getCondition()) {
        os << "IF ";
        condition->print(os);
        os << " THEN ";
    }

    os << tabs << "PROJECT (" << join(values, ", ", print_deref<std::unique_ptr<RamValue>>()) << ") INTO "
       << relation.getName();

    if (hasFilter()) {
        os << " UNLESS IN " << getFilter().getName();
    }
}

}  // end of namespace souffle
