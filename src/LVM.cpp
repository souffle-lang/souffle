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
#include "LVMIndex.h"
#include "LVMRecords.h"
#include "LVMRelation.h"
#include "Logger.h"
#include "ParallelUtils.h"
#include "ProfileEvent.h"
#include "RamExpression.h"
#include "RamIndexAnalysis.h"
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
        LVMGenerator generator(translationUnit.getSymbolTable(), main, relationEncoder);
        mainProgram = generator.getCodeStream();
    }
    LVMContext ctxt;
    SignalHandler::instance()->set();
    if (Global::config().has("verbose")) {
        SignalHandler::instance()->enableLogging();
    }

    if (!profile) {
        execute(mainProgram, ctxt);
    } else {
        ProfileEventSingleton::instance().setOutputFile(Global::config().get("profile"));
        // Prepare the frequency table for threaded use
        visitDepthFirst(main, [&](const RamTupleOperation& node) {
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

void LVM::execute(std::unique_ptr<LVMCode>& codeStream, LVMContext& ctxt, size_t ip) {
    std::stack<RamDomain> stack;
    const LVMCode& code = *codeStream;
    auto& symbolTable = codeStream->getSymbolTable();
    while (true) {
        switch (code[ip]) {
            case LVM_Number:
                stack.push(code[ip + 1]);
                ip += 2;
                break;
            case LVM_TupleElement:
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
                std::stringstream buf;
                for (size_t i = 0; i < size; ++i) {
                    buf << symbolTable.resolve(stack.top());
                    stack.pop();
                }
                stack.push(symbolTable.lookup(buf.str()));
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
                const std::string& str = symbolTable.resolve(symbol);
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
                size_t arity = code[ip + 3];
                auto fn = reinterpret_cast<void (*)()>(getMethodHandle(name));
                if (fn == nullptr) {
                    std::cerr << "Cannot find user-defined operator " << name << std::endl;
                    exit(1);
                }

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
                    if (type[i] == 'S') {
                        args[i] = &ffi_type_pointer;
                        strVal[i] = symbolTable.resolve(arg).c_str();
                        values[i] = &strVal[i];
                    } else {
                        args[i] = &ffi_type_uint32;
                        intVal[i] = arg;
                        values[i] = &intVal[i];
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
                ip += 4;
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
            case LVM_True: {
                stack.push(1);
                ip += 1;
                break;
            };
            case LVM_False: {
                stack.push(0);
                ip += 1;
                break;
            };
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
                size_t relId = code[ip + 1];
                stack.push(getRelation(relId)->empty());
                ip += 2;
                break;
            }
            case LVM_ContainTuple: {
                auto relPtr = getRelation(code[ip + 1]);
                auto arity = relPtr->getArity();
                RamDomain tuple[arity];
                for (auto i = 0; i < arity; ++i) {
                    tuple[i] = stack.top();
                    stack.pop();
                }
                stack.push(relPtr->contains(TupleRef(tuple, arity)));
                ip += 2;
                break;
            }
            case LVM_ExistenceCheck: {
                auto relPtr = getRelation(code[ip + 1]);
                auto arity = relPtr->getArity();
                auto indexPos = code[ip + 2];

                RamDomain low[arity];
                RamDomain high[arity];
                size_t numOfTypeMasks = arity / RAM_DOMAIN_SIZE + (arity % RAM_DOMAIN_SIZE != 0);
                for (size_t i = 0; i < numOfTypeMasks; ++i) {
                    RamDomain typeMask = code[ip + 3 + i];
                    for (auto j = 0; j < RAM_DOMAIN_SIZE; ++j) {
                        auto projectedIndex = i * RAM_DOMAIN_SIZE + j;
                        if (projectedIndex >= arity) {
                            break;
                        }
                        if (1 << j & typeMask) {
                            low[projectedIndex] = stack.top();
                            stack.pop();
                            high[projectedIndex] = low[projectedIndex];
                        } else {
                            low[projectedIndex] = MIN_RAM_DOMAIN;
                            high[projectedIndex] = MAX_RAM_DOMAIN;
                        }
                    }
                }
                auto range = relPtr->range(indexPos, TupleRef(low, arity), TupleRef(high, arity));
                stack.push(range.begin() != range.end());

                ip += (3 + numOfTypeMasks);
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
                if (profile && code[ip + 1] != 0) {
                    const std::string& msg = symbolTable.resolve(code[ip + 2]);
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

                ctxt[id] = TupleRef(unpack(ref, arity), arity);
                ip += 4;
                break;
            }
            case LVM_Filter:
                if (profile) {
                    const std::string& msg = symbolTable.resolve(code[ip + 1]);
                    if (!msg.empty()) {
                        this->frequencies[msg][this->getIterationNumber()]++;
                    }
                }
                ip += 2;
                break;
            case LVM_Project: {
                RamDomain arity = code[ip + 1];
                size_t relId = code[ip + 2];
                LVMRelation& rel = *getRelation(relId);
                RamDomain tuple[arity];
                for (auto i = 0; i < arity; ++i) {
                    tuple[i] = stack.top();
                    stack.pop();
                }
                rel.insert(TupleRef(tuple, arity));
                ip += 3;
                break;
            }
            case LVM_ReturnValue: {
                RamDomain size = code[ip + 1];
                const std::string& types = symbolTable.resolve(code[ip + 2]);
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
                const std::string& msg = symbolTable.resolve(code[ip + 1]);
                size_t timerIndex = code[ip + 2];
                Logger* logger = new Logger(msg.c_str(), this->getIterationNumber());
                insertTimerAt(timerIndex, logger);
                ip += 3;
                break;
            }
            case LVM_LogRelationTimer: {
                const std::string& msg = symbolTable.resolve(code[ip + 1]);
                size_t timerIndex = code[ip + 2];
                size_t relId = code[ip + 3];
                const LVMRelation& rel = *getRelation(relId);
                Logger* logger = new Logger(
                        msg.c_str(), this->getIterationNumber(), std::bind(&LVMRelation::size, &rel));
                insertTimerAt(timerIndex, logger);
                ip += 4;
                break;
            }
            case LVM_StopLogTimer: {
                size_t timerIndex = code[ip + 1];
                stopTimerAt(timerIndex);
                ip += 2;
                break;
            }
            case LVM_DebugInfo: {
                const std::string& msg = symbolTable.resolve(code[ip + 1]);
                SignalHandler::instance()->setMsg(msg.c_str());
                ip += 2;
                break;
            }
            case LVM_Stratum: {
                this->level++;
                // Record all the rleation that is created in the previous level
                if (profile && this->level != 0) {
                    for (const auto& rel : relationEncoder.getRelationMap()) {
                        if (rel == nullptr) {
                            continue;
                        }
                        const std::string& relName = rel->getName();
                        // Skip if it is a temp rel and select only the relation in the same level
                        if (relName[0] == '@' || rel->getLevel() != this->level - 1) continue;

                        ProfileEventSingleton::instance().makeStratumRecord(rel->getLevel(), "relation",
                                relName, "arity", std::to_string(rel->getArity()));
                    }
                }
                ip += 1;
                break;
            }
            case LVM_Create: {
                size_t relId = code[ip + 1];
                auto res = getRelation(relId);
                res->setLevel(level);
                ip += 2;
                break;
            }
            case LVM_Clear: {
                size_t relId = code[ip + 1];
                auto relPtr = getRelation(relId);
                relPtr->purge();
                ip += 2;
                break;
            }
            case LVM_Drop: {
                size_t relId = code[ip + 1];
                dropRelation(relId);
                ip += 2;
                break;
            }
            case LVM_LogSize: {
                size_t relId = code[ip + 1];
                auto relPtr = getRelation(relId);
                const std::string& msg = symbolTable.resolve(code[ip + 2]);
                ProfileEventSingleton::instance().makeQuantityEvent(
                        msg, relPtr->size(), this->getIterationNumber());
                ip += 3;
                break;
            }
            case LVM_Load: {
                size_t relId = code[ip + 1];
                auto IOs = codeStream->getIODirectives()[code[ip + 2]];

                for (auto& io : IOs) {
                    try {
                        auto relPtr = getRelation(relId);
                        std::vector<bool> symbolMask;
                        for (auto& cur : relPtr->getAttributeTypeQualifiers()) {
                            symbolMask.push_back(cur[0] == 's');
                        }
                        IOSystem::getInstance()
                                .getReader(symbolMask, symbolTable, io, provenance)
                                ->readAll(*relPtr);
                    } catch (std::exception& e) {
                        std::cerr << "Error loading data: " << e.what() << "\n";
                    }
                }
                ip += 3;
                break;
            }
            case LVM_Store: {
                size_t relId = code[ip + 1];
                auto IOs = codeStream->getIODirectives()[code[ip + 2]];

                for (auto& io : IOs) {
                    try {
                        auto relPtr = getRelation(relId);
                        std::vector<bool> symbolMask;
                        for (auto& cur : relPtr->getAttributeTypeQualifiers()) {
                            symbolMask.push_back(cur[0] == 's');
                        }
                        IOSystem::getInstance()
                                .getWriter(symbolMask, symbolTable, io, provenance)
                                ->writeAll(*relPtr);
                    } catch (std::exception& e) {
                        std::cerr << "Error Storing data: " << e.what() << "\n";
                    }
                }
                ip += 3;
                break;
            }
            case LVM_Fact: {
                size_t relId = code[ip + 1];
                auto arity = code[ip + 2];
                RamDomain tuple[arity];
                for (auto i = 0; i < arity; ++i) {
                    tuple[i] = stack.top();
                    stack.pop();
                }
                getRelation(relId)->insert(TupleRef(tuple, arity));
                ip += 3;
                break;
            }
            case LVM_Merge: {
                size_t sourceId = code[ip + 1];
                size_t targetId = code[ip + 2];
                // get involved relation
                auto srcPtr = getRelation(sourceId);
                auto trgPtr = getRelation(targetId);

                if (dynamic_cast<LVMEqRelation*>(trgPtr) != nullptr) {
                    // expand src with the new knowledge generated by insertion.
                    srcPtr->extend(*trgPtr);
                }
                // merge in all elements
                trgPtr->insert(*srcPtr);

                ip += 3;
                break;
            }
            case LVM_Swap: {
                size_t firstRelId = code[ip + 1];
                size_t secondRelId = code[ip + 2];
                swapRelation(firstRelId, secondRelId);
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
                RamDomain idx = code[ip + 1];
                auto& stream = streamPool[idx];
                for (auto i = stream.begin(); i != stream.end(); ++i) {
                    res++;
                }
                stack.push(res);
                ip += 2;
                break;
            };
            case LVM_Aggregate_Return: {
                RamDomain id = code[ip + 1];
                RamDomain res = stack.top();
                stack.pop();
                RamDomain* tuple = ctxt.allocateNewTuple(1);
                tuple[0] = res;
                ctxt[id] = TupleRef(tuple, 1);
                ip += 2;
                break;
            };
            case LVM_ITER_InitFullIndex: {
                RamDomain dest = code[ip + 1];
                size_t relId = code[ip + 2];
                const auto& relPtr = getRelation(relId);
                lookUpStream(dest) = relPtr->scan();
                ip += 3;
                break;
            };
            case LVM_ITER_InitRangeIndex: {
                RamDomain dest = code[ip + 1];
                auto relPtr = getRelation(code[ip + 2]);
                auto arity = relPtr->getArity();
                RamDomain indexPos = code[ip + 3];

                // create pattern tuple for range query
                size_t numOfTypeMasks = arity / RAM_DOMAIN_SIZE + (arity % RAM_DOMAIN_SIZE != 0);
                RamDomain low[arity];
                RamDomain high[arity];
                for (size_t i = 0; i < numOfTypeMasks; ++i) {
                    RamDomain typeMask = code[ip + 4 + i];
                    for (auto j = 0; j < RAM_DOMAIN_SIZE; ++j) {
                        auto projectedIndex = i * RAM_DOMAIN_SIZE + j;
                        if (projectedIndex >= arity) {
                            break;
                        }
                        if (1 << j & typeMask) {
                            low[projectedIndex] = stack.top();
                            stack.pop();
                            high[projectedIndex] = low[projectedIndex];
                        } else {
                            low[projectedIndex] = MIN_RAM_DOMAIN;
                            high[projectedIndex] = MAX_RAM_DOMAIN;
                        }
                    }
                }
                // get iterator range
                lookUpStream(dest) = relPtr->range(indexPos, TupleRef(low, arity), TupleRef(high, arity));
                ip += (4 + numOfTypeMasks);
                break;
            };
            case LVM_ITER_NotAtEnd: {
                RamDomain idx = code[ip + 1];
                auto& stream = lookUpStream(idx);
                stack.push(stream.begin() != stream.end());
                ip += 2;
                break;
            }
            case LVM_ITER_Select: {
                RamDomain idx = code[ip + 1];
                RamDomain tupleId = code[ip + 2];
                auto& stream = lookUpStream(idx);
                ctxt[tupleId] = *stream.begin();
                ip += 3;
                break;
            }
            case LVM_ITER_Inc: {
                RamDomain idx = code[ip + 1];
                ++streamPool[idx].begin();
                ip += 2;
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
