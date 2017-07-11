/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ProvenanceTransformer.cpp
 *
 * Defines classes which store relations transformed for provenance
 *
 ***********************************************************************/

#include "ProvenanceTransformer.h"
#include "AstVisitor.h"

namespace souffle {

/**
 * Helper functions
 */
const std::string identifierToString(const AstRelationIdentifier& name) {
    std::stringstream ss;
    ss << name;
    return *(new std::string(ss.str()));
}

inline AstRelationIdentifier makeRelationName(
        const AstRelationIdentifier& orig, const std::string& type, int num = -1) {
    auto newName = new AstRelationIdentifier(identifierToString(orig));
    newName->append(type);
    if (num != -1) {
        newName->append((const std::string&)std::to_string(num));
    }

    return *newName;
}

AstRecordInit* makeNewRecordInit(const std::vector<AstArgument*> args, bool negation = false) {
    auto newRecordInit = new AstRecordInit();

    int* numUnnamed = new int(0);
    struct M : public AstNodeMapper {
        int* numUnnamed;

        using AstNodeMapper::operator();

        M(int* numUnnamed) : numUnnamed(numUnnamed) {}

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            // see whether it is a variable to be substituted
            if (dynamic_cast<AstUnnamedVariable*>(node.get())) {
                return std::unique_ptr<AstNode>(
                        new AstVariable("unnamed_" + std::to_string((*numUnnamed)++)));
            }

            // otherwise - apply mapper recursively
            node->apply(*this);
            return node;
        }
    };

    for (auto arg : args) {
        if (negation) {
            newRecordInit->add(std::unique_ptr<AstArgument>(arg->clone()));
        } else {
            newRecordInit->add(M(numUnnamed)(std::unique_ptr<AstArgument>(arg->clone())));
        }
    }

    return newRecordInit;
}

/**
 * ProvenanceTransformedClause functions
 */
ProvenanceTransformedClause::ProvenanceTransformedClause(AstTranslationUnit& transUnit,
        std::map<AstRelationIdentifier, AstTypeIdentifier> relTypeMap, AstClause& origClause,
        AstRelationIdentifier origName, int num)
        : translationUnit(transUnit), relationToTypeMap(relTypeMap), originalClause(origClause),
          originalName(origName), clauseNumber(num) {}

void ProvenanceTransformedClause::makeInfoRelation() {
    AstRelationIdentifier name = makeRelationName(originalName, "info", clauseNumber);

    // initialise info relation
    infoRelation = new AstRelation();
    infoRelation->setName(name);

    // create new clause containing a single fact
    auto infoClause = new AstClause();
    auto infoClauseHead = new AstAtom();
    infoClauseHead->setName(name);

    // visit all body literals and add to info clause head
    for (auto lit : originalClause.getBodyLiterals()) {
        size_t nextNum = infoRelation->getArity() + 1;

        const AstAtom* atom = lit->getAtom();
        if (atom != nullptr) {
            // do not track provenance for nullary relations
            if (atom->getArity() == 0) {
                return;
            }

            std::string relName = identifierToString(atom->getName());

            infoRelation->addAttribute(std::unique_ptr<AstAttribute>(new AstAttribute(
                    std::string("rel_") + std::to_string(nextNum), AstTypeIdentifier("symbol"))));
            if (dynamic_cast<AstNegation*>(lit)) {
                infoClauseHead->addArgument(std::unique_ptr<AstArgument>(new AstStringConstant(
                        translationUnit.getSymbolTable(), ("negated_" + relName).c_str())));
            } else {
                infoClauseHead->addArgument(std::unique_ptr<AstArgument>(
                        new AstStringConstant(translationUnit.getSymbolTable(), relName.c_str())));
            }
        }
    }

    // add argument storing name of original clause
    infoRelation->addAttribute(
            std::unique_ptr<AstAttribute>(new AstAttribute("orig_name", AstTypeIdentifier("symbol"))));
    infoClauseHead->addArgument(std::unique_ptr<AstArgument>(new AstStringConstant(
            translationUnit.getSymbolTable(), identifierToString(originalName).c_str())));

    // generate and add clause representation
    std::stringstream ss;
    originalClause.print(ss);

    infoRelation->addAttribute(
            std::unique_ptr<AstAttribute>(new AstAttribute("clause_repr", AstTypeIdentifier("symbol"))));
    infoClauseHead->addArgument(std::unique_ptr<AstArgument>(
            new AstStringConstant(translationUnit.getSymbolTable(), ss.str().c_str())));

    // set clause head and add clause to info relation
    infoClause->setHead(std::unique_ptr<AstAtom>(infoClauseHead));
    infoRelation->addClause(std::unique_ptr<AstClause>(infoClause));
}

void ProvenanceTransformedClause::makeProvenanceRelation(AstRelation* recordRelation) {
    AstRelationIdentifier name = makeRelationName(originalName, "provenance", clauseNumber);

    // initialise provenance relation
    provenanceRelation = new AstRelation();
    provenanceRelation->setName(name);

    // create new clause
    auto provenanceClause = new AstClause();
    auto provenanceClauseHead = new AstAtom();
    provenanceClauseHead->setName(name);

    auto args = originalClause.getHead()->getArguments();

    // add first argument corresponding to actual result
    provenanceRelation->addAttribute(std::unique_ptr<AstAttribute>(
            new AstAttribute(std::string("result"), relationToTypeMap[originalName])));
    provenanceClauseHead->addArgument(std::unique_ptr<AstArgument>(makeNewRecordInit(args)));

    // visit all body literals and add to provenance clause
    for (auto lit : originalClause.getBodyLiterals()) {
        const AstAtom* atom = lit->getAtom();
        if (atom != nullptr) {  // literal is atom or negation
            std::string relName = identifierToString(atom->getName());
            auto args = atom->getArguments();

            if (atom->getArity() == 0) {
                continue;
            }

            // add atom converted to record to body
            auto newBody = std::unique_ptr<AstAtom>(new AstAtom(makeRelationName(atom->getName(), "record")));

            if (dynamic_cast<const AstAtom*>(lit)) {
                // add atom to head as a record
                provenanceRelation->addAttribute(std::unique_ptr<AstAttribute>(
                        new AstAttribute("prov_" + relName, relationToTypeMap[atom->getName()])));
                provenanceClauseHead->addArgument(std::unique_ptr<AstArgument>(makeNewRecordInit(args)));

                // add argument to body corresponding to provenance info for atom
                newBody->addArgument(std::unique_ptr<AstRecordInit>(makeNewRecordInit(args)));
                provenanceClause->addToBody(std::move(newBody));
            } else if (dynamic_cast<const AstNegation*>(lit)) {
                // add a marker signifying a negation
                provenanceRelation->addAttribute(std::unique_ptr<AstAttribute>(
                        new AstAttribute("prov_" + relName, AstTypeIdentifier("symbol"))));
                provenanceClauseHead->addArgument(std::unique_ptr<AstArgument>(new AstStringConstant(
                        translationUnit.getSymbolTable(), ("negated_" + relName).c_str())));

                // add argument to body corresponding to record form of original atom
                newBody->addArgument(std::unique_ptr<AstRecordInit>(makeNewRecordInit(args, true)));
                provenanceClause->addToBody(
                        std::unique_ptr<AstNegation>(new AstNegation(std::move(newBody))));
            }
        } else if (const AstConstraint* constr = dynamic_cast<const AstConstraint*>(lit)) {
            // clone constraint and add to body
            provenanceClause->addToBody(std::unique_ptr<AstConstraint>(constr->clone()));
        }
    }

    // add head to clause and add clause to relation
    provenanceClause->setHead(std::unique_ptr<AstAtom>(provenanceClauseHead));
    provenanceRelation->addClause(std::unique_ptr<AstClause>(provenanceClause));

    // add a new clause to recordRelation
    auto newRecordRelationClause = new AstClause();
    auto newRecordRelationClauseHead = new AstAtom();
    newRecordRelationClauseHead->setName(recordRelation->getName());

    // replace functors in args with variables for record relation
    int numFunctors = 0;
    for (size_t i = 0; i < args.size(); i++) {
        if (dynamic_cast<AstFunctor*>(args[i])) {
            auto newVariable = new AstVariable("functor_val_" + std::to_string(numFunctors));
            args[i] = newVariable;
        }
    }
    newRecordRelationClauseHead->addArgument(std::unique_ptr<AstRecordInit>(makeNewRecordInit(args)));

    // construct body atom
    auto newRecordRelationClauseBody = new AstAtom();
    newRecordRelationClauseBody->setName(name);
    newRecordRelationClauseBody->addArgument(std::unique_ptr<AstRecordInit>(makeNewRecordInit(args)));

    for (size_t i = 0; i < provenanceRelation->getArity() - 1; i++) {
        newRecordRelationClauseBody->addArgument(
                std::unique_ptr<AstUnnamedVariable>(new AstUnnamedVariable()));
    }
    assert((newRecordRelationClauseBody->getArity() == provenanceRelation->getArity()) &&
            "record relation clause and provenance relation don't match");

    // add head and body to clause, add clause to relation
    newRecordRelationClause->setHead(std::unique_ptr<AstAtom>(newRecordRelationClauseHead));
    newRecordRelationClause->addToBody(std::unique_ptr<AstAtom>(newRecordRelationClauseBody));
    recordRelation->addClause(std::unique_ptr<AstClause>(newRecordRelationClause));
}

/**
 * ProvenanceTransformedRelation functions
 */
ProvenanceTransformedRelation::ProvenanceTransformedRelation(AstTranslationUnit& transUnit,
        std::map<AstRelationIdentifier, AstTypeIdentifier> relTypeMap, AstRelation& origRelation,
        AstRelationIdentifier origName)
        : translationUnit(transUnit), relationToTypeMap(relTypeMap), originalRelation(origRelation),
          originalName(origName) {
    // check if originalRelation is EDB
    if (originalRelation.isInput()) {
        isEDB = true;
    } else {
        // check each clause
        bool edb = true;
        for (auto clause : originalRelation.getClauses()) {
            if (!clause->isFact()) {
                edb = false;
            }
        }
        isEDB = edb;
    }

    makeRecordRelation();
    makeOutputRelation();

    int count = 0;
    for (auto clause : originalRelation.getClauses()) {
        auto transformedClause =
                new ProvenanceTransformedClause(transUnit, relTypeMap, *clause, origName, count);
        transformedClause->makeInfoRelation();
        if (!isEDB) {
            transformedClause->makeProvenanceRelation(recordRelation);
        }
        transformedClauses.push_back(transformedClause);
        count++;
    }
}

/**
 * Record relation stores the original relation converted to a record
 * Clauses are created afterwards, using provenance relations
 */
void ProvenanceTransformedRelation::makeRecordRelation() {
    AstRelationIdentifier name = makeRelationName(originalName, "record");

    // initialise record relation
    recordRelation = new AstRelation();
    recordRelation->setName(name);

    recordRelation->addAttribute(std::unique_ptr<AstAttribute>(
            new AstAttribute(std::string("x"), relationToTypeMap[originalName])));

    // make clause for recordRelation if original is EDB
    if (isEDB) {
        // create new arguments to add to clause
        std::vector<AstArgument*> args;
        for (size_t i = 0; i < originalRelation.getArity(); i++) {
            args.push_back(new AstVariable("x_" + std::to_string(i)));
        }

        auto recordRelationClause = new AstClause();
        auto recordRelationClauseHead = new AstAtom();
        recordRelationClauseHead->setName(name);
        recordRelationClauseHead->addArgument(std::unique_ptr<AstRecordInit>(makeNewRecordInit(args)));

        // construct clause body
        auto recordRelationClauseBody = new AstAtom();
        recordRelationClauseBody->setName(originalName);
        for (auto a : args) {
            recordRelationClauseBody->addArgument(std::unique_ptr<AstArgument>(a));
        }

        recordRelationClause->setHead(std::unique_ptr<AstAtom>(recordRelationClauseHead));
        recordRelationClause->addToBody(std::unique_ptr<AstAtom>(recordRelationClauseBody));
        recordRelation->addClause(std::unique_ptr<AstClause>(recordRelationClause));
    }
}

void ProvenanceTransformedRelation::makeOutputRelation() {
    AstRelationIdentifier name = makeRelationName(originalName, "output");

    // initialise record relation
    outputRelation = new AstRelation();
    outputRelation->setName(name);

    // get record type
    auto recordType = dynamic_cast<const AstRecordType*>(
            translationUnit.getProgram()->getType(relationToTypeMap[originalName]));
    assert(recordType->getFields().size() == originalRelation.getArity() &&
            "record type does not match original relation");

    // create new clause from record relation
    auto outputClause = new AstClause();
    auto outputClauseHead = new AstAtom();
    outputClauseHead->setName(name);

    // create vector to be used to make RecordInit
    std::vector<AstArgument*> args;
    for (size_t i = 0; i < originalRelation.getArity(); i++) {
        args.push_back(new AstVariable("x_" + std::to_string(i)));
    }

    // add first argument corresponding to the record type
    outputRelation->addAttribute(std::unique_ptr<AstAttribute>(
            new AstAttribute(std::string("result"), relationToTypeMap[originalName])));
    outputClauseHead->addArgument(std::unique_ptr<AstArgument>(makeNewRecordInit(args)));

    // add remaining arguments corresponding to elements of record type
    for (size_t i = 0; i < originalRelation.getArity(); i++) {
        outputRelation->addAttribute(std::unique_ptr<AstAttribute>(
                new AstAttribute(std::string("x_") + std::to_string(i), recordType->getFields()[i].type)));
        outputClauseHead->addArgument(
                std::unique_ptr<AstArgument>(new AstVariable("x_" + std::to_string(i))));
    }

    // make body literal
    auto outputClauseBody = new AstAtom();
    outputClauseBody->setName(makeRelationName(originalName, "record"));
    outputClauseBody->addArgument(std::unique_ptr<AstRecordInit>(makeNewRecordInit(args)));

    // add atoms to clause
    outputClause->setHead(std::unique_ptr<AstAtom>(outputClauseHead));
    outputClause->addToBody(std::unique_ptr<AstAtom>(outputClauseBody));

    // add clause to relation
    outputRelation->addClause(std::unique_ptr<AstClause>(outputClause));
}

bool NaiveProvenanceTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;
    auto program = translationUnit.getProgram();
    auto relationToTypeMap = std::map<AstRelationIdentifier, AstTypeIdentifier>();

    for (auto relation : program->getRelations()) {
        if (relation->getArity() == 0) {
            continue;
        }

        std::string relationName = identifierToString(relation->getName());

        // create new record type
        auto newRecordType = new AstRecordType();
        newRecordType->setName(relationName + "_type");

        for (auto attribute : relation->getAttributes()) {
            newRecordType->add(attribute->getAttributeName(), attribute->getTypeName());
        }

        relationToTypeMap[relation->getName()] = newRecordType->getName();

        program->addType(std::unique_ptr<AstType>(newRecordType));
    }

    for (auto relation : program->getRelations()) {
        if (relation->getArity() == 0) {
            continue;
        }

        changed = true;

        // create new ProvenanceTransformedRelation
        auto transformedRelation = new ProvenanceTransformedRelation(
                translationUnit, relationToTypeMap, *relation, relation->getName());

        // add relations to program
        for (auto transformedClause : transformedRelation->getTransformedClauses()) {
            program->addRelation(transformedClause->getInfoRelation());
            if (!transformedRelation->isEdbRelation()) {
                program->addRelation(transformedClause->getProvenanceRelation());
            }
        }
        program->addRelation(transformedRelation->getRecordRelation());
        program->addRelation(transformedRelation->getOutputRelation());
    }
    return changed;
}

}  // end of namespace souffle
