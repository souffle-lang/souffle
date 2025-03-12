/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ResolveAnonymousRecordAliases.cpp
 *
 ***********************************************************************/

#include "ast/transform/ResolveAnonymousRecordAliases.h"
#include "ast/Argument.h"
#include "ast/BinaryConstraint.h"
#include "ast/BooleanConstraint.h"
#include "ast/Clause.h"
#include "ast/Literal.h"
#include "ast/Node.h"
#include "ast/Program.h"
#include "ast/RecordInit.h"
#include "ast/TranslationUnit.h"
#include "ast/UnnamedVariable.h"
#include "ast/Variable.h"
#include "ast/analysis/Ground.h"
#include "ast/analysis/typesystem/Type.h"
#include "ast/analysis/typesystem/TypeSystem.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/utility/MiscUtil.h"
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace souffle::ast::transform {

std::map<std::string, const RecordInit*> ResolveAnonymousRecordAliasesTransformer::findVariablesRecordMapping(
        TranslationUnit& tu, const Clause& clause) {
    std::map<std::string, const RecordInit*> variableRecordMap;

    auto isVariable = [](Node* node) -> bool { return isA<ast::Variable>(node); };
    auto isRecord = [](Node* node) -> bool { return isA<RecordInit>(node); };

    auto& typeAnalysis = tu.getAnalysis<analysis::TypeAnalysis>();
    auto groundedTerms = analysis::getGroundedTerms(tu, clause);

    for (auto* literal : clause.getBodyLiterals()) {
        if (auto constraint = as<BinaryConstraint>(literal)) {
            if (!isEqConstraint(constraint->getBaseOperator())) {
                continue;
            }

            auto left = constraint->getLHS();
            auto right = constraint->getRHS();

            if (!isVariable(left) && !isVariable(right)) {
                continue;
            }

            if (!isRecord(left) && !isRecord(right)) {
                continue;
            }

            // TODO (darth_tytus): This should change in the future.
            // Currently type system assigns to anonymous records {- All types - }
            // which is inelegant.
            if (!typeAnalysis.getTypes(left).isAll()) {
                continue;
            }

            auto* variable = static_cast<ast::Variable*>(isVariable(left) ? left : right);
            const auto& variableName = variable->getName();

            if (!groundedTerms.find(variable)->second) {
                continue;
            }

            // We are interested only in the first mapping.
            if (variableRecordMap.find(variableName) != variableRecordMap.end()) {
                continue;
            }

            auto* record = static_cast<RecordInit*>(isRecord(left) ? left : right);

            variableRecordMap.insert({variableName, record});
        }
    }

    return variableRecordMap;
}

bool ResolveAnonymousRecordAliasesTransformer::replaceNamedVariables(TranslationUnit& tu, Clause& clause) {
    struct ReplaceVariables : public NodeMapper {
        std::map<std::string, const RecordInit*> varToRecordMap;

        ReplaceVariables(std::map<std::string, const RecordInit*> varToRecordMap)
                : varToRecordMap(std::move(varToRecordMap)) {}

        Own<Node> operator()(Own<Node> node) const override {
            if (auto variable = as<ast::Variable>(node)) {
                auto iteratorToRecord = varToRecordMap.find(variable->getName());
                if (iteratorToRecord != varToRecordMap.end()) {
                    return clone(iteratorToRecord->second);
                }
            }

            node->apply(*this);

            return node;
        }
    };

    auto variableToRecordMap = findVariablesRecordMapping(tu, clause);
    bool changed = variableToRecordMap.size() > 0;
    if (changed) {
        ReplaceVariables update(std::move(variableToRecordMap));
        clause.apply(update);
    }
    return changed;
}

bool ResolveAnonymousRecordAliasesTransformer::replaceUnnamedVariable(Clause& clause) {
    struct ReplaceUnnamed : public NodeMapper {
        mutable bool changed{false};
        ReplaceUnnamed() = default;

        Own<Node> operator()(Own<Node> node) const override {
            auto isUnnamed = [](Node* node) -> bool { return isA<UnnamedVariable>(node); };
            auto isRecord = [](Node* node) -> bool { return isA<RecordInit>(node); };

            if (auto constraint = as<BinaryConstraint>(node)) {
                auto left = constraint->getLHS();
                auto right = constraint->getRHS();
                bool hasUnnamed = isUnnamed(left) || isUnnamed(right);
                bool hasRecord = isRecord(left) || isRecord(right);
                auto op = constraint->getBaseOperator();
                if (hasUnnamed && hasRecord && isEqConstraint(op)) {
                    return mk<BooleanConstraint>(true);
                }
            }

            node->apply(*this);

            return node;
        }
    };

    ReplaceUnnamed update;
    clause.apply(update);

    return update.changed;
}

bool ResolveAnonymousRecordAliasesTransformer::transform(TranslationUnit& translationUnit) {
    bool changed = false;
    Program& program = translationUnit.getProgram();
    for (auto* clause : program.getClauses()) {
        changed |= replaceNamedVariables(translationUnit, *clause);
        changed |= replaceUnnamedVariable(*clause);
    }

    return changed;
}

}  // namespace souffle::ast::transform
