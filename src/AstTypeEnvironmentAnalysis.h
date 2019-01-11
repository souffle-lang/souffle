/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstTypeEnvironmentAnalysis.h
 *
 * A wrapper for TypeEnvironment to be used for AST Analysis
 *
 ***********************************************************************/

#pragma once

#include "AstAnalysis.h"
#include "TypeSystem.h"
#include <ostream>

namespace souffle {

class AstProgram;
class AstTranslationUnit;

class TypeEnvironmentAnalysis : public AstAnalysis {
private:
    TypeEnvironment env;

    void updateTypeEnvironment(const AstProgram& program);

public:
    static constexpr const char* name = "type-environment";

    void run(const AstTranslationUnit& translationUnit) override;

    void print(std::ostream& os) const override;

    const TypeEnvironment& getTypeEnvironment() {
        return env;
    }
};

}  // end of namespace souffle
