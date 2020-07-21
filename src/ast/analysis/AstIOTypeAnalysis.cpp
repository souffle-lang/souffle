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
 * Implements methods to identify a relation as input, output, or printsize.
 *
 ***********************************************************************/

#include "ast/analysis/AstIOTypeAnalysis.h"
#include "ast/AstIO.h"
#include "ast/AstProgram.h"
#include "ast/AstQualifiedName.h"
#include "ast/AstRelation.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstUtils.h"
#include "ast/AstVisitor.h"
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
