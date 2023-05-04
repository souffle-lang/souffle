/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ast_transformers_test.cpp
 *
 * Tests souffle's AST transformers.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "ast/analysis/RecursiveClauses.h"
#include "parser/ParserDriver.h"

namespace souffle::ast::transform::test {
using namespace analysis;

TEST(RecursiveClauses, Simple) {
    Global glb;
    ErrorReport errorReport;
    DebugReport debugReport(glb);
    Own<TranslationUnit> tu = ParserDriver::parseTranslationUnit(glb,
            R"(
              .decl A(i:number, j:number)
              .decl B(b:number)
              .decl C(c:number)

              A(b,c) :- B(b), C(c).
              A(i,j) :- A(j,i).
              B(b) :- b = 1 ; b = 2.
              C(c) :- c = 4 ; c = -1.
            )",
            errorReport, debugReport);

    Program& prog = tu->getProgram();
    RecursiveClausesAnalysis& rec = tu->getAnalysis<RecursiveClausesAnalysis>();

    for (Clause* cl : prog.getClauses(QualifiedName::fromString("A"))) {
        if (toString(*cl) == "A(i,j) :- \n   A(j,i).") {
            EXPECT_TRUE(rec.recursive(cl));
        } else if (toString(*cl) == "A(b,c) :- \n   B(b),\n   C(c).") {
            EXPECT_FALSE(rec.recursive(cl));
        } else {
            EXPECT_TRUE(false);
        }
    }
    for (Clause* cl : prog.getClauses(QualifiedName::fromString("B"))) {
        EXPECT_FALSE(rec.recursive(cl));
    }
    for (Clause* cl : prog.getClauses(QualifiedName::fromString("C"))) {
        EXPECT_FALSE(rec.recursive(cl));
    }
}
}  // namespace souffle::ast::transform::test
