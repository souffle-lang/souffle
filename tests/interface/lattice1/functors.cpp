/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2023, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file functors.cpp
 *
 ***********************************************************************/

#include "souffle/RecordTable.h"
#include "souffle/SymbolTable.h"
#include <cassert>

extern "C" {

enum Z {Cst = 0, Inf, MinusInf};
enum Interval {Bottom = 0, I};

souffle::RamDomain minZ(souffle::RecordTable* recordTable, souffle::RamDomain arg1, souffle::RamDomain arg2) {
    const souffle::RamDomain* t1 = recordTable->unpack(arg1, 2);
    const souffle::RamDomain* t2 = recordTable->unpack(arg2, 2);
    if (t1[0] == MinusInf) {
        return arg1;
    }
    if (t2[0] == MinusInf) {
        return arg2;
    }
    if (t1[0] == Inf) {
        return arg2;
    }
    if (t2[0] == Inf) {
        return arg1;
    }
    if (t2[1] < t1[1]) {
        return arg2;
    }
    return arg1;
}

souffle::RamDomain maxZ(souffle::RecordTable* recordTable, souffle::RamDomain arg1, souffle::RamDomain arg2) {
    const souffle::RamDomain* t1 = recordTable->unpack(arg1, 2);
    const souffle::RamDomain* t2 = recordTable->unpack(arg2, 2);
    if (t1[0] == MinusInf) {
        return arg2;
    }
    if (t2[0] == MinusInf) {
        return arg1;
    }
    if (t1[0] == Inf) {
        return arg1;
    }
    if (t2[0] == Inf) {
        return arg2;
    }
    if (t2[1] < t1[1]) {
        return arg1;
    }
    return arg2;
}

souffle::RamDomain plusZ(souffle::RecordTable* recordTable, souffle::RamDomain arg, souffle::RamDomain x) {
    const souffle::RamDomain* t = recordTable->unpack(arg, 2);
    if (t[0] == MinusInf || t[0] == Inf) {
        return arg;
    }

    const souffle::RamDomain res[2] = {Cst, t[1] + x};
    return recordTable->pack(res, 2);
}

souffle::RamDomain lub(souffle::SymbolTable*, souffle::RecordTable* recordTable,
        souffle::RamDomain arg1, souffle::RamDomain arg2) {
    const souffle::RamDomain* a1 = recordTable->unpack(arg1, 2);
    const souffle::RamDomain* a2 = recordTable->unpack(arg2, 2);

    if (a1[0] == Bottom) return arg2;
    if (a2[0] == Bottom) return arg1;

    const souffle::RamDomain* t1 = recordTable->unpack(a1[1], 2);
    const souffle::RamDomain* t2 = recordTable->unpack(a2[1], 2);

    const auto lb = minZ(recordTable, t1[0], t2[0]);
    const auto ub = maxZ(recordTable, t1[1], t2[1]);

    const souffle::RamDomain res[2] = {lb, ub};
    const souffle::RamDomain interval[2] = {I, recordTable->pack(res, 2)};
    return recordTable->pack(interval, 2);
}

souffle::RamDomain glb(souffle::SymbolTable*, souffle::RecordTable* recordTable,
        souffle::RamDomain arg1, souffle::RamDomain arg2) {
    const souffle::RamDomain* a1 = recordTable->unpack(arg1, 2);
    const souffle::RamDomain* a2 = recordTable->unpack(arg2, 2);

    if (a1[0] == Bottom) return arg1;
    if (a2[0] == Bottom) return arg2;

    const souffle::RamDomain* t1 = recordTable->unpack(a1[1], 2);
    const souffle::RamDomain* t2 = recordTable->unpack(a2[1], 2);

    const auto lb = maxZ(recordTable, t1[0], t2[0]);
    const auto ub = minZ(recordTable, t1[1], t2[1]);

    const souffle::RamDomain* lower = recordTable->unpack(lb, 2);
    const souffle::RamDomain* upper = recordTable->unpack(ub, 2);
    if (lower[0] == Cst && upper[0] == Cst && lower[1] > upper[1]) {
        // bottom
        const souffle::RamDomain bottom[2] = {Bottom, 1};
        return recordTable->pack(bottom, 2);
    }

    const souffle::RamDomain res[2] = {lb, ub};
    const souffle::RamDomain interval[2] = {I, recordTable->pack(res, 2)};
    return recordTable->pack(interval, 2);
}

souffle::RamDomain incr(souffle::SymbolTable*, souffle::RecordTable* recordTable,
        souffle::RamDomain arg, souffle::RamDomain x) {
    const souffle::RamDomain* a = recordTable->unpack(arg, 2);

    if (a[0] == Bottom) return arg;
    const souffle::RamDomain* t = recordTable->unpack(a[1], 2);

    const auto lb = plusZ(recordTable, t[0], x);
    const auto ub = plusZ(recordTable, t[1], x);

    const souffle::RamDomain res[2] = {lb, ub};
    const souffle::RamDomain interval[2] = {I, recordTable->pack(res, 2)};
    return recordTable->pack(interval, 2);
}

}  // end of extern "C"
