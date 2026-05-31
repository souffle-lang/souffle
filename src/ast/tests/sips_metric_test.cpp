/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2026 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file sips_metric_test.cpp
 *
 * Tests souffle's SIPS cost metrics.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "Global.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/TranslationUnit.h"
#include "ast2ram/utility/SipsMetric.h"
#include "parser/ParserDriver.h"
#include "reports/DebugReport.h"
#include "reports/ErrorReport.h"
#include <string>
#include <vector>

namespace souffle::ast {

namespace test {

TEST(SipsMetric, LeastFreeVarsCountsUnboundVariables) {
    Global glb;
    ErrorReport e;
    DebugReport d(glb);

    Own<TranslationUnit> tu = ParserDriver::parseTranslationUnit(glb,
            R"(
                .decl seed(x:number)
                .decl low(z:number, w:number)
                .decl high(x:number, y:number)
                .decl result(y:number, z:number, w:number)

                result(y, z, w) :- seed(x), low(z, w), high(x, y).
            )",
            e, d);

    auto* clause = tu->getProgram().getClauses(QualifiedName::fromString("result"))[0];
    LeastFreeVarsSips sips(*tu);

    EXPECT_EQ(std::vector<std::size_t>({0, 2, 1}), sips.getReordering(clause, {"seed", "low", "high"}));
}

}  // namespace test
}  // namespace souffle::ast
