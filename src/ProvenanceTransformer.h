/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ProvenanceTransformer.h
 *
 * Defines classes which store relations transformed for provenance
 *
 ***********************************************************************/

#pragma once

#include "AstTransforms.h"

namespace souffle {

class ProvenanceTransformedClause {
private:
    AstTranslationUnit& translationUnit;
    std::map<AstRelationIdentifier, AstTypeIdentifier> relationToTypeMap;

    AstClause& originalClause;
    AstRelationIdentifier originalName;
    int clauseNumber;

    AstRelation* infoRelation;
    AstRelation* provenanceRelation;

public:
    ProvenanceTransformedClause(AstTranslationUnit& transUnit,
            std::map<AstRelationIdentifier, AstTypeIdentifier> relTypeMap, AstClause& clause,
            AstRelationIdentifier origName, int num);

    void makeInfoRelation();
    void makeProvenanceRelation(AstRelation* recordRelation);

    // return relations
    std::unique_ptr<AstRelation> getInfoRelation() {
        return std::unique_ptr<AstRelation>(infoRelation);
    }

    std::unique_ptr<AstRelation> getProvenanceRelation() {
        return std::unique_ptr<AstRelation>(provenanceRelation);
    }
};

class ProvenanceTransformedRelation {
private:
    AstTranslationUnit& translationUnit;
    std::map<AstRelationIdentifier, AstTypeIdentifier> relationToTypeMap;

    AstRelation& originalRelation;
    AstRelationIdentifier originalName;
    bool isEDB = false;

    AstRelation* recordRelation;
    AstRelation* outputRelation;
    std::vector<ProvenanceTransformedClause*> transformedClauses;

public:
    ProvenanceTransformedRelation(AstTranslationUnit& transUnit,
            std::map<AstRelationIdentifier, AstTypeIdentifier> relTypeMap, AstRelation& origRelation,
            AstRelationIdentifier origName);

    void makeRecordRelation();
    void makeOutputRelation();

    // return relations
    std::unique_ptr<AstRelation> getRecordRelation() {
        return std::unique_ptr<AstRelation>(recordRelation);
    }

    std::unique_ptr<AstRelation> getOutputRelation() {
        return std::unique_ptr<AstRelation>(outputRelation);
    }

    std::vector<ProvenanceTransformedClause*> getTransformedClauses() {
        return transformedClauses;
    }

    bool isEdbRelation() {
        return isEDB;
    }
};

}  // end of namespace souffle
