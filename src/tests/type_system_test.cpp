/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file type_system_test.h
 *
 * Tests souffle's type system operations.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "RamTypes.h"
#include "ast/TypeSystem.h"
#include "utility/StringUtil.h"
#include <string>
#include <vector>

namespace souffle::test {

TEST(TypeSystem, Basic) {
    TypeEnvironment env;

    const Type& A = env.createType<SubsetType>("A", env.getType("number"));
    const Type& B = env.createType<SubsetType>("B", env.getType("symbol"));

    auto& U = env.createType<UnionType>("U", toVector(&A, &B));

    auto& R = env.createType<RecordType>("R");
    R.setFields(toVector(&A, &B));

    EXPECT_EQ("A <: number", toString(A));
    EXPECT_EQ("B <: symbol", toString(B));

    EXPECT_EQ("U = A | B", toString(U));
    EXPECT_EQ("R = (A, B)", toString(R));
}

TEST(TypeSystem, isNumberType) {
    TypeEnvironment env;

    const Type& N = env.getType("number");

    const Type& A = env.createType<SubsetType>("A", env.getType("number"));
    const Type& B = env.createType<SubsetType>("B", env.getType("number"));

    const Type& C = env.createType<SubsetType>("C", env.getType("symbol"));

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
        const Type& U = env.createType<UnionType>("U", toVector(&A, &B));
        EXPECT_TRUE(isOfKind(U, TypeAttribute::Signed));
        EXPECT_FALSE(isOfKind(U, TypeAttribute::Symbol));
        const Type& U2 = env.createType<UnionType>("U2", toVector(&A, &B, &C));
        EXPECT_FALSE(isOfKind(U2, TypeAttribute::Signed));
        EXPECT_FALSE(isOfKind(U2, TypeAttribute::Symbol));
    }
    {
        const Type& U3 = env.createType<UnionType>("U3", toVector(&A));
        EXPECT_TRUE(isOfKind(U3, TypeAttribute::Signed));
    }
}

bool isNotSubtypeOf(const Type& a, const Type& b) {
    return !isSubtypeOf(a, b);
}

TEST(TypeSystem, isSubtypeOf_Basic) {
    TypeEnvironment env;

    // start with the two predefined types

    const Type& N = env.getType("number");
    const Type& S = env.getType("symbol");

    EXPECT_PRED2(isSubtypeOf, N, N);
    EXPECT_PRED2(isSubtypeOf, S, S);

    EXPECT_PRED2(isNotSubtypeOf, N, S);
    EXPECT_PRED2(isNotSubtypeOf, S, N);

    // check primitive type

    const Type& A = env.createType<SubsetType>("A", env.getType("number"));
    const Type& B = env.createType<SubsetType>("B", env.getType("number"));

    EXPECT_PRED2(isSubtypeOf, A, A);
    EXPECT_PRED2(isSubtypeOf, B, B);

    EXPECT_PRED2(isNotSubtypeOf, A, B);
    EXPECT_PRED2(isNotSubtypeOf, B, A);

    EXPECT_PRED2(isSubtypeOf, A, N);
    EXPECT_PRED2(isSubtypeOf, B, N);

    EXPECT_PRED2(isNotSubtypeOf, A, S);
    EXPECT_PRED2(isNotSubtypeOf, B, S);

    // check union types

    const Type& U = env.createType<UnionType>("U", toVector(&A, &B));

    EXPECT_PRED2(isSubtypeOf, U, U);
    EXPECT_PRED2(isSubtypeOf, A, U);
    EXPECT_PRED2(isSubtypeOf, B, U);
    EXPECT_PRED2(isSubtypeOf, U, N);

    EXPECT_PRED2(isNotSubtypeOf, U, A);
    EXPECT_PRED2(isNotSubtypeOf, U, B);
    EXPECT_PRED2(isNotSubtypeOf, N, U);

    auto& V = env.createType<UnionType>("V", toVector(&A, &B, &U));

    EXPECT_PRED2(isSubtypeOf, V, U);
    EXPECT_PRED2(isSubtypeOf, U, V);
}

TEST(TypeSystem, isSubtypeOf_Records) {
    TypeEnvironment env;

    const Type& A = env.createType<SubsetType>("A", env.getType("number"));
    const Type& B = env.createType<SubsetType>("B", env.getType("number"));

    auto& R1 = env.createType<RecordType>("R1");
    auto& R2 = env.createType<RecordType>("R2");

    EXPECT_FALSE(isSubtypeOf(R1, R2));
    EXPECT_FALSE(isSubtypeOf(R2, R1));

    R1.setFields(toVector(&A));
    R2.setFields(toVector(&B));

    EXPECT_FALSE(isSubtypeOf(R1, R2));
    EXPECT_FALSE(isSubtypeOf(R2, R1));
}

TEST(TypeSystem, GreatestCommonSubtype) {
    TypeEnvironment env;

    const Type& N = env.getType("number");

    const Type& A = env.createType<SubsetType>("A", env.getType("number"));
    const Type& B = env.createType<SubsetType>("B", env.getType("number"));
    const Type& C = env.createType<SubsetType>("C", env.getType("symbol"));

    EXPECT_EQ("{number}", toString(getGreatestCommonSubtypes(N, N)));

    EXPECT_EQ("{A}", toString(getGreatestCommonSubtypes(A, A)));
    EXPECT_EQ("{B}", toString(getGreatestCommonSubtypes(B, B)));
    EXPECT_EQ("{C}", toString(getGreatestCommonSubtypes(C, C)));

    EXPECT_EQ("{}", toString(getGreatestCommonSubtypes(A, B)));
    EXPECT_EQ("{}", toString(getGreatestCommonSubtypes(A, C)));
    EXPECT_EQ("{}", toString(getGreatestCommonSubtypes(B, C)));

    EXPECT_EQ("{}", toString(getGreatestCommonSubtypes(A, B, C)));

    EXPECT_EQ("{A}", toString(getGreatestCommonSubtypes(A, N)));
    EXPECT_EQ("{A}", toString(getGreatestCommonSubtypes(N, A)));

    EXPECT_EQ("{B}", toString(getGreatestCommonSubtypes(B, N)));
    EXPECT_EQ("{B}", toString(getGreatestCommonSubtypes(N, B)));

    EXPECT_EQ("{}", toString(getGreatestCommonSubtypes(C, N)));
    EXPECT_EQ("{}", toString(getGreatestCommonSubtypes(N, C)));

    // // bring in unions

    auto& U = env.createType<UnionType>("U");
    auto& S = env.createType<UnionType>("S");

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
    auto& R = env.createType<UnionType>("R");

    EXPECT_EQ("{R}", toString(getGreatestCommonSubtypes(U, R)));
    EXPECT_EQ("{R}", toString(getGreatestCommonSubtypes(S, R)));

    EXPECT_EQ("{R}", toString(getGreatestCommonSubtypes(U, R, N)));
    EXPECT_EQ("{R}", toString(getGreatestCommonSubtypes(S, R, N)));

    R.setElements(toVector(static_cast<const Type*>(&U)));  // R = U = S

    EXPECT_EQ("{U}", toString(getGreatestCommonSubtypes(U, R)));
    EXPECT_EQ("{S}", toString(getGreatestCommonSubtypes(S, R)));

    EXPECT_EQ("{U}", toString(getGreatestCommonSubtypes(U, R, N)));
    EXPECT_EQ("{S}", toString(getGreatestCommonSubtypes(S, R, N)));

    R.setElements(toVector(static_cast<const Type*>(&U), static_cast<const Type*>(&S)));

    EXPECT_EQ("{U}", toString(getGreatestCommonSubtypes(U, R)));
    EXPECT_EQ("{S}", toString(getGreatestCommonSubtypes(S, R)));

    EXPECT_EQ("{U}", toString(getGreatestCommonSubtypes(U, R, N)));
    EXPECT_EQ("{S}", toString(getGreatestCommonSubtypes(S, R, N)));
}

TEST(TypeSystem, complexSubsetTypes) {
    TypeEnvironment env;

    auto& A = env.createType<SubsetType>("A", env.getType("number"));
    auto& BfromA = env.createType<SubsetType>("B", A);
    auto& CfromA = env.createType<SubsetType>("C", A);

    EXPECT_EQ("{B}", toString(getGreatestCommonSubtypes(A, BfromA)));
    EXPECT_EQ("{C}", toString(getGreatestCommonSubtypes(A, CfromA)));
    EXPECT_EQ("{}", toString(getGreatestCommonSubtypes(A, BfromA, CfromA)));
    EXPECT_EQ("{}", toString(getGreatestCommonSubtypes(BfromA, CfromA)));

    auto* base = &env.createType<SubsetType>("B0", BfromA);
    for (size_t i = 1; i <= 10; ++i) {
        base = &env.createType<SubsetType>("B" + std::to_string(i), *base);
        EXPECT_PRED2(isSubtypeOf, *base, A);
    }
}

TEST(TypeSystem, RecordSubsets) {
    TypeEnvironment env;

    auto& R = env.createType<RecordType>("R");

    auto& A = env.createType<SubsetType>("A", R);
    EXPECT_PRED2(isSubtypeOf, A, R);

    EXPECT_EQ("{A}", toString(getGreatestCommonSubtypes(A, R)));
}

}  // namespace souffle::test
