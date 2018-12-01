/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstTranslator.cpp
 *
 * Translator from AST to RAM structures.
 *
 ***********************************************************************/

#include "AstTranslator.h"
#include "AstArgument.h"
#include "AstAttribute.h"
#include "AstClause.h"
#include "AstIODirective.h"
#include "AstLiteral.h"
#include "AstNode.h"
#include "AstProgram.h"
#include "AstRelation.h"
#include "AstTranslationUnit.h"
#include "AstTypeAnalysis.h"
#include "AstVisitor.h"
#include "BinaryConstraintOps.h"
#include "DebugReport.h"
#include "Global.h"
#include "IODirectives.h"
#include "LogStatement.h"
#include "PrecedenceGraph.h"
#include "RamCondition.h"
#include "RamNode.h"
#include "RamOperation.h"
#include "RamProgram.h"
#include "RamRelation.h"
#include "RamStatement.h"
#include "RamTranslationUnit.h"
#include "RamValue.h"
#include "SrcLocation.h"
#include "SymbolMask.h"
#include "TypeSystem.h"
#include "Util.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <typeinfo>
#include <utility>
#include <vector>

namespace souffle {

class ErrorReport;
class SymbolTable;

/** get symbol mask */
SymbolMask AstTranslator::getSymbolMask(const AstRelation& rel) {
    auto arity = rel.getArity();
    SymbolMask res(arity);
    for (size_t i = 0; i < arity; i++) {
        res.setSymbol(i, isSymbolType(typeEnv->getType(rel.getAttribute(i)->getTypeName())));
    }
    return res;
}

void AstTranslator::makeIODirective(IODirectives& ioDirective, const AstRelation* rel,
        const std::string& filePath, const std::string& fileExt, const bool isIntermediate) {
    // set relation name correctly
    ioDirective.setRelationName(getRelationName(rel->getName()));

    // set a default IO type of file and a default filename if not supplied
    if (!ioDirective.has("IO")) {
        ioDirective.setIOType("file");
    }

    // load intermediate relations from correct files
    if (ioDirective.getIOType() == "file") {
        // all intermediate relations are given the default delimiter and have no headers
        if (isIntermediate) {
            ioDirective.set("intermediate", "true");
            ioDirective.set("delimiter", "\t");
            ioDirective.set("headers", "false");
        }

        // set filename by relation if not given, or if relation is intermediate
        if (!ioDirective.has("filename") || isIntermediate) {
            ioDirective.setFileName(ioDirective.getRelationName() + fileExt);
        }

        // if filename is not an absolute path, concat with cmd line facts directory
        if (ioDirective.getIOType() == "file" && ioDirective.getFileName().front() != '/') {
            ioDirective.setFileName(filePath + "/" + ioDirective.getFileName());
        }
    }
}

std::vector<IODirectives> AstTranslator::getInputIODirectives(
        const AstRelation* rel, std::string filePath, const std::string& fileExt) {
    std::vector<IODirectives> inputDirectives;

    for (const auto& current : rel->getIODirectives()) {
        if (current->isInput()) {
            IODirectives ioDirectives;
            for (const auto& currentPair : current->getIODirectiveMap()) {
                ioDirectives.set(currentPair.first, currentPair.second);
            }
            inputDirectives.push_back(ioDirectives);
        }
    }

    if (inputDirectives.empty()) {
        inputDirectives.emplace_back();
    }

    const std::string inputFilePath = (filePath.empty()) ? Global::config().get("fact-dir") : filePath;
    const std::string inputFileExt = (fileExt.empty()) ? ".facts" : fileExt;

    const bool isIntermediate =
            (Global::config().has("engine") && inputFilePath == Global::config().get("output-dir") &&
                    inputFileExt == ".facts");

    for (auto& ioDirective : inputDirectives) {
        makeIODirective(ioDirective, rel, inputFilePath, inputFileExt, isIntermediate);
    }

    return inputDirectives;
}

std::vector<IODirectives> AstTranslator::getOutputIODirectives(
        const AstRelation* rel, std::string filePath, const std::string& fileExt) {
    std::vector<IODirectives> outputDirectives;

    for (const auto& current : rel->getIODirectives()) {
        if (current->isOutput()) {
            IODirectives ioDirectives;
            for (const auto& currentPair : current->getIODirectiveMap()) {
                ioDirectives.set(currentPair.first, currentPair.second);
            }
            outputDirectives.push_back(ioDirectives);
        }
    }

    // If stdout is requested then remove all directives from the datalog file.
    if (Global::config().get("output-dir") == "-") {
        outputDirectives.clear();
        IODirectives ioDirectives;
        ioDirectives.setIOType("stdout");
        ioDirectives.set("headers", "true");
        outputDirectives.push_back(ioDirectives);
    }

    if (outputDirectives.empty()) {
        outputDirectives.emplace_back();
    }

    const std::string outputFilePath = (filePath.empty()) ? Global::config().get("output-dir") : filePath;
    const std::string outputFileExt = (fileExt.empty()) ? ".csv" : fileExt;

    const bool isIntermediate =
            (Global::config().has("engine") && outputFilePath == Global::config().get("output-dir") &&
                    outputFileExt == ".facts");

    for (auto& ioDirective : outputDirectives) {
        makeIODirective(ioDirective, rel, outputFilePath, outputFileExt, isIntermediate);

        if (!ioDirective.has("attributeNames")) {
            std::string delimiter("\t");
            if (ioDirective.has("delimiter")) {
                delimiter = ioDirective.get("delimiter");
            }
            std::vector<std::string> attributeNames;
            for (unsigned int i = 0; i < rel->getArity(); i++) {
                attributeNames.push_back(rel->getAttribute(i)->getAttributeName());
            }

            if (Global::config().has("provenance")) {
                std::vector<std::string> originalAttributeNames(
                        attributeNames.begin(), attributeNames.end() - 2);
                ioDirective.set("attributeNames", toString(join(originalAttributeNames, delimiter)));
            } else {
                ioDirective.set("attributeNames", toString(join(attributeNames, delimiter)));
            }
        }
    }

    return outputDirectives;
}

std::unique_ptr<RamRelationReference> AstTranslator::createRelationReference(const std::string name,
        const size_t arity, const std::vector<std::string> attributeNames,
        const std::vector<std::string> attributeTypeQualifiers, const SymbolMask mask, const bool input,
        const bool computed, const bool output, const bool btree, const bool brie, const bool eqrel) {
    const RamRelation* ramRel = ramProg->getRelation(name);
    if (ramRel == nullptr) {
        ramProg->addRelation(std::make_unique<RamRelation>(name, arity, attributeNames,
                attributeTypeQualifiers, mask, input, computed, output, btree, brie, eqrel));
        ramRel = ramProg->getRelation(name);
        assert(ramRel != nullptr && "cannot find relation");
    }
    return std::make_unique<RamRelationReference>(ramRel);
}

std::unique_ptr<RamRelationReference> AstTranslator::createRelationReference(
        const std::string name, const size_t arity) {
    return createRelationReference(
            name, arity, {}, {}, SymbolMask(arity), false, false, false, false, false, false);
}

std::unique_ptr<RamRelationReference> AstTranslator::translateRelation(const AstAtom* atom) {
    if (const auto rel = getAtomRelation(atom, program)) {
        return translateRelation(rel);
    } else {
        return createRelationReference(getRelationName(atom->getName()), atom->getArity());
    }
}

std::unique_ptr<RamRelationReference> AstTranslator::translateRelation(
        const AstRelation* rel, const std::string relationNamePrefix) {
    std::vector<std::string> attributeNames;
    std::vector<std::string> attributeTypeQualifiers;
    for (size_t i = 0; i < rel->getArity(); ++i) {
        attributeNames.push_back(rel->getAttribute(i)->getAttributeName());
        if (typeEnv) {
            attributeTypeQualifiers.push_back(
                    getTypeQualifier(typeEnv->getType(rel->getAttribute(i)->getTypeName())));
        }
    }

    return createRelationReference(relationNamePrefix + getRelationName(rel->getName()), rel->getArity(),
            attributeNames, attributeTypeQualifiers, getSymbolMask(*rel), rel->isInput(), rel->isComputed(),
            rel->isOutput(), rel->isBTree(), rel->isBrie(), rel->isEqRel());
}

std::unique_ptr<RamRelationReference> AstTranslator::translateDeltaRelation(const AstRelation* rel) {
    return translateRelation(rel, "@delta_");
}

std::unique_ptr<RamRelationReference> AstTranslator::translateNewRelation(const AstRelation* rel) {
    return translateRelation(rel, "@new_");
}

std::unique_ptr<RamValue> AstTranslator::translateValue(const AstArgument* arg, const ValueIndex& index) {
    if (arg == nullptr) {
        return std::unique_ptr<RamValue>();
    }

    class ValueTranslator : public AstVisitor<std::unique_ptr<RamValue>> {
        AstTranslator& translator;
        const ValueIndex& index;

    public:
        ValueTranslator(AstTranslator& translator, const ValueIndex& index)
                : translator(translator), index(index) {}

        std::unique_ptr<RamValue> visitVariable(const AstVariable& var) override {
            assert(index.isDefined(var) && "variable not grounded");
            const Location& loc = index.getDefinitionPoint(var);
            return std::make_unique<RamElementAccess>(loc.level, loc.element, loc.name);
        }

        std::unique_ptr<RamValue> visitUnnamedVariable(const AstUnnamedVariable& var) override {
            return nullptr;  // utilised to identify _ values
        }

        std::unique_ptr<RamValue> visitConstant(const AstConstant& c) override {
            return std::make_unique<RamNumber>(c.getIndex());
        }

        std::unique_ptr<RamValue> visitUnaryFunctor(const AstUnaryFunctor& uf) override {
            return std::make_unique<RamUnaryOperator>(
                    uf.getFunction(), translator.translateValue(uf.getOperand(), index));
        }

        std::unique_ptr<RamValue> visitUserDefinedFunctor(const AstUserDefinedFunctor& udf) override {
            std::vector<std::unique_ptr<RamValue>> values;
            for (const auto& cur : udf.getArguments()) {
                values.push_back(translator.translateValue(cur, index));
            }
            const AstFunctorDeclaration* decl = translator.program->getFunctorDeclaration(udf.getName());
            std::string type = decl->getType();
            return std::make_unique<RamUserDefinedOperator>(udf.getName(), type, std::move(values));
        }

        std::unique_ptr<RamValue> visitBinaryFunctor(const AstBinaryFunctor& bf) override {
            return std::make_unique<RamBinaryOperator>(bf.getFunction(),
                    translator.translateValue(bf.getLHS(), index),
                    translator.translateValue(bf.getRHS(), index));
        }

        std::unique_ptr<RamValue> visitTernaryFunctor(const AstTernaryFunctor& tf) override {
            return std::make_unique<RamTernaryOperator>(tf.getFunction(),
                    translator.translateValue(tf.getArg(0), index),
                    translator.translateValue(tf.getArg(1), index),
                    translator.translateValue(tf.getArg(2), index));
        }

        std::unique_ptr<RamValue> visitCounter(const AstCounter&) override {
            return std::make_unique<RamAutoIncrement>();
        }

        std::unique_ptr<RamValue> visitRecordInit(const AstRecordInit& init) override {
            std::vector<std::unique_ptr<RamValue>> values;
            for (const auto& cur : init.getArguments()) {
                values.push_back(translator.translateValue(cur, index));
            }
            return std::make_unique<RamPack>(std::move(values));
        }

        std::unique_ptr<RamValue> visitAggregator(const AstAggregator& agg) override {
            // here we look up the location the aggregation result gets bound
            auto loc = index.getAggregatorLocation(agg);
            return std::make_unique<RamElementAccess>(loc.level, loc.element, loc.name);
        }

        std::unique_ptr<RamValue> visitSubroutineArgument(const AstSubroutineArgument& subArg) override {
            return std::make_unique<RamArgument>(subArg.getNumber());
        }
    };

    return ValueTranslator(*this, index)(*arg);
}

std::unique_ptr<RamCondition> AstTranslator::translateConstraint(
        const AstLiteral* lit, const ValueIndex& index) {
    class ConstraintTranslator : public AstVisitor<std::unique_ptr<RamCondition>> {
        AstTranslator& translator;
        const ValueIndex& index;

    public:
        ConstraintTranslator(AstTranslator& translator, const ValueIndex& index)
                : translator(translator), index(index) {}

        /** for atoms */
        std::unique_ptr<RamCondition> visitAtom(const AstAtom&) override {
            return nullptr;  // covered already within the scan/lookup generation step
        }

        /** for binary relations */
        std::unique_ptr<RamCondition> visitBinaryConstraint(const AstBinaryConstraint& binRel) override {
            std::unique_ptr<RamValue> valLHS = translator.translateValue(binRel.getLHS(), index);
            std::unique_ptr<RamValue> valRHS = translator.translateValue(binRel.getRHS(), index);
            return std::make_unique<RamBinaryRelation>(binRel.getOperator(),
                    translator.translateValue(binRel.getLHS(), index),
                    translator.translateValue(binRel.getRHS(), index));
        }

        /** for negations */
        std::unique_ptr<RamCondition> visitNegation(const AstNegation& neg) override {
            // get contained atom
            const AstAtom* atom = neg.getAtom();

            // create constraint
            RamNotExists* notExists = new RamNotExists(translator.translateRelation(atom));

            auto arity = atom->getArity();

            // account for two extra provenance columns
            if (Global::config().has("provenance")) {
                arity -= 2;
            }

            for (size_t i = 0; i < arity; i++) {
                const auto& arg = atom->getArgument(i);
                // for (const auto& arg : atom->getArguments()) {
                notExists->addArg(translator.translateValue(arg, index));
            }

            // we don't care about the provenance columns when doing the existence check
            if (Global::config().has("provenance")) {
                notExists->addArg(nullptr);
                notExists->addArg(nullptr);
            }

            // add constraint
            return std::unique_ptr<RamCondition>(notExists);
        }

        /** for provenance negation */
        std::unique_ptr<RamCondition> visitProvenanceNegation(const AstProvenanceNegation& neg) override {
            // get contained atom
            const AstAtom* atom = neg.getAtom();

            // create constraint
            RamProvenanceNotExists* notExists =
                    new RamProvenanceNotExists(translator.translateRelation(atom));

            auto arity = atom->getArity();

            // account for two extra provenance columns
            if (Global::config().has("provenance")) {
                arity -= 2;
            }

            for (size_t i = 0; i < arity; i++) {
                const auto& arg = atom->getArgument(i);
                // for (const auto& arg : atom->getArguments()) {
                notExists->addArg(translator.translateValue(arg, index));
            }

            // we don't care about the provenance columns when doing the existence check
            if (Global::config().has("provenance")) {
                notExists->addArg(nullptr);
                // add the height annotation for provenanceNotExists
                notExists->addArg(translator.translateValue(atom->getArgument(arity + 1), index));
            }

            // add constraint
            return std::unique_ptr<RamCondition>(notExists);
        }
    };

    return ConstraintTranslator(*this, index)(*lit);
}

std::unique_ptr<AstClause> AstTranslator::ClauseTranslator::getReorderedClause(
        const AstClause& clause, const int version) const {
    const auto plan = clause.getExecutionPlan();

    // check whether there is an imposed order constraint
    if (plan != nullptr && plan->hasOrderFor(version)) {
        // get the imposed order
        const auto& order = plan->getOrderFor(version);

        // create a copy and fix order
        std::unique_ptr<AstClause> reorderedClause(clause.clone());

        // Change order to start at zero
        std::vector<unsigned int> newOrder(order.size());
        std::transform(order.begin(), order.end(), newOrder.begin(),
                [](unsigned int i) -> unsigned int { return i - 1; });

        // re-order atoms
        reorderedClause->reorderAtoms(newOrder);

        // clear other order and fix plan
        reorderedClause->clearExecutionPlan();
        reorderedClause->setFixedExecutionPlan();

        return reorderedClause;
    }

    return nullptr;
}

AstTranslator::ClauseTranslator::arg_list* AstTranslator::ClauseTranslator::getArgList(
        const AstNode* curNode, std::map<const AstNode*, std::unique_ptr<arg_list>>& nodeArgs) const {
    if (!nodeArgs.count(curNode)) {
        if (auto rec = dynamic_cast<const AstRecordInit*>(curNode)) {
            nodeArgs[curNode] = std::make_unique<arg_list>(rec->getArguments());
        } else if (auto atom = dynamic_cast<const AstAtom*>(curNode)) {
            nodeArgs[curNode] = std::make_unique<arg_list>(atom->getArguments());
        } else {
            assert(false && "node type doesn't have arguments!");
        }
    }
    return nodeArgs[curNode].get();
}

void AstTranslator::ClauseTranslator::indexValues(const AstNode* curNode,
        std::map<const AstNode*, std::unique_ptr<arg_list>>& nodeArgs,
        std::map<const arg_list*, int>& arg_level, RamRelationReference* relation) {
    arg_list* cur = getArgList(curNode, nodeArgs);
    for (size_t pos = 0; pos < cur->size(); ++pos) {
        // get argument
        auto& arg = (*cur)[pos];

        // check for variable references
        if (auto var = dynamic_cast<const AstVariable*>(arg)) {
            if (pos < relation->getArity()) {
                valueIndex.addVarReference(*var, arg_level[cur], pos, relation->getArg(pos));
            } else {
                valueIndex.addVarReference(*var, arg_level[cur], pos);
            }
        }

        // check for nested records
        if (auto rec = dynamic_cast<const AstRecordInit*>(arg)) {
            // introduce new nesting level for unpack
            int unpack_level = level++;
            op_nesting.push_back(rec);
            arg_level[getArgList(rec, nodeArgs)] = unpack_level;
            valueIndex.setRecordUnpackLevel(*rec, unpack_level);

            // register location of record
            valueIndex.setRecordDefinition(*rec, arg_level[cur], pos);

            // resolve nested components
            indexValues(rec, nodeArgs, arg_level, relation);
        }
    }
}

/** index values in rule */
void AstTranslator::ClauseTranslator::createValueIndex(const AstClause& clause) {
    for (const AstAtom* atom : clause.getAtoms()) {
        // std::map<const arg_list*, int> arg_level;
        std::map<const AstNode*, std::unique_ptr<arg_list>> nodeArgs;

        std::map<const arg_list*, int> arg_level;
        nodeArgs[atom] = std::make_unique<arg_list>(atom->getArguments());
        // the atom is obtained at the current level
        // increment nesting level for the atom
        arg_level[nodeArgs[atom].get()] = level++;
        op_nesting.push_back(atom);

        indexValues(atom, nodeArgs, arg_level, translator.translateRelation(atom).get());
    }

    // add aggregation functions
    visitDepthFirstPostOrder(clause, [&](const AstAggregator& cur) {
        // add each aggregator expression only once
        if (any_of(aggregators, [&](const AstAggregator* agg) { return *agg == cur; })) {
            return;
        }

        int aggLoc = level++;
        valueIndex.setAggregatorLocation(cur, Location({aggLoc, 0}));

        // bind aggregator variables to locations
        assert(nullptr != dynamic_cast<const AstAtom*>(cur.getBodyLiterals()[0]));
        const AstAtom& atom = static_cast<const AstAtom&>(*cur.getBodyLiterals()[0]);
        for (size_t pos = 0; pos < atom.getArguments().size(); ++pos) {
            if (const auto* var = dynamic_cast<const AstVariable*>(atom.getArgument(pos))) {
                valueIndex.addVarReference(
                        *var, aggLoc, (int)pos, translator.translateRelation(&atom)->getArg(pos));
            }
        };

        // and remember aggregator
        aggregators.push_back(&cur);
    });
}

/** begin with projection */
std::unique_ptr<RamOperation> AstTranslator::ClauseTranslator::createOperation(const AstClause& clause) {
    const auto head = clause.getHead();

    std::unique_ptr<RamProject> project =
            std::make_unique<RamProject>(translator.translateRelation(head), level);

    for (AstArgument* arg : head->getArguments()) {
        project->addArg(translator.translateValue(arg, valueIndex));
    }

    // check existence for original tuple if we have provenance
    // only if we don't compile
    if (Global::config().has("provenance") &&
            ((!Global::config().has("compile") && !Global::config().has("dl-program") &&
                    !Global::config().has("generate")))) {
        auto uniquenessEnforcement = std::make_unique<RamNotExists>(translator.translateRelation(head));
        auto arity = head->getArity() - 2;

        bool isVolatile = true;
        // add args for original tuple
        for (size_t i = 0; i < arity; i++) {
            auto arg = head->getArgument(i);

            // don't add counters
            visitDepthFirst(*arg, [&](const AstCounter& cur) { isVolatile = false; });
            uniquenessEnforcement->addArg(translator.translateValue(arg, valueIndex));
        }

        // add two unnamed args for provenance columns
        uniquenessEnforcement->addArg(nullptr);
        uniquenessEnforcement->addArg(nullptr);

        if (isVolatile) {
            project->addCondition(std::move(uniquenessEnforcement), *project);
        }
    }

    // build up insertion call
    return std::move(project);  // start with innermost
}

std::unique_ptr<RamOperation> AstTranslator::ProvenanceClauseTranslator::createOperation(
        const AstClause& clause) {
    auto* returnValue = new RamReturn(level);

    // get all values in the body
    for (AstLiteral* lit : clause.getBodyLiterals()) {
        if (auto atom = dynamic_cast<AstAtom*>(lit)) {
            for (AstArgument* arg : atom->getArguments()) {
                returnValue->addValue(translator.translateValue(arg, valueIndex));
            }
        } else if (auto neg = dynamic_cast<AstNegation*>(lit)) {
            for (AstArgument* arg : neg->getAtom()->getArguments()) {
                returnValue->addValue(translator.translateValue(arg, valueIndex));
            }
        } else if (auto neg = dynamic_cast<AstProvenanceNegation*>(lit)) {
            for (size_t i = 0; i < neg->getAtom()->getArguments().size() - 2; ++i) {
                auto arg = neg->getAtom()->getArguments()[i];
                returnValue->addValue(translator.translateValue(arg, valueIndex));
            }
            returnValue->addValue(std::make_unique<RamNumber>(-1));
            returnValue->addValue(std::make_unique<RamNumber>(-1));
        }
    }

    return std::unique_ptr<RamOperation>(returnValue);
}

std::unique_ptr<RamCondition> AstTranslator::ClauseTranslator::createCondition(
        const AstClause& originalClause) {
    const auto head = originalClause.getHead();

    // add stopping criteria for nullary relations
    // (if it contains already the null tuple, don't re-compute)
    std::unique_ptr<RamCondition> ramInsertCondition;
    if (head->getArity() == 0) {
        ramInsertCondition = std::make_unique<RamEmpty>(translator.translateRelation(head));
    }
    return ramInsertCondition;
}

std::unique_ptr<RamCondition> AstTranslator::ProvenanceClauseTranslator::createCondition(
        const AstClause& originalClause) {
    return std::unique_ptr<RamCondition>();
}

/** generate RAM code for a clause */
std::unique_ptr<RamStatement> AstTranslator::ClauseTranslator::translateClause(
        const AstClause& clause, const AstClause& originalClause, const int version) {
    if (auto reorderedClause = getReorderedClause(clause, version)) {
        // translate reordered clause
        return translateClause(*reorderedClause, originalClause, version);
    }

    // get extract some details
    const AstAtom* head = clause.getHead();

    // handle facts
    if (clause.isFact()) {
        // translate arguments
        std::vector<std::unique_ptr<RamValue>> values;
        for (auto& arg : head->getArguments()) {
            values.push_back(translator.translateValue(arg, ValueIndex()));
        }

        // create a fact statement
        return std::make_unique<RamFact>(translator.translateRelation(head), std::move(values));
    }

    // the rest should be rules
    assert(clause.isRule());

    createValueIndex(clause);

    // -- create RAM statement --

    std::unique_ptr<RamOperation> op = createOperation(clause);

    // add aggregator levels
    for (auto it = aggregators.rbegin(); it != aggregators.rend(); ++it) {
        const AstAggregator* cur = *it;

        // translate aggregation function
        RamAggregate::Function fun = RamAggregate::MIN;
        switch (cur->getOperator()) {
            case AstAggregator::min:
                fun = RamAggregate::MIN;
                break;
            case AstAggregator::max:
                fun = RamAggregate::MAX;
                break;
            case AstAggregator::count:
                fun = RamAggregate::COUNT;
                break;
            case AstAggregator::sum:
                fun = RamAggregate::SUM;
                break;
        }

        // translate target expression
        std::unique_ptr<RamValue> value = translator.translateValue(cur->getTargetExpression(), valueIndex);

        // translate body literal
        assert(cur->getBodyLiterals().size() == 1 && "Unsupported complex aggregation body encountered!");
        const AstAtom* atom = dynamic_cast<const AstAtom*>(cur->getBodyLiterals()[0]);
        assert(atom && "Unsupported complex aggregation body encountered!");

        // add Ram-Aggregation layer
        op = std::make_unique<RamAggregate>(
                std::move(op), fun, std::move(value), translator.translateRelation(atom));

        // add constant constraints
        for (size_t pos = 0; pos < atom->argSize(); ++pos) {
            if (auto* c = dynamic_cast<AstConstant*>(atom->getArgument(pos))) {
                op->addCondition(std::make_unique<RamBinaryRelation>(BinaryConstraintOp::EQ,
                        std::make_unique<RamElementAccess>(
                                --level, pos, translator.translateRelation(atom)->getArg(pos)),
                        std::make_unique<RamNumber>(c->getIndex())));
            }
        }
    }

    // build operation bottom-up
    while (!op_nesting.empty()) {
        // get next operator
        const AstNode* cur = op_nesting.back();
        op_nesting.pop_back();

        // get current nesting level
        auto level = op_nesting.size();

        if (const auto* atom = dynamic_cast<const AstAtom*>(cur)) {
            // find out whether a "search" or "if" should be issued
            bool isExistCheck = !valueIndex.isSomethingDefinedOn(level);
            for (size_t pos = 0; pos < atom->argSize(); ++pos) {
                if (dynamic_cast<AstAggregator*>(atom->getArgument(pos))) {
                    isExistCheck = false;
                }
            }

            // add a scan level
            if (Global::config().has("profile")) {
                std::stringstream ss;
                ss << head->getName();
                ss.str("");
                ss << "@frequency-atom" << ';';
                ss << originalClause.getHead()->getName() << ';';
                ss << version << ';';
                ss << stringify(toString(clause)) << ';';
                ss << stringify(toString(*atom)) << ';';
                ss << stringify(toString(originalClause)) << ';';
                ss << level << ';';
                op = std::make_unique<RamScan>(
                        translator.translateRelation(atom), std::move(op), isExistCheck, ss.str());
            } else {
                op = std::make_unique<RamScan>(
                        translator.translateRelation(atom), std::move(op), isExistCheck);
            }

            // add constraints
            for (size_t pos = 0; pos < atom->argSize(); ++pos) {
                if (auto* c = dynamic_cast<AstConstant*>(atom->getArgument(pos))) {
                    op->addCondition(std::make_unique<RamBinaryRelation>(BinaryConstraintOp::EQ,
                            std::make_unique<RamElementAccess>(
                                    level, pos, translator.translateRelation(atom)->getArg(pos)),
                            std::make_unique<RamNumber>(c->getIndex())));
                } else if (auto* agg = dynamic_cast<AstAggregator*>(atom->getArgument(pos))) {
                    auto loc = valueIndex.getAggregatorLocation(*agg);
                    op->addCondition(std::make_unique<RamBinaryRelation>(BinaryConstraintOp::EQ,
                            std::make_unique<RamElementAccess>(
                                    level, pos, translator.translateRelation(atom)->getArg(pos)),
                            std::make_unique<RamElementAccess>(loc.level, loc.element, loc.name)));
                }
            }

            // TODO: support constants in nested records!
        } else if (const auto* rec = dynamic_cast<const AstRecordInit*>(cur)) {
            // add an unpack level
            const Location& loc = valueIndex.getDefinitionPoint(*rec);
            op = std::make_unique<RamLookup>(
                    std::move(op), loc.level, loc.element, rec->getArguments().size());

            // add constant constraints
            for (size_t pos = 0; pos < rec->getArguments().size(); ++pos) {
                if (AstConstant* c = dynamic_cast<AstConstant*>(rec->getArguments()[pos])) {
                    op->addCondition(std::make_unique<RamBinaryRelation>(BinaryConstraintOp::EQ,
                            std::make_unique<RamElementAccess>(level, pos),
                            std::make_unique<RamNumber>(c->getIndex())));
                }
            }
        } else {
            std::cout << "Unsupported AST node type: " << typeid(*cur).name() << "\n";
            assert(false && "Unsupported AST node for creation of scan-level!");
        }
    }

    /* add equivalence constraints imposed by variable binding */
    for (const auto& cur : valueIndex.getVariableReferences()) {
        // the first appearance
        const Location& first = *cur.second.begin();
        // all other appearances
        for (const Location& loc : cur.second) {
            if (first != loc) {
                op->addCondition(std::make_unique<RamBinaryRelation>(BinaryConstraintOp::EQ,
                        std::make_unique<RamElementAccess>(first.level, first.element, first.name),
                        std::make_unique<RamElementAccess>(loc.level, loc.element, loc.name)));
            }
        }
    }

    /* add conditions caused by atoms, negations, and binary relations */
    for (const auto& lit : clause.getBodyLiterals()) {
        if (auto condition = translator.translateConstraint(lit, valueIndex)) {
            op->addCondition(std::move(condition));
        }
    }

    /* generate the final RAM Insert statement */
    return std::make_unique<RamInsert>(std::move(op), createCondition(originalClause));
}

/* utility for appending statements */
void AstTranslator::appendStmt(std::unique_ptr<RamStatement>& stmtList, std::unique_ptr<RamStatement> stmt) {
    if (stmt) {
        if (stmtList) {
            RamSequence* stmtSeq;
            if ((stmtSeq = dynamic_cast<RamSequence*>(stmtList.get()))) {
                stmtSeq->add(std::move(stmt));
            } else {
                stmtList = std::make_unique<RamSequence>(std::move(stmtList), std::move(stmt));
            }
        } else {
            stmtList = std::move(stmt);
        }
    }
}

/** generate RAM code for a non-recursive relation */
std::unique_ptr<RamStatement> AstTranslator::translateNonRecursiveRelation(
        const AstRelation& rel, const RecursiveClauses* recursiveClauses) {
    /* start with an empty sequence */
    std::unique_ptr<RamStatement> res;

    // the ram table reference
    std::unique_ptr<RamRelationReference> rrel = translateRelation(&rel);

    /* iterate over all clauses that belong to the relation */
    for (AstClause* clause : rel.getClauses()) {
        // skip recursive rules
        if (recursiveClauses->recursive(clause)) {
            continue;
        }

        // translate clause
        std::unique_ptr<RamStatement> rule = ClauseTranslator(*this).translateClause(*clause, *clause);

        // add logging
        if (Global::config().has("profile")) {
            const std::string& relationName = toString(rel.getName());
            const SrcLocation& srcLocation = clause->getSrcLoc();
            const std::string clauseText = stringify(toString(*clause));
            const std::string logTimerStatement =
                    LogStatement::tNonrecursiveRule(relationName, srcLocation, clauseText);
            const std::string logSizeStatement =
                    LogStatement::nNonrecursiveRule(relationName, srcLocation, clauseText);
            rule = std::make_unique<RamSequence>(std::make_unique<RamLogTimer>(std::move(rule),
                    logTimerStatement, std::unique_ptr<RamRelationReference>(rrel->clone())));
        }

        // add debug info
        std::ostringstream ds;
        ds << toString(*clause) << "\nin file ";
        ds << clause->getSrcLoc();
        rule = std::make_unique<RamDebugInfo>(std::move(rule), ds.str());

        // add rule to result
        appendStmt(res, std::move(rule));
    }

    // add logging for entire relation
    if (Global::config().has("profile")) {
        const std::string& relationName = toString(rel.getName());
        const SrcLocation& srcLocation = rel.getSrcLoc();
        const std::string logSizeStatement = LogStatement::nNonrecursiveRelation(relationName, srcLocation);

        // add timer if we did any work
        if (res) {
            const std::string logTimerStatement =
                    LogStatement::tNonrecursiveRelation(relationName, srcLocation);
            res = std::make_unique<RamLogTimer>(
                    std::move(res), logTimerStatement, std::unique_ptr<RamRelationReference>(rrel->clone()));
        } else {
            // add table size printer
            appendStmt(res, std::make_unique<RamLogSize>(
                                    std::unique_ptr<RamRelationReference>(rrel->clone()), logSizeStatement));
        }
    }

    // done
    return res;
}

/**
 * A utility function assigning names to unnamed variables such that enclosing
 * constructs may be cloned without losing the variable-identity.
 */
void AstTranslator::nameUnnamedVariables(AstClause* clause) {
    // the node mapper conducting the actual renaming
    struct Instantiator : public AstNodeMapper {
        mutable int counter = 0;

        Instantiator() = default;

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            // apply recursive
            node->apply(*this);

            // replace unknown variables
            if (dynamic_cast<AstUnnamedVariable*>(node.get())) {
                auto name = " _unnamed_var" + toString(++counter);
                return std::make_unique<AstVariable>(name);
            }

            // otherwise nothing
            return node;
        }
    };

    // name all variables in the atoms
    Instantiator init;
    for (auto& atom : clause->getAtoms()) {
        atom->apply(init);
    }
}

/** generate RAM code for recursive relations in a strongly-connected component */
std::unique_ptr<RamStatement> AstTranslator::translateRecursiveRelation(
        const std::set<const AstRelation*>& scc, const RecursiveClauses* recursiveClauses) {
    // initialize sections
    std::unique_ptr<RamStatement> preamble;
    std::unique_ptr<RamSequence> updateTable(new RamSequence());
    std::unique_ptr<RamStatement> postamble;

    // --- create preamble ---

    // mappings for temporary relations
    std::map<const AstRelation*, std::unique_ptr<RamRelationReference>> rrel;
    std::map<const AstRelation*, std::unique_ptr<RamRelationReference>> relDelta;
    std::map<const AstRelation*, std::unique_ptr<RamRelationReference>> relNew;

    /* Compute non-recursive clauses for relations in scc and push
       the results in their delta tables. */
    for (const AstRelation* rel : scc) {
        std::unique_ptr<RamStatement> updateRelTable;

        /* create two temporary tables for relaxed semi-naive evaluation */
        rrel[rel] = translateRelation(rel);
        relDelta[rel] = translateDeltaRelation(rel);
        relNew[rel] = translateNewRelation(rel);

        /* create update statements for fixpoint (even iteration) */
        appendStmt(updateRelTable,
                std::make_unique<RamSequence>(
                        std::make_unique<RamMerge>(std::unique_ptr<RamRelationReference>(rrel[rel]->clone()),
                                std::unique_ptr<RamRelationReference>(relNew[rel]->clone())),
                        std::make_unique<RamSwap>(
                                std::unique_ptr<RamRelationReference>(relDelta[rel]->clone()),
                                std::unique_ptr<RamRelationReference>(relNew[rel]->clone())),
                        std::make_unique<RamClear>(
                                std::unique_ptr<RamRelationReference>(relNew[rel]->clone()))));

        /* measure update time for each relation */
        if (Global::config().has("profile")) {
            updateRelTable = std::make_unique<RamLogTimer>(std::move(updateRelTable),
                    LogStatement::cRecursiveRelation(toString(rel->getName()), rel->getSrcLoc()),
                    std::unique_ptr<RamRelationReference>(relNew[rel]->clone()));
        }

        /* drop temporary tables after recursion */
        appendStmt(postamble, std::make_unique<RamSequence>(
                                      std::make_unique<RamDrop>(
                                              std::unique_ptr<RamRelationReference>(relDelta[rel]->clone())),
                                      std::make_unique<RamDrop>(
                                              std::unique_ptr<RamRelationReference>(relNew[rel]->clone()))));

        /* Generate code for non-recursive part of relation */
        appendStmt(preamble, translateNonRecursiveRelation(*rel, recursiveClauses));

        /* Generate merge operation for temp tables */
        appendStmt(preamble,
                std::make_unique<RamMerge>(std::unique_ptr<RamRelationReference>(relDelta[rel]->clone()),
                        std::unique_ptr<RamRelationReference>(rrel[rel]->clone())));

        /* Add update operations of relations to parallel statements */
        updateTable->add(std::move(updateRelTable));
    }

    // --- build main loop ---

    std::unique_ptr<RamParallel> loopSeq(new RamParallel());

    // create a utility to check SCC membership
    auto isInSameSCC = [&](const AstRelation* rel) {
        return std::find(scc.begin(), scc.end(), rel) != scc.end();
    };

    /* Compute temp for the current tables */
    for (const AstRelation* rel : scc) {
        std::unique_ptr<RamStatement> loopRelSeq;

        /* Find clauses for relation rel */
        for (size_t i = 0; i < rel->clauseSize(); i++) {
            AstClause* cl = rel->getClause(i);

            // skip non-recursive clauses
            if (!recursiveClauses->recursive(cl)) {
                continue;
            }

            // each recursive rule results in several operations
            int version = 0;
            const auto& atoms = cl->getAtoms();
            for (size_t j = 0; j < atoms.size(); ++j) {
                const AstAtom* atom = atoms[j];
                const AstRelation* atomRelation = getAtomRelation(atom, program);

                // only interested in atoms within the same SCC
                if (!isInSameSCC(atomRelation)) {
                    continue;
                }

                // modify the processed rule to use relDelta and write to relNew
                std::unique_ptr<AstClause> r1(cl->clone());
                r1->getHead()->setName(relNew[rel]->getName());
                r1->getAtoms()[j]->setName(relDelta[atomRelation]->getName());
                if (Global::config().has("provenance")) {
                    r1->addToBody(std::make_unique<AstProvenanceNegation>(
                            std::unique_ptr<AstAtom>(cl->getHead()->clone())));
                } else {
                    r1->addToBody(
                            std::make_unique<AstNegation>(std::unique_ptr<AstAtom>(cl->getHead()->clone())));
                }

                // replace wildcards with variables (reduces indices when wildcards are used in recursive
                // atoms)
                nameUnnamedVariables(r1.get());

                // reduce R to P ...
                for (size_t k = j + 1; k < atoms.size(); k++) {
                    if (isInSameSCC(getAtomRelation(atoms[k], program))) {
                        AstAtom* cur = r1->getAtoms()[k]->clone();
                        cur->setName(relDelta[getAtomRelation(atoms[k], program)]->getName());
                        r1->addToBody(std::make_unique<AstNegation>(std::unique_ptr<AstAtom>(cur)));
                    }
                }

                std::unique_ptr<RamStatement> rule =
                        ClauseTranslator(*this).translateClause(*r1, *cl, version);

                /* add logging */
                if (Global::config().has("profile")) {
                    const std::string& relationName = toString(rel->getName());
                    const SrcLocation& srcLocation = cl->getSrcLoc();
                    const std::string clauseText = stringify(toString(*cl));
                    const std::string logTimerStatement =
                            LogStatement::tRecursiveRule(relationName, version, srcLocation, clauseText);
                    const std::string logSizeStatement =
                            LogStatement::nRecursiveRule(relationName, version, srcLocation, clauseText);
                    rule = std::make_unique<RamSequence>(std::make_unique<RamLogTimer>(std::move(rule),
                            logTimerStatement, std::unique_ptr<RamRelationReference>(relNew[rel]->clone())));
                }

                // add debug info
                std::ostringstream ds;
                ds << toString(*cl) << "\nin file ";
                ds << cl->getSrcLoc();
                rule = std::make_unique<RamDebugInfo>(std::move(rule), ds.str());

                // add to loop body
                appendStmt(loopRelSeq, std::move(rule));

                // increment version counter
                version++;
            }
            assert(cl->getExecutionPlan() == nullptr || version > cl->getExecutionPlan()->getMaxVersion());
        }

        // if there was no rule, continue
        if (!loopRelSeq) {
            continue;
        }

        // label all versions
        if (Global::config().has("profile")) {
            const std::string& relationName = toString(rel->getName());
            const SrcLocation& srcLocation = rel->getSrcLoc();
            const std::string logTimerStatement = LogStatement::tRecursiveRelation(relationName, srcLocation);
            const std::string logSizeStatement = LogStatement::nRecursiveRelation(relationName, srcLocation);
            loopRelSeq = std::make_unique<RamLogTimer>(std::move(loopRelSeq), logTimerStatement,
                    std::unique_ptr<RamRelationReference>(relNew[rel]->clone()));
        }

        /* add rule computations of a relation to parallel statement */
        loopSeq->add(std::move(loopRelSeq));
    }

    /* construct exit conditions for odd and even iteration */
    auto addCondition = [](std::unique_ptr<RamCondition>& cond, std::unique_ptr<RamCondition> clause) {
        cond = ((cond) ? std::make_unique<RamAnd>(std::move(cond), std::move(clause)) : std::move(clause));
    };

    std::unique_ptr<RamCondition> exitCond;
    for (const AstRelation* rel : scc) {
        addCondition(exitCond,
                std::make_unique<RamEmpty>(std::unique_ptr<RamRelationReference>(relNew[rel]->clone())));
    }

    /* construct fixpoint loop  */
    std::unique_ptr<RamStatement> res;
    if (preamble) appendStmt(res, std::move(preamble));
    if (!loopSeq->getStatements().empty() && exitCond && updateTable) {
        appendStmt(res, std::make_unique<RamLoop>(std::move(loopSeq),
                                std::make_unique<RamExit>(std::move(exitCond)), std::move(updateTable)));
    }
    if (postamble) {
        appendStmt(res, std::move(postamble));
    }
    if (res) return res;

    assert(false && "Not Implemented");
    return nullptr;
}

/** make a subroutine to search for subproofs */
std::unique_ptr<RamStatement> AstTranslator::makeSubproofSubroutine(const AstClause& clause) {
    // make intermediate clause with constraints
    std::unique_ptr<AstClause> intermediateClause(clause.clone());

    // name unnamed variables
    nameUnnamedVariables(intermediateClause.get());

    // add constraint for each argument in head of atom
    AstAtom* head = intermediateClause->getHead();
    for (size_t i = 0; i < head->getArguments().size() - 2; i++) {
        auto arg = head->getArgument(i);

        if (auto var = dynamic_cast<AstVariable*>(arg)) {
            intermediateClause->addToBody(std::make_unique<AstBinaryConstraint>(BinaryConstraintOp::EQ,
                    std::unique_ptr<AstArgument>(var->clone()), std::make_unique<AstSubroutineArgument>(i)));
        } else if (auto func = dynamic_cast<AstFunctor*>(arg)) {
            intermediateClause->addToBody(std::make_unique<AstBinaryConstraint>(BinaryConstraintOp::EQ,
                    std::unique_ptr<AstArgument>(func->clone()), std::make_unique<AstSubroutineArgument>(i)));
        } else if (auto rec = dynamic_cast<AstRecordInit*>(arg)) {
            intermediateClause->addToBody(std::make_unique<AstBinaryConstraint>(BinaryConstraintOp::EQ,
                    std::unique_ptr<AstArgument>(rec->clone()), std::make_unique<AstSubroutineArgument>(i)));
        }
    }

    // index of level argument in argument list
    size_t levelIndex = head->getArguments().size() - 2;

    // add level constraints
    for (size_t i = 0; i < intermediateClause->getBodyLiterals().size(); i++) {
        auto lit = intermediateClause->getBodyLiteral(i);
        if (auto atom = dynamic_cast<AstAtom*>(lit)) {
            auto arity = atom->getArity();

            // arity - 1 is the level number in body atoms
            intermediateClause->addToBody(std::make_unique<AstBinaryConstraint>(BinaryConstraintOp::LT,
                    std::unique_ptr<AstArgument>(atom->getArgument(arity - 1)->clone()),
                    std::make_unique<AstSubroutineArgument>(levelIndex)));
        }
    }

    return ProvenanceClauseTranslator(*this).translateClause(*intermediateClause, clause);
}

/** translates the given datalog program into an equivalent RAM program  */
void AstTranslator::translateProgram(const AstTranslationUnit& translationUnit) {
    // obtain type environment from analysis
    typeEnv = &translationUnit.getAnalysis<TypeEnvironmentAnalysis>()->getTypeEnvironment();

    // obtain recursive clauses from analysis
    const auto* recursiveClauses = translationUnit.getAnalysis<RecursiveClauses>();

    // obtain strongly connected component (SCC) graph from analysis
    const auto& sccGraph = *translationUnit.getAnalysis<SCCGraph>();

    // obtain some topological order over the nodes of the SCC graph
    const auto& sccOrder = *translationUnit.getAnalysis<TopologicallySortedSCCGraph>();

    // obtain the schedule of relations expired at each index of the topological order
    const auto& expirySchedule = translationUnit.getAnalysis<RelationSchedule>()->schedule();

    // start with an empty sequence of ram statements
    std::unique_ptr<RamStatement> res = std::make_unique<RamSequence>();

    // start with an empty program
    ramProg = std::make_unique<RamProgram>(std::make_unique<RamSequence>());

    // handle the case of an empty SCC graph
    if (sccGraph.getNumberOfSCCs() == 0) return;

    // a function to load relations
    const auto& makeRamLoad = [&](std::unique_ptr<RamStatement>& current, const AstRelation* relation,
                                      const std::string& inputDirectory, const std::string& fileExtension) {
        std::unique_ptr<RamStatement> statement =
                std::make_unique<RamLoad>(std::unique_ptr<RamRelationReference>(translateRelation(relation)),
                        getInputIODirectives(relation, Global::config().get(inputDirectory), fileExtension));
        if (Global::config().has("profile")) {
            const std::string logTimerStatement =
                    LogStatement::tRelationLoadTime(toString(relation->getName()), relation->getSrcLoc());
            statement = std::make_unique<RamLogTimer>(std::move(statement), logTimerStatement,
                    std::unique_ptr<RamRelationReference>(translateRelation(relation)));
        }
        appendStmt(current, std::move(statement));
    };

    // a function to print the size of relations
    const auto& makeRamPrintSize = [&](std::unique_ptr<RamStatement>& current, const AstRelation* relation) {
        appendStmt(current, std::make_unique<RamPrintSize>(
                                    std::unique_ptr<RamRelationReference>(translateRelation(relation))));
    };

    // a function to store relations
    const auto& makeRamStore = [&](std::unique_ptr<RamStatement>& current, const AstRelation* relation,
                                       const std::string& outputDirectory, const std::string& fileExtension) {
        std::unique_ptr<RamStatement> statement = std::make_unique<RamStore>(
                std::unique_ptr<RamRelationReference>(translateRelation(relation)),
                getOutputIODirectives(relation, Global::config().get(outputDirectory), fileExtension));
        if (Global::config().has("profile")) {
            const std::string logTimerStatement =
                    LogStatement::tRelationSaveTime(toString(relation->getName()), relation->getSrcLoc());
            statement = std::make_unique<RamLogTimer>(std::move(statement), logTimerStatement,
                    std::unique_ptr<RamRelationReference>(translateRelation(relation)));
        }
        appendStmt(current, std::move(statement));
    };

    // a function to drop relations
    const auto& makeRamDrop = [&](std::unique_ptr<RamStatement>& current, const AstRelation* relation) {
        appendStmt(current, std::make_unique<RamDrop>(translateRelation(relation)));
    };

#ifdef USE_MPI
    const auto& makeRamSend = [&](std::unique_ptr<RamStatement>& current, const AstRelation* relation,
                                      const std::set<size_t> destinationStrata) {
        appendStmt(current, std::make_unique<RamSend>(translateRelation(relation), destinationStrata));
    };

    const auto& makeRamRecv = [&](std::unique_ptr<RamStatement>& current, const AstRelation* relation,
                                      const size_t sourceStrata) {
        appendStmt(current, std::make_unique<RamRecv>(translateRelation(relation), sourceStrata));
    };

    const auto& makeRamNotify = [&](std::unique_ptr<RamStatement>& current) {
        appendStmt(current, std::make_unique<RamNotify>());
    };

    const auto& makeRamWait = [&](std::unique_ptr<RamStatement>& current, const size_t count) {
        appendStmt(current, std::make_unique<RamWait>(count));
    };
#endif

    // maintain the index of the SCC within the topological order
    size_t indexOfScc = 0;

    // iterate over each SCC according to the topological order
    for (const auto& scc : sccOrder.order()) {
        // make a new ram statement for the current SCC
        std::unique_ptr<RamStatement> current;

        // find out if the current SCC is recursive
        const auto& isRecursive = sccGraph.isRecursive(scc);

        // make variables for particular sets of relations contained within the current SCC, and, predecessors
        // and successor SCCs thereof
        const auto& allInterns = sccGraph.getInternalRelations(scc);
        const auto& internIns = sccGraph.getInternalInputRelations(scc);
        const auto& internOuts = sccGraph.getInternalOutputRelations(scc);
        const auto& externOutPreds = sccGraph.getExternalOutputPredecessorRelations(scc);
        const auto& externNonOutPreds = sccGraph.getExternalNonOutputPredecessorRelations(scc);

        // const auto& externPreds = sccGraph.getExternalPredecessorRelations(scc);
        // const auto& internsWithExternSuccs = sccGraph.getInternalRelationsWithExternalSuccessors(scc);
        const auto& internNonOutsWithExternSuccs =
                sccGraph.getInternalNonOutputRelationsWithExternalSuccessors(scc);

        // make a variable for all relations that are expired at the current SCC
        const auto& internExps = expirySchedule.at(indexOfScc).expired();

        // create all internal relations of the current scc
        for (const auto& relation : allInterns) {
            appendStmt(current, std::make_unique<RamCreate>(
                                        std::unique_ptr<RamRelationReference>(translateRelation(relation))));
            // create new and delta relations if required
            if (isRecursive) {
                appendStmt(current, std::make_unique<RamCreate>(std::unique_ptr<RamRelationReference>(
                                            translateDeltaRelation(relation))));
                appendStmt(current, std::make_unique<RamCreate>(std::unique_ptr<RamRelationReference>(
                                            translateNewRelation(relation))));
            }
        }

#ifdef USE_MPI
        const auto& externPreds = sccGraph.getExternalPredecessorRelations(scc);
        const auto& internsWithExternSuccs = sccGraph.getInternalRelationsWithExternalSuccessors(scc);
        // note that the order of receives is first by relation then second destination
        if (Global::config().get("engine") == "mpi") {
            // first, recv all internal input relations from the master process
            for (const auto& relation : internIns) {
                makeRamRecv(current, relation, (size_t)-1);
            }
            // second, recv all predecessor relations from their source slave process
            for (const auto& relation : externPreds) {
                makeRamRecv(current, relation, sccOrder.indexOfScc(sccGraph.getSCC(relation)));
            }
        } else
#endif
        {
            // load all internal input relations from the facts dir with a .facts extension
            for (const auto& relation : internIns) {
                makeRamLoad(current, relation, "fact-dir", ".facts");
            }

            // if a communication engine has been specified...
            if (Global::config().has("engine")) {
                // load all external output predecessor relations from the output dir with a .csv extension
                for (const auto& relation : externOutPreds) {
                    makeRamLoad(current, relation, "output-dir", ".csv");
                }
                // load all external output predecessor relations from the output dir with a .facts extension
                for (const auto& relation : externNonOutPreds) {
                    makeRamLoad(current, relation, "output-dir", ".facts");
                }
            }
        }
        // compute the relations themselves
        std::unique_ptr<RamStatement> bodyStatement =
                (!isRecursive) ? translateNonRecursiveRelation(
                                         *((const AstRelation*)*allInterns.begin()), recursiveClauses)
                               : translateRecursiveRelation(allInterns, recursiveClauses);
        appendStmt(current, std::move(bodyStatement));

        // print the size of all printsize relations in the current SCC
        for (const auto& relation : allInterns) {
            if (relation->isPrintSize()) {
                makeRamPrintSize(current, relation);
            }
        }
#ifdef USE_MPI
        // note that the order of sends is first by relation then second destination
        if (Global::config().get("engine") == "mpi") {
            // first, send all internal relations with external successors to their destination slave
            // processes
            for (const auto& relation : internsWithExternSuccs) {
                makeRamSend(current, relation, sccOrder.indexOfScc(sccGraph.getSuccessorSCCs(relation)));
            }
            // notify the master process
            makeRamNotify(current);
            // second, send all internal output relations to the master process
            for (const auto& relation : internOuts) {
                makeRamSend(current, relation, std::set<size_t>({(size_t)-1}));
            }
        } else
#endif
        {
            // if a communication engine is enabled...
            if (Global::config().has("engine")) {
                // store all internal non-output relations with external successors to the output dir with a
                // .facts extension
                for (const auto& relation : internNonOutsWithExternSuccs) {
                    makeRamStore(current, relation, "output-dir", ".facts");
                }
            }

            // store all internal output relations to the output dir with a .csv extension
            for (const auto& relation : internOuts) {
                makeRamStore(current, relation, "output-dir", ".csv");
            }
        }

        // if provenance is not enabled...
        if (!Global::config().has("provenance")) {
            // if a communication engine is enabled...
            if (Global::config().has("engine")) {
                // drop all internal relations
                for (const auto& relation : allInterns) {
                    makeRamDrop(current, relation);
                }
                // drop external output predecessor relations
                for (const auto& relation : externOutPreds) {
                    makeRamDrop(current, relation);
                }
                // drop external non-output predecessor relations
                for (const auto& relation : externNonOutPreds) {
                    makeRamDrop(current, relation);
                }
            } else {
                // otherwise, drop all  relations expired as per the topological order
                for (const auto& relation : internExps) {
                    makeRamDrop(current, relation);
                }
            }
        }

        if (current) {
            // append the current SCC as a stratum to the sequence
            appendStmt(res, std::make_unique<RamStratum>(std::move(current), indexOfScc));

            // increment the index of the current SCC
            indexOfScc++;
        }
    }

#ifdef USE_MPI
    if (Global::config().get("engine") == "mpi") {
        // make a new ram statement for the master process
        std::unique_ptr<RamStatement> current;

        // load all internal input relations from fact-dir with a .facts extension
        indexOfScc = 0;
        for (const auto scc : sccOrder.order()) {
            for (const auto& relation : sccGraph.getInternalInputRelations(scc)) {
                makeRamLoad(current, relation, "fact-dir", ".facts");
            }
            ++indexOfScc;
        }

        // send all internal input relations to their slave processes
        indexOfScc = 0;
        for (const auto scc : sccOrder.order()) {
            for (const auto& relation : sccGraph.getInternalInputRelations(scc)) {
                // note that the order of sends is first by relation then second destination
                const auto destinations = std::set<size_t>({indexOfScc});
                makeRamSend(current, relation, destinations);
                makeRamDrop(current, relation);
            }
            ++indexOfScc;
        }

        // wait for notifications from all slaves
        makeRamWait(current, sccGraph.getNumberOfSCCs());

        // recv all internal output relations from their slave processes
        indexOfScc = 0;
        for (const auto scc : sccOrder.order()) {
            for (const auto& relation : sccGraph.getInternalOutputRelations(scc)) {
                // note that the order of receives is first by relation then second destination
                makeRamRecv(current, relation, indexOfScc);
            }
            ++indexOfScc;
        }

        // write to output-dir with .csv extension
        indexOfScc = 0;
        for (const auto scc : sccOrder.order()) {
            for (const auto& relation : sccGraph.getInternalOutputRelations(scc)) {
                makeRamStore(current, relation, "output-dir", ".csv");
                makeRamDrop(current, relation);
            }
            ++indexOfScc;
        }

        // append the master process as a stratum with index of the max int
        appendStmt(res, std::make_unique<RamStratum>(std::move(current), std::numeric_limits<int>::max()));
    }
#endif

    // add main timer if profiling
    if (res && Global::config().has("profile")) {
        res = std::make_unique<RamLogTimer>(std::move(res), LogStatement::runtime(), nullptr);
    }

    // done for main prog
    ramProg->setMain(std::move(res));

    // add subroutines for each clause
    if (Global::config().has("provenance")) {
        visitDepthFirst(program->getRelations(), [&](const AstClause& clause) {
            std::stringstream relName;
            relName << clause.getHead()->getName();

            if (relName.str().find("@info") != std::string::npos || clause.getBodyLiterals().empty()) {
                return;
            }

            std::string subroutineLabel =
                    relName.str() + "_" + std::to_string(clause.getClauseNum()) + "_subproof";
            ramProg->addSubroutine(subroutineLabel, makeSubproofSubroutine(clause));
        });
    }
}

std::unique_ptr<RamTranslationUnit> AstTranslator::translateUnit(AstTranslationUnit& tu) {
    auto ram_start = std::chrono::high_resolution_clock::now();
    program = tu.getProgram();
    translateProgram(tu);
    SymbolTable& symTab = tu.getSymbolTable();
    ErrorReport& errReport = tu.getErrorReport();
    DebugReport& debugReport = tu.getDebugReport();
    if (!Global::config().get("debug-report").empty()) {
        if (ramProg) {
            auto ram_end = std::chrono::high_resolution_clock::now();
            std::string runtimeStr =
                    "(" + std::to_string(std::chrono::duration<double>(ram_end - ram_start).count()) + "s)";
            std::stringstream ramProgStr;
            ramProgStr << *ramProg;
            debugReport.addSection(DebugReporter::getCodeSection(
                    "ram-program", "RAM Program " + runtimeStr, ramProgStr.str()));
        }

        if (!debugReport.empty()) {
            std::ofstream debugReportStream(Global::config().get("debug-report"));
            debugReportStream << debugReport;
        }
    }
    return std::make_unique<RamTranslationUnit>(std::move(ramProg), symTab, errReport, debugReport);
}

}  // end of namespace souffle
