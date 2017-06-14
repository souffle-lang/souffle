#include "ProvenanceTransformer.h"
#include "AstVisitor.h"

namespace souffle {

const std::string& identifierToString(AstRelationIdentifier& name) {
    std::stringstream ss;
    ss << name;
    return *(new std::string(ss.str()));
}

void addAttrAndArg(AstRelation* rel, AstAttribute* attr, AstAtom* head, AstArgument* arg) {
    rel->addAttribute(std::unique_ptr<AstAttribute>(attr));
    head->addArgument(std::unique_ptr<AstArgument>(arg));
}

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
    visitDepthFirst(originalClause.getBodyLiterals(), [&](const AstLiteral& lit) {
            size_t nextNum = infoRelation->getArity() + 1;

            const AstAtom* atom = lit.getAtom();
            if (atom != nullptr) {
                const char* relName = atom->getName().getNames()[0].c_str();

                addAttrAndArg(
                        infoRelation,
                        new AstAttribute(std::string("rel_") + std::to_string(nextNum), AstTypeIdentifier("symbol")),
                        infoClauseHead,
                        new AstStringConstant(translationUnit.getSymbolTable(), relName));
            }
        });

    // add argument storing name of original clause
    addAttrAndArg(
            infoRelation,
            new AstAttribute("orig_name", AstTypeIdentifier("symbol")),
            infoClauseHead,
            new AstStringConstant(translationUnit.getSymbolTable(), identifierToString(originalName).c_str()));

    // generate and add clause representation
    std::stringstream ss;
    originalClause.print(ss);
    addAttrAndArg(
            infoRelation,
            new AstAttribute("clause_repr", AstTypeIdentifier("symbol")),
            infoClauseHead,
            new AstStringConstant(translationUnit.getSymbolTable(), ss.str().c_str()));

    // set clause head and add clause to info relation
    infoClause->setHead(std::unique_ptr<AstAtom>(infoClauseHead));
    infoRelation->addClause(std::unique_ptr<AstClause>(infoClause));
}

void ProvenanceTransformedClause::makeProvenanceRelation() {
    AstRelationIdentifier name = makeRelationName(originalName, "provenance", clauseNumber);

    // initialise provenance relation
    provenanceRelation = new AstRelation();
    provenanceRelation->setName(name);
}

bool ProvenanceRecordTransformer::transform(AstTranslationUnit& translationUnit) {
    return true;
}

} // end of namespace souffle
