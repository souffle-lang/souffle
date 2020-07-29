/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IOTypeAnalysis.cpp
 *
 * Implements methods to identify a relation as input, output, or printsize.
 *
 ***********************************************************************/

#include "ast/analysis/IOType.h"
#include "ast/IO.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/Utils.h"
#include "ast/Visitor.h"
#include "utility/StreamUtil.h"
#include <ostream>
#include <vector>

namespace souffle {

void IOType::run(const AstTranslationUnit& translationUnit) {
    const AstProgram& program = *translationUnit.getProgram();
    visitDepthFirst(program, [&](const AstIO& io) {
        auto* relation = getRelation(program, io.getQualifiedName());
        if (relation == nullptr) {
            return;
        }
        switch (io.getType()) {
            case AstIoType::input: inputRelations.insert(relation); break;
            case AstIoType::output: outputRelations.insert(relation); break;
            case AstIoType::printsize:
                printSizeRelations.insert(relation);
                outputRelations.insert(relation);
                break;
        }
    });
}

void IOType::print(std::ostream& os) const {
    auto show = [](std::ostream& os, const AstRelation* r) { os << r->getQualifiedName(); };
    os << "input relations: {" << join(inputRelations, ", ", show) << "}\n";
    os << "output relations: {" << join(outputRelations, ", ", show) << "}\n";
    os << "printsize relations: {" << join(printSizeRelations, ", ", show) << "}\n";
}

}  // end of namespace souffle
