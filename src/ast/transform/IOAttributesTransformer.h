/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IOAttributesTransformer.h
 *
 * Defines AST transformation to set attribute names and types in IO
 * operations.
 *
 ***********************************************************************/

#pragma once

#include "ast/AstUtils.h"
#include "ast/analysis/AstTypeEnvironmentAnalysis.h"
#include "ast/analysis/AuxArityAnalysis.h"
#include "ast/transform/AstTransformer.h"
#include <map>
#include <string>
#include <vector>

namespace souffle {

/**
 * Transformation pass to set attribute names and types in IO operations.
 */
class IOAttributesTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "IOAttributesTransformer";
    }

    IOAttributesTransformer* clone() const override {
        return new IOAttributesTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override {
        bool changed = false;

        changed |= setAttributeNames(translationUnit);
        changed |= setAttributeTypes(translationUnit);

        return changed;
    }

    bool setAttributeNames(AstTranslationUnit& translationUnit) {
        bool changed = false;
        AstProgram* program = translationUnit.getProgram();
        for (AstIO* io : program->getIOs()) {
            if (io->hasDirective("attributeNames")) {
                continue;
            }
            AstRelation* rel = getRelation(*translationUnit.getProgram(), io->getQualifiedName());
            std::string delimiter("\t");
            if (io->hasDirective("delimiter")) {
                delimiter = io->getDirective("delimiter");
            }

            std::vector<std::string> attributeNames;
            for (const auto* attribute : rel->getAttributes()) {
                attributeNames.push_back(attribute->getName());
            }

            if (Global::config().has("provenance")) {
                auto auxArityAnalysis = translationUnit.getAnalysis<AuxiliaryArity>();
                std::vector<std::string> originalAttributeNames(
                        attributeNames.begin(), attributeNames.end() - auxArityAnalysis->getArity(rel));
                io->addDirective("attributeNames", toString(join(originalAttributeNames, delimiter)));
            } else {
                io->addDirective("attributeNames", toString(join(attributeNames, delimiter)));
            }
            changed = true;
        }
        return changed;
    }

    bool setAttributeTypes(AstTranslationUnit& translationUnit) {
        bool changed = false;
        AstProgram* program = translationUnit.getProgram();
        auto auxArityAnalysis = translationUnit.getAnalysis<AuxiliaryArity>();
        auto typeEnv = &translationUnit.getAnalysis<TypeEnvironmentAnalysis>()->getTypeEnvironment();

        for (AstIO* io : program->getIOs()) {
            AstRelation* rel = getRelation(*translationUnit.getProgram(), io->getQualifiedName());
            // Prepare type system information.
            std::vector<std::string> attributesTypes;

            for (const auto* attribute : rel->getAttributes()) {
                auto typeName = attribute->getTypeName();
                auto type = getTypeQualifier(typeEnv->getType(typeName));
                attributesTypes.push_back(type);
            }

            // Casting due to json11.h type requirements.
            long long arity{static_cast<long long>(rel->getArity() - auxArityAnalysis->getArity(rel))};
            long long auxArity{static_cast<long long>(auxArityAnalysis->getArity(rel))};

            Json relJson = Json::object{{"arity", arity}, {"auxArity", auxArity},
                    {"types", Json::array(attributesTypes.begin(), attributesTypes.end())}};

            Json types = Json::object{{"relation", relJson}, {"records", getRecordsTypes(translationUnit)}};

            io->addDirective("types", types.dump());
            changed = true;
        }
        return changed;
    }

    std::string getRelationName(const AstIO* node) {
        return toString(join(node->getQualifiedName().getQualifiers(), "."));
    }

    Json getRecordsTypes(AstTranslationUnit& translationUnit) const {
        static Json ramRecordTypes;
        // Check if the types where already constructed
        if (!ramRecordTypes.is_null()) {
            return ramRecordTypes;
        }

        AstProgram* program = translationUnit.getProgram();
        auto typeEnv = &translationUnit.getAnalysis<TypeEnvironmentAnalysis>()->getTypeEnvironment();
        std::vector<std::string> elementTypes;
        std::map<std::string, Json> records;

        // Iterate over all record types in the program populating the records map.
        for (auto* astType : program->getTypes()) {
            const auto& type = typeEnv->getType(astType->getQualifiedName());
            if (isA<RecordType>(type)) {
                elementTypes.clear();

                for (const Type* field : as<RecordType>(type)->getFields()) {
                    elementTypes.push_back(getTypeQualifier(*field));
                }
                const size_t recordArity = elementTypes.size();
                Json recordInfo = Json::object{
                        {"types", std::move(elementTypes)}, {"arity", static_cast<long long>(recordArity)}};
                records.emplace(getTypeQualifier(type), std::move(recordInfo));
            }
        }

        ramRecordTypes = Json(records);
        return ramRecordTypes;
    }
};

}  // namespace souffle
