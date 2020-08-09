/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file InterpreterEngine.cpp
 *
 * Define the Interpreter Engine class.
 ***********************************************************************/

#include "interpreter/InterpreterEngine.h"
#include "AggregateOp.h"
#include "BinaryConstraintOps.h"
#include "FunctorOps.h"
#include "profile/Logger.h"
#include "profile/ProfileEvent.h"
#include "RamTypes.h"
#include "RecordTable.h"
#include "SignalHandler.h"
#include "SymbolTable.h"
#include "TypeAttribute.h"
#include "interpreter/InterpreterContext.h"
#include "interpreter/InterpreterGenerator.h"
#include "interpreter/InterpreterIndex.h"
#include "interpreter/InterpreterNode.h"
#include "interpreter/InterpreterPreamble.h"
#include "interpreter/InterpreterRelation.h"
#include "io/IOSystem.h"
#include "io/ReadStream.h"
#include "io/WriteStream.h"
#include "ram/Aggregate.h"
#include "ram/AutoIncrement.h"
#include "ram/Break.h"
#include "ram/Call.h"
#include "ram/Choice.h"
#include "ram/Clear.h"
#include "ram/Conjunction.h"
#include "ram/Constant.h"
#include "ram/Constraint.h"
#include "ram/DebugInfo.h"
#include "ram/EmptinessCheck.h"
#include "ram/ExistenceCheck.h"
#include "ram/Exit.h"
#include "ram/Extend.h"
#include "ram/False.h"
#include "ram/Filter.h"
#include "ram/IO.h"
#include "ram/IndexAggregate.h"
#include "ram/IndexChoice.h"
#include "ram/IndexScan.h"
#include "ram/IntrinsicOperator.h"
#include "ram/LogRelationTimer.h"
#include "ram/LogSize.h"
#include "ram/LogTimer.h"
#include "ram/Loop.h"
#include "ram/Negation.h"
#include "ram/NestedIntrinsicOperator.h"
#include "ram/PackRecord.h"
#include "ram/Parallel.h"
#include "ram/ParallelAggregate.h"
#include "ram/ParallelChoice.h"
#include "ram/ParallelIndexAggregate.h"
#include "ram/ParallelIndexChoice.h"
#include "ram/ParallelIndexScan.h"
#include "ram/ParallelScan.h"
#include "ram/Program.h"
#include "ram/Project.h"
#include "ram/ProvenanceExistenceCheck.h"
#include "ram/Query.h"
#include "ram/Relation.h"
#include "ram/Scan.h"
#include "ram/Sequence.h"
#include "ram/Statement.h"
#include "ram/SubroutineArgument.h"
#include "ram/SubroutineReturn.h"
#include "ram/Swap.h"
#include "ram/TranslationUnit.h"
#include "ram/True.h"
#include "ram/TupleElement.h"
#include "ram/TupleOperation.h"
#include "ram/UnpackRecord.h"
#include "ram/UserDefinedOperator.h"
#include "ram/Visitor.h"
#include "utility/EvaluatorUtil.h"
#include "utility/MiscUtil.h"
#include "utility/ParallelUtil.h"
#include "utility/StringUtil.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <dlfcn.h>
#include <ffi.h>

namespace souffle {

// Handle difference in dynamic libraries suffixes.
#ifdef __APPLE__
#define dynamicLibSuffix ".dylib";
#else
#define dynamicLibSuffix ".so";
#endif

// Aliases for foreign function interface.
#if RAM_DOMAIN_SIZE == 64
#define FFI_RamSigned ffi_type_sint64
#define FFI_RamUnsigned ffi_type_uint64
#define FFI_RamFloat ffi_type_double
#else
#define FFI_RamSigned ffi_type_sint32
#define FFI_RamUnsigned ffi_type_uint32
#define FFI_RamFloat ffi_type_float
#endif

#define FFI_Symbol ffi_type_pointer

namespace {
constexpr RamDomain RAM_BIT_SHIFT_MASK = RAM_DOMAIN_SIZE - 1;
}

InterpreterEngine::RelationHandle& InterpreterEngine::getRelationHandle(const size_t idx) {
    return generator.getRelationHandle(idx);
}

void InterpreterEngine::swapRelation(const size_t ramRel1, const size_t ramRel2) {
    RelationHandle& rel1 = getRelationHandle(ramRel1);
    RelationHandle& rel2 = getRelationHandle(ramRel2);
    std::swap(rel1, rel2);
}

int InterpreterEngine::incCounter() {
    return counter++;
}

SymbolTable& InterpreterEngine::getSymbolTable() {
    return tUnit.getSymbolTable();
}

RecordTable& InterpreterEngine::getRecordTable() {
    return recordTable;
}

RamTranslationUnit& InterpreterEngine::getTranslationUnit() {
    return tUnit;
}

void* InterpreterEngine::getMethodHandle(const std::string& method) {
    // load DLLs (if not done yet)
    for (void* libHandle : loadDLL()) {
        auto* methodHandle = dlsym(libHandle, method.c_str());
        if (methodHandle != nullptr) {
            return methodHandle;
        }
    }
    return nullptr;
}

std::vector<std::unique_ptr<InterpreterEngine::RelationHandle>>& InterpreterEngine::getRelationMap() {
    return generator.getRelations();
}

const std::vector<void*>& InterpreterEngine::loadDLL() {
    if (!dll.empty()) {
        return dll;
    }

    if (!Global::config().has("libraries")) {
        Global::config().set("libraries", "functors");
    }
    if (!Global::config().has("library-dir")) {
        Global::config().set("library-dir", ".");
    }

    for (const std::string& library : splitString(Global::config().get("libraries"), ' ')) {
        // The library may be blank
        if (library.empty()) {
            continue;
        }
        auto paths = splitString(Global::config().get("library-dir"), ' ');
        // Set up our paths to have a library appended
        for (std::string& path : paths) {
            if (path.back() != '/') {
                path += '/';
            }
        }

        if (library.find('/') != std::string::npos) {
            paths.clear();
        }

        paths.push_back("");

        void* tmp = nullptr;
        for (const std::string& path : paths) {
            std::string fullpath = path + "lib" + library + dynamicLibSuffix;
            tmp = dlopen(fullpath.c_str(), RTLD_LAZY);
            if (tmp != nullptr) {
                dll.push_back(tmp);
                break;
            }
        }
    }

    return dll;
}

size_t InterpreterEngine::getIterationNumber() const {
    return iteration;
}
void InterpreterEngine::incIterationNumber() {
    ++iteration;
}
void InterpreterEngine::resetIterationNumber() {
    iteration = 0;
}

void InterpreterEngine::executeMain() {
    SignalHandler::instance()->set();
    if (Global::config().has("verbose")) {
        SignalHandler::instance()->enableLogging();
    }

    generateIR();
    assert(main != nullptr && "Executing an empty program");

    InterpreterContext ctxt;

    if (!profileEnabled) {
        InterpreterContext ctxt;
        execute(main.get(), ctxt);
    } else {
        ProfileEventSingleton::instance().setOutputFile(Global::config().get("profile"));
        // Prepare the frequency table for threaded use
        const RamProgram& program = tUnit.getProgram();
        visitDepthFirst(program, [&](const RamTupleOperation& node) {
            if (!node.getProfileText().empty()) {
                frequencies.emplace(node.getProfileText(), std::deque<std::atomic<size_t>>());
                frequencies[node.getProfileText()].emplace_back(0);
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
        for (auto rel : tUnit.getProgram().getRelations()) {
            if (rel->getName()[0] != '@') {
                ++relationCount;
                reads[rel->getName()] = 0;
            }
        }
        ProfileEventSingleton::instance().makeConfigRecord("relationCount", std::to_string(relationCount));

        // Store count of rules
        size_t ruleCount = 0;
        visitDepthFirst(program, [&](const RamQuery&) { ++ruleCount; });
        ProfileEventSingleton::instance().makeConfigRecord("ruleCount", std::to_string(ruleCount));

        InterpreterContext ctxt;
        execute(main.get(), ctxt);
        ProfileEventSingleton::instance().stopTimer();
        for (auto const& cur : frequencies) {
            for (size_t i = 0; i < cur.second.size(); ++i) {
                ProfileEventSingleton::instance().makeQuantityEvent(cur.first, cur.second[i], i);
            }
        }
        for (auto const& cur : reads) {
            ProfileEventSingleton::instance().makeQuantityEvent(
                    "@relation-reads;" + cur.first, cur.second, 0);
        }
    }
    SignalHandler::instance()->reset();
}

void InterpreterEngine::generateIR() {
    const RamProgram& program = tUnit.getProgram();
    if (subroutine.empty()) {
        for (const auto& sub : program.getSubroutines()) {
            subroutine.push_back(generator.generateTree(*sub.second, program));
        }
    }
    if (main == nullptr) {
        main = generator.generateTree(program.getMain(), program);
    }
}

void InterpreterEngine::executeSubroutine(
        const std::string& name, const std::vector<RamDomain>& args, std::vector<RamDomain>& ret) {
    InterpreterContext ctxt;
    ctxt.setReturnValues(ret);
    ctxt.setArguments(args);
    generateIR();
    const RamProgram& program = tUnit.getProgram();
    auto subs = program.getSubroutines();
    size_t i = distance(subs.begin(), subs.find(name));
    execute(subroutine[i].get(), ctxt);
}

RamDomain InterpreterEngine::execute(const InterpreterNode* node, InterpreterContext& ctxt) {
#define DEBUG(Kind) std::cout << "Running Node: " << #Kind << "\n";
#define EVAL_CHILD(ty, idx) ramBitCast<ty>(execute(shadow.getChild(idx), ctxt))
#define EVAL_LEFT(ty) ramBitCast<ty>(execute(shadow.getLhs(), ctxt))
#define EVAL_RIGHT(ty) ramBitCast<ty>(execute(shadow.getRhs(), ctxt))

#define CASE(Kind)     \
    case (I_##Kind): { \
        return [&]() -> RamDomain { \
            const auto& shadow = *static_cast<const Interpreter##Kind*>(node); \
            const auto& cur = *static_cast<const Ram##Kind*>(node->getShadow());
#define ESAC(Kind) \
    }              \
    ();            \
    }

#define CAL_SEARCH_BOUND(superInfo, low, high)                          \
    /** Unbounded and Constant */                                       \
    memcpy(low, superInfo.first.data(), sizeof(low));                   \
    memcpy(high, superInfo.second.data(), sizeof(high));                \
    /* TupleElement */                                                  \
    for (const auto& tupleElement : superInfo.tupleFirst) {             \
        low[tupleElement[0]] = ctxt[tupleElement[1]][tupleElement[2]];  \
    }                                                                   \
    for (const auto& tupleElement : superInfo.tupleSecond) {            \
        high[tupleElement[0]] = ctxt[tupleElement[1]][tupleElement[2]]; \
    }                                                                   \
    /* Generic */                                                       \
    for (const auto& expr : superInfo.exprFirst) {                      \
        low[expr.first] = execute(expr.second.get(), ctxt);             \
    }                                                                   \
    for (const auto& expr : superInfo.exprSecond) {                     \
        high[expr.first] = execute(expr.second.get(), ctxt);            \
    }

    switch (node->getType()) {
        CASE(Constant)
            return cur.getConstant();
        ESAC(Constant)

        CASE(TupleElement)
            return ctxt[cur.getTupleId()][cur.getElement()];
        ESAC(TupleElement)

        CASE(AutoIncrement)
            return incCounter();
        ESAC(AutoIncrement)

        CASE(IntrinsicOperator)
// clang-format off
#define BINARY_OP_TYPED(ty, op) return ramBitCast(static_cast<ty>(EVAL_CHILD(ty, 0) op EVAL_CHILD(ty, 1)))

#define BINARY_OP_LOGICAL(opcode, op) BINARY_OP_INTEGRAL(opcode, op)
#define BINARY_OP_INTEGRAL(opcode, op)                           \
    case FunctorOp::   opcode: BINARY_OP_TYPED(RamSigned  , op); \
    case FunctorOp::U##opcode: BINARY_OP_TYPED(RamUnsigned, op);
#define BINARY_OP_NUMERIC(opcode, op)                         \
    BINARY_OP_INTEGRAL(opcode, op)                            \
    case FunctorOp::F##opcode: BINARY_OP_TYPED(RamFloat, op);

#define BINARY_OP_SHIFT_MASK(ty, op)                                                 \
    return ramBitCast(EVAL_CHILD(ty, 0) op (EVAL_CHILD(ty, 1) & RAM_BIT_SHIFT_MASK))
#define BINARY_OP_INTEGRAL_SHIFT(opcode, op, tySigned, tyUnsigned)    \
    case FunctorOp::   opcode: BINARY_OP_SHIFT_MASK(tySigned   , op); \
    case FunctorOp::U##opcode: BINARY_OP_SHIFT_MASK(tyUnsigned , op);

#define MINMAX_OP_SYM(op)                                        \
    {                                                            \
        auto result = EVAL_CHILD(RamDomain, 0);                  \
        auto* result_val = &getSymbolTable().resolve(result);    \
        for (size_t i = 1; i < args.size(); i++) {               \
            auto alt = EVAL_CHILD(RamDomain, i);                 \
            if (alt == result) continue;                         \
                                                                 \
            const auto& alt_val = getSymbolTable().resolve(alt); \
            if (*result_val op alt_val) {                        \
                result_val = &alt_val;                           \
                result = alt;                                    \
            }                                                    \
        }                                                        \
        return result;                                           \
    }
#define MINMAX_OP(ty, op)                           \
    {                                               \
        auto result = EVAL_CHILD(ty, 0);            \
        for (size_t i = 1; i < args.size(); i++) {  \
            result = op(result, EVAL_CHILD(ty, i)); \
        }                                           \
        return ramBitCast(result);                  \
    }
#define MINMAX_NUMERIC(opCode, op)                        \
    case FunctorOp::   opCode: MINMAX_OP(RamSigned  , op) \
    case FunctorOp::U##opCode: MINMAX_OP(RamUnsigned, op) \
    case FunctorOp::F##opCode: MINMAX_OP(RamFloat   , op)

#define UNARY_OP(op, ty, func)                                      \
    case FunctorOp::op: { \
        auto x = EVAL_CHILD(ty, 0); \
        return ramBitCast(func(x)); \
    }
#define CONV_TO_STRING(op, ty)                                                             \
    case FunctorOp::op: return getSymbolTable().lookup(std::to_string(EVAL_CHILD(ty, 0)));
#define CONV_FROM_STRING(op, ty)                              \
    case FunctorOp::op: return evaluator::symbol2numeric<ty>( \
        getSymbolTable().resolve(EVAL_CHILD(RamDomain, 0)));
            // clang-format on

            const auto& args = cur.getArguments();
            switch (cur.getOperator()) {
                /** Unary Functor Operators */
                case FunctorOp::ORD: return execute(shadow.getChild(0), ctxt);
                case FunctorOp::STRLEN:
                    return getSymbolTable().resolve(execute(shadow.getChild(0), ctxt)).size();
                case FunctorOp::NEG: return -execute(shadow.getChild(0), ctxt);
                case FunctorOp::FNEG: {
                    RamDomain result = execute(shadow.getChild(0), ctxt);
                    return ramBitCast(-ramBitCast<RamFloat>(result));
                }
                case FunctorOp::BNOT: return ~execute(shadow.getChild(0), ctxt);
                case FunctorOp::UBNOT: {
                    RamDomain result = execute(shadow.getChild(0), ctxt);
                    return ramBitCast(~ramBitCast<RamUnsigned>(result));
                }
                case FunctorOp::LNOT: return !execute(shadow.getChild(0), ctxt);

                case FunctorOp::ULNOT: {
                    RamDomain result = execute(shadow.getChild(0), ctxt);
                    // Casting is a bit tricky here, since ! returns a boolean.
                    return ramBitCast(static_cast<RamUnsigned>(!ramBitCast<RamUnsigned>(result)));
                }

                    // clang-format off
                /** numeric coersions follow C++ semantics. */
                UNARY_OP(F2I, RamFloat   , static_cast<RamSigned>)
                UNARY_OP(F2U, RamFloat   , static_cast<RamUnsigned>)
                UNARY_OP(I2U, RamSigned  , static_cast<RamUnsigned>)
                UNARY_OP(I2F, RamSigned  , static_cast<RamFloat>)
                UNARY_OP(U2I, RamUnsigned, static_cast<RamSigned>)
                UNARY_OP(U2F, RamUnsigned, static_cast<RamFloat>)

                CONV_TO_STRING(F2S, RamFloat)
                CONV_TO_STRING(I2S, RamSigned)
                CONV_TO_STRING(U2S, RamUnsigned)

                CONV_FROM_STRING(S2F, RamFloat)
                CONV_FROM_STRING(S2I, RamSigned)
                CONV_FROM_STRING(S2U, RamUnsigned)

                /** Binary Functor Operators */
                BINARY_OP_NUMERIC(ADD, +)
                BINARY_OP_NUMERIC(SUB, -)
                BINARY_OP_NUMERIC(MUL, *)
                BINARY_OP_NUMERIC(DIV, /)
                    // clang-format on

                case FunctorOp::EXP: {
                    return std::pow(execute(shadow.getChild(0), ctxt), execute(shadow.getChild(1), ctxt));
                }

                case FunctorOp::UEXP: {
                    auto first = ramBitCast<RamUnsigned>(execute(shadow.getChild(0), ctxt));
                    auto second = ramBitCast<RamUnsigned>(execute(shadow.getChild(1), ctxt));
                    // Extra casting required: pow returns a floating point.
                    return ramBitCast(static_cast<RamUnsigned>(std::pow(first, second)));
                }

                case FunctorOp::FEXP: {
                    auto first = ramBitCast<RamFloat>(execute(shadow.getChild(0), ctxt));
                    auto second = ramBitCast<RamFloat>(execute(shadow.getChild(1), ctxt));
                    return ramBitCast(static_cast<RamFloat>(std::pow(first, second)));
                }

                    // clang-format off
                BINARY_OP_INTEGRAL(MOD, %)
                BINARY_OP_INTEGRAL(BAND, &)
                BINARY_OP_INTEGRAL(BOR, |)
                BINARY_OP_INTEGRAL(BXOR, ^)
                // Handle left-shift as unsigned to match Java semantics of `<<`, namely:
                //  "... `n << s` is `n` left-shifted `s` bit positions; ..."
                // Using `RamSigned` would imply UB due to signed overflow when shifting negatives.
                BINARY_OP_INTEGRAL_SHIFT(BSHIFT_L         , <<, RamUnsigned, RamUnsigned)
                // For right-shift, we do need sign extension.
                BINARY_OP_INTEGRAL_SHIFT(BSHIFT_R         , >>, RamSigned  , RamUnsigned)
                BINARY_OP_INTEGRAL_SHIFT(BSHIFT_R_UNSIGNED, >>, RamUnsigned, RamUnsigned)

                BINARY_OP_LOGICAL(LAND, &&)
                BINARY_OP_LOGICAL(LOR , ||)
                BINARY_OP_LOGICAL(LXOR, + souffle::evaluator::lxor_infix() +)

                MINMAX_NUMERIC(MAX, std::max)
                MINMAX_NUMERIC(MIN, std::min)

                case FunctorOp::SMAX: MINMAX_OP_SYM(<)
                case FunctorOp::SMIN: MINMAX_OP_SYM(>)
                    // clang-format on

                case FunctorOp::CAT: {
                    std::stringstream ss;
                    for (size_t i = 0; i < args.size(); i++) {
                        ss << getSymbolTable().resolve(execute(shadow.getChild(i), ctxt));
                    }
                    return getSymbolTable().lookup(ss.str());
                }
                /** Ternary Functor Operators */
                case FunctorOp::SUBSTR: {
                    auto symbol = execute(shadow.getChild(0), ctxt);
                    const std::string& str = getSymbolTable().resolve(symbol);
                    auto idx = execute(shadow.getChild(1), ctxt);
                    auto len = execute(shadow.getChild(2), ctxt);
                    std::string sub_str;
                    try {
                        sub_str = str.substr(idx, len);
                    } catch (...) {
                        std::cerr << "warning: wrong index position provided by substr(\"";
                        std::cerr << str << "\"," << (int32_t)idx << "," << (int32_t)len << ") functor.\n";
                    }
                    return getSymbolTable().lookup(sub_str);
                }

                case FunctorOp::RANGE:
                case FunctorOp::URANGE:
                case FunctorOp::FRANGE:
                    fatal("ICE: functor `%s` must map onto `RamNestedIntrinsicOperator`", cur.getOperator());
            }

            { UNREACHABLE_BAD_CASE_ANALYSIS }

#undef BINARY_OP_LOGICAL
#undef BINARY_OP_INTEGRAL
#undef BINARY_OP_NUMERIC
#undef BINARY_OP_SHIFT_MASK
#undef BINARY_OP_INTEGRAL_SHIFT
#undef MINMAX_OP_SYM
#undef MINMAX_OP
#undef MINMAX_NUMERIC
#undef UNARY_OP
#undef CONV_TO_STRING
#undef CONV_FROM_STRING
        ESAC(IntrinsicOperator)

        CASE(NestedIntrinsicOperator)
            auto numArgs = cur.getArguments().size();
            auto runNested = [&](auto&& tuple) {
                ctxt[cur.getTupleId()] = tuple.data;
                execute(shadow.getChild(numArgs), ctxt);
            };

#define RUN_RANGE(ty)                                                                                     \
    numArgs == 3                                                                                          \
            ? evaluator::runRange<ty>(EVAL_CHILD(ty, 0), EVAL_CHILD(ty, 1), EVAL_CHILD(ty, 2), runNested) \
            : evaluator::runRange<ty>(EVAL_CHILD(ty, 0), EVAL_CHILD(ty, 1), runNested),                   \
            true

            switch (cur.getFunction()) {
                case RamNestedIntrinsicOp::RANGE: return RUN_RANGE(RamSigned);
                case RamNestedIntrinsicOp::URANGE: return RUN_RANGE(RamUnsigned);
                case RamNestedIntrinsicOp::FRANGE: return RUN_RANGE(RamFloat);
            }

            { UNREACHABLE_BAD_CASE_ANALYSIS }
#undef RUN_RANGE
        ESAC(NestedIntrinsicOperator)

        CASE(UserDefinedOperator)
            // get name and type
            const std::string& name = cur.getName();
            const std::vector<TypeAttribute>& type = cur.getArgsTypes();

            auto fn = reinterpret_cast<void (*)()>(getMethodHandle(name));
            if (fn == nullptr) fatal("cannot find user-defined operator `%s`", name);

            // prepare dynamic call environment
            size_t arity = cur.getArguments().size();
            ffi_cif cif;
            ffi_type* args[arity];
            void* values[arity];
            RamDomain intVal[arity];
            RamUnsigned uintVal[arity];
            RamFloat floatVal[arity];
            const char* strVal[arity];
            ffi_arg rc;

            /* Initialize arguments for ffi-call */
            for (size_t i = 0; i < arity; i++) {
                RamDomain arg = execute(shadow.getChild(i), ctxt);
                switch (type[i]) {
                    case TypeAttribute::Symbol:
                        args[i] = &FFI_Symbol;
                        strVal[i] = getSymbolTable().resolve(arg).c_str();
                        values[i] = &strVal[i];
                        break;
                    case TypeAttribute::Signed:
                        args[i] = &FFI_RamSigned;
                        intVal[i] = arg;
                        values[i] = &intVal[i];
                        break;
                    case TypeAttribute::Unsigned:
                        args[i] = &FFI_RamUnsigned;
                        uintVal[i] = ramBitCast<RamUnsigned>(arg);
                        values[i] = &uintVal[i];
                        break;
                    case TypeAttribute::Float:
                        args[i] = &FFI_RamFloat;
                        floatVal[i] = ramBitCast<RamFloat>(arg);
                        values[i] = &floatVal[i];
                        break;
                    case TypeAttribute::Record: fatal("Record support is not implemented");
                }
            }

            // Get codomain.
            auto codomain = &FFI_RamSigned;
            switch (cur.getReturnType()) {
                // initialize for string value.
                case TypeAttribute::Symbol: codomain = &FFI_Symbol; break;
                case TypeAttribute::Signed: codomain = &FFI_RamSigned; break;
                case TypeAttribute::Unsigned: codomain = &FFI_RamUnsigned; break;
                case TypeAttribute::Float: codomain = &FFI_RamFloat; break;
                case TypeAttribute::Record: fatal("Not implemented");
            }

            // Call the external function.
            const auto prepStatus = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, arity, codomain, args);
            if (prepStatus != FFI_OK) {
                fatal("Failed to prepare CIF for user-defined operator `%s`; error code = %d", name,
                        prepStatus);
            }
            ffi_call(&cif, fn, &rc, values);

            switch (cur.getReturnType()) {
                case TypeAttribute::Signed: return static_cast<RamDomain>(rc);
                case TypeAttribute::Symbol: return getSymbolTable().lookup(reinterpret_cast<const char*>(rc));

                case TypeAttribute::Unsigned: return ramBitCast(static_cast<RamUnsigned>(rc));
                case TypeAttribute::Float: return ramBitCast(static_cast<RamFloat>(rc));
                case TypeAttribute::Record: fatal("Not implemented");
            }
            fatal("Unsupported user defined operator");

        ESAC(UserDefinedOperator)

        CASE(PackRecord)
            auto values = cur.getArguments();
            size_t arity = values.size();
            RamDomain data[arity];
            for (size_t i = 0; i < arity; ++i) {
                data[i] = execute(shadow.getChild(i), ctxt);
            }
            return getRecordTable().pack(data, arity);
        ESAC(PackRecord)

        CASE(SubroutineArgument)
            return ctxt.getArgument(cur.getArgument());
        ESAC(SubroutineArgument)

        CASE(True)
            return true;
        ESAC(True)

        CASE(False)
            return false;
        ESAC(False)

        CASE(Conjunction)
            return execute(shadow.getLhs(), ctxt) && execute(shadow.getRhs(), ctxt);
        ESAC(Conjunction)

        CASE(Negation)
            return !execute(shadow.getChild(), ctxt);
        ESAC(Negation)

        CASE(EmptinessCheck)
            return node->getRelation()->empty();
        ESAC(EmptinessCheck)

        CASE(ExistenceCheck)
            // construct the pattern tuple
            size_t arity = cur.getRelation().getArity();

            size_t viewPos = shadow.getViewId();

            if (profileEnabled && !cur.getRelation().isTemp()) {
                reads[cur.getRelation().getName()]++;
            }

            const auto& superInfo = shadow.getSuperInst();
            // for total we use the exists test
            if (shadow.isTotalSearch()) {
                RamDomain tuple[arity];
                memcpy(tuple, superInfo.first.data(), sizeof(tuple));
                /* TupleElement */
                for (const auto& tupleElement : superInfo.tupleFirst) {
                    tuple[tupleElement[0]] = ctxt[tupleElement[1]][tupleElement[2]];
                }
                /* Generic */
                for (const auto& expr : superInfo.exprFirst) {
                    tuple[expr.first] = execute(expr.second.get(), ctxt);
                }
                return ctxt.getView(viewPos)->contains(TupleRef(tuple, arity));
            }

            // for partial we search for lower and upper boundaries
            RamDomain low[arity];
            RamDomain high[arity];
            memcpy(low, superInfo.first.data(), sizeof(low));
            memcpy(high, superInfo.second.data(), sizeof(high));

            /* TupleElement */
            for (const auto& tupleElement : superInfo.tupleFirst) {
                low[tupleElement[0]] = ctxt[tupleElement[1]][tupleElement[2]];
                high[tupleElement[0]] = low[tupleElement[0]];
            }
            /* Generic */
            for (const auto& expr : superInfo.exprFirst) {
                low[expr.first] = execute(expr.second.get(), ctxt);
                high[expr.first] = low[expr.first];
            }

            return ctxt.getView(viewPos)->contains(TupleRef(low, arity), TupleRef(high, arity));
        ESAC(ExistenceCheck)

        CASE(ProvenanceExistenceCheck)
            // construct the pattern tuple
            const auto& superInfo = shadow.getSuperInst();
            size_t arity = cur.getRelation().getArity();

            // for partial we search for lower and upper boundaries
            RamDomain low[arity];
            RamDomain high[arity];
            memcpy(low, superInfo.first.data(), sizeof(low));
            memcpy(high, superInfo.second.data(), sizeof(high));

            /* TupleElement */
            for (const auto& tupleElement : superInfo.tupleFirst) {
                low[tupleElement[0]] = ctxt[tupleElement[1]][tupleElement[2]];
                high[tupleElement[0]] = low[tupleElement[0]];
            }
            /* Generic */
            for (const auto& expr : superInfo.exprFirst) {
                assert(expr.second.get() != nullptr &&
                        "ProvenanceExistenceCheck should always be specified for payload");
                low[expr.first] = execute(expr.second.get(), ctxt);
                high[expr.first] = low[expr.first];
            }

            low[arity - 2] = MIN_RAM_SIGNED;
            low[arity - 1] = MIN_RAM_SIGNED;
            high[arity - 2] = MAX_RAM_SIGNED;
            high[arity - 1] = MAX_RAM_SIGNED;

            // obtain view
            size_t viewPos = shadow.getViewId();

            // get an equalRange
            auto equalRange = ctxt.getView(viewPos)->range(TupleRef(low, arity), TupleRef(high, arity));

            // if range is empty
            if (equalRange.begin() == equalRange.end()) {
                return false;
            }

            // check whether the height is less than the current height
            return (*equalRange.begin())[arity - 1] <= execute(shadow.getChild(), ctxt);
        ESAC(ProvenanceExistenceCheck)

        CASE(Constraint)
        // clang-format off
#define COMPARE_NUMERIC(ty, op) return EVAL_LEFT(ty) op EVAL_RIGHT(ty)
#define COMPARE_STRING(op)                                        \
    return (getSymbolTable().resolve(EVAL_LEFT(RamDomain)) op \
            getSymbolTable().resolve(EVAL_RIGHT(RamDomain)))
#define COMPARE_EQ_NE(opCode, op)                                         \
    case BinaryConstraintOp::   opCode: COMPARE_NUMERIC(RamDomain  , op); \
    case BinaryConstraintOp::F##opCode: COMPARE_NUMERIC(RamFloat   , op);
#define COMPARE(opCode, op)                                               \
    case BinaryConstraintOp::   opCode: COMPARE_NUMERIC(RamSigned  , op); \
    case BinaryConstraintOp::U##opCode: COMPARE_NUMERIC(RamUnsigned, op); \
    case BinaryConstraintOp::F##opCode: COMPARE_NUMERIC(RamFloat   , op); \
    case BinaryConstraintOp::S##opCode: COMPARE_STRING(op);
            // clang-format on

            switch (cur.getOperator()) {
                COMPARE_EQ_NE(EQ, ==)
                COMPARE_EQ_NE(NE, !=)

                COMPARE(LT, <)
                COMPARE(LE, <=)
                COMPARE(GT, >)
                COMPARE(GE, >=)

                case BinaryConstraintOp::MATCH: {
                    RamDomain left = execute(shadow.getLhs(), ctxt);
                    RamDomain right = execute(shadow.getRhs(), ctxt);
                    const std::string& pattern = getSymbolTable().resolve(left);
                    const std::string& text = getSymbolTable().resolve(right);
                    bool result = false;
                    try {
                        result = std::regex_match(text, std::regex(pattern));
                    } catch (...) {
                        std::cerr << "warning: wrong pattern provided for match(\"" << pattern << "\",\""
                                  << text << "\").\n";
                    }
                    return result;
                }
                case BinaryConstraintOp::NOT_MATCH: {
                    RamDomain left = execute(shadow.getLhs(), ctxt);
                    RamDomain right = execute(shadow.getRhs(), ctxt);
                    const std::string& pattern = getSymbolTable().resolve(left);
                    const std::string& text = getSymbolTable().resolve(right);
                    bool result = false;
                    try {
                        result = !std::regex_match(text, std::regex(pattern));
                    } catch (...) {
                        std::cerr << "warning: wrong pattern provided for !match(\"" << pattern << "\",\""
                                  << text << "\").\n";
                    }
                    return result;
                }
                case BinaryConstraintOp::CONTAINS: {
                    RamDomain left = execute(shadow.getLhs(), ctxt);
                    RamDomain right = execute(shadow.getRhs(), ctxt);
                    const std::string& pattern = getSymbolTable().resolve(left);
                    const std::string& text = getSymbolTable().resolve(right);
                    return text.find(pattern) != std::string::npos;
                }
                case BinaryConstraintOp::NOT_CONTAINS: {
                    RamDomain left = execute(shadow.getLhs(), ctxt);
                    RamDomain right = execute(shadow.getRhs(), ctxt);
                    const std::string& pattern = getSymbolTable().resolve(left);
                    const std::string& text = getSymbolTable().resolve(right);
                    return text.find(pattern) == std::string::npos;
                }
            }

            { UNREACHABLE_BAD_CASE_ANALYSIS }

#undef COMPARE_NUMERIC
#undef COMPARE_STRING
#undef COMPARE
#undef COMPARE_EQ_NE
        ESAC(Constraint)

        CASE(TupleOperation)
            bool result = execute(shadow.getChild(), ctxt);

            if (profileEnabled && !cur.getProfileText().empty()) {
                auto& currentFrequencies = frequencies[cur.getProfileText()];
                while (currentFrequencies.size() <= getIterationNumber()) {
                    currentFrequencies.emplace_back(0);
                }
                frequencies[cur.getProfileText()][getIterationNumber()]++;
            }
            return result;
        ESAC(TupleOperation)

        CASE(Scan)
            // get the targeted relation
            auto& rel = *node->getRelation();

            // use simple iterator
            for (const RamDomain* tuple : rel) {
                ctxt[cur.getTupleId()] = tuple;
                if (!execute(shadow.getNestedOperation(), ctxt)) {
                    break;
                }
            }
            return true;
        ESAC(Scan)

        CASE(ParallelScan)
            auto preamble = shadow.getPreamble();
            auto& rel = *node->getRelation();

            auto pStream = rel.partitionScan(numOfThreads);

            PARALLEL_START
                InterpreterContext newCtxt(ctxt);
                auto viewInfo = preamble->getViewInfoForNested();
                for (const auto& info : viewInfo) {
                    newCtxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
                }
                pfor(auto it = pStream.begin(); it < pStream.end(); it++) {
                    for (const TupleRef& val : *it) {
                        newCtxt[cur.getTupleId()] = val.getBase();
                        if (!execute(shadow.getNestedOperation(), newCtxt)) {
                            break;
                        }
                    }
                }
            PARALLEL_END
            return true;
        ESAC(ParallelScan)

        CASE(IndexScan)
            // create pattern tuple for range query
            size_t arity = cur.getRelation().getArity();
            const auto& superInfo = shadow.getSuperInst();
            RamDomain low[arity];
            RamDomain high[arity];
            CAL_SEARCH_BOUND(superInfo, low, high);

            size_t viewId = shadow.getViewId();
            auto& view = ctxt.getView(viewId);
            // conduct range query
            for (auto data : view->range(TupleRef(low, arity), TupleRef(high, arity))) {
                ctxt[cur.getTupleId()] = &data[0];
                if (!execute(shadow.getNestedOperation(), ctxt)) {
                    break;
                }
            }
            return true;
        ESAC(IndexScan)

        CASE(ParallelIndexScan)
            auto preamble = shadow.getPreamble();
            auto& rel = *node->getRelation();

            // create pattern tuple for range query
            size_t arity = rel.getArity();
            const auto& superInfo = shadow.getSuperInst();
            RamDomain low[arity];
            RamDomain high[arity];
            CAL_SEARCH_BOUND(superInfo, low, high);

            size_t indexPos = shadow.getViewId();
            auto pStream =
                    rel.partitionRange(indexPos, TupleRef(low, arity), TupleRef(high, arity), numOfThreads);

            PARALLEL_START
                InterpreterContext newCtxt(ctxt);
                auto viewInfo = preamble->getViewInfoForNested();
                for (const auto& info : viewInfo) {
                    newCtxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
                }
                pfor(auto it = pStream.begin(); it < pStream.end(); it++) {
                    for (const TupleRef& val : *it) {
                        newCtxt[cur.getTupleId()] = val.getBase();
                        if (!execute(shadow.getNestedOperation(), newCtxt)) {
                            break;
                        }
                    }
                }
            PARALLEL_END

            return true;
        ESAC(ParallelIndexScan)

        CASE(Choice)
            // get the targeted relation
            auto& rel = *node->getRelation();

            // use simple iterator
            for (const RamDomain* tuple : rel) {
                ctxt[cur.getTupleId()] = tuple;
                if (execute(shadow.getCondition(), ctxt)) {
                    execute(shadow.getNestedOperation(), ctxt);
                    break;
                }
            }
            return true;
        ESAC(Choice)

        CASE(ParallelChoice)
            auto preamble = shadow.getPreamble();
            auto& rel = *node->getRelation();

            auto pStream = rel.partitionScan(numOfThreads);
            auto viewInfo = preamble->getViewInfoForNested();
            PARALLEL_START
                InterpreterContext newCtxt(ctxt);
                for (const auto& info : viewInfo) {
                    newCtxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
                }
                pfor(auto it = pStream.begin(); it < pStream.end(); it++) {
                    for (const TupleRef& val : *it) {
                        newCtxt[cur.getTupleId()] = val.getBase();
                        if (execute(shadow.getCondition(), newCtxt)) {
                            execute(shadow.getNestedOperation(), newCtxt);
                            break;
                        }
                    }
                }
            PARALLEL_END
            return true;
        ESAC(ParallelChoice)

        CASE(IndexChoice)
            // create pattern tuple for range query
            size_t arity = cur.getRelation().getArity();
            const auto& superInfo = shadow.getSuperInst();
            RamDomain low[arity];
            RamDomain high[arity];
            CAL_SEARCH_BOUND(superInfo, low, high);

            size_t viewId = shadow.getViewId();
            auto& view = ctxt.getView(viewId);

            for (auto ip : view->range(TupleRef(low, arity), TupleRef(high, arity))) {
                const RamDomain* data = &ip[0];
                ctxt[cur.getTupleId()] = data;
                if (execute(shadow.getCondition(), ctxt)) {
                    execute(shadow.getNestedOperation(), ctxt);
                    break;
                }
            }
            return true;
        ESAC(IndexChoice)

        CASE(ParallelIndexChoice)
            auto preamble = shadow.getPreamble();
            auto& rel = *node->getRelation();

            auto viewInfo = preamble->getViewInfoForNested();

            // create pattern tuple for range query
            size_t arity = rel.getArity();
            const auto& superInfo = shadow.getSuperInst();
            RamDomain low[arity];
            RamDomain high[arity];
            CAL_SEARCH_BOUND(superInfo, low, high);

            size_t indexPos = shadow.getViewId();
            auto pStream =
                    rel.partitionRange(indexPos, TupleRef(low, arity), TupleRef(high, arity), numOfThreads);

            PARALLEL_START
                InterpreterContext newCtxt(ctxt);
                for (const auto& info : viewInfo) {
                    newCtxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
                }
                pfor(auto it = pStream.begin(); it < pStream.end(); it++) {
                    for (const TupleRef& val : *it) {
                        newCtxt[cur.getTupleId()] = val.getBase();
                        if (execute(shadow.getCondition(), newCtxt)) {
                            execute(shadow.getNestedOperation(), newCtxt);
                            break;
                        }
                    }
                }
            PARALLEL_END

            return true;
        ESAC(ParallelIndexChoice)

        CASE(UnpackRecord)
            RamDomain ref = execute(shadow.getExpr(), ctxt);

            // check for nil
            if (ref == 0) {
                return true;
            }

            // update environment variable
            size_t arity = cur.getArity();
            const RamDomain* tuple = getRecordTable().unpack(ref, arity);

            // save reference to temporary value
            ctxt[cur.getTupleId()] = tuple;

            // run nested part - using base class visitor
            return execute(shadow.getNestedOperation(), ctxt);
        ESAC(UnpackRecord)

        CASE(ParallelAggregate)
            // TODO (rdowavic): make parallel
            auto preamble = shadow.getPreamble();

            InterpreterContext newCtxt(ctxt);
            auto viewInfo = preamble->getViewInfoForNested();
            for (const auto& info : viewInfo) {
                newCtxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
            }
            return executeAggregate(newCtxt, cur, *shadow.getCondition(), shadow.getExpr(),
                    *shadow.getNestedOperation(), node->getRelation()->scan());
        ESAC(ParallelAggregate)

        CASE(Aggregate)
            return executeAggregate(ctxt, cur, *shadow.getCondition(), shadow.getExpr(),
                    *shadow.getNestedOperation(), node->getRelation()->scan());
        ESAC(Aggregate)

        CASE(ParallelIndexAggregate)
            // TODO (rdowavic): make parallel
            auto preamble = shadow.getPreamble();

            InterpreterContext newCtxt(ctxt);
            auto viewInfo = preamble->getViewInfoForNested();
            for (const auto& info : viewInfo) {
                newCtxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
            }
            // init temporary tuple for this level
            size_t arity = cur.getRelation().getArity();
            const auto& superInfo = shadow.getSuperInst();
            // get lower and upper boundaries for iteration
            RamDomain low[arity];
            RamDomain high[arity];
            CAL_SEARCH_BOUND(superInfo, low, high);

            size_t viewId = shadow.getViewId();
            auto& view = newCtxt.getView(viewId);

            return executeAggregate(newCtxt, cur, *shadow.getCondition(), shadow.getExpr(),
                    *shadow.getNestedOperation(), view->range(TupleRef(low, arity), TupleRef(high, arity)));
        ESAC(ParallelIndexAggregate)

        CASE(IndexAggregate)
            // init temporary tuple for this level
            size_t arity = cur.getRelation().getArity();
            const auto& superInfo = shadow.getSuperInst();
            RamDomain low[arity];
            RamDomain high[arity];
            CAL_SEARCH_BOUND(superInfo, low, high)

            size_t viewId = shadow.getViewId();
            auto& view = ctxt.getView(viewId);

            return executeAggregate(ctxt, cur, *shadow.getCondition(), shadow.getExpr(),
                    *shadow.getNestedOperation(), view->range(TupleRef(low, arity), TupleRef(high, arity)));
        ESAC(IndexAggregate)

        CASE(Break)
            // check condition
            if (execute(shadow.getCondition(), ctxt)) {
                return false;
            }
            return execute(shadow.getNestedOperation(), ctxt);
        ESAC(Break)

        CASE(Filter)
            bool result = true;
            // check condition
            if (execute(shadow.getCondition(), ctxt)) {
                // process nested
                result = execute(shadow.getNestedOperation(), ctxt);
            }

            if (profileEnabled && !cur.getProfileText().empty()) {
                auto& currentFrequencies = frequencies[cur.getProfileText()];
                while (currentFrequencies.size() <= getIterationNumber()) {
                    currentFrequencies.emplace_back(0);
                }
                frequencies[cur.getProfileText()][getIterationNumber()]++;
            }
            return result;
        ESAC(Filter)

        CASE(Project)
            const auto& superInfo = shadow.getSuperInst();
            size_t arity = cur.getRelation().getArity();
            RamDomain tuple[arity];
            memcpy(tuple, superInfo.first.data(), sizeof(tuple));
            /* TupleElement */
            for (const auto& tupleElement : superInfo.tupleFirst) {
                tuple[tupleElement[0]] = ctxt[tupleElement[1]][tupleElement[2]];
            }
            /* Generic */
            for (const auto& expr : superInfo.exprFirst) {
                tuple[expr.first] = execute(expr.second.get(), ctxt);
            }

            // insert in target relation
            InterpreterRelation& rel = *node->getRelation();
            rel.insert(tuple);
            return true;
        ESAC(Project)

        CASE(SubroutineReturn)
            for (size_t i = 0; i < cur.getValues().size(); ++i) {
                if (shadow.getChild(i) == nullptr) {
                    ctxt.addReturnValue(0);
                } else {
                    ctxt.addReturnValue(execute(shadow.getChild(i), ctxt));
                }
            }
            return true;
        ESAC(SubroutineReturn)

        CASE(Sequence)
            for (const auto& child : shadow.getChildren()) {
                if (!execute(child.get(), ctxt)) {
                    return false;
                }
            }
            return true;
        ESAC(Sequence)

        CASE(Parallel)
            for (const auto& child : shadow.getChildren()) {
                if (!execute(child.get(), ctxt)) {
                    return false;
                }
            }
            return true;
        ESAC(Parallel)

        CASE(Loop)
            resetIterationNumber();
            while (execute(shadow.getChild(), ctxt)) {
                incIterationNumber();
            }
            resetIterationNumber();
            return true;
        ESAC(Loop)

        CASE(Exit)
            return !execute(shadow.getChild(), ctxt);
        ESAC(Exit)

        CASE(LogRelationTimer)
            Logger logger(cur.getMessage(), getIterationNumber(),
                    std::bind(&InterpreterRelation::size, node->getRelation()));
            return execute(shadow.getChild(), ctxt);
        ESAC(LogRelationTimer)

        CASE(LogTimer)
            Logger logger(cur.getMessage(), getIterationNumber());
            return execute(shadow.getChild(), ctxt);
        ESAC(LogTimer)

        CASE(DebugInfo)
            SignalHandler::instance()->setMsg(cur.getMessage().c_str());
            return execute(shadow.getChild(), ctxt);
        ESAC(DebugInfo)

        CASE(Clear)
            node->getRelation()->purge();
            return true;
        ESAC(Clear)

        CASE(Call)
            execute(subroutine[shadow.getSubroutineId()].get(), ctxt);
            return true;
        ESAC(Call)

        CASE(LogSize)
            const InterpreterRelation& rel = *node->getRelation();
            ProfileEventSingleton::instance().makeQuantityEvent(
                    cur.getMessage(), rel.size(), getIterationNumber());
            return true;
        ESAC(LogSize)

        CASE(IO)

            const auto& directive = cur.getDirectives();
            const std::string& op = cur.get("operation");

            if (op == "input") {
                try {
                    InterpreterRelation& relation = *node->getRelation();
                    IOSystem::getInstance()
                            .getReader(directive, getSymbolTable(), getRecordTable())
                            ->readAll(relation);
                } catch (std::exception& e) {
                    std::cerr << "Error loading data: " << e.what() << "\n";
                }
                return true;
            } else if (op == "output" || op == "printsize") {
                try {
                    IOSystem::getInstance()
                            .getWriter(directive, getSymbolTable(), getRecordTable())
                            ->writeAll(*node->getRelation());
                } catch (std::exception& e) {
                    std::cerr << e.what();
                    exit(EXIT_FAILURE);
                }
                return true;
            } else {
                assert("wrong i/o operation");
                return true;
            }
        ESAC(IO)

        CASE(Query)
            InterpreterPreamble* preamble = shadow.getPreamble();

            // Execute view-free operations in outer filter if any.
            auto& viewFreeOps = preamble->getOuterFilterViewFreeOps();
            for (auto& op : viewFreeOps) {
                if (!execute(op.get(), ctxt)) {
                    return true;
                }
            }

            // Create Views for outer filter operation if any.
            auto& viewsForOuter = preamble->getViewInfoForFilter();
            for (auto& info : viewsForOuter) {
                ctxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
            }

            // Execute outer filter operation.
            auto& viewOps = preamble->getOuterFilterViewOps();
            for (auto& op : viewOps) {
                if (!execute(op.get(), ctxt)) {
                    return true;
                }
            }

            if (preamble->isParallel) {
                // If Parallel is true, holds views creation unitl parallel instructions.
            } else {
                // Issue views for nested operation.
                auto& viewsForNested = preamble->getViewInfoForNested();
                for (auto& info : viewsForNested) {
                    ctxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
                }
            }
            execute(shadow.getChild(), ctxt);
            return true;
        ESAC(Query)

        CASE(Extend)
            InterpreterRelation& src = *getRelationHandle(shadow.getSourceId());
            InterpreterRelation& trg = *getRelationHandle(shadow.getTargetId());
            src.extend(trg);
            trg.insert(src);
            return true;
        ESAC(Extend)

        CASE(Swap)
            swapRelation(shadow.getSourceId(), shadow.getTargetId());
            return true;
        ESAC(Swap)
    }

    UNREACHABLE_BAD_CASE_ANALYSIS

#undef EVAL_CHILD
#undef DEBUG
}  // namespace souffle

template <typename Aggregate>
RamDomain InterpreterEngine::executeAggregate(InterpreterContext& ctxt, const Aggregate& aggregate,
        const InterpreterNode& filter, const InterpreterNode* expression,
        const InterpreterNode& nestedOperation, Stream stream) {
    bool shouldRunNested = false;

    // initialize result
    RamDomain res = 0;

    // Use for calculating mean.
    std::pair<RamFloat, RamFloat> accumulateMean;

    switch (aggregate.getFunction()) {
        case AggregateOp::MIN: res = ramBitCast(MAX_RAM_SIGNED); break;
        case AggregateOp::UMIN: res = ramBitCast(MAX_RAM_UNSIGNED); break;
        case AggregateOp::FMIN: res = ramBitCast(MAX_RAM_FLOAT); break;

        case AggregateOp::MAX: res = ramBitCast(MIN_RAM_SIGNED); break;
        case AggregateOp::UMAX: res = ramBitCast(MIN_RAM_UNSIGNED); break;
        case AggregateOp::FMAX: res = ramBitCast(MIN_RAM_FLOAT); break;

        case AggregateOp::SUM:
            res = ramBitCast(static_cast<RamSigned>(0));
            shouldRunNested = true;
            break;
        case AggregateOp::USUM:
            res = ramBitCast(static_cast<RamUnsigned>(0));
            shouldRunNested = true;
            break;
        case AggregateOp::FSUM:
            res = ramBitCast(static_cast<RamFloat>(0));
            shouldRunNested = true;
            break;

        case AggregateOp::MEAN:
            res = 0;
            accumulateMean = {0, 0};
            break;

        case AggregateOp::COUNT:
            res = 0;
            shouldRunNested = true;
            break;
    }

    for (auto ip : stream) {
        const RamDomain* data = &ip[0];
        ctxt[aggregate.getTupleId()] = data;

        if (!execute(&filter, ctxt)) {
            continue;
        }

        shouldRunNested = true;

        // count is a special case.
        if (aggregate.getFunction() == AggregateOp::COUNT) {
            ++res;
            continue;
        }

        // eval target expression
        assert(expression);  // only case where this is null is `COUNT`
        RamDomain val = execute(expression, ctxt);

        switch (aggregate.getFunction()) {
            case AggregateOp::MIN: res = std::min(res, val); break;
            case AggregateOp::FMIN:
                res = ramBitCast(std::min(ramBitCast<RamFloat>(res), ramBitCast<RamFloat>(val)));
                break;
            case AggregateOp::UMIN:
                res = ramBitCast(std::min(ramBitCast<RamUnsigned>(res), ramBitCast<RamUnsigned>(val)));
                break;

            case AggregateOp::MAX: res = std::max(res, val); break;
            case AggregateOp::FMAX:
                res = ramBitCast(std::max(ramBitCast<RamFloat>(res), ramBitCast<RamFloat>(val)));
                break;
            case AggregateOp::UMAX:
                res = ramBitCast(std::max(ramBitCast<RamUnsigned>(res), ramBitCast<RamUnsigned>(val)));
                break;

            case AggregateOp::SUM: res += val; break;
            case AggregateOp::FSUM:
                res = ramBitCast(ramBitCast<RamFloat>(res) + ramBitCast<RamFloat>(val));
                break;
            case AggregateOp::USUM:
                res = ramBitCast(ramBitCast<RamUnsigned>(res) + ramBitCast<RamUnsigned>(val));
                break;

            case AggregateOp::MEAN:
                accumulateMean.first += ramBitCast<RamFloat>(val);
                accumulateMean.second++;
                break;

            case AggregateOp::COUNT: fatal("This should never be executed");
        }
    }

    if (aggregate.getFunction() == AggregateOp::MEAN && accumulateMean.second != 0) {
        res = ramBitCast(accumulateMean.first / accumulateMean.second);
    }

    // write result to environment
    RamDomain tuple[1];
    tuple[0] = res;
    ctxt[aggregate.getTupleId()] = tuple;

    if (!shouldRunNested) {
        return true;
    } else {
        return execute(&nestedOperation, ctxt);
    }
}

}  // namespace souffle
