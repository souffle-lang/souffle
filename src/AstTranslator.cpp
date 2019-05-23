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
#include "AstFunctorDeclaration.h"
#include "AstIO.h"
#include "AstLiteral.h"
#include "AstNode.h"
#include "AstProgram.h"
#include "AstRelation.h"
#include "AstTranslationUnit.h"
#include "AstTypeEnvironmentAnalysis.h"
#include "AstUtils.h"
#include "AstVisitor.h"
#include "BinaryConstraintOps.h"
#include "DebugReport.h"
#include "Global.h"
#include "IODirectives.h"
#include "LogStatement.h"
#include "PrecedenceGraph.h"
#include "RamCondition.h"
#include "RamExpression.h"
#include "RamNode.h"
#include "RamOperation.h"
#include "RamProgram.h"
#include "RamRelation.h"
#include "RamStatement.h"
#include "RamTranslationUnit.h"
#include "SrcLocation.h"
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

std::unique_ptr<RamElementAccess> AstTranslator::makeRamElementAccess(const Location& loc) {
    return std::make_unique<RamElementAccess>(loc.identifier, loc.element);
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

    for (const auto& current : rel->getLoads()) {
        IODirectives ioDirectives;
        for (const auto& currentPair : current->getIODirectiveMap()) {
            ioDirectives.set(currentPair.first, currentPair.second);
        }
        inputDirectives.push_back(ioDirectives);
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

    // If stdout is requested then remove all directives from the datalog file.
    if (Global::config().get("output-dir") == "-") {
        bool hasOutput = false;
        for (const auto* current : rel->getStores()) {
            IODirectives ioDirectives;
            if (dynamic_cast<const AstPrintSize*>(current) != nullptr) {
                ioDirectives.setIOType("stdoutprintsize");
                outputDirectives.push_back(ioDirectives);
            } else if (!hasOutput) {
                hasOutput = true;
                ioDirectives.setIOType("stdout");
                ioDirectives.set("headers", "true");
                outputDirectives.push_back(ioDirectives);
            }
        }
    } else {
        for (const auto* current : rel->getStores()) {
            IODirectives ioDirectives;
            for (const auto& currentPair : current->getIODirectiveMap()) {
                ioDirectives.set(currentPair.first, currentPair.second);
            }
            outputDirectives.push_back(ioDirectives);
        }
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
        const std::vector<std::string> attributeTypeQualifiers, const RelationRepresentation representation) {
    const RamRelation* ramRel = ramProg->getRelation(name);
    if (ramRel == nullptr) {
        ramProg->addRelation(std::make_unique<RamRelation>(
                name, arity, attributeNames, attributeTypeQualifiers, representation));
        ramRel = ramProg->getRelation(name);
        assert(ramRel != nullptr && "cannot find relation");
    }
    return std::make_unique<RamRelationReference>(ramRel);
}

std::unique_ptr<RamRelationReference> AstTranslator::createRelationReference(
        const std::string name, const size_t arity) {
    return createRelationReference(name, arity, {}, {}, {});
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
            attributeNames, attributeTypeQualifiers, rel->getRepresentation());
}

std::unique_ptr<RamRelationReference> AstTranslator::translateDeltaRelation(const AstRelation* rel) {
    return translateRelation(rel, "@delta_");
}

std::unique_ptr<RamRelationReference> AstTranslator::translateNewRelation(const AstRelation* rel) {
    return translateRelation(rel, "@new_");
}

std::unique_ptr<RamExpression> AstTranslator::translateValue(
        const AstArgument* arg, const ValueIndex& index) {
    if (arg == nullptr) {
        return std::unique_ptr<RamExpression>();
    }

    class ValueTranslator : public AstVisitor<std::unique_ptr<RamExpression>> {
        AstTranslator& translator;
        const ValueIndex& index;

    public:
        ValueTranslator(AstTranslator& translator, const ValueIndex& index)
                : translator(translator), index(index) {}

        std::unique_ptr<RamExpression> visitVariable(const AstVariable& var) override {
            assert(index.isDefined(var) && "variable not grounded");
            return makeRamElementAccess(index.getDefinitionPoint(var));
        }

        std::unique_ptr<RamExpression> visitUnnamedVariable(const AstUnnamedVariable& var) override {
            return nullptr;  // utilised to identify _ values
        }

        std::unique_ptr<RamExpression> visitConstant(const AstConstant& c) override {
            return std::make_unique<RamNumber>(c.getIndex());
        }

        std::unique_ptr<RamExpression> visitIntrinsicFunctor(const AstIntrinsicFunctor& inf) override {
            std::vector<std::unique_ptr<RamExpression>> values;
            for (const auto& cur : inf.getArguments()) {
                values.push_back(translator.translateValue(cur, index));
            }
            return std::make_unique<RamIntrinsicOperator>(inf.getFunction(), std::move(values));
        }

        std::unique_ptr<RamExpression> visitUserDefinedFunctor(const AstUserDefinedFunctor& udf) override {
            std::vector<std::unique_ptr<RamExpression>> values;
            for (const auto& cur : udf.getArguments()) {
                values.push_back(translator.translateValue(cur, index));
            }
            const AstFunctorDeclaration* decl = translator.program->getFunctorDeclaration(udf.getName());
            std::string type = decl->getType();
            return std::make_unique<RamUserDefinedOperator>(udf.getName(), type, std::move(values));
        }

        std::unique_ptr<RamExpression> visitCounter(const AstCounter&) override {
            return std::make_unique<RamAutoIncrement>();
        }

        std::unique_ptr<RamExpression> visitRecordInit(const AstRecordInit& init) override {
            std::vector<std::unique_ptr<RamExpression>> values;
            for (const auto& cur : init.getArguments()) {
                values.push_back(translator.translateValue(cur, index));
            }
            return std::make_unique<RamPackRecord>(std::move(values));
        }

        std::unique_ptr<RamExpression> visitAggregator(const AstAggregator& agg) override {
            // here we look up the location the aggregation result gets bound
            return translator.makeRamElementAccess(index.getAggregatorLocation(agg));
        }

        std::unique_ptr<RamExpression> visitSubroutineArgument(const AstSubroutineArgument& subArg) override {
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
            std::unique_ptr<RamExpression> valLHS = translator.translateValue(binRel.getLHS(), index);
            std::unique_ptr<RamExpression> valRHS = translator.translateValue(binRel.getRHS(), index);
            return std::make_unique<RamConstraint>(binRel.getOperator(),
                    translator.translateValue(binRel.getLHS(), index),
                    translator.translateValue(binRel.getRHS(), index));
        }

        /** for negations */
        std::unique_ptr<RamCondition> visitNegation(const AstNegation& neg) override {
            // get contained atom
            const auto* atom = neg.getAtom();
            auto arity = atom->getArity();

            // account for two extra provenance columns
            if (Global::config().has("provenance")) {
                arity -= 2;
            }

            std::vector<std::unique_ptr<RamExpression>> values;

            for (size_t i = 0; i < arity; i++) {
                values.push_back(translator.translateValue(atom->getArgument(i), index));
            }

            // we don't care about the provenance columns when doing the existence check
            if (Global::config().has("provenance")) {
                values.push_back(nullptr);
                values.push_back(nullptr);
            }

            // add constraint
            if (arity > 0) {
                return std::make_unique<RamNegation>(std::make_unique<RamExistenceCheck>(
                        translator.translateRelation(atom), std::move(values)));
            } else {
                return std::make_unique<RamEmptinessCheck>(translator.translateRelation(atom));
            }
        }

        /** for provenance negation */
        std::unique_ptr<RamCondition> visitProvenanceNegation(const AstProvenanceNegation& neg) override {
            // get contained atom
            const AstAtom* atom = neg.getAtom();
            auto arity = atom->getArity();

            // account for two extra provenance columns
            if (Global::config().has("provenance")) {
                arity -= 2;
            }

            std::vector<std::unique_ptr<RamExpression>> values;

            for (size_t i = 0; i < arity; i++) {
                const auto& arg = atom->getArgument(i);
                // for (const auto& arg : atom->getArguments()) {
                values.push_back(translator.translateValue(arg, index));
            }

            // we don't care about the provenance columns when doing the existence check
            if (Global::config().has("provenance")) {
                values.push_back(nullptr);
                // add the height annotation for provenanceNotExists
                values.push_back(translator.translateValue(atom->getArgument(arity + 1), index));
            }

            // add constraint
            return std::make_unique<RamNegation>(std::make_unique<RamProvenanceExistenceCheck>(
                    translator.translateRelation(atom), std::move(values)));
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
            if (pos < relation->get()->getArity()) {
                valueIndex.addVarReference(
                        *var, arg_level[cur], pos, std::unique_ptr<RamRelationReference>(relation->clone()));
            } else {
                valueIndex.addVarReference(*var, arg_level[cur], pos);
            }
        }

        // check for nested records
        if (auto rec = dynamic_cast<const AstRecordInit*>(arg)) {
            // introduce new nesting level for unpack
            op_nesting.push_back(rec);
            arg_level[getArgList(rec, nodeArgs)] = level++;

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
                valueIndex.addVarReference(*var, aggLoc, (int)pos, translator.translateRelation(&atom));
            }
        };

        // and remember aggregator
        aggregators.push_back(&cur);
    });
}

std::unique_ptr<RamOperation> AstTranslator::ClauseTranslator::createOperation(const AstClause& clause) {
    const auto head = clause.getHead();

    std::vector<std::unique_ptr<RamExpression>> values;
    for (AstArgument* arg : head->getArguments()) {
        values.push_back(translator.translateValue(arg, valueIndex));
    }

    std::unique_ptr<RamOperation> project =
            std::make_unique<RamProject>(translator.translateRelation(head), std::move(values));

    if (head->getArity() == 0) {
        project = std::make_unique<RamFilter>(
                std::make_unique<RamEmptinessCheck>(translator.translateRelation(head)), std::move(project));
    }

    // check existence for original tuple if we have provenance
    // only if we don't compile
    if (Global::config().has("provenance") &&
            ((!Global::config().has("compile") && !Global::config().has("dl-program") &&
                    !Global::config().has("generate")))) {
        auto arity = head->getArity() - 2;

        std::vector<std::unique_ptr<RamExpression>> values;

        bool isVolatile = true;
        // add args for original tuple
        for (size_t i = 0; i < arity; i++) {
            auto arg = head->getArgument(i);

            // don't add counters
            visitDepthFirst(*arg, [&](const AstCounter& cur) { isVolatile = false; });
            values.push_back(translator.translateValue(arg, valueIndex));
        }

        // add two unnamed args for provenance columns
        values.push_back(nullptr);
        values.push_back(nullptr);

        if (isVolatile) {
            return std::make_unique<RamFilter>(
                    std::make_unique<RamNegation>(std::make_unique<RamExistenceCheck>(
                            translator.translateRelation(head), std::move(values))),
                    std::move(project));
        }
    }

    // build up insertion call
    return project;  // start with innermost
}

std::unique_ptr<RamOperation> AstTranslator::ProvenanceClauseTranslator::createOperation(
        const AstClause& clause) {
    std::vector<std::unique_ptr<RamExpression>> values;

    // get all values in the body
    for (AstLiteral* lit : clause.getBodyLiterals()) {
        if (auto atom = dynamic_cast<AstAtom*>(lit)) {
            for (AstArgument* arg : atom->getArguments()) {
                values.push_back(translator.translateValue(arg, valueIndex));
            }
        } else if (auto neg = dynamic_cast<AstNegation*>(lit)) {
            for (AstArgument* arg : neg->getAtom()->getArguments()) {
                values.push_back(translator.translateValue(arg, valueIndex));
            }
        } else if (auto con = dynamic_cast<AstBinaryConstraint*>(lit)) {
            values.push_back(translator.translateValue(con->getLHS(), valueIndex));
            values.push_back(translator.translateValue(con->getRHS(), valueIndex));
        } else if (auto neg = dynamic_cast<AstProvenanceNegation*>(lit)) {
            for (size_t i = 0; i < neg->getAtom()->getArguments().size() - 2; ++i) {
                auto arg = neg->getAtom()->getArguments()[i];
                values.push_back(translator.translateValue(arg, valueIndex));
            }
            values.push_back(std::make_unique<RamNumber>(-1));
            values.push_back(std::make_unique<RamNumber>(-1));
        }
    }

    return std::make_unique<RamReturnValue>(std::move(values));
}

std::unique_ptr<RamCondition> AstTranslator::ClauseTranslator::createCondition(
        const AstClause& originalClause) {
    const auto head = originalClause.getHead();

    // add stopping criteria for nullary relations
    // (if it contains already the null tuple, don't re-compute)
    if (head->getArity() == 0) {
        return std::make_unique<RamEmptinessCheck>(translator.translateRelation(head));
    }
    return nullptr;
}

std::unique_ptr<RamCondition> AstTranslator::ProvenanceClauseTranslator::createCondition(
        const AstClause& originalClause) {
    return nullptr;
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
        std::vector<std::unique_ptr<RamExpression>> values;
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

    /* add equivalence constraints imposed by variable binding */
    for (const auto& cur : valueIndex.getVariableReferences()) {
        // the first appearance
        const Location& first = *cur.second.begin();
        // all other appearances
        for (const Location& loc : cur.second) {
            if (first != loc && !valueIndex.isAggregator(loc.identifier)) {
                op = std::make_unique<RamFilter>(
                        std::make_unique<RamConstraint>(BinaryConstraintOp::EQ, makeRamElementAccess(first),
                                makeRamElementAccess(loc)),
                        std::move(op));
            }
        }
    }

    /* add conditions caused by atoms, negations, and binary relations */
    for (const auto& lit : clause.getBodyLiterals()) {
        if (auto condition = translator.translateConstraint(lit, valueIndex)) {
            op = std::make_unique<RamFilter>(std::move(condition), std::move(op));
        }
    }

    // add aggregator conditions
    size_t curLevel = op_nesting.size() - 1;
    for (auto it = op_nesting.rbegin(); it != op_nesting.rend(); ++it, --curLevel) {
        const AstNode* cur = *it;

        if (const auto* atom = dynamic_cast<const AstAtom*>(cur)) {
            // add constraints
            for (size_t pos = 0; pos < atom->argSize(); ++pos) {
                if (auto* agg = dynamic_cast<AstAggregator*>(atom->getArgument(pos))) {
                    auto loc = valueIndex.getAggregatorLocation(*agg);
                    op = std::make_unique<RamFilter>(
                            std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
                                    std::make_unique<RamElementAccess>(curLevel, pos),
                                    makeRamElementAccess(loc)),
                            std::move(op));
                }
            }
        }
    }

    // add aggregator levels
    --level;
    for (auto it = aggregators.rbegin(); it != aggregators.rend(); ++it, --level) {
        const AstAggregator* cur = *it;

        // translate aggregation function
        AggregateFunction fun = souffle::MIN;
        switch (cur->getOperator()) {
            case AstAggregator::min:
                fun = souffle::MIN;
                break;
            case AstAggregator::max:
                fun = souffle::MAX;
                break;
            case AstAggregator::count:
                fun = souffle::COUNT;
                break;
            case AstAggregator::sum:
                fun = souffle::SUM;
                break;
        }

        // condition for aggregate and helper function to add terms
        std::unique_ptr<RamCondition> aggCondition;
        auto addAggCondition = [&](std::unique_ptr<RamCondition>& arg) {
            if (aggCondition == nullptr) {
                aggCondition = std::move(arg);
            } else {
                aggCondition = std::make_unique<RamConjunction>(std::move(aggCondition), std::move(arg));
            }
        };

        // translate constraints of sub-clause
        for (const auto& lit : cur->getBodyLiterals()) {
            if (auto newCondition = translator.translateConstraint(lit, valueIndex)) {
                addAggCondition(newCondition);
            }
        }

        // get the first predicate of the sub-clause
        // NB: at most one atom is permitted in a sub-clause
        const AstAtom* atom = nullptr;
        for (const auto& lit : cur->getBodyLiterals()) {
            if (atom == nullptr) {
                atom = dynamic_cast<const AstAtom*>(lit);
            } else {
                assert(dynamic_cast<const AstAtom*>(lit) != nullptr &&
                        "Unsupported complex aggregation body encountered!");
            }
        }

        // translate arguments's of atom (if exists) to conditions
        if (atom != nullptr) {
            for (size_t pos = 0; pos < atom->argSize(); ++pos) {
                // variable bindings are issued differently since we don't want self
                // referential variable bindings
                if (const auto* var = dynamic_cast<const AstVariable*>(atom->getArgument(pos))) {
                    for (const Location& loc :
                            valueIndex.getVariableReferences().find(var->getName())->second) {
                        if (level != loc.identifier || (int)pos != loc.element) {
                            std::unique_ptr<RamCondition> newCondition = std::make_unique<RamConstraint>(
                                    BinaryConstraintOp::EQ, makeRamElementAccess(loc),
                                    std::make_unique<RamElementAccess>(level, pos));
                            addAggCondition(newCondition);
                            break;
                        }
                    }
                } else if (atom->getArgument(pos) != nullptr) {
                    std::unique_ptr<RamExpression> value =
                            translator.translateValue(atom->getArgument(pos), valueIndex);
                    if (value != nullptr) {
                        std::unique_ptr<RamCondition> newCondition =
                                std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
                                        std::make_unique<RamElementAccess>(level, pos), std::move(value));
                        addAggCondition(newCondition);
                    }
                }
            }
        }

        // translate aggregate expression
        std::unique_ptr<RamExpression> expr =
                translator.translateValue(cur->getTargetExpression(), valueIndex);
        if (expr == nullptr) {
            expr = std::make_unique<RamUndefValue>();
        }

        if (aggCondition == nullptr) {
            aggCondition = std::make_unique<RamTrue>();
        }

        // add Ram-Aggregation layer
        std::unique_ptr<RamAggregate> aggregate = std::make_unique<RamAggregate>(std::move(op), fun,
                translator.translateRelation(atom), std::move(expr), std::move(aggCondition), level);
        op = std::move(aggregate);
    }

    // build operation bottom-up
    while (!op_nesting.empty()) {
        // get next operator
        const AstNode* cur = op_nesting.back();
        op_nesting.pop_back();

        // get current nesting level
        auto level = op_nesting.size();

        if (const auto* atom = dynamic_cast<const AstAtom*>(cur)) {
            // add constraints
            for (size_t pos = 0; pos < atom->argSize(); ++pos) {
                if (auto* c = dynamic_cast<AstConstant*>(atom->getArgument(pos))) {
                    op = std::make_unique<RamFilter>(std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
                                                             std::make_unique<RamElementAccess>(level, pos),
                                                             std::make_unique<RamNumber>(c->getIndex())),
                            std::move(op));
                }
            }

            // add check for emptiness for an atom
            op = std::make_unique<RamFilter>(
                    std::make_unique<RamNegation>(
                            std::make_unique<RamEmptinessCheck>(translator.translateRelation(atom))),
                    std::move(op));

            // add a scan level
            if (atom->getArity() != 0) {
                if (head->getArity() == 0) {
                    op = std::make_unique<RamBreak>(
                            std::make_unique<RamNegation>(
                                    std::make_unique<RamEmptinessCheck>(translator.translateRelation(head))),
                            std::move(op));
                }
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
                            translator.translateRelation(atom), level, std::move(op), ss.str());
                } else {
                    op = std::make_unique<RamScan>(translator.translateRelation(atom), level, std::move(op));
                }
            }

            // TODO: support constants in nested records!
        } else if (const auto* rec = dynamic_cast<const AstRecordInit*>(cur)) {
            // add constant constraints
            for (size_t pos = 0; pos < rec->getArguments().size(); ++pos) {
                if (AstConstant* c = dynamic_cast<AstConstant*>(rec->getArguments()[pos])) {
                    op = std::make_unique<RamFilter>(std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
                                                             std::make_unique<RamElementAccess>(level, pos),
                                                             std::make_unique<RamNumber>(c->getIndex())),
                            std::move(op));
                } else if (AstFunctor* func = dynamic_cast<AstFunctor*>(rec->getArguments()[pos])) {
                    op = std::make_unique<RamFilter>(std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
                                                             std::make_unique<RamElementAccess>(level, pos),
                                                             translator.translateValue(func, valueIndex)),
                            std::move(op));
                }
            }

            // add an unpack level
            const Location& loc = valueIndex.getDefinitionPoint(*rec);
            op = std::make_unique<RamUnpackRecord>(
                    std::move(op), level, makeRamElementAccess(loc), rec->getArguments().size());
        } else {
            assert(false && "Unsupported AST node for creation of scan-level!");
        }
    }

    /* generate the final RAM Insert statement */
    std::unique_ptr<RamCondition> cond = createCondition(originalClause);
    if (cond != nullptr) {
        return std::make_unique<RamQuery>(std::make_unique<RamFilter>(std::move(cond), std::move(op)));
    } else {
        return std::make_unique<RamQuery>(std::move(op));
    }
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
                r1->getHead()->setName(relNew[rel]->get()->getName());
                r1->getAtoms()[j]->setName(relDelta[atomRelation]->get()->getName());
                if (Global::config().has("provenance")) {
                    r1->addToBody(std::make_unique<AstProvenanceNegation>(
                            std::unique_ptr<AstAtom>(cl->getHead()->clone())));
                } else {
                    if (r1->getHead()->getArity() > 0)
                        r1->addToBody(std::make_unique<AstNegation>(
                                std::unique_ptr<AstAtom>(cl->getHead()->clone())));
                }

                // replace wildcards with variables (reduces indices when wildcards are used in recursive
                // atoms)
                nameUnnamedVariables(r1.get());

                // reduce R to P ...
                for (size_t k = j + 1; k < atoms.size(); k++) {
                    if (isInSameSCC(getAtomRelation(atoms[k], program))) {
                        AstAtom* cur = r1->getAtoms()[k]->clone();
                        cur->setName(relDelta[getAtomRelation(atoms[k], program)]->get()->getName());
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
        cond = ((cond) ? std::make_unique<RamConjunction>(std::move(cond), std::move(clause))
                       : std::move(clause));
    };

    std::unique_ptr<RamCondition> exitCond;
    for (const AstRelation* rel : scc) {
        addCondition(exitCond, std::make_unique<RamEmptinessCheck>(
                                       std::unique_ptr<RamRelationReference>(relNew[rel]->clone())));
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

/** make a subroutine to search for subproofs for the non-existence of a tuple */
std::unique_ptr<RamStatement> AstTranslator::makeNegationSubproofSubroutine(const AstClause& clause) {
    // TODO (taipan-snake): Currently we only deal with atoms (no constraints or negations or aggregates
    // or anything else...)

    // clone clause for mutation
    auto clauseReplacedAggregates = std::unique_ptr<AstClause>(clause.clone());

    int aggNumber = 0;
    struct AggregatesToVariables : public AstNodeMapper {
        int& aggNumber;

        AggregatesToVariables(int& aggNumber) : aggNumber(aggNumber) {}

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            if (dynamic_cast<AstAggregator*>(node.get())) {
                return std::make_unique<AstVariable>("agg_" + std::to_string(aggNumber++));
            }

            node->apply(*this);
            return node;
        }
    };

    AggregatesToVariables aggToVar(aggNumber);
    clauseReplacedAggregates->apply(aggToVar);

    // build a vector of unique variables
    std::vector<const AstVariable*> uniqueVariables;

    visitDepthFirst(*clauseReplacedAggregates, [&](const AstVariable& var) {
        if (var.getName().find("@level_num") == std::string::npos) {
            // use find_if since uniqueVariables stores pointers, and we need to dereference the pointer to
            // check equality
            if (std::find_if(uniqueVariables.begin(), uniqueVariables.end(),
                        [&](const AstVariable* v) { return *v == var; }) == uniqueVariables.end()) {
                uniqueVariables.push_back(&var);
            }
        }
    });

    // a mapper to replace variables with subroutine arguments
    struct VariablesToArguments : public AstNodeMapper {
        const std::vector<const AstVariable*>& uniqueVariables;

        VariablesToArguments() = default;
        VariablesToArguments(const std::vector<const AstVariable*>& uniqueVariables)
                : uniqueVariables(uniqueVariables) {}

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            // replace unknown variables
            if (auto varPtr = dynamic_cast<const AstVariable*>(node.get())) {
                if (varPtr->getName().find("@level_num") == std::string::npos) {
                    size_t argNum = std::find_if(uniqueVariables.begin(), uniqueVariables.end(),
                                            [&](const AstVariable* v) { return *v == *varPtr; }) -
                                    uniqueVariables.begin();

                    return std::make_unique<AstSubroutineArgument>(argNum);
                } else {
                    return std::make_unique<AstUnnamedVariable>();
                }
            }

            // apply recursive
            node->apply(*this);

            // otherwise nothing
            return node;
        }
    };

    // the structure of this subroutine is a sequence where each nested statement is a search in each
    // relation
    std::unique_ptr<RamSequence> searchSequence = std::make_unique<RamSequence>();

    // make a copy so that when we mutate clause, pointers to objects in newClause are not affected
    auto newClause = std::unique_ptr<AstClause>(clauseReplacedAggregates->clone());

    // go through each body atom and create a return
    size_t litNumber = 0;
    for (const auto& lit : newClause->getBodyLiterals()) {
        if (auto atom = dynamic_cast<AstAtom*>(lit)) {
            // get a RamRelationReference
            auto relRef = translateRelation(atom);

            // construct a query
            std::vector<std::unique_ptr<RamExpression>> query;

            // translate variables to subroutine arguments
            VariablesToArguments varsToArgs(uniqueVariables);
            atom->apply(varsToArgs);

            // add each value (subroutine argument) to the search query
            for (size_t i = 0; i < atom->getArity() - 2; i++) {
                auto arg = atom->getArgument(i);
                query.push_back(translateValue(arg, ValueIndex()));
            }

            // fill up query with nullptrs for the provenance columns
            query.push_back(nullptr);
            query.push_back(nullptr);

            // ensure the length of query tuple is correct
            assert(query.size() == atom->getArity() && "wrong query tuple size");

            // make the nested operation to return the atom number if it exists
            std::vector<std::unique_ptr<RamExpression>> returnValue;
            returnValue.push_back(std::make_unique<RamNumber>(litNumber));

            // create a search
            // a filter to find whether the current atom exists or not
            auto searchFilter = std::make_unique<RamFilter>(
                    std::make_unique<RamExistenceCheck>(
                            std::unique_ptr<RamRelationReference>(relRef->clone()), std::move(query)),
                    std::make_unique<RamReturnValue>(std::move(returnValue)));

            // now, return the values of the atoms, with a separator
            // between atom number and atom
            std::vector<std::unique_ptr<RamExpression>> returnAtom;
            returnAtom.push_back(nullptr);
            // the actual atom
            for (size_t i = 0; i < atom->getArity() - 2; i++) {
                returnAtom.push_back(translateValue(atom->getArgument(i), ValueIndex()));
            }

            // chain the atom number and atom value together
            auto atomSequence = std::make_unique<RamSequence>();
            atomSequence->add(std::make_unique<RamQuery>(std::move(searchFilter)));
            atomSequence->add(
                    std::make_unique<RamQuery>(std::make_unique<RamReturnValue>(std::move(returnAtom))));

            // append search to the sequence
            searchSequence->add(std::move(atomSequence));
        } else if (auto con = dynamic_cast<AstConstraint*>(lit)) {
            VariablesToArguments varsToArgs(uniqueVariables);
            con->apply(varsToArgs);

            // translate to a RamCondition
            auto condition = translateConstraint(con, ValueIndex());

            // create a return value
            std::vector<std::unique_ptr<RamExpression>> returnValue;
            returnValue.push_back(std::make_unique<RamNumber>(litNumber));

            // create a filter
            auto filter = std::make_unique<RamFilter>(
                    std::move(condition), std::make_unique<RamReturnValue>(std::move(returnValue)));

            // now, return the values of the literal, with a separator
            // between atom number and atom
            std::vector<std::unique_ptr<RamExpression>> returnLit;
            returnLit.push_back(nullptr);
            // add return values for binary constraints and negations
            if (auto binaryConstraint = dynamic_cast<AstBinaryConstraint*>(con)) {
                returnLit.push_back(translateValue(binaryConstraint->getLHS(), ValueIndex()));
                returnLit.push_back(translateValue(binaryConstraint->getRHS(), ValueIndex()));
            } else if (auto negation = dynamic_cast<AstNegation*>(con)) {
                auto vals = negation->getAtom()->getArguments();
                for (size_t i = 0; i < vals.size() - 2; i++) {
                    returnLit.push_back(translateValue(vals[i], ValueIndex()));
                }
            }

            // chain the atom number and atom value together
            auto litSequence = std::make_unique<RamSequence>();
            litSequence->add(std::make_unique<RamQuery>(std::move(filter)));
            litSequence->add(
                    std::make_unique<RamQuery>(std::make_unique<RamReturnValue>(std::move(returnLit))));

            // append search to the sequence
            searchSequence->add(std::move(litSequence));
        }

        litNumber++;
    }

    return std::move(searchSequence);
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

        // make variables for particular sets of relations contained within the current SCC, and,
        // predecessors and successor SCCs thereof
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
                // load all external output predecessor relations from the output dir with a .csv
                // extension
                for (const auto& relation : externOutPreds) {
                    makeRamLoad(current, relation, "output-dir", ".csv");
                }
                // load all external output predecessor relations from the output dir with a .facts
                // extension
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
                // store all internal non-output relations with external successors to the output dir with
                // a .facts extension
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

            // do not add subroutines for info relations or facts
            if (relName.str().find("@info") != std::string::npos || clause.getBodyLiterals().empty()) {
                return;
            }

            std::string subroutineLabel =
                    relName.str() + "_" + std::to_string(clause.getClauseNum()) + "_subproof";
            ramProg->addSubroutine(subroutineLabel, makeSubproofSubroutine(clause));

            std::string negationSubroutineLabel =
                    relName.str() + "_" + std::to_string(clause.getClauseNum()) + "_negation_subproof";
            ramProg->addSubroutine(negationSubroutineLabel, makeNegationSubproofSubroutine(clause));
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
    }
    return std::make_unique<RamTranslationUnit>(std::move(ramProg), symTab, errReport, debugReport);
}

}  // end of namespace souffle
