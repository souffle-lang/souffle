/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ResolveAnonymousRecordsAliases.cpp
 *
 ***********************************************************************/

#include "ast/transform/ResolveAnonymousRecordsAliases.h"
#include "BinaryConstraintOps.h"
#include "ast/Abstract.h"
#include "ast/Argument.h"
#include "ast/Clause.h"
#include "ast/Literal.h"
#include "ast/Node.h"
#include "ast/Program.h"
#include "ast/TranslationUnit.h"
#include "ast/TypeSystem.h"
#include "ast/analysis/Ground.h"
#include "ast/analysis/Type.h"
#include "utility/MiscUtil.h"
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace souffle {

std::map<std::string, const AstRecordInit*> ResolveAnonymousRecordsAliases::findVariablesRecordMapping(
        AstTranslationUnit& tu, const AstClause& clause) {
    std::map<std::string, const AstRecordInit*> variableRecordMap;

    auto isVariable = [](AstNode* node) -> bool { return dynamic_cast<AstVariable*>(node) != nullptr; };
    auto isRecord = [](AstNode* node) -> bool { return dynamic_cast<AstRecordInit*>(node) != nullptr; };

    auto& typeAnalysis = *tu.getAnalysis<TypeAnalysis>();
    auto groundedTerms = getGroundedTerms(tu, clause);

    for (auto* literal : clause.getBodyLiterals()) {
        if (auto constraint = dynamic_cast<AstBinaryConstraint*>(literal)) {
            if (!isEqConstraint(constraint->getOperator())) {
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

            auto* variable = static_cast<AstVariable*>(isVariable(left) ? left : right);
            const auto& variableName = variable->getName();

            if (!groundedTerms.find(variable)->second) {
                continue;
            }

            // We are interested only in the first mapping.
            if (variableRecordMap.find(variableName) != variableRecordMap.end()) {
                continue;
            }

            auto* record = static_cast<AstRecordInit*>(isRecord(left) ? left : right);

            variableRecordMap.insert({variableName, record});
        }
    }

    return variableRecordMap;
}

bool ResolveAnonymousRecordsAliases::replaceNamedVariables(AstTranslationUnit& tu, AstClause& clause) {
    struct ReplaceVariables : public AstNodeMapper {
        std::map<std::string, const AstRecordInit*> varToRecordMap;

        ReplaceVariables(std::map<std::string, const AstRecordInit*> varToRecordMap)
                : varToRecordMap(std::move(varToRecordMap)){};

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            if (auto variable = dynamic_cast<AstVariable*>(node.get())) {
                auto iteratorToRecord = varToRecordMap.find(variable->getName());
                if (iteratorToRecord != varToRecordMap.end()) {
                    return souffle::clone(iteratorToRecord->second);
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

bool ResolveAnonymousRecordsAliases::replaceUnnamedVariable(AstClause& clause) {
    struct ReplaceUnnamed : public AstNodeMapper {
        mutable bool changed{false};
        ReplaceUnnamed() = default;

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            auto isUnnamed = [](AstNode* node) -> bool {
                return dynamic_cast<AstUnnamedVariable*>(node) != nullptr;
            };
            auto isRecord = [](AstNode* node) -> bool {
                return dynamic_cast<AstRecordInit*>(node) != nullptr;
            };

            if (auto constraint = dynamic_cast<AstBinaryConstraint*>(node.get())) {
                auto left = constraint->getLHS();
                auto right = constraint->getRHS();
                bool hasUnnamed = isUnnamed(left) || isUnnamed(right);
                bool hasRecord = isRecord(left) || isRecord(right);
                auto op = constraint->getOperator();
                if (hasUnnamed && hasRecord && isEqConstraint(op)) {
                    return std::make_unique<AstBooleanConstraint>(true);
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

bool ResolveAnonymousRecordsAliases::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;
    for (auto* clause : translationUnit.getProgram()->getClauses()) {
        changed |= replaceNamedVariables(translationUnit, *clause);
        changed |= replaceUnnamedVariable(*clause);
    }

    return changed;
}

}  // end of namespace souffle
