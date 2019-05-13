/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file LVM.cpp
 *
 * Implementation of Souffle's bytecode interpreter.
 *
 ***********************************************************************/

#include "LVM.h"
#include "BTree.h"
#include "BinaryConstraintOps.h"
#include "FunctorOps.h"
#include "Global.h"
#include "IODirectives.h"
#include "IOSystem.h"
#include "InterpreterIndex.h"
#include "InterpreterRecords.h"
#include "Logger.h"
#include "ParallelUtils.h"
#include "ProfileEvent.h"
#include "RamExpression.h"
#include "RamNode.h"
#include "RamOperation.h"
#include "RamProgram.h"
#include "RamVisitor.h"
#include "ReadStream.h"
#include "SignalHandler.h"
#include "SymbolTable.h"
#include "Util.h"
#include "WriteStream.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <typeinfo>
#include <utility>
#include <ffi.h>

namespace souffle {

void LVM::executeMain() {
    const RamStatement& main = *translationUnit.getProgram()->getMain();
    if (mainProgram.get() == nullptr) {
        LVMGenerator generator(translationUnit.getSymbolTable(), main);
        mainProgram = generator.getCodeStream();
    }
    InterpreterContext ctxt;
    SignalHandler::instance()->set();
    if (Global::config().has("verbose")) {
        SignalHandler::instance()->enableLogging();
    }

    if (!Global::config().has("profile")) {
        execute(mainProgram, ctxt);
    } else {
        ProfileEventSingleton::instance().setOutputFile(Global::config().get("profile"));
        // Prepare the frequency table for threaded use
        visitDepthFirst(main, [&](const RamSearch& node) {
            if (!node.getProfileText().empty()) {
                frequencies.emplace(node.getProfileText(), std::map<size_t, size_t>());
            }
        });
        // Enable profiling for execution of main
        ProfileEventSingleton::instance().startTimer();
        ProfileEventSingleton::instance().makeTimeEvent("@time;starttime");
        // Store configuration
        for (const auto& cur : Global::config().data()) {
            ProfileEventSingleton::instance().makeConfigRecord(cur.first, cur.second);
        }
        // Store count of relations
        size_t relationCount = 0;
        visitDepthFirst(main, [&](const RamCreate& create) {
            if (create.getRelation().getName()[0] != '@') {
                ++relationCount;
                reads[create.getRelation().getName()] = 0;
            }
        });
        ProfileEventSingleton::instance().makeConfigRecord("relationCount", std::to_string(relationCount));

        // Store count of rules
        size_t ruleCount = 0;
        visitDepthFirst(main, [&](const RamQuery& rule) { ++ruleCount; });
        ProfileEventSingleton::instance().makeConfigRecord("ruleCount", std::to_string(ruleCount));

        execute(mainProgram, ctxt);
        ProfileEventSingleton::instance().stopTimer();
        for (auto const& cur : frequencies) {
            for (auto const& iter : cur.second) {
                ProfileEventSingleton::instance().makeQuantityEvent(cur.first, iter.second, iter.first);
            }
        }
        for (auto const& cur : reads) {
            ProfileEventSingleton::instance().makeQuantityEvent(
                    "@relation-reads;" + cur.first, cur.second, 0);
        }
    }
    SignalHandler::instance()->reset();
}

void LVM::execute(std::unique_ptr<LVMCode>& codeStream, InterpreterContext& ctxt, size_t ip) {
    std::stack<RamDomain> stack;
    const auto& code = codeStream->getCode();
    auto& symbolTable = codeStream->getSymbolTable();
    while (true) {
        switch (code[ip]) {
            case LVM_Number:
                stack.push(code[ip + 1]);
                ip += 2;
                break;
            case LVM_ElementAccess:
                stack.push(ctxt[code[ip + 1]][code[ip + 2]]);
                ip += 3;
                break;
            case LVM_AutoIncrement:
                stack.push(this->counter);
                incCounter();
                ip += 1;
                break;
            case LVM_OP_ORD:
                // Does nothing
                ip += 1;
                break;
            case LVM_OP_STRLEN: {
                RamDomain relNameId = stack.top();
                stack.pop();
                stack.push(symbolTable.resolve(relNameId).size());
                ip += 1;
                break;
            }
            case LVM_OP_NEG: {
                RamDomain val = stack.top();
                stack.pop();
                stack.push(-val);
                ip += 1;
                break;
            }
            case LVM_OP_BNOT: {
                RamDomain val = stack.top();
                stack.pop();
                stack.push(~val);
                ip += 1;
                break;
            }
            case LVM_OP_LNOT: {
                RamDomain val = stack.top();
                stack.pop();
                stack.push(!val);
                ip += 1;
                break;
            }
            case LVM_OP_TONUMBER: {
                RamDomain val = stack.top();
                stack.pop();
                RamDomain result = 0;
                try {
                    result = stord(symbolTable.resolve(val));
                } catch (...) {
                    std::cerr << "error: wrong string provided by to_number(\"";
                    std::cerr << symbolTable.resolve(val);
                    std::cerr << "\") functor.\n";
                    raise(SIGFPE);
                }
                stack.push(result);
                ip += 1;
                break;
            }
            case LVM_OP_TOSTRING: {
                RamDomain val = stack.top();
                RamDomain result = symbolTable.lookup(std::to_string(val));
                stack.pop();
                stack.push(result);
                ip += 1;
                break;
            }
            case LVM_OP_ADD: {
                RamDomain x = stack.top();
                stack.pop();
                RamDomain y = stack.top();
                stack.pop();
                stack.push(x + y);
                ip += 1;
                break;
            }
            case LVM_OP_SUB: {
                // Rhs was pushed last in the generator, so it should be on top.
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();
                stack.push(lhs - rhs);
                ip += 1;
                break;
            }
            case LVM_OP_MUL: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();
                stack.push(lhs * rhs);
                ip += 1;
                break;
            }
            case LVM_OP_DIV: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();
                stack.push(lhs / rhs);
                ip += 1;
                break;
            }
            case LVM_OP_EXP: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();
                stack.push(std::pow(lhs, rhs));
                ip += 1;
                break;
            }
            case LVM_OP_MOD: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();
                stack.push(lhs % rhs);
                ip += 1;
                break;
            }
            case LVM_OP_BAND: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();
                stack.push(lhs & rhs);
                ip += 1;
                break;
            }
            case LVM_OP_BOR: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();
                stack.push(lhs | rhs);
                ip += 1;
                break;
            }
            case LVM_OP_BXOR: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();
                stack.push(lhs ^ rhs);
                ip += 1;
                break;
            }
            case LVM_OP_LAND: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();
                stack.push(lhs && rhs);
                ip += 1;
                break;
            }
            case LVM_OP_LOR: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();
                stack.push(lhs || rhs);
                ip += 1;
                break;
            }
            case LVM_OP_MAX: {
                size_t size = code[ip + 1];
                RamDomain val = MIN_RAM_DOMAIN;
                for (size_t i = 0; i < size; ++i) {
                    val = std::max(val, stack.top());
                    stack.pop();
                }
                stack.push(val);
                ip += 2;
                break;
            }
            case LVM_OP_MIN: {
                size_t size = code[ip + 1];
                RamDomain val = MAX_RAM_DOMAIN;
                for (size_t i = 0; i < size; ++i) {
                    val = std::min(val, stack.top());
                    stack.pop();
                }
                stack.push(val);
                ip += 2;
                break;
            }
            case LVM_OP_CAT: {
                size_t size = code[ip + 1];
                std::string cat;
                for (size_t i = 0; i < size; ++i) {
                    cat += symbolTable.resolve(stack.top());
                    stack.pop();
                }
                stack.push(symbolTable.lookup(cat));
                ip += 2;
                break;
            }
            case LVM_OP_SUBSTR: {
                RamDomain len = stack.top();
                stack.pop();
                RamDomain idx = stack.top();
                stack.pop();
                RamDomain symbol = stack.top();
                stack.pop();
                std::string str = symbolTable.resolve(symbol);
                std::string sub_str;
                try {
                    sub_str = str.substr(idx, len);
                } catch (...) {
                    std::cerr << "warning: wrong index position provided by substr(\"";
                    std::cerr << str << "\"," << (int32_t)idx << "," << (int32_t)len << ") functor.\n";
                }
                stack.push(symbolTable.lookup(sub_str));

                ip += 1;
                break;
            }
            case LVM_OP_EQ: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();
                stack.push(lhs == rhs);
                ip += 1;
                break;
            }
            case LVM_OP_NE: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();
                stack.push(lhs != rhs);
                ip += 1;
                break;
            }
            case LVM_OP_LT: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();
                stack.push(lhs < rhs);
                ip += 1;
                break;
            }
            case LVM_OP_LE: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();
                stack.push(lhs <= rhs);
                ip += 1;
                break;
            }
            case LVM_OP_GT: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();
                stack.push(lhs > rhs);
                ip += 1;
                break;
            }
            case LVM_OP_GE: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();
                stack.push(lhs >= rhs);
                ip += 1;
                break;
            }
            case LVM_OP_MATCH: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();

                const std::string& pattern = symbolTable.resolve(lhs);
                const std::string& text = symbolTable.resolve(rhs);
                bool result = false;

                try {
                    result = std::regex_match(text, std::regex(pattern));
                } catch (...) {
                    std::cerr << "warning: wrong pattern provided for match(\"" << pattern << "\",\"" << text
                              << "\").\n";
                }
                stack.push(result);
                ip += 1;
                break;
            }
            case LVM_OP_NOT_MATCH: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();

                const std::string& pattern = symbolTable.resolve(lhs);
                const std::string& text = symbolTable.resolve(rhs);
                bool result = false;

                try {
                    result = !std::regex_match(text, std::regex(pattern));
                } catch (...) {
                    std::cerr << "warning: wrong pattern provided for match(\"" << pattern << "\",\"" << text
                              << "\").\n";
                }
                stack.push(result);
                ip += 1;
                break;
            }
            case LVM_OP_CONTAINS: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();
                const std::string& pattern = symbolTable.resolve(lhs);
                const std::string& text = symbolTable.resolve(rhs);
                stack.push(text.find(pattern) != std::string::npos);
                ip += 1;
                break;
            }
            case LVM_OP_NOT_CONTAINS: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();
                const std::string& pattern = symbolTable.resolve(lhs);
                const std::string& text = symbolTable.resolve(rhs);
                stack.push(text.find(pattern) == std::string::npos);
                ip += 1;
                break;
            }
            case LVM_UserDefinedOperator: {
                // get name and type
                const std::string name = symbolTable.resolve(code[ip + 1]);
                const std::string type = symbolTable.resolve(code[ip + 2]);

                auto fn = reinterpret_cast<void (*)()>(getMethodHandle(name));
                if (fn == nullptr) {
                    std::cerr << "Cannot find user-defined operator " << name << std::endl;
                    exit(1);
                }

                size_t arity = type.length();
                ffi_cif cif;
                ffi_type* args[arity];
                void* values[arity];
                RamDomain intVal[arity];
                const char* strVal[arity];
                ffi_arg rc;

                /* Initialize arguments for ffi-call */
                for (size_t i = 0; i < arity; i++) {
                    RamDomain arg = stack.top();
                    stack.pop();
                    if (type[arity - i - 1] == 'S') {
                        args[arity - i - 1] = &ffi_type_pointer;
                        strVal[arity - i - 1] = symbolTable.resolve(arg).c_str();
                        values[arity - i - 1] = &strVal[arity - i - 1];
                    } else {
                        args[arity - i - 1] = &ffi_type_uint32;
                        intVal[arity - i - 1] = arg;
                        values[arity - i - 1] = &intVal[arity - i - 1];
                    }
                }

                // call external function
                if (type[arity] == 'N') {
                    // Initialize for numerical return value
                    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, arity, &ffi_type_uint32, args) != FFI_OK) {
                        std::cerr << "Failed to prepare CIF for user-defined operator ";
                        std::cerr << name << std::endl;
                        exit(1);
                    }
                } else {
                    // Initialize for string return value
                    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, arity, &ffi_type_pointer, args) != FFI_OK) {
                        std::cerr << "Failed to prepare CIF for user-defined operator ";
                        std::cerr << name << std::endl;
                        exit(1);
                    }
                }
                ffi_call(&cif, fn, &rc, values);
                RamDomain result;
                if (type[arity] == 'N') {
                    result = ((RamDomain)rc);
                } else {
                    result = symbolTable.lookup(((const char*)rc));
                }
                stack.push(result);
                ip += 3;
                break;
            }
            case LVM_PackRecord: {
                RamDomain arity = code[ip + 1];
                RamDomain data[arity];
                for (auto i = 0; i < arity; ++i) {
                    data[arity - i - 1] = stack.top();
                    stack.pop();
                }
                stack.push(pack(data, arity));
                ip += 2;
                break;
            }
            case LVM_Argument: {
                stack.push(ctxt.getArgument(code[ip + 1]));
                ip += 2;
                break;
            }
            case LVM_Conjunction: {
                RamDomain rhs = stack.top();
                stack.pop();
                RamDomain lhs = stack.top();
                stack.pop();
                stack.push(lhs && rhs);
                ip += 1;
                break;
            }
            case LVM_Negation: {
                RamDomain val = stack.top();
                stack.pop();
                stack.push(!val);
                ip += 1;
                break;
            }
            case LVM_EmptinessCheck: {
                std::string relName = symbolTable.resolve(code[ip + 1]);
                stack.push(getRelation(relName).empty());
                ip += 2;
                break;
            }
            case LVM_ExistenceCheck: {
                std::string relName = symbolTable.resolve(code[ip + 1]);
                std::string patterns = symbolTable.resolve(code[ip + 2]);
                const InterpreterRelation& rel = getRelation(relName);
                size_t arity = rel.getArity();

                if (Global::config().has("profile") && !(relName[0] == '@')) {
                    this->reads[relName]++;
                }

                // for total we use the exists test
                if (patterns.find("_") == std::string::npos) {
                    RamDomain tuple[arity];
                    for (size_t i = 0; i < arity; i++) {
                        tuple[arity - i - 1] = stack.top();
                        stack.pop();
                    }
                    stack.push(rel.exists(tuple));
                    ip += 3;
                    break;
                } else {  // for partial we search for lower and upper boundaries
                    RamDomain low[arity];
                    RamDomain high[arity];

                    for (size_t i = 0; i < arity; i++) {
                        if (patterns[arity - i - 1] == 'V') {
                            low[arity - i - 1] = stack.top();
                            stack.pop();
                            high[arity - i - 1] = low[arity - i - 1];
                        } else {
                            low[arity - i - 1] = MIN_RAM_DOMAIN;
                            high[arity - i - 1] = MAX_RAM_DOMAIN;
                        }
                    }

                    SearchSignature res = getSearchSignature(patterns, arity);
                    auto idx = rel.getIndex(res);
                    auto range = idx->lowerUpperBound(low, high);

                    stack.push(range.first != range.second);
                    ip += 3;
                    break;
                }

                break;
            }
            case LVM_ProvenanceExistenceCheck: {
                std::string relName = symbolTable.resolve(code[ip + 1]);
                std::string patterns = symbolTable.resolve(code[ip + 2]);
                const InterpreterRelation& rel = getRelation(relName);
                auto arity = rel.getArity();

                RamDomain low[arity];
                RamDomain high[arity];

                for (size_t i = 2; i < arity; i++) {
                    if (patterns[arity - i - 1] == 'V') {
                        low[arity - i - 1] = stack.top();
                        stack.pop();
                        high[arity - i - 1] = low[arity - i - 1];
                    } else {
                        low[arity - i - 1] = MIN_RAM_DOMAIN;
                        low[arity - i - 1] = MAX_RAM_DOMAIN;
                    }
                }

                low[arity - 2] = MIN_RAM_DOMAIN;
                low[arity - 1] = MIN_RAM_DOMAIN;
                high[arity - 2] = MAX_RAM_DOMAIN;
                high[arity - 1] = MAX_RAM_DOMAIN;

                // obtain index
                SearchSignature res = 0;
                // values.size() - 1 because we discard the height annotation
                for (std::size_t i = 0; i < arity - 1; i++) {
                    if (patterns[i] == 'V') {
                        res |= (1 << i);
                    }
                }

                auto idx = rel.getIndex(res);
                auto range = idx->lowerUpperBound(low, high);
                stack.push(range.first != range.second);
                ip += 3;
                break;
            }
            case LVM_Constraint:
                /** Does nothing, just a label */
                ip += 1;
                break;
            case LVM_Scan:
                /** Does nothing, just a label */
                ip += 1;
                break;
            case LVM_IndexScan:
                /** Does nothing, just a label */
                ip += 1;
                break;
            case LVM_Choice:
                /** Does nothing, just a label */
                ip += 1;
                break;
            case LVM_IndexChoice:
                /** Does nothing, just a label */
                ip += 1;
                break;
            case LVM_Search: {
                if (Global::config().has("profile") && code[ip + 1] != 0) {
                    std::string msg = symbolTable.resolve(code[ip + 2]);
                    this->frequencies[msg][this->getIterationNumber()]++;
                }
                ip += 3;
                break;
            }
            case LVM_UnpackRecord: {
                RamDomain arity = code[ip + 1];
                RamDomain id = code[ip + 2];
                RamDomain exitAddress = code[ip + 3];

                RamDomain ref = stack.top();
                stack.pop();

                if (isNull(ref)) {
                    ip = exitAddress;
                    break;
                }

                RamDomain* tuple = unpack(ref, arity);
                ctxt[id] = tuple;
                ip += 4;
                break;
            }
            case LVM_Filter:
                if (Global::config().has("profile")) {
                    std::string msg = symbolTable.resolve(code[ip + 1]);
                    if (!msg.empty()) {
                        this->frequencies[msg][this->getIterationNumber()]++;
                    }
                }
                ip += 2;
                break;
            case LVM_Project: {
                RamDomain arity = code[ip + 1];
                std::string relName = symbolTable.resolve(code[ip + 2]);
                RamDomain tuple[arity];
                for (auto i = 0; i < arity; ++i) {
                    tuple[arity - i - 1] = stack.top();
                    stack.pop();
                }
                InterpreterRelation& rel = getRelation(relName);
                rel.insert(tuple);
                ip += 3;
                break;
            }
            case LVM_ReturnValue: {
                RamDomain size = code[ip + 1];
                std::string types = symbolTable.resolve(code[ip + 2]);
                for (auto i = 0; i < size; ++i) {
                    if (types[size - i - 1] == '_') {
                        ctxt.addReturnValue(0, true);
                    } else {
                        ctxt.addReturnValue(stack.top(), false);
                        stack.pop();
                    }
                }
                ip += 3;
                break;
            }
            case LVM_Sequence: {
                ip += 1;
                break;
            }
            case LVM_Parallel: {
                size_t size = code[ip + 1];
                size_t end = code[ip + 2];
                size_t startAddresses[size];
                for (size_t i = 0; i < size; ++i) {
                    startAddresses[i] = code[ip + 3 + i];
                }
#pragma omp parallel for
                for (size_t i = 0; i < size; ++i) {
                    this->execute(codeStream, ctxt, startAddresses[i]);
                }

                ip = end;
                break;
            }
            case LVM_Stop_Parallel: {
                ip += 2;
                break;
            }
            case LVM_Loop: {
                /** Does nothing, jus a label */
                ip += 1;
                break;
            }
            case LVM_IncIterationNumber: {
                incIterationNumber();
                ip += 1;
                break;
            };
            case LVM_ResetIterationNumber: {
                resetIterationNumber();
                ip += 1;
                break;
            };
            case LVM_Exit: {
                RamDomain val = stack.top();
                stack.pop();
                if (val) {
                    ip = code[ip + 1];
                    break;
                }
                ip += 2;
                break;
            }
            case LVM_LogTimer: {
                std::string msg = symbolTable.resolve(code[ip + 1]);
                size_t timerIndex = code[ip + 4];
                Logger* logger;
                if (code[ip + 2] == 0) {
                    logger = new Logger(msg.c_str(), this->getIterationNumber());
                } else {
                    std::string relName = symbolTable.resolve(code[ip + 3]);
                    const InterpreterRelation& rel = getRelation(relName);
                    logger = new Logger(msg.c_str(), this->getIterationNumber(),
                            std::bind(&InterpreterRelation::size, &rel));
                }
                insertTimerAt(timerIndex, logger);
                ip += 5;
                break;
            }
            case LVM_StopLogTimer: {
                size_t timerIndex = code[ip + 1];
                stopTimerAt(timerIndex);
                ip += 2;
                break;
            }
            case LVM_DebugInfo: {
                std::string msg = symbolTable.resolve(code[ip + 1]);
                SignalHandler::instance()->setMsg(msg.c_str());
                ip += 2;
                break;
            }
            case LVM_Stratum: {
                this->level++;
                // Record all the rleation that is created in the previous level
                if (Global::config().has("profile") || this->level != 0) {
                    for (const auto& rel : environment) {
                        // Skip if it is a temp rel and select only the relation in the same level
                        if (rel.first[0] == '@' || rel.second->getLevel() != this->level - 1) continue;

                        ProfileEventSingleton::instance().makeStratumRecord(rel.second->getLevel(),
                                "relation", rel.first, "arity", std::to_string(rel.second->getArity()));
                    }
                }
                ip += 1;
                break;
            }
            case LVM_Create: {
                InterpreterRelation* res = nullptr;
                std::string relName = symbolTable.resolve(code[ip + 1]);
                auto arity = code[ip + 2];
                assert(environment.find(relName) == environment.end());
                if (code[ip + 3] == LVM_EQREL) {
                    res = new InterpreterEqRelation(arity);
                } else {
                    res = new InterpreterRelation(arity);
                }
                std::vector<std::string> attributeTypes;
                for (int i = 0; i < code[ip + 2]; ++i) {
                    attributeTypes.push_back(symbolTable.resolve(code[ip + 4 + i]));
                }
                attributeTypes.reserve(attributeTypes.size());
                res->setAttributes(attributeTypes);
                res->setLevel(level);
                environment[relName] = res;
                ip += 3 + code[ip + 2] + 1;
                break;
            }
            case LVM_Clear: {
                std::string relName = symbolTable.resolve(code[ip + 1]);
                auto& rel = getRelation(relName);
                rel.purge();
                ip += 2;
                break;
            }
            case LVM_Drop: {
                std::string relName = symbolTable.resolve(code[ip + 1]);
                dropRelation(relName);
                ip += 2;
                break;
            }
            case LVM_LogSize: {
                std::string relName = symbolTable.resolve(code[ip + 1]);
                const InterpreterRelation& rel = getRelation(relName);
                std::string msg = symbolTable.resolve(code[ip + 2]);
                ProfileEventSingleton::instance().makeQuantityEvent(
                        msg, rel.size(), this->getIterationNumber());
                ip += 3;
                break;
            }
            case LVM_Load: {
                std::string relName = symbolTable.resolve(code[ip + 1]);
                auto IOs = codeStream->getIODirectives()[code[ip + 2]];

                for (auto& io : IOs) {
                    try {
                        InterpreterRelation& relation = getRelation(relName);
                        std::vector<bool> symbolMask;
                        for (auto& cur : relation.getAttributeTypeQualifiers()) {
                            symbolMask.push_back(cur[0] == 's');
                        }
                        IOSystem::getInstance()
                                .getReader(symbolMask, symbolTable, io, Global::config().has("provenance"))
                                ->readAll(relation);
                    } catch (std::exception& e) {
                        std::cerr << "Error loading data: " << e.what() << "\n";
                    }
                }
                ip += 3;
                break;
            }
            case LVM_Store: {
                std::string relName = symbolTable.resolve(code[ip + 1]);
                auto IOs = codeStream->getIODirectives()[code[ip + 2]];

                for (auto& io : IOs) {
                    try {
                        InterpreterRelation& relation = getRelation(relName);
                        std::vector<bool> symbolMask;
                        for (auto& cur : relation.getAttributeTypeQualifiers()) {
                            symbolMask.push_back(cur[0] == 's');
                        }
                        IOSystem::getInstance()
                                .getWriter(symbolMask, symbolTable, io, Global::config().has("provenance"))
                                ->writeAll(relation);
                    } catch (std::exception& e) {
                        std::cerr << "Error Storing data: " << e.what() << "\n";
                    }
                }
                ip += 3;
                break;
            }
            case LVM_Fact: {
                std::string relName = symbolTable.resolve(code[ip + 1]);
                auto arity = code[ip + 2];
                RamDomain tuple[arity];
                for (auto i = 0; i < arity; ++i) {
                    tuple[arity - i - 1] = stack.top();
                    stack.pop();
                }
                getRelation(relName).insert(tuple);
                ip += 3;
                break;
            }
            case LVM_Merge: {
                std::string source = symbolTable.resolve(code[ip + 1]);
                std::string target = symbolTable.resolve(code[ip + 2]);
                // get involved relation
                InterpreterRelation& src = getRelation(source);
                InterpreterRelation& trg = getRelation(target);

                if (dynamic_cast<InterpreterEqRelation*>(&trg)) {
                    // expand src with the new knowledge generated by insertion.
                    src.extend(trg);
                }
                // merge in all elements
                trg.insert(src);

                ip += 3;
                break;
            }
            case LVM_Swap: {
                std::string firstRel = symbolTable.resolve(code[ip + 1]);
                std::string secondRel = symbolTable.resolve(code[ip + 2]);
                swapRelation(firstRel, secondRel);
                ip += 3;
                break;
            }
            case LVM_Query:
                /** Does nothing, just a label */
                ip += 1;
                break;
            case LVM_Goto:
                ip = code[ip + 1];
                break;
            case LVM_Jmpnz: {
                RamDomain val = stack.top();
                stack.pop();
                ip = (val != 0 ? code[ip + 1] : ip + 2);
                break;
            }
            case LVM_Jmpez: {
                RamDomain val = stack.top();
                stack.pop();
                ip = (val == 0 ? code[ip + 1] : ip + 2);
                break;
            }
            case LVM_Aggregate: {
                ip += 1;
                break;
            };
            case LVM_IndexAggregate: {
                ip += 1;
                break;
            };
            case LVM_Aggregate_COUNT: {
                RamDomain res = 0;
                RamDomain idx = code[ip + 2];
                if (code[ip + 1] == LVM_ITER_TypeScan) {
                    auto& iters = scanIteratorPool[idx];
                    for (auto i = iters.first; i != iters.second; ++i) {
                        res++;
                    }
                    stack.push(res);
                } else {
                    auto& iters = indexScanIteratorPool[idx];
                    for (auto i = iters.first; i != iters.second; ++i) {
                        res++;
                    }
                    stack.push(res);
                }
                ip += 3;
                break;
            };
            case LVM_Aggregate_Return: {
                RamDomain id = code[ip + 1];
                RamDomain res = stack.top();
                stack.pop();
                RamDomain* tuple = ctxt.allocateNewTuple(1);
                tuple[0] = res;
                ctxt[id] = tuple;
                ip += 2;
                break;
            };
            case LVM_ITER_TypeScan: {
                RamDomain idx = code[ip + 1];

                std::string relName = symbolTable.resolve(code[ip + 2]);
                InterpreterRelation& rel = getRelation(relName);
                lookUpScanIterator(idx) =
                        std::pair<InterpreterRelation::iterator, InterpreterRelation::iterator>(
                                rel.begin(), rel.end());
                assert(rel.begin() != rel.end() || rel.empty());

                ip += 3;
                break;
            }
            case LVM_ITER_TypeChoice: {
                RamDomain idx = code[ip + 1];

                std::string relName = symbolTable.resolve(code[ip + 2]);
                InterpreterRelation& rel = getRelation(relName);
                lookUpChoiceIterator(idx) =
                        std::pair<InterpreterRelation::iterator, InterpreterRelation::iterator>(
                                rel.begin(), rel.end());
                assert(rel.begin() != rel.end() || rel.empty());

                ip += 3;
                break;
            }
            case LVM_ITER_TypeIndexScan: {
                RamDomain idx = code[ip + 1];
                std::string relName = symbolTable.resolve(code[ip + 2]);
                InterpreterRelation& rel = getRelation(relName);
                std::string pattern = symbolTable.resolve(code[ip + 3]);

                // create pattern tuple for range query
                auto arity = rel.getArity();
                RamDomain low[arity];
                RamDomain hig[arity];
                for (size_t i = 0; i < arity; i++) {
                    if (pattern[arity - i - 1] == 'V') {
                        low[arity - i - 1] = stack.top();
                        stack.pop();
                        hig[arity - i - 1] = low[arity - i - 1];
                    } else {
                        low[arity - i - 1] = MIN_RAM_DOMAIN;
                        hig[arity - i - 1] = MAX_RAM_DOMAIN;
                    }
                }

                SearchSignature keys = getSearchSignature(pattern, arity);
                auto index = rel.getIndex(keys);

                // get iterator range
                lookUpIndexScanIterator(idx) = index->lowerUpperBound(low, hig);
                ip += 4;
                break;
            }
            case LVM_ITER_TypeIndexChoice: {
                RamDomain idx = code[ip + 1];
                std::string relName = symbolTable.resolve(code[ip + 2]);
                InterpreterRelation& rel = getRelation(relName);
                std::string pattern = symbolTable.resolve(code[ip + 3]);

                // create pattern tuple for range query
                auto arity = rel.getArity();
                RamDomain low[arity];
                RamDomain hig[arity];
                for (size_t i = 0; i < arity; i++) {
                    if (pattern[arity - i - 1] == 'V') {
                        low[arity - i - 1] = stack.top();
                        stack.pop();
                        hig[arity - i - 1] = low[arity - i - 1];
                    } else {
                        low[arity - i - 1] = MIN_RAM_DOMAIN;
                        hig[arity - i - 1] = MAX_RAM_DOMAIN;
                    }
                }

                SearchSignature keys = getSearchSignature(pattern, arity);
                auto index = rel.getIndex(keys);

                // get iterator range
                lookUpIndexChoiceIterator(idx) = index->lowerUpperBound(low, hig);
                ip += 4;
                break;
            }
            case LVM_ITER_NotAtEnd: {
                RamDomain idx = code[ip + 1];
                switch (code[ip + 2]) {
                    case LVM_ITER_TypeScan: {
                        auto iter = scanIteratorPool[idx];
                        stack.push(iter.first != iter.second);
                        break;
                    }
                    case LVM_ITER_TypeIndexScan: {
                        auto iter = indexScanIteratorPool[idx];
                        stack.push(iter.first != iter.second);
                        break;
                    }
                    case LVM_ITER_TypeChoice: {
                        auto iter = choiceIteratorPool[idx];
                        stack.push(iter.first != iter.second);
                        break;
                    }
                    case LVM_ITER_TypeIndexChoice: {
                        auto iter = indexChoiceIteratorPool[idx];
                        stack.push(iter.first != iter.second);
                        break;
                    }
                    default:
                        printf("Unknown iter type at LVM_ITER_NotAtEnd\n");
                        break;
                }
                ip += 3;
                break;
            }
            case LVM_ITER_Select: {
                RamDomain idx = code[ip + 1];
                RamDomain id = code[ip + 3];
                switch (code[ip + 2]) {
                    case LVM_ITER_TypeScan: {
                        auto iter = scanIteratorPool[idx];
                        ctxt[id] = *iter.first;
                        break;
                    }
                    case LVM_ITER_TypeIndexScan: {
                        auto iter = indexScanIteratorPool[idx];
                        ctxt[id] = *iter.first;
                        break;
                    }
                    case LVM_ITER_TypeChoice: {
                        auto iter = choiceIteratorPool[idx];
                        ctxt[id] = *iter.first;
                        break;
                    }
                    case LVM_ITER_TypeIndexChoice: {
                        auto iter = indexChoiceIteratorPool[idx];
                        ctxt[id] = *iter.first;
                        break;
                    }
                    default:
                        printf("Unknown iter type at LVM_ITER_Select\n");
                        break;
                }
                ip += 4;
                break;
            }
            case LVM_ITER_Inc: {
                RamDomain idx = code[ip + 1];
                switch (code[ip + 2]) {
                    case LVM_ITER_TypeScan: {
                        ++scanIteratorPool[idx].first;
                        break;
                    }
                    case LVM_ITER_TypeIndexScan: {
                        ++indexScanIteratorPool[idx].first;
                        break;
                    }
                    case LVM_ITER_TypeChoice: {
                        ++choiceIteratorPool[idx].first;
                        break;
                    }
                    case LVM_ITER_TypeIndexChoice: {
                        ++indexChoiceIteratorPool[idx].first;
                        break;
                    }
                    default:
                        printf("Unknown iter\n");
                        break;
                }
                ip += 3;
                break;
            }
            case LVM_STOP:
                assert(stack.size() == 0);
                return;
            default:
                printf("Unknown. eval()\n");
                break;
        }
    }
}

}  // end of namespace souffle
