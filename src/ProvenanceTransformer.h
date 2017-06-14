#pragma once

#include "AstTransforms.h"

namespace souffle {

class ProvenanceTransformedClause {
private:
    AstTranslationUnit& translationUnit;

    AstClause& originalClause;
    AstRelationIdentifier& originalName;
    size_t clauseNumber;

    AstRelation* infoRelation;
    AstRelation* provenanceRelation;
    AstRelation* outputRelation;

public:
    ProvenanceTransformedClause(AstTranslationUnit transUnit, AstClause clause, size_t num);

    static AstRelationIdentifier makeRelationName(AstRelationIdentifier orig, std::string type, size_t num) {
        AstRelationIdentifier newName(orig);
        newName.append(type);
        newName.append(std::to_string(num));
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
