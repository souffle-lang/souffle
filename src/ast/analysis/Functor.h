/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Functor.h
 *
 * Analysis that provides type information for functors
 *
 ***********************************************************************/

#pragma once

#include "ast/TranslationUnit.h"
#include "souffle/TypeAttribute.h"
#include <iosfwd>
#include <vector>

namespace souffle::ast {
class Functor;
class IntrinsicFunctor;
class Type;
class UserDefinedFunctor;
}  // namespace souffle::ast

namespace souffle::ast::analysis {

class TypeAnalysis;
class Type;

class FunctorAnalysis : public Analysis {
public:
    static constexpr const char* name = "functor-analysis";

    FunctorAnalysis() : Analysis(name) {}

    void run(const TranslationUnit& translationUnit) override;

    void print(std::ostream& /* os */) const override {}

    /** Return return type of functor */
    TypeAttribute getReturnTypeAttribute(const Functor& functor) const;
    Type const& getReturnType(const UserDefinedFunctor& functor) const;

    /** Return paramument type of functor */
    TypeAttribute getParamTypeAttribute(const Functor& functor, const std::size_t idx) const;
    Type const& getParamType(const UserDefinedFunctor& functor, const std::size_t idx) const;

    static bool isMultiResult(const Functor& functor);

    std::vector<TypeAttribute> getParamTypeAttributes(const UserDefinedFunctor& functor) const;

    /** Return whether a UDF is stateful */
    bool isStateful(const UserDefinedFunctor& udf) const;

private:
    const TypeAnalysis* typeAnalysis = nullptr;
};

}  // namespace souffle::ast::analysis
