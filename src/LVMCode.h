/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file LVMCode.h
 *
 * LVM ByteCode representation class
 *
 ***********************************************************************/

#pragma once

#include "IODirectives.h"
#include "SymbolTable.h"

#include <iostream>
#include <vector>

namespace souffle {

enum LVM_Type {
    // Expressions
    LVM_Number,
    LVM_TupleElement,
    LVM_AutoIncrement,

    // Unary Functor Operations
    LVM_OP_ORD,
    LVM_OP_STRLEN,
    LVM_OP_NEG,
    LVM_OP_BNOT,
    LVM_OP_LNOT,
    LVM_OP_TONUMBER,
    LVM_OP_TOSTRING,

    // Binary Functor Operators
    LVM_OP_ADD,
    LVM_OP_SUB,
    LVM_OP_MUL,
    LVM_OP_DIV,
    LVM_OP_EXP,
    LVM_OP_MOD,
    LVM_OP_BAND,
    LVM_OP_BOR,
    LVM_OP_BXOR,
    LVM_OP_LAND,
    LVM_OP_LOR,
    LVM_OP_MAX,
    LVM_OP_MIN,
    LVM_OP_CAT,

    // Ternary Functor Operators
    LVM_OP_SUBSTR,

    // Constraint Op
    LVM_OP_EQ,
    LVM_OP_NE,
    LVM_OP_LT,
    LVM_OP_LE,
    LVM_OP_GT,
    LVM_OP_GE,
    LVM_OP_MATCH,
    LVM_OP_NOT_MATCH,
    LVM_OP_CONTAINS,
    LVM_OP_NOT_CONTAINS,

    LVM_UserDefinedOperator,
    LVM_PackRecord,
    LVM_Argument,
    LVM_Aggregate_COUNT,
    LVM_Aggregate_Return,

    // LVM Conditions
    LVM_Conjunction,
    LVM_Negation,
    LVM_ContainTuple,
    LVM_EmptinessCheck,
    LVM_ExistenceCheck,
    LVM_ProvenanceExistenceCheck,
    LVM_Constraint,
    LVM_True,
    LVM_False,

    // LVM Operations;
    LVM_Scan,
    LVM_IndexScan,
    LVM_Choice,
    LVM_IndexChoice,
    LVM_UnpackRecord,
    LVM_Aggregate,
    LVM_IndexAggregate,
    LVM_Filter,
    LVM_Project,
    LVM_ReturnValue,
    LVM_Search,

    // LVM Stmts
    LVM_Sequence,
    LVM_Parallel,
    LVM_Stop_Parallel,
    LVM_Loop,
    LVM_IncIterationNumber,
    LVM_ResetIterationNumber,
    LVM_Exit,
    LVM_LogTimer,
    LVM_LogRelationTimer,
    LVM_StopLogTimer,
    LVM_DebugInfo,
    LVM_Stratum,
    LVM_Create,
    LVM_Clear,
    LVM_Drop,
    LVM_LogSize,
    LVM_Load,
    LVM_Store,
    LVM_Fact,
    LVM_Merge,
    LVM_Swap,
    LVM_Query,

    // LVM Branch
    LVM_Goto,
    LVM_Jmpnz,
    LVM_Jmpez,
    LVM_STOP,
    LVM_NOP,

    // LVM Relation Structure Representation
    LVM_BTREE,
    LVM_BRIE,
    LVM_EQREL,
    LVM_DEFAULT,

    LVM_ITER_InitFullIndex,
    LVM_ITER_InitRangeIndex,
    LVM_ITER_Select,
    LVM_ITER_Inc,
    LVM_ITER_NotAtEnd,

};

/**
 * LVMCode is an array of LVM Opcode and operands.
 * It also contains information (e.g. IODirectives and SymbolTable) which is necessary for it to be executed
 * by the LVM.
 */
class LVMCode : protected std::vector<RamDomain> {
public:
    LVMCode(SymbolTable& symbolTable) : symbolTable(symbolTable) {}

    using std::vector<RamDomain>::push_back;
    using std::vector<RamDomain>::clear;
    using std::vector<RamDomain>::size;
    using std::vector<RamDomain>::operator[];
    using std::vector<RamDomain>::begin;
    using std::vector<RamDomain>::end;

    /** Return reference to code stream */
    std::vector<RamDomain>& getCode() {
        return *this;
    }

    /** Return code stream */
    std::vector<RamDomain> getCode() const {
        return std::vector<RamDomain>(begin(), end());
    }

    /** Return IODirectives pool */
    std::vector<std::vector<IODirectives>>& getIODirectives() {
        return IODirectivesPool;
    }

    /** Return size of the IODirectives pool */
    size_t getIODirectivesSize() const {
        return IODirectivesPool.size();
    }

    /** Return SymbolTabel */
    SymbolTable& getSymbolTable() {
        return symbolTable;
    }

    /** Print out the code stream */
    virtual void print() const;

    virtual ~LVMCode() = default;

private:
    /** Store reference to IODirectives */
    std::vector<std::vector<IODirectives>> IODirectivesPool;

    /** Class for converting string to number and vice versa */
    SymbolTable& symbolTable;
};

}  // End of namespace souffle
