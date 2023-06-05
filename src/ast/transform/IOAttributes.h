/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IOAttributes.h
 *
 * Defines AST transformation to set attribute names and types in IO
 * operations.
 *
 ***********************************************************************/

#pragma once

#include "ast/AlgebraicDataType.h"
#include "ast/Attribute.h"
#include "ast/Directive.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/RecordType.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/Type.h"
#include "ast/analysis/typesystem/TypeEnvironment.h"
#include "ast/analysis/typesystem/TypeSystem.h"
#include "ast/transform/Transformer.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/StringUtil.h"
#include "souffle/utility/json11.h"
#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace souffle::ast::transform {

/**
 * Transformation pass to set attribute names and types in IO operations.
 */
class IOAttributesTransformer : public Transformer {
public:
    std::string getName() const override {
        return "IOAttributesTransformer";
    }

private:
    IOAttributesTransformer* cloning() const override {
        return new IOAttributesTransformer();
    }

    bool transform(TranslationUnit& translationUnit) override {
        bool changed = false;

        changed |= setAttributeNames(translationUnit);
        changed |= setAttributeTypes(translationUnit);
        changed |= setAttributeParams(translationUnit);

        return changed;
    }

    bool setAttributeParams(TranslationUnit& translationUnit) {
        bool changed = false;
        Program& program = translationUnit.getProgram();

        for (Directive* io : program.getDirectives()) {
            if (io->getType() == ast::DirectiveType::limitsize) {
                continue;
            }
            Relation* rel = program.getRelation(*io);
            // Prepare type system information.
            std::vector<std::string> attributesParams;

            for (const auto* attribute : rel->getAttributes()) {
                attributesParams.push_back(attribute->getName());
            }

            // Casting due to json11.h type requirements.
            long long arity{static_cast<long long>(rel->getArity())};

            json11::Json relJson = json11::Json::object{{"arity", arity},
                    {"params", json11::Json::array(attributesParams.begin(), attributesParams.end())}};

            json11::Json params = json11::Json::object{
                    {"relation", relJson}, {"records", getRecordsParams(translationUnit)}};

            io->addParameter("params", params.dump());
            changed = true;
        }
        return changed;
    }

    bool setAttributeNames(TranslationUnit& translationUnit) {
        bool changed = false;
        Program& program = translationUnit.getProgram();
        for (Directive* io : program.getDirectives()) {
            if (io->getType() == ast::DirectiveType::limitsize) {
                continue;
            }
            if (io->hasParameter("attributeNames")) {
                continue;
            }
            Relation* rel = program.getRelation(*io);
            std::string delimiter("\t");
            if (io->hasParameter("delimiter")) {
                delimiter = io->getParameter("delimiter");
            }

            std::vector<std::string> attributeNames;
            for (const auto* attribute : rel->getAttributes()) {
                attributeNames.push_back(attribute->getName());
            }
            io->addParameter("attributeNames", toString(join(attributeNames, delimiter)));
            changed = true;
        }
        return changed;
    }

    bool setAttributeTypes(TranslationUnit& translationUnit) {
        bool changed = false;
        Program& program = translationUnit.getProgram();
        auto& typeEnv = translationUnit.getAnalysis<analysis::TypeEnvironmentAnalysis>().getTypeEnvironment();

        for (Directive* io : program.getDirectives()) {
            Relation* rel = program.getRelation(*io);
            // Prepare type system information.
            std::vector<std::string> attributesTypes;

            for (const auto* attribute : rel->getAttributes()) {
                auto typeName = attribute->getTypeName();
                auto type = getTypeQualifier(typeEnv.getType(typeName));
                attributesTypes.push_back(type);
            }

            // Casting due to json11.h type requirements.
            long long arity{static_cast<long long>(rel->getArity())};

            json11::Json relJson = json11::Json::object{{"arity", arity},
                    {"types", json11::Json::array(attributesTypes.begin(), attributesTypes.end())}};

            json11::Json types =
                    json11::Json::object{{"relation", relJson}, {"records", getRecordsTypes(translationUnit)},
                            {"ADTs", getAlgebraicDataTypes(translationUnit)}};

            io->addParameter("types", types.dump());
            changed = true;
        }
        return changed;
    }

    std::string getRelationName(const Directive* node) {
        return toString(join(node->getQualifiedName().getQualifiers(), "."));
    }

    /**
     * Get sum types info for IO.
     * If they don't exists - create them.
     *
     * The structure of JSON is approximately:
     * {"ADTs" : {ADT_NAME : {"branches" : [branch..]}, {"arity": ...}}}
     * branch = {{"types": [types ...]}, ["name": ...]}
     */
    json11::Json getAlgebraicDataTypes(TranslationUnit& translationUnit) const {
        static json11::Json sumTypesInfo;

        // Check if the types were already constructed
        if (!sumTypesInfo.is_null()) {
            return sumTypesInfo;
        }

        Program& program = translationUnit.getProgram();
        auto& typeEnv = translationUnit.getAnalysis<analysis::TypeEnvironmentAnalysis>().getTypeEnvironment();

        std::map<std::string, json11::Json> sumTypes;

        for (auto* astType : program.getTypes()) {
            const auto& type = typeEnv.getType(*astType);

            if (isA<analysis::AlgebraicDataType>(skipAliasesType(type))) {
                // resolve alias-type to adt
                auto& sumType = asAssert<analysis::AlgebraicDataType>(skipAliasesType(type));
                auto& branches = sumType.getBranches();

                std::vector<json11::Json> branchesInfo;

                for (const auto& branch : branches) {
                    std::vector<json11::Json> branchTypes;
                    for (auto* type : branch.types) {
                        branchTypes.push_back(getTypeQualifier(*type));
                    }

                    auto branchInfo = json11::Json::object{
                            {{"types", std::move(branchTypes)}, {"name", branch.name.toString()}}};
                    branchesInfo.push_back(std::move(branchInfo));
                }

                auto typeQualifier = analysis::getTypeQualifier(type);
                auto&& sumInfo = json11::Json::object{{{"branches", std::move(branchesInfo)},
                        {"arity", static_cast<long long>(branches.size())}, {"enum", isADTEnum(sumType)}}};
                sumTypes.emplace(std::move(typeQualifier), std::move(sumInfo));
            }
        }

        sumTypesInfo = json11::Json(sumTypes);
        return sumTypesInfo;
    }

    json11::Json getRecordsTypes(TranslationUnit& translationUnit) const {
        static json11::Json ramRecordTypes;
        // Check if the types where already constructed
        if (!ramRecordTypes.is_null()) {
            return ramRecordTypes;
        }

        Program& program = translationUnit.getProgram();
        auto& typeEnv = translationUnit.getAnalysis<analysis::TypeEnvironmentAnalysis>().getTypeEnvironment();
        std::vector<std::string> elementTypes;
        std::map<std::string, json11::Json> records;

        // Iterate over all record types in the program populating the records map.
        for (auto* astType : program.getTypes()) {
            const auto& type = typeEnv.getType(*astType);
            if (isA<analysis::RecordType>(skipAliasesType(type))) {
                elementTypes.clear();

                for (const analysis::Type* field :
                        as<analysis::RecordType>(skipAliasesType(type))->getFields()) {
                    elementTypes.push_back(getTypeQualifier(*field));
                }
                const std::size_t recordArity = elementTypes.size();
                json11::Json recordInfo = json11::Json::object{
                        {"types", std::move(elementTypes)}, {"arity", static_cast<long long>(recordArity)}};
                records.emplace(getTypeQualifier(type), std::move(recordInfo));
            }
        }

        ramRecordTypes = json11::Json(records);
        return ramRecordTypes;
    }

    json11::Json getRecordsParams(TranslationUnit& translationUnit) const {
        static json11::Json ramRecordParams;
        // Check if the types where already constructed
        if (!ramRecordParams.is_null()) {
            return ramRecordParams;
        }

        Program& program = translationUnit.getProgram();
        auto& typeEnv = translationUnit.getAnalysis<analysis::TypeEnvironmentAnalysis>().getTypeEnvironment();
        std::vector<std::string> elementParams;
        std::map<std::string, json11::Json> records;

        // Iterate over all record types in the program populating the records map.
        const auto programTypes = program.getTypes();
        for (auto* astType : programTypes) {
            // if the ast type is an alias, we have to traverse it:
            const auto& unaliasedType = skipAliasesType(typeEnv.getType(*astType));

            if (isA<analysis::RecordType>(unaliasedType)) {
                elementParams.clear();

                // find the ast type associated with the unaliased type
                const auto unaliasedAstType = std::find_if(programTypes.begin(), programTypes.end(),
                        [&](Type* x) { return x->getQualifiedName() == unaliasedType.getName(); });

                // list the fields from the unaliased type
                for (const auto field : as<ast::RecordType>(*unaliasedAstType)->getFields()) {
                    elementParams.push_back(field->getName());
                }
                const std::size_t recordArity = elementParams.size();
                json11::Json recordInfo = json11::Json::object{
                        {"params", std::move(elementParams)}, {"arity", static_cast<long long>(recordArity)}};
                records.emplace(astType->getQualifiedName().toString(), std::move(recordInfo));
            }
        }

        ramRecordParams = json11::Json(records);
        return ramRecordParams;
    }
};

}  // namespace souffle::ast::transform
