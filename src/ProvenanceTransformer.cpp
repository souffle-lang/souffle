#include "ProvenanceTransformer.h"
#include "AstVisitor.h"

namespace souffle {

/**
 * Helper functions
 */
const std::string& identifierToString(const AstRelationIdentifier& name) {
    std::stringstream ss;
    ss << name;
    return *(new std::string(ss.str()));
}

inline AstRelationIdentifier makeRelationName(AstRelationIdentifier orig, const std::string& type, int num = -1) {
    auto newName = new AstRelationIdentifier(identifierToString(orig));
    newName->append(type);
    if (num != -1) {
        newName->append(std::to_string(num));
    }

    return *newName;
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

/**
 * ProvenanceTransformedClause functions
 */
ProvenanceTransformedClause::ProvenanceTransformedClause(AstTranslationUnit& transUnit
    , std::map<AstRelationIdentifier, AstTypeIdentifier> relTypeMap
    , AstClause& origClause
    , AstRelationIdentifier origName
    , int num
    )
    : translationUnit(transUnit)
    , relationToTypeMap(relTypeMap)
    , originalClause(origClause)
    , originalName(origName)
    , clauseNumber(num)
{
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

    std::cout << infoRelation << std::endl;
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

    // add first argument corresponding to actual result
    addAttrAndArg(
            provenanceRelation,
            new AstAttribute(std::string("result"), relationToTypeMap[originalName]),
            provenanceClauseHead,
            makeNewRecordInit(originalClause.getHead()->getArguments())); 

    // visit all body literals and add to provenance clause
    for (auto lit : originalClause.getBodyLiterals()) {
    // visitDepthFirst(originalClause.getBodyLiterals(), [&](const AstLiteral& lit) {
            const AstAtom* atom = lit->getAtom();
            if (atom != nullptr) { // literal is atom or negation
                std::string relName = identifierToString(atom->getName());

                addAttrAndArg(
                        provenanceRelation,
                        new AstAttribute(std::string("prov_") + std::string(relName), relationToTypeMap[atom->getName()]),
                        provenanceClauseHead,
                        makeNewRecordInit(atom->getArguments()));
            }
            
            // add body literals
            if (const AstAtom* at = dynamic_cast<const AstAtom*>(lit)) {
                auto newBody = std::unique_ptr<AstAtom>(new AstAtom(makeRelationName(at->getName(), "record")));
                newBody->addArgument(std::unique_ptr<AstRecordInit>(makeNewRecordInit(at->getArguments())));

                // add new atom to body
                provenanceClause->addToBody(std::move(newBody));
            } else if (const AstNegation* neg = dynamic_cast<const AstNegation*>(lit)) {
                const AstAtom* at = neg->getAtom();
                auto newBody = std::unique_ptr<AstAtom>(new AstAtom(makeRelationName(at->getName(), "record")));
                newBody->addArgument(std::unique_ptr<AstRecordInit>(makeNewRecordInit(at->getArguments())));
                
                // create negation and add to body
                provenanceClause->addToBody(std::unique_ptr<AstNegation>(new AstNegation(std::move(newBody))));
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
    newRecordRelationClauseHead->addArgument(std::unique_ptr<AstRecordInit>(makeNewRecordInit(originalClause.getHead()->getArguments())));

    // construct body atom
    auto newRecordRelationClauseBody = new AstAtom();
    newRecordRelationClauseBody->setName(name);
    newRecordRelationClauseBody->addArgument(std::unique_ptr<AstRecordInit>(makeNewRecordInit(originalClause.getHead()->getArguments())));

    for (size_t i = 0; i < provenanceRelation->getArity() - 1; i++) {
        newRecordRelationClauseBody->addArgument(std::unique_ptr<AstUnnamedVariable>(new AstUnnamedVariable()));
    }
    assert((newRecordRelationClauseBody->getArity() == provenanceRelation->getArity()) && "record relation clause and provenance relation don't match");

    // add head and body to clause, add clause to relation
    newRecordRelationClause->setHead(std::unique_ptr<AstAtom>(newRecordRelationClauseHead));
    newRecordRelationClause->addToBody(std::unique_ptr<AstAtom>(newRecordRelationClauseBody));
    recordRelation->addClause(std::unique_ptr<AstClause>(newRecordRelationClause));

    std::cout << provenanceRelation << std::endl;
}

/**
 * ProvenanceTransformedRelation functions
 */
ProvenanceTransformedRelation::ProvenanceTransformedRelation(AstTranslationUnit& transUnit
    , std::map<AstRelationIdentifier, AstTypeIdentifier> relTypeMap
    , AstRelation& origRelation
    , AstRelationIdentifier origName
    )
    : translationUnit(transUnit)
    , relationToTypeMap(relTypeMap)
    , originalRelation(origRelation)
    , originalName(origName)
{
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
    // visitDepthFirst(originalRelation.getClauses(), [&](AstClause& clause) {
            auto transformedClause = new ProvenanceTransformedClause(translationUnit, relationToTypeMap, *clause, origName, count);
            transformedClause->makeInfoRelation();
            if (isEDB) {
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

    recordRelation->addAttribute(std::unique_ptr<AstAttribute>(new AstAttribute(std::string("x"), relationToTypeMap[originalName])));

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

    std::cout << recordRelation << std::endl;
}
// void ProvenanceTransformedRelation::makeRecordRelation() {}

void ProvenanceTransformedRelation::makeOutputRelation() {
    AstRelationIdentifier name = makeRelationName(originalName, "output");

    // initialise record relation
    outputRelation = new AstRelation();
    outputRelation->setName(name);

    // get record type
    auto recordType = dynamic_cast<const AstRecordType*>(translationUnit.getProgram()->getType(relationToTypeMap[originalName]));
    assert(recordType->getFields().size() == originalRelation.getArity() && "record type does not match original relation");

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
    addAttrAndArg(
            outputRelation,
            new AstAttribute(std::string("result"), relationToTypeMap[originalName]),
            outputClauseHead,
            makeNewRecordInit(args));

    // add remaining arguments corresponding to elements of record type
    for (size_t i = 0; i < originalRelation.getArity(); i++) {
        addAttrAndArg(
                outputRelation,
                new AstAttribute(std::string("x_") + std::to_string(i), recordType->getFields()[i].type),
                outputClauseHead,
                new AstVariable("x_" + std::to_string(i)));
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

    // set relation to output
    if (originalRelation.isOutput()) {
        outputRelation->setQualifier(OUTPUT_RELATION);
    }

    std::cout << outputRelation << std::endl;
}

// void ProvenanceTransformedRelation::makeOutputRelation() {}

bool ProvenanceRecordTransformer::transform(AstTranslationUnit& translationUnit) {
    auto program = translationUnit.getProgram();
    auto relationToTypeMap = std::map<AstRelationIdentifier, AstTypeIdentifier>();
    
    for (auto relation : program->getRelations()) {
    // visitDepthFirst(program->getRelations(), [&](AstRelation& relation) {
            std::cout << relation << std::endl;
            std::string relationName = identifierToString(relation->getName());

            // create new record type
            auto newRecordType = new AstRecordType();
            newRecordType->setName(relationName + "_type");

            for (auto attribute : relation->getAttributes()) {
                newRecordType->add(attribute->getAttributeName(), attribute->getTypeName());
            }

            relationToTypeMap[relation->getName()] = newRecordType->getName();

            program->addType(std::unique_ptr<AstType>(newRecordType));

            // create new ProvenanceTransformedRelation
            auto transformedRelation = new ProvenanceTransformedRelation(translationUnit, relationToTypeMap, *relation, relation->getName());

            // add relations to program
            for (auto transformedClause : transformedRelation->getTransformedClauses()) {
                program->addRelation(std::move(transformedClause->getInfoRelation()));
                if (!transformedRelation->isEdbRelation()) {
                    program->addRelation(std::move(transformedClause->getProvenanceRelation()));
                }
            }
            program->addRelation(std::move(transformedRelation->getRecordRelation()));
            program->addRelation(std::move(transformedRelation->getOutputRelation()));
    }

    return true;
}

} // end of namespace souffle
