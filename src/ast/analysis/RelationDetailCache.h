/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RelationDetailCache.h
 *
 * Defines the class to build the precedence graph,
 * compute strongly connected components of the precedence graph, and
 * build the strongly connected component graph.
 *
 ***********************************************************************/

#pragma once

#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/analysis/Analysis.h"
#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include <string>

namespace souffle {

class AstClause;
class AstTranslationUnit;

/**
 * Analysis pass mapping identifiers with relations and clauses.
 */
class RelationDetailCacheAnalysis : public AstAnalysis {
public:
    static constexpr const char* name = "relation-detail";

    RelationDetailCacheAnalysis() : AstAnalysis(name) {}

    void run(const AstTranslationUnit& translationUnit) override;

    void print(std::ostream& os) const override;

    AstRelation* getRelation(const AstQualifiedName& name) const {
        if (nameToRelation.find(name) != nameToRelation.end()) {
            return nameToRelation.at(name);
        }
        return nullptr;
    }

    std::set<AstClause*> getClauses(const AstRelation* rel) const {
        assert(rel != nullptr && "invalid relation");
        return getClauses(rel->getQualifiedName());
    }

    std::set<AstClause*> getClauses(const AstQualifiedName& name) const {
        if (nameToClauses.find(name) != nameToClauses.end()) {
            return nameToClauses.at(name);
        }
        return std::set<AstClause*>();
    }

private:
    std::map<AstQualifiedName, AstRelation*> nameToRelation;
    std::map<AstQualifiedName, std::set<AstClause*>> nameToClauses;
};

}  // end of namespace souffle
