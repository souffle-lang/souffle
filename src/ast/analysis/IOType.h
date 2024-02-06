/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IOType.h
 *
 * Declares methods to identify a relation as input, output, or printsize.
 *
 ***********************************************************************/

#pragma once

#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace souffle::ast {

class TranslationUnit;

namespace analysis {

class IOTypeAnalysis : public Analysis {
public:
    static constexpr const char* name = "IO-type-analysis";

    IOTypeAnalysis() : Analysis(name) {}

    void run(const TranslationUnit& translationUnit) override;

    void print(std::ostream& os) const override;

    bool isInput(const Relation* relation) const {
        return inputRelations.count(relation) != 0;
    }

    bool isOutput(const Relation* relation) const {
        return outputRelations.count(relation) != 0;
    }

    bool isPrintSize(const Relation* relation) const {
        return printSizeRelations.count(relation) != 0;
    }

    bool isLimitSize(const Relation* relation) const {
        return limitSizeRelations.count(relation) != 0;
    }

    std::size_t getLimitSize(const Relation* relation) const {
        auto iter = limitSize.find(relation);
        if (iter != limitSize.end()) {
            return (*iter).second;
        }
        return 0;
    }

    bool isIO(const Relation* relation) const {
        return isInput(relation) || isOutput(relation) || isPrintSize(relation);
    }

private:
    RelationSet inputRelations;
    RelationSet outputRelations;
    RelationSet printSizeRelations;
    RelationSet limitSizeRelations;
    std::map<const Relation*, std::size_t> limitSize;
};

}  // namespace analysis
}  // namespace souffle::ast
