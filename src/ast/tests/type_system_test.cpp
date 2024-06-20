/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file type_system_test.cpp
 *
 * Tests souffle's type system operations.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "ast/analysis/typesystem/TypeSystem.h"
#include "souffle/TypeAttribute.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/StringUtil.h"
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace souffle::ast::test {
using namespace analysis;

QualifiedName qn(std::string_view s) {
    return QualifiedName::fromString(s);
}

TEST(TypeSystem, Basic) {
    TypeEnvironment env;

    const analysis::Type& A = env.createType<SubsetType>(qn("A"), env.getType(qn("number")));
    const analysis::Type& B = env.createType<SubsetType>(qn("B"), env.getType(qn("symbol")));

    auto& U = env.createType<UnionType>(qn("U"), toVector(&A, &B));

    auto& R = env.createType<RecordType>(qn("R"));
    R.setFields(toVector(&A, &B));

    EXPECT_EQ("A <: number", toString(A));
    EXPECT_EQ("B <: symbol", toString(B));

    EXPECT_EQ("U = A | B", toString(U));
    EXPECT_EQ("R = (A, B)", toString(R));
}

TEST(TypeSystem, isNumberType) {
    TypeEnvironment env;

    const analysis::Type& N = env.getType(qn("number"));

    const analysis::Type& A = env.createType<SubsetType>(qn("A"), env.getType(qn("number")));
    const analysis::Type& B = env.createType<SubsetType>(qn("B"), env.getType(qn("number")));

    const analysis::Type& C = env.createType<SubsetType>(qn("C"), env.getType(qn("symbol")));

    EXPECT_TRUE(isOfKind(N, TypeAttribute::Signed));
    EXPECT_TRUE(isOfKind(A, TypeAttribute::Signed));
    EXPECT_TRUE(isOfKind(B, TypeAttribute::Signed));
    EXPECT_TRUE(isOfKind(C, TypeAttribute::Symbol));

    EXPECT_FALSE(isOfKind(N, TypeAttribute::Symbol));
    EXPECT_FALSE(isOfKind(A, TypeAttribute::Symbol));
    EXPECT_FALSE(isOfKind(B, TypeAttribute::Symbol));
    EXPECT_FALSE(isOfKind(C, TypeAttribute::Signed));

    // check the union type
    {
        const analysis::Type& U = env.createType<UnionType>(qn("U"), toVector(&A, &B));
        EXPECT_TRUE(isOfKind(U, TypeAttribute::Signed));
        EXPECT_FALSE(isOfKind(U, TypeAttribute::Symbol));
        const analysis::Type& U2 = env.createType<UnionType>(qn("U2"), toVector(&A, &B, &C));
        EXPECT_FALSE(isOfKind(U2, TypeAttribute::Signed));
        EXPECT_FALSE(isOfKind(U2, TypeAttribute::Symbol));
    }
    {
        const analysis::Type& U3 = env.createType<UnionType>(qn("U3"), toVector(&A));
        EXPECT_TRUE(isOfKind(U3, TypeAttribute::Signed));
    }
}

bool isNotSubtypeOf(const analysis::Type& a, const analysis::Type& b) {
    return !isSubtypeOf(a, b);
}

TEST(TypeSystem, isSubtypeOf_Basic) {
    TypeEnvironment env;

    // start with the two predefined types

    const analysis::Type& N = env.getType(qn("number"));
    const analysis::Type& S = env.getType(qn("symbol"));

    EXPECT_PRED2(isSubtypeOf, N, N);
    EXPECT_PRED2(isSubtypeOf, S, S);

    EXPECT_PRED2(isNotSubtypeOf, N, S);
    EXPECT_PRED2(isNotSubtypeOf, S, N);

    // check primitive type

    const analysis::Type& A = env.createType<SubsetType>(qn("A"), env.getType(qn("number")));
    const analysis::Type& B = env.createType<SubsetType>(qn("B"), env.getType(qn("number")));

    EXPECT_PRED2(isSubtypeOf, A, A);
    EXPECT_PRED2(isSubtypeOf, B, B);

    EXPECT_PRED2(isNotSubtypeOf, A, B);
    EXPECT_PRED2(isNotSubtypeOf, B, A);

    EXPECT_PRED2(isSubtypeOf, A, N);
    EXPECT_PRED2(isSubtypeOf, B, N);

    EXPECT_PRED2(isNotSubtypeOf, A, S);
    EXPECT_PRED2(isNotSubtypeOf, B, S);

    // check union types

    const analysis::Type& U = env.createType<UnionType>(qn("U"), toVector(&A, &B));

    EXPECT_PRED2(isSubtypeOf, U, U);
    EXPECT_PRED2(isSubtypeOf, A, U);
    EXPECT_PRED2(isSubtypeOf, B, U);
    EXPECT_PRED2(isSubtypeOf, U, N);

    EXPECT_PRED2(isNotSubtypeOf, U, A);
    EXPECT_PRED2(isNotSubtypeOf, U, B);
    EXPECT_PRED2(isNotSubtypeOf, N, U);

    auto& V = env.createType<UnionType>(qn("V"), toVector(&A, &B, &U));

    EXPECT_PRED2(isSubtypeOf, V, U);
    EXPECT_PRED2(isSubtypeOf, U, V);
}

TEST(TypeSystem, isSubtypeOf_Records) {
    TypeEnvironment env;

    const analysis::Type& A = env.createType<SubsetType>(qn("A"), env.getType(qn("number")));
    const analysis::Type& B = env.createType<SubsetType>(qn("B"), env.getType(qn("number")));

    auto& R1 = env.createType<RecordType>(qn("R1"));
    auto& R2 = env.createType<RecordType>(qn("R2"));

    EXPECT_FALSE(isSubtypeOf(R1, R2));
    EXPECT_FALSE(isSubtypeOf(R2, R1));

    R1.setFields(toVector(&A));
    R2.setFields(toVector(&B));

    EXPECT_FALSE(isSubtypeOf(R1, R2));
    EXPECT_FALSE(isSubtypeOf(R2, R1));
}

TEST(TypeSystem, GreatestCommonSubtype) {
    TypeEnvironment env;

    const analysis::Type& N = env.getType(qn("number"));

    const analysis::Type& A = env.createType<SubsetType>(qn("A"), env.getType(qn("number")));
    const analysis::Type& B = env.createType<SubsetType>(qn("B"), env.getType(qn("number")));
    const analysis::Type& C = env.createType<SubsetType>(qn("C"), env.getType(qn("symbol")));

    EXPECT_EQ("{number}", toString(getGreatestCommonSubtypes(N, N)));

    EXPECT_EQ("{A}", toString(getGreatestCommonSubtypes(A, A)));
    EXPECT_EQ("{B}", toString(getGreatestCommonSubtypes(B, B)));
    EXPECT_EQ("{C}", toString(getGreatestCommonSubtypes(C, C)));

    EXPECT_EQ("∅", toString(getGreatestCommonSubtypes(A, B)));
    EXPECT_EQ("∅", toString(getGreatestCommonSubtypes(A, C)));
    EXPECT_EQ("∅", toString(getGreatestCommonSubtypes(B, C)));

    EXPECT_EQ("∅", toString(getGreatestCommonSubtypes(A, B, C)));

    EXPECT_EQ("{A}", toString(getGreatestCommonSubtypes(A, N)));
    EXPECT_EQ("{A}", toString(getGreatestCommonSubtypes(N, A)));

    EXPECT_EQ("{B}", toString(getGreatestCommonSubtypes(B, N)));
    EXPECT_EQ("{B}", toString(getGreatestCommonSubtypes(N, B)));

    EXPECT_EQ("∅", toString(getGreatestCommonSubtypes(C, N)));
    EXPECT_EQ("∅", toString(getGreatestCommonSubtypes(N, C)));

    // // bring in unions

    auto& U = env.createType<UnionType>(qn("U"));
    auto& S = env.createType<UnionType>(qn("S"));

    U.setElements(toVector(&A));
    EXPECT_EQ("{S}", toString(getGreatestCommonSubtypes(U, S)));

    S.setElements(toVector(&A));
    EXPECT_EQ("{U}", toString(getGreatestCommonSubtypes(U, S)));

    U.setElements(toVector(&A, &B));
    EXPECT_EQ("{S}", toString(getGreatestCommonSubtypes(U, S)));
    EXPECT_EQ("{S}", toString(getGreatestCommonSubtypes(U, S, N)));

    S.setElements(toVector(&A, &B));
    EXPECT_EQ("{U}", toString(getGreatestCommonSubtypes(U, S)));
    EXPECT_EQ("{U}", toString(getGreatestCommonSubtypes(U, S, N)));

    // bring in a union of unions
    auto& R = env.createType<UnionType>(qn("R"));

    EXPECT_EQ("{R}", toString(getGreatestCommonSubtypes(U, R)));
    EXPECT_EQ("{R}", toString(getGreatestCommonSubtypes(S, R)));

    EXPECT_EQ("{R}", toString(getGreatestCommonSubtypes(U, R, N)));
    EXPECT_EQ("{R}", toString(getGreatestCommonSubtypes(S, R, N)));

    R.setElements(toVector(static_cast<const analysis::Type*>(&U)));  // R = U = S

    EXPECT_EQ("{U}", toString(getGreatestCommonSubtypes(U, R)));
    EXPECT_EQ("{S}", toString(getGreatestCommonSubtypes(S, R)));

    EXPECT_EQ("{U}", toString(getGreatestCommonSubtypes(U, R, N)));
    EXPECT_EQ("{S}", toString(getGreatestCommonSubtypes(S, R, N)));

    R.setElements(toVector(static_cast<const analysis::Type*>(&U), static_cast<const analysis::Type*>(&S)));

    EXPECT_EQ("{U}", toString(getGreatestCommonSubtypes(U, R)));
    EXPECT_EQ("{S}", toString(getGreatestCommonSubtypes(S, R)));

    EXPECT_EQ("{U}", toString(getGreatestCommonSubtypes(U, R, N)));
    EXPECT_EQ("{S}", toString(getGreatestCommonSubtypes(S, R, N)));
}

TEST(TypeSystem, complexSubsetTypes) {
    TypeEnvironment env;

    auto& A = env.createType<SubsetType>(qn("A"), env.getType(qn("number")));
    auto& BfromA = env.createType<SubsetType>(qn("B"), A);
    auto& CfromA = env.createType<SubsetType>(qn("C"), A);

    EXPECT_EQ("{B}", toString(getGreatestCommonSubtypes(A, BfromA)));
    EXPECT_EQ("{C}", toString(getGreatestCommonSubtypes(A, CfromA)));
    EXPECT_EQ("∅", toString(getGreatestCommonSubtypes(A, BfromA, CfromA)));
    EXPECT_EQ("∅", toString(getGreatestCommonSubtypes(BfromA, CfromA)));

    auto* base = &env.createType<SubsetType>(qn("B0"), BfromA);
    for (std::size_t i = 1; i <= 10; ++i) {
        base = &env.createType<SubsetType>(qn("B" + std::to_string(i)), *base);
        EXPECT_PRED2(isSubtypeOf, *base, A);
    }
}

TEST(TypeSystem, RecordSubsets) {
    TypeEnvironment env;

    auto& R = env.createType<RecordType>(qn("R"));

    auto& A = env.createType<SubsetType>(qn("A"), R);
    EXPECT_PRED2(isSubtypeOf, A, R);

    EXPECT_EQ("{A}", toString(getGreatestCommonSubtypes(A, R)));
}

TEST(TypeSystem, EquivTypes) {
    TypeEnvironment env;

    auto& A = env.createType<SubsetType>(qn("A"), env.getType(qn("number")));
    auto& U = env.createType<UnionType>(qn("U"), toVector(dynamic_cast<const analysis::Type*>(&A)));

    EXPECT_TRUE(areEquivalentTypes(A, U));
}

TEST(TypeSystem, AlgebraicDataType) {
    TypeEnvironment env;

    auto& A = env.createType<AlgebraicDataType>(qn("A"));

    EXPECT_TRUE(isSubtypeOf(A, A));
    EXPECT_EQ("{A}", toString(getGreatestCommonSubtypes(A, A)));
}

}  // namespace souffle::ast::test
