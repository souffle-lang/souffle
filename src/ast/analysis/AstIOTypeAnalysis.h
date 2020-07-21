/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstIOTypeAnalysis.h
 *
 * Declares methods to identify a relation as input, output, or printsize.
 *
 ***********************************************************************/

#pragma once

#include "ast/analysis/AstAnalysis.h"
#include <iosfwd>
#include <set>
#include <string>

namespace souffle {

class AstRelation;
class AstTranslationUnit;

class IOType : public AstAnalysis {
public:
    static constexpr const char* name = "IO-type-analysis";

    IOType() : AstAnalysis(name) {}

    void run(const AstTranslationUnit& translationUnit) override;

    void print(std::ostream& os) const override;

    bool isInput(const AstRelation* relation) const {
        return inputRelations.count(relation) != 0;
    }

    bool isOutput(const AstRelation* relation) const {
        return outputRelations.count(relation) != 0;
    }

    bool isPrintSize(const AstRelation* relation) const {
        return printSizeRelations.count(relation) != 0;
    }

    bool isIO(const AstRelation* relation) const {
        return isInput(relation) || isOutput(relation) || isPrintSize(relation);
    }

private:
    std::set<const AstRelation*> inputRelations;
    std::set<const AstRelation*> outputRelations;
    std::set<const AstRelation*> printSizeRelations;
};
}  // end of namespace souffle
