#pragma once

#include "AstTransforms.h"

namespace souffle {

class ProvenanceTransformedClause {
private:
    AstTranslationUnit& translationUnit;
    std::map<AstRelationIdentifier, AstTypeIdentifier>& relationToTypeMap;

    AstClause& originalClause;
    AstRelationIdentifier& originalName;
    int clauseNumber;

    AstRelation* infoRelation;
    AstRelation* provenanceRelation;
    AstRelation* outputRelation;

public:
    ProvenanceTransformedClause(AstTranslationUnit transUnit, std::map<AstRelationIdentifier, AstTypeIdentifier> relTypeMap, AstClause clause, int num);

    static AstRelationIdentifier makeRelationName(AstRelationIdentifier orig, std::string type, int num = -1) {
        AstRelationIdentifier newName(orig);
        newName.append(type);
        if (num != -1) {
            newName.append(std::to_string(num));
        }
        return newName;
    }

    void makeInfoRelation();
    void makeProvenanceRelation();

    // return relations
    std::unique_ptr<AstRelation> getInfoRelation() {
        return std::unique_ptr<AstRelation>(infoRelation);
    }

    std::unique_ptr<AstRelation> getProvenanceRelation() {
        return std::unique_ptr<AstRelation>(provenanceRelation);
    }

    std::unique_ptr<AstRelation> getOutputRelation() {
        return std::unique_ptr<AstRelation>(outputRelation);
    }
};


} // end of namespace souffle
