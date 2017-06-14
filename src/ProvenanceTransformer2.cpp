#include "ProvenanceTransformer.h"

bool ProvenanceRecordTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;

    auto relationToTypeMap = std::map<AstRelationIdentifier, AstTypeIdentifier>();
    
    auto program = translationUnit.getProgram();
    auto symtable = translationUnit.getSymbolTable();
    for (const auto relation : program->getRelations()) {
        
        std::stringstream relationNameStream;
        relationNameStream << relation;
        std::string relationName = relationNameStream.str();
        
        std::cout << relation << std::endl;
        
        std::unique_ptr<AstRecordType> newRecordType = makeNewRecordType(*relation);
        relationToTypeMap[relation->getName()] = newRecordType->getName();

        auto newRelation = std::unique_ptr<AstRelation>(new AstRelation());
        newRelation->setName(*(new AstRelationIdentifier(relationName + "_new")));
        newRelation->addAttribute(std::unique_ptr<AstAttribute>(new AstAttribute(std::string("0"), newRecordType->getName())));

        
        // add clause for newRelation
        if (relation->isInput()) {
            auto newClause = std::unique_ptr<AstClause>(new AstClause());
            
            // make head and body of newClause
            auto newHead = std::unique_ptr<AstAtom>(new AstAtom(newRelation->getName()));
            auto newBody = std::unique_ptr<AstAtom>(new AstAtom(relation->getName()));

            std::vector<AstArgument*> newRecordInitArgs;
            for (auto attr : relation->getAttributes()) {
                newRecordInitArgs.push_back(dynamic_cast<AstArgument*>(new AstVariable(attr->getAttributeName())));
                newBody->addArgument(std::unique_ptr<AstArgument>(new AstVariable(attr->getAttributeName())));
            }
            auto newHeadArgument = makeNewRecordInit(newRecordInitArgs);
            newHead->addArgument(std::move(newHeadArgument));
            newClause->setHead(std::move(newHead));
            newClause->addToBody(std::move(newBody));

            newClause->print(std::cout);
            std::cout << std::endl;
            newRelation->addClause(std::move(newClause));
        } // else, we add a new clause for each existing clause
        

        for (size_t i = 0; i < relation->getClauses().size(); ++i) {
            const auto clause = relation->getClauses()[i];

            // create new info relation describing clause
            std::unique_ptr<AstRelation> newInfoRelation = makeNewInfoRelation(*clause, relationName, translationUnit, *(new AstRelationIdentifier(relationName + "_new_" + std::to_string(i) + "_info")));
            newInfoRelation->setQualifier(OUTPUT_RELATION);
            program->appendRelation(std::move(newInfoRelation));

            std::vector<AstTypeIdentifier> types;
            
            types.push_back(relationToTypeMap[relation->getName()]);
            for (auto literal : clause->getBodyLiterals()) {
                if (AstAtom* atom = dynamic_cast<AstAtom*>(literal)) {
                    types.push_back(relationToTypeMap[atom->getName()]);
                }
            }

            // create new relation for each clause
            auto newProvenanceRelation = makeNewRelation(*clause, *(new AstRelationIdentifier(relationName + "_new_" + std::to_string(i))), types);

            // make new clause for newRelation
            auto newRelationClause = std::unique_ptr<AstClause>(new AstClause());
            auto newRelationClauseHead = std::unique_ptr<AstAtom>(new AstAtom(newRelation->getName()));
            newRelationClauseHead->addArgument(std::unique_ptr<AstArgument>(new AstVariable(std::string("x"))));
            auto newRelationClauseBody = std::unique_ptr<AstAtom>(new AstAtom(newProvenanceRelation->getName()));
            newRelationClauseBody->addArgument(std::unique_ptr<AstArgument>(new AstVariable(std::string("x"))));
            for (auto literal : clause->getBodyLiterals()) {
                if (AstAtom* atom = dynamic_cast<AstAtom*>(literal)) {
                    newRelationClauseBody->addArgument(std::unique_ptr<AstArgument>(new AstUnnamedVariable()));
                }
            }
            newRelationClause->setHead(std::move(newRelationClauseHead));
            newRelationClause->addToBody(std::move(newRelationClauseBody));
            std::cout << *newRelationClause << std::endl;
            newRelation->addClause(std::move(newRelationClause));

            newProvenanceRelation->setQualifier(OUTPUT_RELATION);

            // add new relations to program
            program->appendRelation(std::move(newProvenanceRelation));
            // program->appendRelation(std::move(newOutputRelation));
        }

        std::cout << *newRelation << std::endl;

        auto newOutputRelation = std::unique_ptr<AstRelation>(new AstRelation());
        auto newOutputRelationName = new AstRelationIdentifier(relationName + "_output");
        newOutputRelation->setName(*newOutputRelationName);
        newOutputRelation->addAttribute(std::unique_ptr<AstAttribute>(new AstAttribute(std::string("x"), newRecordType->getName())));

        auto newOutputRelationClause = std::unique_ptr<AstClause>(new AstClause());
        auto newOutputRelationHead = std::unique_ptr<AstAtom>(new AstAtom(*newOutputRelationName));
        // newOutputRelationHead->addArgument(std::unique_ptr<AstArgument>(newOutputRecordVar->clone()));

        auto newOutputRelationBody = std::unique_ptr<AstAtom>(new AstAtom(newRelation->getName()));
        auto recordInitArgs = std::vector<AstArgument*>();

        for (size_t j = 0; j < newRecordType->getFields().size(); j++) {
            auto field = newRecordType->getFields()[j].type;
            auto newOutputFieldVar = new AstVariable("x_" + std::to_string(j));
            newOutputRelation->addAttribute(std::unique_ptr<AstAttribute>(new AstAttribute("x_" + std::to_string(j), field)));
            recordInitArgs.push_back(dynamic_cast<AstArgument*>(newOutputFieldVar->clone()));
        }

        newOutputRelationHead->addArgument(makeNewRecordInit(recordInitArgs));
        for (size_t j = 0; j < newRecordType->getFields().size(); j++) {
            newOutputRelationHead->addArgument(std::unique_ptr<AstArgument>(new AstVariable("x_" + std::to_string(j))));
        }
        newOutputRelationBody->addArgument(makeNewRecordInit(recordInitArgs));

        newOutputRelationClause->setHead(std::move(newOutputRelationHead));
        newOutputRelationClause->addToBody(std::move(newOutputRelationBody));

        newOutputRelationClause->print(std::cout);
        std::cout << std::endl;

        newOutputRelation->addClause(std::move(newOutputRelationClause));
        newOutputRelation->print(std::cout);
        std::cout << std::endl;

        newOutputRelation->setQualifier(OUTPUT_RELATION);
        program->appendRelation(std::move(newOutputRelation));

        // newRelation->setQualifier(OUTPUT_RELATION);
        program->appendRelation(std::move(newRelation));

        program->addType(std::move(newRecordType));

        if (!relation->isInput()) {
            program->removeRelation(relation->getName());
        }
        changed = true;
    }
    symtable.print(std::cout);
    std::cout << std::endl;
    return changed;
}
