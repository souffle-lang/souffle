/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Utils.cpp
 *
 * A collection of utilities used in translation
 *
 ***********************************************************************/

#include "ast2ram/utility/Utils.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/UnnamedVariable.h"
#include "ast/Variable.h"

#include "ast/utility/Utils.h"
#include "ast2ram/utility/Location.h"
#include "ram/Clear.h"
#include "ram/Condition.h"
#include "ram/Conjunction.h"
#include "ram/TupleElement.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/StringUtil.h"
#include <string>
#include <vector>

namespace souffle::ast2ram {

std::string getConcreteRelationName(const ast::QualifiedName& name, const std::string prefix) {
    return prefix + getRelationName(name);
}

std::string getDeltaRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@delta_");
}

std::string getNewRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@new_");
}

std::string getLeqRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@leq_");
}

std::string getToEraseRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@delete_");
}

std::string getRelationName(const ast::QualifiedName& name) {
    return toString(join(name.getQualifiers(), "."));
}

std::string getBaseRelationName(const ast::QualifiedName& name) {
    return stripPrefix("@new_", stripPrefix("@delta_", stripPrefix("@info_", name.toString())));
}

void appendStmt(VecOwn<ram::Statement>& stmtList, Own<ram::Statement> stmt) {
    if (stmt) {
        stmtList.push_back(std::move(stmt));
    }
}

void nameUnnamedVariables(ast::Clause* clause) {
    // the node mapper conducting the actual renaming
    struct Instantiator : public ast::NodeMapper {
        mutable int counter = 0;

        Instantiator() = default;

        Own<ast::Node> operator()(Own<ast::Node> node) const override {
            // apply recursive
            node->apply(*this);

            // replace unknown variables
            if (isA<ast::UnnamedVariable>(node)) {
                auto name = " _unnamed_var" + toString(++counter);
                return mk<ast::Variable>(name);
            }

            // otherwise nothing
            return node;
        }
    };

    // name all variables in the atoms
    Instantiator init;
    for (auto& atom : ast::getBodyLiterals<ast::Atom>(*clause)) {
        atom->apply(init);
    }
}

Own<ram::TupleElement> makeRamTupleElement(const Location& loc) {
    return mk<ram::TupleElement>(loc.identifier, loc.element);
}

Own<ram::Condition> addConjunctiveTerm(Own<ram::Condition> curCondition, Own<ram::Condition> newTerm) {
    return curCondition ? mk<ram::Conjunction>(std::move(curCondition), std::move(newTerm))
                        : std::move(newTerm);
}

}  // namespace souffle::ast2ram
