/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstPragma.cpp
 *
 * Define the class AstPragma to update global options based on parameter.
 *
 ***********************************************************************/

#include "AstPragma.h"
#include "AstTranslationUnit.h"
#include "AstVisitor.h"
#include "Global.h"

namespace souffle {
bool AstPragmaChecker::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;
    AstProgram* program = translationUnit.getProgram();

    visitDepthFirst(*program, [&](const AstPragma& pragma) {
        std::pair<std::string, std::string> kvp = pragma.getkvp();
        // command line options take precedence
        if (!Global::config().has(kvp.first)) {
            changed = true;
            Global::config().set(kvp.first, kvp.second);
        }
    });
    return changed;
}
}  // end of namespace souffle
