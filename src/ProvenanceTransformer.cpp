#include "ProvenanceTransformer.h"
#include "AstVisitor.h"

namespace souffle {

const std::string& identifierToString(const AstRelationIdentifier& name) {
    std::stringstream ss;
    ss << name;
    return *(new std::string(ss.str()));
}

void addAttrAndArg(AstRelation* rel, AstAttribute* attr, AstAtom* head, AstArgument* arg) {
    rel->addAttribute(std::unique_ptr<AstAttribute>(attr));
    head->addArgument(std::unique_ptr<AstArgument>(arg));
}

AstRecordInit* makeNewRecordInit(const std::vector<AstArgument*> args) {
    auto newRecordInit = new AstRecordInit();
    for (auto arg : args) {
        newRecordInit->add(std::unique_ptr<AstArgument>(arg->clone()));
    }
    return newRecordInit;
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
                const char* relName = identifierToString(atom->getName()).c_str();

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

    // create new clause
    auto provenanceClause = new AstClause();
    auto provenanceClauseHead = new AstAtom();
    provenanceClauseHead->setName(name);

    // add first argument corresponding to actual result
    addAttrAndArg(
            provenanceRelation,
            new AstAttribute(std::string("result"), relationToTypeMap[originalName]),
            provenanceClauseHead,
            makeNewRecordInit(originalClause.getHead()->getArguments())); 

    // visit all body literals and add to provenance clause
    visitDepthFirst(originalClause.getBodyLiterals(), [&](const AstLiteral& lit) {
            const AstAtom* atom = lit.getAtom();
            if (atom != nullptr) { // literal is atom or negation
                std::string relName = identifierToString(atom->getName());

                addAttrAndArg(
                        provenanceRelation,
                        new AstAttribute(std::string("prov_") + std::string(relName), relationToTypeMap[atom->getName()]),
                        provenanceClauseHead,
                        makeNewRecordInit(atom->getArguments()));
            }
            
            // add body literals
            if (const AstAtom* at = dynamic_cast<const AstAtom*>(&lit)) {
                auto newBody = std::unique_ptr<AstAtom>(new AstAtom(makeRelationName(at->getName(), "record")));
                newBody->addArgument(std::unique_ptr<AstArgument>(makeNewRecordInit(at->getArguments())));

                // add new atom to body
                provenanceClause->addToBody(std::move(newBody));
            } else if (const AstNegation* neg = dynamic_cast<const AstNegation*>(&lit)) {
                const AstAtom* at = neg->getAtom();
                auto newBody = std::unique_ptr<AstAtom>(new AstAtom(makeRelationName(at->getName(), "record")));
                newBody->addArgument(std::unique_ptr<AstArgument>(makeNewRecordInit(at->getArguments())));
                
                // create negation and add to body
                provenanceClause->addToBody(std::unique_ptr<AstLiteral>(new AstNegation(std::move(newBody))));
            } else if (const AstConstraint* constr = dynamic_cast<const AstConstraint*>(&lit)) {

                // clone constraint and add to body
                provenanceClause->addToBody(std::unique_ptr<AstConstraint>(constr->clone()));
            }
                
    });

    // add head to clause and add clause to relation
    provenanceClause->setHead(std::unique_ptr<AstAtom>(provenanceClauseHead));
    provenanceRelation->addClause(std::unique_ptr<AstClause>(provenanceClause));
}

bool ProvenanceRecordTransformer::transform(AstTranslationUnit& translationUnit) {
    return true;
}

} // end of namespace souffle
