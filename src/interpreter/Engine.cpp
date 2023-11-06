/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Engine.cpp
 *
 * Define the Interpreter Engine class.
 ***********************************************************************/

#include "interpreter/Engine.h"
#include "AggregateOp.h"
#include "FunctorOps.h"
#include "Global.h"
#include "interpreter/Context.h"
#include "interpreter/Index.h"
#include "interpreter/Node.h"
#include "interpreter/Relation.h"
#include "interpreter/ViewContext.h"
#include "ram/Aggregate.h"
#include "ram/Aggregator.h"
#include "ram/Assign.h"
#include "ram/AutoIncrement.h"
#include "ram/Break.h"
#include "ram/Call.h"
#include "ram/Clear.h"
#include "ram/Conjunction.h"
#include "ram/Constraint.h"
#include "ram/DebugInfo.h"
#include "ram/EmptinessCheck.h"
#include "ram/Erase.h"
#include "ram/EstimateJoinSize.h"
#include "ram/ExistenceCheck.h"
#include "ram/Exit.h"
#include "ram/False.h"
#include "ram/Filter.h"
#include "ram/IO.h"
#include "ram/IfExists.h"
#include "ram/IndexAggregate.h"
#include "ram/IndexIfExists.h"
#include "ram/IndexScan.h"
#include "ram/Insert.h"
#include "ram/IntrinsicAggregator.h"
#include "ram/IntrinsicOperator.h"
#include "ram/LogRelationTimer.h"
#include "ram/LogSize.h"
#include "ram/LogTimer.h"
#include "ram/Loop.h"
#include "ram/MergeExtend.h"
#include "ram/Negation.h"
#include "ram/NestedIntrinsicOperator.h"
#include "ram/NumericConstant.h"
#include "ram/PackRecord.h"
#include "ram/Parallel.h"
#include "ram/ParallelAggregate.h"
#include "ram/ParallelIfExists.h"
#include "ram/ParallelIndexAggregate.h"
#include "ram/ParallelIndexIfExists.h"
#include "ram/ParallelIndexScan.h"
#include "ram/ParallelScan.h"
#include "ram/Program.h"
#include "ram/ProvenanceExistenceCheck.h"
#include "ram/Query.h"
#include "ram/Relation.h"
#include "ram/RelationSize.h"
#include "ram/Scan.h"
#include "ram/Sequence.h"
#include "ram/Statement.h"
#include "ram/StringConstant.h"
#include "ram/SubroutineArgument.h"
#include "ram/SubroutineReturn.h"
#include "ram/Swap.h"
#include "ram/TranslationUnit.h"
#include "ram/True.h"
#include "ram/TupleElement.h"
#include "ram/TupleOperation.h"
#include "ram/UnpackRecord.h"
#include "ram/UserDefinedAggregator.h"
#include "ram/UserDefinedOperator.h"
#include "ram/Variable.h"
#include "ram/utility/Visitor.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/RamTypes.h"
#include "souffle/RecordTable.h"
#include "souffle/SignalHandler.h"
#include "souffle/SymbolTable.h"
#include "souffle/TypeAttribute.h"
#include "souffle/datastructure/RecordTableImpl.h"
#include "souffle/datastructure/SymbolTableImpl.h"
#include "souffle/io/IOSystem.h"
#include "souffle/io/ReadStream.h"
#include "souffle/io/WriteStream.h"
#include "souffle/profile/Logger.h"
#include "souffle/profile/ProfileEvent.h"
#include "souffle/utility/EvaluatorUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/ParallelUtil.h"
#include "souffle/utility/StringUtil.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _MSC_VER
#define dlopen(libname, flags) LoadLibrary((libname))
#define dlsym(lib, fn) GetProcAddress(static_cast<HMODULE>(lib), (fn))
#else
#include <dlfcn.h>
#endif

#ifdef USE_LIBFFI
#include <ffi.h>
#endif

namespace souffle::interpreter {

// Handle difference in dynamic libraries suffixes.
#ifdef __APPLE__
#define dynamicLibSuffix ".dylib";
#else
#ifdef _MSC_VER
#define dynamicLibSuffix ".dll";
#else
#define dynamicLibSuffix ".so";
#endif
#endif

namespace {
constexpr RamDomain RAM_BIT_SHIFT_MASK = RAM_DOMAIN_SIZE - 1;

#ifdef _OPENMP
std::size_t number_of_threads(const std::size_t user_specified) {
    if (user_specified > 0) {
        omp_set_num_threads(static_cast<int>(user_specified));
        return user_specified;
    } else {
        return omp_get_max_threads();
    }
}
#else
std::size_t number_of_threads(const std::size_t) {
    return 1;
}
#endif

/** Construct an arguments tuple for a stateful functor call. */
template <std::size_t Arity, std::size_t... Is>
constexpr auto statefulCallTuple(souffle::SymbolTable* symbolTable, souffle::RecordTable* recordTable,
        std::array<RamDomain, Arity>& args, std::index_sequence<Is...>) {
    return std::make_tuple(symbolTable, recordTable, args[Is]...);
}

/** Call the given function with the arguments from the tuple. */
template <typename RetT, typename... Ts>
RetT callWithTuple(void (*userFunctor)(), const std::tuple<Ts...>& args) {
    using FunType = std::function<RetT(Ts...)>;
    FunType Fn(reinterpret_cast<RetT (*)(Ts...)>(userFunctor));
    return std::apply(Fn, args);
}

/** Call a stateful functor. */
template <std::size_t Arity, typename ExecuteFn, typename Shadow>
RamDomain callStateful(ExecuteFn&& execute, Context& ctxt, Shadow& shadow, void (*userFunctor)(),
        souffle::SymbolTable* symbolTable, souffle::RecordTable* recordTable) {
    std::array<RamDomain, Arity> args;

    if constexpr (Arity > 0) {
        for (std::size_t i = 0; i < Arity; ++i) {
            args[i] = execute(shadow.getChild(i), ctxt);
        }
    }

    auto argsTuple = statefulCallTuple(symbolTable, recordTable, args, std::make_index_sequence<Arity>{});
    return callWithTuple<RamDomain>(userFunctor, argsTuple);
}

/** Call a stateful aggregate functor. */
template <typename AnyFunctor>
RamDomain callStatefulAggregate(AnyFunctor&& userFunctor, souffle::SymbolTable* symbolTable,
        souffle::RecordTable* recordTable, souffle::RamDomain arg1, souffle::RamDomain arg2) {
    std::array<RamDomain, 2> args;
    args[0] = arg1;
    args[1] = arg2;
    auto argsTuple = statefulCallTuple(symbolTable, recordTable, args, std::make_index_sequence<2>{});
    return callWithTuple<RamDomain>(std::forward<AnyFunctor>(userFunctor), argsTuple);
}

/**
 * Governs the maximum supported arity for stateless functors.
 *
 * Be very careful, increasing this value will increase the number of template generated
 * function exponentially.
 *
 * The returned value and each argument of a stateless functor can take one of four types:
 * signed, unsigned, float, symbol.
 *
 * The number of generated functions G for arity up to A is:
 *    G(A) = 4^1 + 4^2 + ... + 4^(A+1)
 *    G(0) = 4
 *    G(1) = G(0) + 4^2 = 20
 *    G(2) = G(1) + 4^3 = 84
 *    G(3) = 340
 *
 */
static constexpr std::size_t StatelessFunctorMaxArity = 2;

/** Construct a native argument value for a stateless functor. */
template <typename T>
T nativeArgument(souffle::SymbolTable& symbolTable, const RamDomain value) {
    if constexpr (std::is_same_v<T, const char*>) {
        return symbolTable.decode(value).c_str();
    } else {
        return ramBitCast<T>(value);
    }
}

/** Construct an arguments tuple for a stateless functor call. */
template <typename... ArgTs, std::size_t... Is>
std::tuple<ArgTs...> statelessCallTuple(souffle::SymbolTable& symbolTable,
        std::array<RamDomain, sizeof...(ArgTs)>& args, std::index_sequence<Is...>) {
    return std::make_tuple(
            nativeArgument<std::tuple_element_t<Is, std::tuple<ArgTs...>>>(symbolTable, args[Is])...);
}

/** Call a stateful functor. */
template <std::size_t I = 0, typename ExecuteFn, typename Shadow, typename... ArgTs>
RamDomain callStateless(ExecuteFn&& execute, Context& ctxt, Shadow& shadow, souffle::SymbolTable& symbolTable,
        const TypeAttribute returnType, const std::vector<TypeAttribute>& argTypes, void (*userFunctor)()) {
    if (I == argTypes.size()) {
        constexpr std::size_t Arity = sizeof...(ArgTs);
        std::array<RamDomain, Arity> args;

        if constexpr (Arity > 0) {
            for (std::size_t i = 0; i < Arity; ++i) {
                args[i] = execute(shadow.getChild(i), ctxt);
            }
        }

        auto argsTuple = statelessCallTuple<ArgTs...>(symbolTable, args, std::make_index_sequence<Arity>{});

        if (returnType == TypeAttribute::Symbol) {
            const char* ret = callWithTuple<const char*, ArgTs...>(userFunctor, argsTuple);
            return symbolTable.encode(ret);
        } else if (returnType == TypeAttribute::Signed) {
            return ramBitCast(callWithTuple<RamDomain, ArgTs...>(userFunctor, argsTuple));
        } else if (returnType == TypeAttribute::Unsigned) {
            return ramBitCast(callWithTuple<RamUnsigned, ArgTs...>(userFunctor, argsTuple));
        } else if (returnType == TypeAttribute::Float) {
            return ramBitCast(callWithTuple<RamFloat, ArgTs...>(userFunctor, argsTuple));
        } else {
            fatal("unsupported return type");
        }

    } else {
        if constexpr (I < StatelessFunctorMaxArity) {
            // construct argument tuple type

            if (argTypes[I] == TypeAttribute::Signed) {
                return callStateless<I + 1, ExecuteFn, Shadow, ArgTs..., RamDomain>(
                        std::forward<ExecuteFn>(execute), ctxt, std::forward<Shadow>(shadow), symbolTable,
                        returnType, argTypes, userFunctor);
            } else if (argTypes[I] == TypeAttribute::Unsigned) {
                return callStateless<I + 1, ExecuteFn, Shadow, ArgTs..., RamUnsigned>(
                        std::forward<ExecuteFn>(execute), ctxt, std::forward<Shadow>(shadow), symbolTable,
                        returnType, argTypes, userFunctor);
            } else if (argTypes[I] == TypeAttribute::Float) {
                return callStateless<I + 1, ExecuteFn, Shadow, ArgTs..., RamFloat>(
                        std::forward<ExecuteFn>(execute), ctxt, std::forward<Shadow>(shadow), symbolTable,
                        returnType, argTypes, userFunctor);
            } else if (argTypes[I] == TypeAttribute::Symbol) {
                return callStateless<I + 1, ExecuteFn, Shadow, ArgTs..., const char*>(
                        std::forward<ExecuteFn>(execute), ctxt, std::forward<Shadow>(shadow), symbolTable,
                        returnType, argTypes, userFunctor);
            } else {
                fatal("unsupported argument type");
            }
        } else {
            fatal("too many arguments to functor for template expension");
        }
    }
}

}  // namespace

Engine::Engine(ram::TranslationUnit& tUnit, const std::size_t numberOfThreadsOrZero)
        : tUnit(tUnit), global(tUnit.global()), profileEnabled(global.config().has("profile")),
          frequencyCounterEnabled(global.config().has("profile-frequency")),
          numOfThreads(number_of_threads(numberOfThreadsOrZero)),
          isa(tUnit.getAnalysis<ram::analysis::IndexAnalysis>()), recordTable(numOfThreads),
          symbolTable(numOfThreads), regexCache(numOfThreads) {}

Engine::RelationHandle& Engine::getRelationHandle(const std::size_t idx) {
    return *relations[idx];
}

void Engine::swapRelation(const std::size_t ramRel1, const std::size_t ramRel2) {
    RelationHandle& rel1 = getRelationHandle(ramRel1);
    RelationHandle& rel2 = getRelationHandle(ramRel2);
    std::swap(rel1, rel2);
}

RamDomain Engine::incCounter() {
    return counter++;
}

Global& Engine::getGlobal() {
    return global;
}

SymbolTable& Engine::getSymbolTable() {
    return symbolTable;
}

RecordTable& Engine::getRecordTable() {
    return recordTable;
}

ram::TranslationUnit& Engine::getTranslationUnit() {
    return tUnit;
}

void* Engine::getMethodHandle(const std::string& method) {
    for (void* libHandle : dll) {
        auto* methodHandle = dlsym(libHandle, method.c_str());
        if (methodHandle != nullptr) {
            return methodHandle;
        }
    }
    return nullptr;
}

VecOwn<Engine::RelationHandle>& Engine::getRelationMap() {
    return relations;
}

void Engine::createRelation(const ram::Relation& id, const std::size_t idx) {
    if (relations.size() < idx + 1) {
        relations.resize(idx + 1);
    }

    RelationHandle res;
    bool hasProvenance = id.getAttributeNames().back() == "@level_number";
    if (hasProvenance) {
        res = createProvenanceRelation(id, isa.getIndexSelection(id.getName()));
    } else if (id.getRepresentation() == RelationRepresentation::EQREL) {
        res = createEqrelRelation(id, isa.getIndexSelection(id.getName()));
    } else if (id.getRepresentation() == RelationRepresentation::BTREE_DELETE) {
        res = createBTreeDeleteRelation(id, isa.getIndexSelection(id.getName()));
    } else {
        res = createBTreeRelation(id, isa.getIndexSelection(id.getName()));
    }
    relations[idx] = mk<RelationHandle>(std::move(res));
}

const std::vector<void*>& Engine::loadDLL() {
    if (!dll.empty()) {
        return dll;
    }

    if (!global.config().has("libraries")) {
        global.config().set("libraries", "functors");
    }
    if (!global.config().has("library-dir")) {
        global.config().set("library-dir", ".");
    }

    for (auto&& library : global.config().getMany("libraries")) {
        // The library may be blank
        if (library.empty()) {
            continue;
        }
        auto paths = global.config().getMany("library-dir");
        // Set up our paths to have a library appended
        for (std::string& path : paths) {
            if (path.back() != pathSeparator) {
                path += pathSeparator;
            }
        }

        if (library.find(pathSeparator) != std::string::npos) {
            paths.clear();
        }

        paths.push_back("");

        void* tmp = nullptr;
        for (const std::string& path : paths) {
            std::string fullpath = path + "lib" + library + dynamicLibSuffix;
#ifndef EMSCRIPTEN
            tmp = dlopen(fullpath.c_str(), RTLD_LAZY);
#else
            tmp = nullptr;
#endif
            if (tmp != nullptr) {
                dll.push_back(tmp);
                break;
            }
        }
    }

    return dll;
}

std::size_t Engine::getIterationNumber() const {
    return iteration;
}
void Engine::incIterationNumber() {
    ++iteration;
}
void Engine::resetIterationNumber() {
    iteration = 0;
}

void Engine::executeMain() {
    SignalHandler::instance()->set();
    if (global.config().has("verbose")) {
        SignalHandler::instance()->enableLogging();
    }

    /* Must load functor libraries before generating IR, because the generator
     * must be able to find actual functions for each user-defined functor. */
    loadDLL();

    generateIR();
    assert(main != nullptr && "Executing an empty program");

    if (!profileEnabled) {
        Context ctxt;
        execute(main.get(), ctxt);
    } else {
        ProfileEventSingleton::instance().setOutputFile(global.config().get("profile"));
        // Prepare the frequency table for threaded use
        const ram::Program& program = tUnit.getProgram();
        visit(program, [&](const ram::TupleOperation& node) {
            if (!node.getProfileText().empty()) {
                frequencies.emplace(node.getProfileText(), std::deque<std::atomic<std::size_t>>());
                frequencies[node.getProfileText()].emplace_back(0);
            }
        });
        // Enable profiling for execution of main
        ProfileEventSingleton::instance().startTimer();
        ProfileEventSingleton::instance().makeTimeEvent("@time;starttime");
        // Store configuration
        for (auto&& [k, vs] : global.config().data())
            for (auto&& v : vs)
                ProfileEventSingleton::instance().makeConfigRecord(k, v);

        // Store count of relations
        std::size_t relationCount = 0;
        for (auto rel : tUnit.getProgram().getRelations()) {
            if (rel->getName()[0] != '@') {
                ++relationCount;
                reads[rel->getName()] = 0;
            }
        }
        ProfileEventSingleton::instance().makeConfigRecord("relationCount", std::to_string(relationCount));

        // Store count of rules
        std::size_t ruleCount = 0;
        visit(program, [&](const ram::Query&) { ++ruleCount; });
        ProfileEventSingleton::instance().makeConfigRecord("ruleCount", std::to_string(ruleCount));

        Context ctxt;
        execute(main.get(), ctxt);
        ProfileEventSingleton::instance().stopTimer();
        for (auto const& cur : frequencies) {
            for (std::size_t i = 0; i < cur.second.size(); ++i) {
                ProfileEventSingleton::instance().makeQuantityEvent(
                        cur.first, cur.second[i], static_cast<int>(i));
            }
        }
        for (auto const& cur : reads) {
            ProfileEventSingleton::instance().makeQuantityEvent(
                    "@relation-reads;" + cur.first, cur.second, 0);
        }
    }
    SignalHandler::instance()->reset();
}

void Engine::generateIR() {
    const ram::Program& program = tUnit.getProgram();
    NodeGenerator generator(*this);
    if (subroutine.empty()) {
        for (const auto& sub : program.getSubroutines()) {
            subroutine.emplace(std::make_pair("stratum_" + sub.first, generator.generateTree(*sub.second)));
        }
    }
    if (main == nullptr) {
        main = generator.generateTree(program.getMain());
    }
}

void Engine::executeSubroutine(
        const std::string& name, const std::vector<RamDomain>& args, std::vector<RamDomain>& ret) {
    Context ctxt;
    ctxt.setReturnValues(ret);
    ctxt.setArguments(args);
    generateIR();
    execute(subroutine["stratum_" + name].get(), ctxt);
}

RamDomain Engine::execute(const Node* node, Context& ctxt) {
#define DEBUG(Kind) std::cout << "Running Node: " << #Kind << "\n";
#define EVAL_CHILD(ty, idx) ramBitCast<ty>(execute(shadow.getChild(idx), ctxt))
#define EVAL_LEFT(ty) ramBitCast<ty>(execute(shadow.getLhs(), ctxt))
#define EVAL_RIGHT(ty) ramBitCast<ty>(execute(shadow.getRhs(), ctxt))

// Overload CASE based on number of arguments.
// CASE(Kind) -> BASE_CASE(Kind)
// CASE(Kind, Structure, Arity, AuxiliaryArity) -> EXTEND_CASE(Kind, Structure, Arity, AuxiliaryArity)
#define GET_MACRO(_1, _2, _3, _4, NAME, ...) NAME
#define CASE(...) GET_MACRO(__VA_ARGS__, EXTEND_CASE, _Dummy, _Dummy2, BASE_CASE)(__VA_ARGS__)

#define BASE_CASE(Kind) \
    case (I_##Kind): {  \
        return [&]() -> RamDomain { \
            [[maybe_unused]] const auto& shadow = *static_cast<const interpreter::Kind*>(node); \
            [[maybe_unused]] const auto& cur = *static_cast<const ram::Kind*>(node->getShadow());
// EXTEND_CASE also defer the relation type
#define EXTEND_CASE(Kind, Structure, Arity, AuxiliaryArity)    \
    case (I_##Kind##_##Structure##_##Arity##_##AuxiliaryArity): { \
        return [&]() -> RamDomain { \
            [[maybe_unused]] const auto& shadow = *static_cast<const interpreter::Kind*>(node); \
            [[maybe_unused]] const auto& cur = *static_cast<const ram::Kind*>(node->getShadow());\
            using RelType = Relation<Arity, AuxiliaryArity, interpreter::Structure>;
#define ESAC(Kind) \
    }              \
    ();            \
    }

#define TUPLE_COPY_FROM(dst, src)     \
    assert(dst.size() == src.size()); \
    std::copy_n(src.begin(), dst.size(), dst.begin())

#define CAL_SEARCH_BOUND(superInfo, low, high)                          \
    /** Unbounded and Constant */                                       \
    TUPLE_COPY_FROM(low, superInfo.first);                              \
    TUPLE_COPY_FROM(high, superInfo.second);                            \
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
        CASE(NumericConstant)
            return cur.getConstant();
        ESAC(NumericConstant)

        CASE(Variable)
            return ctxt.getVariable(cur.getName());
        ESAC(Variable)

        CASE(StringConstant)
            return shadow.getConstant();
        ESAC(StringConstant)

        CASE(TupleElement)
            return ctxt[shadow.getTupleId()][shadow.getElement()];
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
        auto* result_val = &getSymbolTable().decode(result);     \
        for (std::size_t i = 1; i < args.size(); i++) {          \
            auto alt = EVAL_CHILD(RamDomain, i);                 \
            if (alt == result) continue;                         \
                                                                 \
            const auto& alt_val = getSymbolTable().decode(alt);  \
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
        for (std::size_t i = 1; i < args.size(); i++) {  \
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
    case FunctorOp::op: return getSymbolTable().encode(std::to_string(EVAL_CHILD(ty, 0)));
#define CONV_FROM_STRING(op, ty)                              \
    case FunctorOp::op: return ramBitCast(evaluator::symbol2numeric<ty>( \
        getSymbolTable().decode(EVAL_CHILD(RamDomain, 0))));
            // clang-format on

            const auto& args = cur.getArguments();
            switch (cur.getOperator()) {
                /** Unary Functor Operators */
                case FunctorOp::ORD: return execute(shadow.getChild(0), ctxt);
                case FunctorOp::STRLEN:
                    return getSymbolTable().decode(execute(shadow.getChild(0), ctxt)).size();
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

                // Identity overloads
                case FunctorOp::F2F:
                case FunctorOp::I2I:
                case FunctorOp::U2U:
                case FunctorOp::S2S:
                    return execute(shadow.getChild(0), ctxt);

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
                    auto first = ramBitCast<RamSigned>(execute(shadow.getChild(0), ctxt));
                    auto second = ramBitCast<RamSigned>(execute(shadow.getChild(1), ctxt));
                    // std::pow return a double
                    static_assert(std::is_same_v<double, decltype(std::pow(first, second))>);
                    return ramBitCast(static_cast<RamSigned>(std::pow(first, second)));
                }

                case FunctorOp::UEXP: {
                    auto first = ramBitCast<RamUnsigned>(execute(shadow.getChild(0), ctxt));
                    auto second = ramBitCast<RamUnsigned>(execute(shadow.getChild(1), ctxt));
                    // std::pow return a double
                    static_assert(std::is_same_v<double, decltype(std::pow(first, second))>);
                    return ramBitCast(static_cast<RamUnsigned>(std::pow(first, second)));
                }

                case FunctorOp::FEXP: {
                    auto first = ramBitCast<RamFloat>(execute(shadow.getChild(0), ctxt));
                    auto second = ramBitCast<RamFloat>(execute(shadow.getChild(1), ctxt));
                    // std::pow return the same type as the float arguments
                    static_assert(std::is_same_v<RamFloat, decltype(std::pow(first, second))>);
                    return ramBitCast(std::pow(first, second));
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
                    for (std::size_t i = 0; i < args.size(); i++) {
                        ss << getSymbolTable().decode(execute(shadow.getChild(i), ctxt));
                    }
                    return getSymbolTable().encode(ss.str());
                }
                /** Ternary Functor Operators */
                case FunctorOp::SUBSTR: {
                    auto symbol = execute(shadow.getChild(0), ctxt);
                    const std::string& str = getSymbolTable().decode(symbol);
                    auto idx = execute(shadow.getChild(1), ctxt);
                    auto len = execute(shadow.getChild(2), ctxt);
                    std::string sub_str;
                    try {
                        sub_str = str.substr(idx, len);
                    } catch (...) {
                        std::cerr << "warning: wrong index position provided by substr(\"";
                        std::cerr << str << "\"," << (int32_t)idx << "," << (int32_t)len << ") functor.\n";
                    }
                    return getSymbolTable().encode(sub_str);
                }

                case FunctorOp::RANGE:
                case FunctorOp::URANGE:
                case FunctorOp::FRANGE:
                    fatal("ICE: functor `%s` must map onto `NestedIntrinsicOperator`", cur.getOperator());
            }

        {UNREACHABLE_BAD_CASE_ANALYSIS}

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
                ctxt[cur.getTupleId()] = tuple.data();
                execute(shadow.getChild(numArgs), ctxt);
            };

#define RUN_RANGE(ty)                                                                                     \
    numArgs == 3                                                                                          \
            ? evaluator::runRange<ty>(EVAL_CHILD(ty, 0), EVAL_CHILD(ty, 1), EVAL_CHILD(ty, 2), runNested) \
            : evaluator::runRange<ty>(EVAL_CHILD(ty, 0), EVAL_CHILD(ty, 1), runNested),                   \
            true

            switch (cur.getFunction()) {
                case ram::NestedIntrinsicOp::RANGE: return RUN_RANGE(RamSigned);
                case ram::NestedIntrinsicOp::URANGE: return RUN_RANGE(RamUnsigned);
                case ram::NestedIntrinsicOp::FRANGE: return RUN_RANGE(RamFloat);
            }

        {UNREACHABLE_BAD_CASE_ANALYSIS}
#undef RUN_RANGE
        ESAC(NestedIntrinsicOperator)

        CASE(UserDefinedOperator)
            const std::string& name = cur.getName();

            auto userFunctor = reinterpret_cast<void (*)()>(shadow.getFunctionPointer());
            if (userFunctor == nullptr) fatal("cannot find user-defined operator `%s`", name);
            std::size_t arity = cur.getArguments().size();

            if (cur.isStateful()) {
                auto exec = std::bind(&Engine::execute, this, std::placeholders::_1, std::placeholders::_2);
#define CALL_STATEFUL(ARITY) \
    case ARITY:              \
        return callStateful<ARITY>(exec, ctxt, shadow, userFunctor, &getSymbolTable(), &getRecordTable())

                // inlined call to stateful functor with arity 0 to 16.
                switch (arity) {
                    CALL_STATEFUL(0);
                    CALL_STATEFUL(1);
                    CALL_STATEFUL(2);
                    CALL_STATEFUL(3);
                    CALL_STATEFUL(4);
                    CALL_STATEFUL(5);
                    CALL_STATEFUL(6);
                    CALL_STATEFUL(7);
                    CALL_STATEFUL(8);
                    CALL_STATEFUL(9);
                    CALL_STATEFUL(10);
                    CALL_STATEFUL(11);
                    CALL_STATEFUL(12);
                    CALL_STATEFUL(13);
                    CALL_STATEFUL(14);
                    CALL_STATEFUL(15);
                    CALL_STATEFUL(16);
                }
#ifdef USE_LIBFFI
                // prepare dynamic call environment
                std::unique_ptr<void*[]> values = std::make_unique<void*[]>(arity + 2);
                std::unique_ptr<RamDomain[]> intVal = std::make_unique<RamDomain[]>(arity);
                RamDomain rc;

                /* Initialize arguments for ffi-call */
                void* symbolTable = (void*)&getSymbolTable();
                values[0] = &symbolTable;
                void* recordTable = (void*)&getRecordTable();
                values[1] = &recordTable;
                for (std::size_t i = 0; i < arity; i++) {
                    intVal[i] = execute(shadow.getChild(i), ctxt);
                    values[i + 2] = &intVal[i];
                }

                ffi_call(shadow.getFFIcif(), userFunctor, &rc, values.get());
                return rc;
#else
                fatal("unsupported stateful functor arity without libffi support");
#endif
            } else {
                const std::vector<TypeAttribute>& types = cur.getArgsTypes();
                const auto returnType = cur.getReturnType();

                if (types.size() <= StatelessFunctorMaxArity) {
                    auto exec =
                            std::bind(&Engine::execute, this, std::placeholders::_1, std::placeholders::_2);
                    return callStateless(
                            exec, ctxt, shadow, getSymbolTable(), returnType, types, userFunctor);
                }

#ifdef USE_LIBFFI
                // prepare dynamic call environment
                std::unique_ptr<void*[]> values = std::make_unique<void*[]>(arity);
                std::unique_ptr<RamSigned[]> intVal = std::make_unique<RamSigned[]>(arity);
                std::unique_ptr<RamUnsigned[]> uintVal = std::make_unique<RamUnsigned[]>(arity);
                std::unique_ptr<RamFloat[]> floatVal = std::make_unique<RamFloat[]>(arity);
                std::unique_ptr<const char*[]> strVal = std::make_unique<const char*[]>(arity);

                /* Initialize arguments for ffi-call */
                for (std::size_t i = 0; i < arity; i++) {
                    RamDomain arg = execute(shadow.getChild(i), ctxt);
                    switch (types[i]) {
                        case TypeAttribute::Symbol:
                            strVal[i] = getSymbolTable().decode(arg).c_str();
                            values[i] = &strVal[i];
                            break;
                        case TypeAttribute::Signed:
                            intVal[i] = arg;
                            values[i] = &intVal[i];
                            break;
                        case TypeAttribute::Unsigned:
                            uintVal[i] = ramBitCast<RamUnsigned>(arg);
                            values[i] = &uintVal[i];
                            break;
                        case TypeAttribute::Float:
                            floatVal[i] = ramBitCast<RamFloat>(arg);
                            values[i] = &floatVal[i];
                            break;
                        case TypeAttribute::ADT: fatal("ADT support is not implemented");
                        case TypeAttribute::Record: fatal("Record support is not implemented");
                    }
                }

                union {
                    RamDomain s;
                    RamUnsigned u;
                    RamFloat f;
                    const char* c;
                    ffi_arg dummy;  // ensures minium size
                } rvalue;

                ffi_call(shadow.getFFIcif(), userFunctor, &rvalue, values.get());

                switch (cur.getReturnType()) {
                    case TypeAttribute::Signed: return static_cast<RamDomain>(rvalue.s);
                    case TypeAttribute::Symbol: return getSymbolTable().encode(rvalue.c);
                    case TypeAttribute::Unsigned: return ramBitCast(rvalue.u);
                    case TypeAttribute::Float: return ramBitCast(rvalue.f);
                    case TypeAttribute::ADT: fatal("Not implemented");
                    case TypeAttribute::Record: fatal("Not implemented");
                }
                fatal("Unsupported user defined operator");
#else
                fatal("unsupported stateless functor arity without libffi support");
#endif
            }

        ESAC(UserDefinedOperator)

        CASE(PackRecord)
            auto values = cur.getArguments();
            std::size_t arity = values.size();
            std::unique_ptr<RamDomain[]> data = std::make_unique<RamDomain[]>(arity);
            for (std::size_t i = 0; i < arity; ++i) {
                data[i] = execute(shadow.getChild(i), ctxt);
            }
            return getRecordTable().pack(data.get(), arity);
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

#define EMPTINESS_CHECK(Structure, Arity, AuxiliaryArity, ...)                          \
    CASE(EmptinessCheck, Structure, Arity, AuxiliaryArity)                              \
        const auto& rel = *static_cast<RelType*>(shadow.getRelation()); \
        return rel.empty();                                             \
    ESAC(EmptinessCheck)

        FOR_EACH(EMPTINESS_CHECK)
#undef EMPTINESS_CHECK

#define RELATION_SIZE(Structure, Arity, AuxiliaryArity, ...)                            \
    CASE(RelationSize, Structure, Arity, AuxiliaryArity)                                \
        const auto& rel = *static_cast<RelType*>(shadow.getRelation()); \
        return rel.size();                                              \
    ESAC(RelationSize)

        FOR_EACH(RELATION_SIZE)
#undef RELATION_SIZE

#define EXISTENCE_CHECK(Structure, Arity, AuxiliaryArity, ...)            \
    CASE(ExistenceCheck, Structure, Arity, AuxiliaryArity)                \
        return evalExistenceCheck<RelType>(shadow, ctxt); \
    ESAC(ExistenceCheck)

        FOR_EACH(EXISTENCE_CHECK)
#undef EXISTENCE_CHECK

#define PROVENANCE_EXISTENCE_CHECK(Structure, Arity, AuxiliaryArity, ...)           \
    CASE(ProvenanceExistenceCheck, Structure, Arity, AuxiliaryArity)                \
        return evalProvenanceExistenceCheck<RelType>(shadow, ctxt); \
    ESAC(ProvenanceExistenceCheck)

        FOR_EACH_PROVENANCE(PROVENANCE_EXISTENCE_CHECK)
#undef PROVENANCE_EXISTENCE_CHECK

        CASE(Constraint)
        // clang-format off
#define COMPARE_NUMERIC(ty, op) return EVAL_LEFT(ty) op EVAL_RIGHT(ty)
#define COMPARE_STRING(op)                                        \
    return (getSymbolTable().decode(EVAL_LEFT(RamDomain)) op \
            getSymbolTable().decode(EVAL_RIGHT(RamDomain)))
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
                    bool result = false;
                    RamDomain right = execute(shadow.getRhs(), ctxt);
                    const std::string& text = getSymbolTable().decode(right);

                    const Node* patternNode = shadow.getLhs();
                    if (const RegexConstant* regexNode = dynamic_cast<const RegexConstant*>(patternNode);
                            regexNode) {
                        const auto& regex = regexNode->getRegex();
                        if (regex) {
                            result = std::regex_match(text, *regex);
                        }
                    } else {
                        RamDomain left = execute(patternNode, ctxt);
                        const std::string& pattern = getSymbolTable().decode(left);
                        try {
                            const std::regex& regex = regexCache.getOrCreate(pattern);
                            result = std::regex_match(text, regex);
                        } catch (...) {
                            std::cerr << "warning: wrong pattern provided for match(\"" << pattern << "\",\""
                                      << text << "\").\n";
                        }
                    }

                    return result;
                }
                case BinaryConstraintOp::NOT_MATCH: {
                    bool result = false;
                    RamDomain right = execute(shadow.getRhs(), ctxt);
                    const std::string& text = getSymbolTable().decode(right);

                    const Node* patternNode = shadow.getLhs();
                    if (const RegexConstant* regexNode = dynamic_cast<const RegexConstant*>(patternNode);
                            regexNode) {
                        const auto& regex = regexNode->getRegex();
                        if (regex) {
                            result = !std::regex_match(text, *regex);
                        }
                    } else {
                        RamDomain left = execute(patternNode, ctxt);
                        const std::string& pattern = getSymbolTable().decode(left);
                        try {
                            const std::regex& regex = regexCache.getOrCreate(pattern);
                            result = !std::regex_match(text, regex);
                        } catch (...) {
                            std::cerr << "warning: wrong pattern provided for !match(\"" << pattern << "\",\""
                                      << text << "\").\n";
                        }
                    }
                    return result;
                }
                case BinaryConstraintOp::CONTAINS: {
                    RamDomain left = execute(shadow.getLhs(), ctxt);
                    RamDomain right = execute(shadow.getRhs(), ctxt);
                    const std::string& pattern = getSymbolTable().decode(left);
                    const std::string& text = getSymbolTable().decode(right);
                    return text.find(pattern) != std::string::npos;
                }
                case BinaryConstraintOp::NOT_CONTAINS: {
                    RamDomain left = execute(shadow.getLhs(), ctxt);
                    RamDomain right = execute(shadow.getRhs(), ctxt);
                    const std::string& pattern = getSymbolTable().decode(left);
                    const std::string& text = getSymbolTable().decode(right);
                    return text.find(pattern) == std::string::npos;
                }
            }

        {UNREACHABLE_BAD_CASE_ANALYSIS}

#undef COMPARE_NUMERIC
#undef COMPARE_STRING
#undef COMPARE
#undef COMPARE_EQ_NE
        ESAC(Constraint)

        CASE(TupleOperation)
            bool result = execute(shadow.getChild(), ctxt);

            auto& currentFrequencies = frequencies[cur.getProfileText()];
            while (currentFrequencies.size() <= getIterationNumber()) {
#ifdef _OPENMP
#pragma omp critical(frequencies)
#endif
                currentFrequencies.emplace_back(0);
            }
            frequencies[cur.getProfileText()][getIterationNumber()]++;

            return result;
        ESAC(TupleOperation)

#define SCAN(Structure, Arity, AuxiliaryArity, ...)                                     \
    CASE(Scan, Structure, Arity, AuxiliaryArity)                                        \
        const auto& rel = *static_cast<RelType*>(shadow.getRelation()); \
        return evalScan(rel, cur, shadow, ctxt);                        \
    ESAC(Scan)

        FOR_EACH(SCAN)
#undef SCAN

#define PARALLEL_SCAN(Structure, Arity, AuxiliaryArity, ...)                            \
    CASE(ParallelScan, Structure, Arity, AuxiliaryArity)                                \
        const auto& rel = *static_cast<RelType*>(shadow.getRelation()); \
        return evalParallelScan(rel, cur, shadow, ctxt);                \
    ESAC(ParallelScan)
        FOR_EACH(PARALLEL_SCAN)
#undef PARALLEL_SCAN

#define INDEX_SCAN(Structure, Arity, AuxiliaryArity, ...)                 \
    CASE(IndexScan, Structure, Arity, AuxiliaryArity)                     \
        return evalIndexScan<RelType>(cur, shadow, ctxt); \
    ESAC(IndexScan)

        FOR_EACH(INDEX_SCAN)
#undef INDEX_SCAN

#define PARALLEL_INDEX_SCAN(Structure, Arity, AuxiliaryArity, ...)                      \
    CASE(ParallelIndexScan, Structure, Arity, AuxiliaryArity)                           \
        const auto& rel = *static_cast<RelType*>(shadow.getRelation()); \
        return evalParallelIndexScan(rel, cur, shadow, ctxt);           \
    ESAC(ParallelIndexScan)

        FOR_EACH(PARALLEL_INDEX_SCAN)
#undef PARALLEL_INDEX_SCAN

#define IFEXISTS(Structure, Arity, AuxiliaryArity, ...)                                 \
    CASE(IfExists, Structure, Arity, AuxiliaryArity)                                    \
        const auto& rel = *static_cast<RelType*>(shadow.getRelation()); \
        return evalIfExists(rel, cur, shadow, ctxt);                    \
    ESAC(IfExists)

        FOR_EACH(IFEXISTS)
#undef IFEXISTS

#define PARALLEL_IFEXISTS(Structure, Arity, AuxiliaryArity, ...)                        \
    CASE(ParallelIfExists, Structure, Arity, AuxiliaryArity)                            \
        const auto& rel = *static_cast<RelType*>(shadow.getRelation()); \
        return evalParallelIfExists(rel, cur, shadow, ctxt);            \
    ESAC(ParallelIfExists)

        FOR_EACH(PARALLEL_IFEXISTS)
#undef PARALLEL_IFEXISTS

#define INDEX_IFEXISTS(Structure, Arity, AuxiliaryArity, ...)                 \
    CASE(IndexIfExists, Structure, Arity, AuxiliaryArity)                     \
        return evalIndexIfExists<RelType>(cur, shadow, ctxt); \
    ESAC(IndexIfExists)

        FOR_EACH(INDEX_IFEXISTS)
#undef INDEX_IFEXISTS

#define PARALLEL_INDEX_IFEXISTS(Structure, Arity, AuxiliaryArity, ...)                  \
    CASE(ParallelIndexIfExists, Structure, Arity, AuxiliaryArity)                       \
        const auto& rel = *static_cast<RelType*>(shadow.getRelation()); \
        return evalParallelIndexIfExists(rel, cur, shadow, ctxt);       \
    ESAC(ParallelIndexIfExists)

        FOR_EACH(PARALLEL_INDEX_IFEXISTS)
#undef PARALLEL_INDEX_IFEXISTS

        CASE(UnpackRecord)
            RamDomain ref = execute(shadow.getExpr(), ctxt);

            // check for nil
            if (ref == 0) {
                return true;
            }

            // update environment variable
            std::size_t arity = cur.getArity();
            const RamDomain* tuple = getRecordTable().unpack(ref, arity);

            // save reference to temporary value
            ctxt[cur.getTupleId()] = tuple;

            // run nested part - using base class visitor
            return execute(shadow.getNestedOperation(), ctxt);
        ESAC(UnpackRecord)

#define PARALLEL_AGGREGATE(Structure, Arity, AuxiliaryArity, ...)                       \
    CASE(ParallelAggregate, Structure, Arity, AuxiliaryArity)                           \
        const auto& rel = *static_cast<RelType*>(shadow.getRelation()); \
        return evalParallelAggregate(rel, cur, shadow, ctxt);           \
    ESAC(ParallelAggregate)

        FOR_EACH(PARALLEL_AGGREGATE)
#undef PARALLEL_AGGREGATE

#define AGGREGATE(Structure, Arity, AuxiliaryArity, ...)                                \
    CASE(Aggregate, Structure, Arity, AuxiliaryArity)                                   \
        const auto& rel = *static_cast<RelType*>(shadow.getRelation()); \
        return evalAggregate(cur, shadow, rel.scan(), ctxt);            \
    ESAC(Aggregate)

        FOR_EACH(AGGREGATE)
#undef AGGREGATE

#define PARALLEL_INDEX_AGGREGATE(Structure, Arity, AuxiliaryArity, ...)                \
    CASE(ParallelIndexAggregate, Structure, Arity, AuxiliaryArity)                     \
        return evalParallelIndexAggregate<RelType>(cur, shadow, ctxt); \
    ESAC(ParallelIndexAggregate)

        FOR_EACH(PARALLEL_INDEX_AGGREGATE)
#undef PARALLEL_INDEX_AGGREGATE

#define INDEX_AGGREGATE(Structure, Arity, AuxiliaryArity, ...)                 \
    CASE(IndexAggregate, Structure, Arity, AuxiliaryArity)                     \
        return evalIndexAggregate<RelType>(cur, shadow, ctxt); \
    ESAC(IndexAggregate)

        FOR_EACH(INDEX_AGGREGATE)
#undef INDEX_AGGREGATE

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

            if (profileEnabled && frequencyCounterEnabled && !cur.getProfileText().empty()) {
                auto& currentFrequencies = frequencies[cur.getProfileText()];
                while (currentFrequencies.size() <= getIterationNumber()) {
                    currentFrequencies.emplace_back(0);
                }
                frequencies[cur.getProfileText()][getIterationNumber()]++;
            }
            return result;
        ESAC(Filter)

#define GUARDED_INSERT(Structure, Arity, AuxiliaryArity, ...)                     \
    CASE(GuardedInsert, Structure, Arity, AuxiliaryArity)                         \
        auto& rel = *static_cast<RelType*>(shadow.getRelation()); \
        return evalGuardedInsert(rel, shadow, ctxt);              \
    ESAC(GuardedInsert)

        FOR_EACH(GUARDED_INSERT)
#undef GUARDED_INSERT

#define INSERT(Structure, Arity, AuxiliaryArity, ...)                             \
    CASE(Insert, Structure, Arity, AuxiliaryArity)                                \
        auto& rel = *static_cast<RelType*>(shadow.getRelation()); \
        return evalInsert(rel, shadow, ctxt);                     \
    ESAC(Insert)

        FOR_EACH(INSERT)
#undef INSERT

#define ERASE(Structure, Arity, AuxiliaryArity, ...)                                                 \
    CASE(Erase, Structure, Arity, AuxiliaryArity)                                                    \
        void(static_cast<RelType*>(shadow.getRelation()));                           \
        auto& rel = *static_cast<BtreeDeleteRelation<Arity,AuxiliaryArity>*>(shadow.getRelation()); \
        return evalErase(rel, shadow, ctxt);                                         \
    ESAC(Erase)

        FOR_EACH_BTREE_DELETE(ERASE)
#undef ERASE

        CASE(SubroutineReturn)
            for (std::size_t i = 0; i < cur.getValues().size(); ++i) {
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
                    std::bind(&RelationWrapper::size, shadow.getRelation()));
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

#define CLEAR(Structure, Arity, AuxiliaryArity, ...)                              \
    CASE(Clear, Structure, Arity, AuxiliaryArity)                                 \
        auto& rel = *static_cast<RelType*>(shadow.getRelation()); \
        rel.__purge();                                            \
        return true;                                              \
    ESAC(Clear)

        FOR_EACH(CLEAR)
#undef CLEAR

#define ESTIMATEJOINSIZE(Structure, Arity, AuxiliaryArity, ...)                         \
    CASE(EstimateJoinSize, Structure, Arity, AuxiliaryArity)                            \
        const auto& rel = *static_cast<RelType*>(shadow.getRelation()); \
        return evalEstimateJoinSize<RelType>(rel, cur, shadow, ctxt);   \
    ESAC(EstimateJoinSize)

        FOR_EACH(ESTIMATEJOINSIZE)
#undef ESTIMATEJOINSIZE

        CASE(Call)
            execute(subroutine[shadow.getSubroutineName()].get(), ctxt);
            return true;
        ESAC(Call)

        CASE(LogSize)
            const auto& rel = *shadow.getRelation();
            ProfileEventSingleton::instance().makeQuantityEvent(
                    cur.getMessage(), rel.size(), static_cast<int>(getIterationNumber()));
            return true;
        ESAC(LogSize)

        CASE(IO)
            const auto& directive = cur.getDirectives();
            const std::string& op = cur.get("operation");
            auto& rel = *shadow.getRelation();

            if (op == "input") {
                try {
                    IOSystem::getInstance()
                            .getReader(directive, getSymbolTable(), getRecordTable())
                            ->readAll(rel);
                } catch (std::exception& e) {
                    std::cerr << "Error loading " << rel.getName() << " data: " << e.what() << "\n";
                    exit(EXIT_FAILURE);
                }
                return true;
            } else if (op == "output" || op == "printsize") {
                try {
                    IOSystem::getInstance()
                            .getWriter(directive, getSymbolTable(), getRecordTable())
                            ->writeAll(rel);
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
            ViewContext* viewContext = shadow.getViewContext();

            // Execute view-free operations in outer filter if any.
            auto& viewFreeOps = viewContext->getOuterFilterViewFreeOps();
            for (auto& op : viewFreeOps) {
                if (!execute(op.get(), ctxt)) {
                    return true;
                }
            }

            // Create Views for outer filter operation if any.
            auto& viewsForOuter = viewContext->getViewInfoForFilter();
            for (auto& info : viewsForOuter) {
                ctxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
            }

            // Execute outer filter operation.
            auto& viewOps = viewContext->getOuterFilterViewOps();
            for (auto& op : viewOps) {
                if (!execute(op.get(), ctxt)) {
                    return true;
                }
            }

            if (viewContext->isParallel) {
                // If Parallel is true, holds views creation unitl parallel instructions.
            } else {
                // Issue views for nested operation.
                auto& viewsForNested = viewContext->getViewInfoForNested();
                for (auto& info : viewsForNested) {
                    ctxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
                }
            }
            execute(shadow.getChild(), ctxt);
            return true;
        ESAC(Query)

        CASE(MergeExtend)
            auto& src = *static_cast<EqrelRelation*>(getRelationHandle(shadow.getSourceId()).get());
            auto& trg = *static_cast<EqrelRelation*>(getRelationHandle(shadow.getTargetId()).get());
            src.extendAndInsert(trg);
            return true;
        ESAC(MergeExtend)

        CASE(Swap)
            swapRelation(shadow.getSourceId(), shadow.getTargetId());
            return true;
        ESAC(Swap)

        CASE(Assign)
            const std::string& name = cur.getVariable().getName();
            const RamDomain val = execute(shadow.getRhs(), ctxt);
            ctxt.setVariable(name, val);
            return true;
        ESAC(Assign)
    }

    UNREACHABLE_BAD_CASE_ANALYSIS

#undef EVAL_CHILD
#undef DEBUG
}

template <typename Rel>
RamDomain Engine::evalExistenceCheck(const ExistenceCheck& shadow, Context& ctxt) {
    constexpr std::size_t Arity = Rel::Arity;
    std::size_t viewPos = shadow.getViewId();

    if (profileEnabled && !shadow.isTemp()) {
        reads[shadow.getRelationName()]++;
    }

    const auto& superInfo = shadow.getSuperInst();
    // for total we use the exists test
    if (shadow.isTotalSearch()) {
        souffle::Tuple<RamDomain, Arity> tuple;
        TUPLE_COPY_FROM(tuple, superInfo.first);
        /* TupleElement */
        for (const auto& tupleElement : superInfo.tupleFirst) {
            tuple[tupleElement[0]] = ctxt[tupleElement[1]][tupleElement[2]];
        }
        /* Generic */
        for (const auto& expr : superInfo.exprFirst) {
            tuple[expr.first] = execute(expr.second.get(), ctxt);
        }
        return Rel::castView(ctxt.getView(viewPos))->contains(tuple);
    }

    // for partial we search for lower and upper boundaries
    souffle::Tuple<RamDomain, Arity> low;
    souffle::Tuple<RamDomain, Arity> high;
    TUPLE_COPY_FROM(low, superInfo.first);
    TUPLE_COPY_FROM(high, superInfo.second);

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

    return Rel::castView(ctxt.getView(viewPos))->contains(low, high);
}

template <typename Rel>
RamDomain Engine::evalProvenanceExistenceCheck(const ProvenanceExistenceCheck& shadow, Context& ctxt) {
    // construct the pattern tuple
    constexpr std::size_t Arity = Rel::Arity;
    const auto& superInfo = shadow.getSuperInst();

    // for partial we search for lower and upper boundaries
    souffle::Tuple<RamDomain, Arity> low;
    souffle::Tuple<RamDomain, Arity> high;
    TUPLE_COPY_FROM(low, superInfo.first);
    TUPLE_COPY_FROM(high, superInfo.second);

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

    low[Arity - 2] = MIN_RAM_SIGNED;
    low[Arity - 1] = MIN_RAM_SIGNED;
    high[Arity - 2] = MAX_RAM_SIGNED;
    high[Arity - 1] = MAX_RAM_SIGNED;

    // obtain view
    std::size_t viewPos = shadow.getViewId();

    // get an equalRange
    auto equalRange = Rel::castView(ctxt.getView(viewPos))->range(low, high);

    // if range is empty
    if (equalRange.begin() == equalRange.end()) {
        return false;
    }

    // check whether the height is less than the current height
    return (*equalRange.begin())[Arity - 1] <= execute(shadow.getChild(), ctxt);
}

template <typename Rel>
RamDomain Engine::evalScan(const Rel& rel, const ram::Scan& cur, const Scan& shadow, Context& ctxt) {
    for (const auto& tuple : rel.scan()) {
        ctxt[cur.getTupleId()] = tuple.data();
        if (!execute(shadow.getNestedOperation(), ctxt)) {
            break;
        }
    }
    return true;
}

template <typename Rel>
RamDomain Engine::evalParallelScan(
        const Rel& rel, const ram::ParallelScan& cur, const ParallelScan& shadow, Context& ctxt) {
    auto viewContext = shadow.getViewContext();

    auto pStream = rel.partitionScan(numOfThreads * 20);

    PARALLEL_START
        Context newCtxt(ctxt);
        auto viewInfo = viewContext->getViewInfoForNested();
        for (const auto& info : viewInfo) {
            newCtxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
        }
#if defined _OPENMP && _OPENMP < 200805
        auto count = std::distance(pStream.begin(), pStream.end());
        auto b = pStream.begin();
        pfor(int i = 0; i < count; i++) {
            auto it = b + i;
#else
        pfor(auto it = pStream.begin(); it < pStream.end(); it++) {
#endif
            for (const auto& tuple : *it) {
                newCtxt[cur.getTupleId()] = tuple.data();
                if (!execute(shadow.getNestedOperation(), newCtxt)) {
                    break;
                }
            }
        }
    PARALLEL_END
    return true;
}

template <typename Rel>
RamDomain Engine::evalEstimateJoinSize(
        const Rel& rel, const ram::EstimateJoinSize& cur, const EstimateJoinSize& shadow, Context& ctxt) {
    (void)ctxt;
    constexpr std::size_t Arity = Rel::Arity;
    bool onlyConstants = true;

    for (auto col : cur.getKeyColumns()) {
        if (cur.getConstantsMap().count(col) == 0) {
            onlyConstants = false;
            break;
        }
    }

    // save a copy of the columns and index
    std::vector<std::size_t> keyColumns(cur.getKeyColumns().size());
    std::iota(keyColumns.begin(), keyColumns.end(), 0);

    std::size_t indexPos = shadow.getViewId();
    auto order = rel.getIndexOrder(indexPos);

    std::vector<std::size_t> inverseOrder;
    inverseOrder.resize(order.size());

    for (std::size_t i = 0; i < order.size(); ++i) {
        inverseOrder[order[i]] = i;
    }

    // create a copy of the map to the real numeric constants
    std::map<std::size_t, RamDomain> keyConstants;
    for (auto [k, constant] : cur.getConstantsMap()) {
        RamDomain value;
        if (const auto* signedConstant = as<ram::SignedConstant>(constant)) {
            value = ramBitCast<RamDomain>(signedConstant->getValue());
        } else if (const auto* stringConstant = as<ram::StringConstant>(constant)) {
            auto& symTable = getSymbolTable();
            assert(symTable.weakContains(stringConstant->getConstant()));
            value = ramBitCast<RamDomain>(symTable.encode(stringConstant->getConstant()));
        } else if (const auto* unsignedConstant = as<ram::UnsignedConstant>(constant)) {
            value = ramBitCast<RamDomain>(unsignedConstant->getValue());
        } else if (const auto* floatConstant = as<ram::FloatConstant>(constant)) {
            value = ramBitCast<RamDomain>(floatConstant->getValue());
        } else {
            fatal("Something went wrong. Should have gotten a constant!");
        }

        keyConstants[inverseOrder[k]] = value;
    }

    // ensure range is non-empty
    auto* index = rel.getIndex(indexPos);
    // initial values
    double total = 0;
    double duplicates = 0;

    if (!index->scan().empty()) {
        // assign first tuple as prev as a dummy
        bool first = true;
        Tuple<RamDomain, Arity> prev = *index->scan().begin();

        for (const auto& tuple : index->scan()) {
            // only if every constant matches do we consider the tuple
            bool matchesConstants = std::all_of(keyConstants.begin(), keyConstants.end(),
                    [tuple](const auto& p) { return tuple[p.first] == p.second; });
            if (!matchesConstants) {
                continue;
            }
            if (first) {
                first = false;
            } else {
                // only if on every column do we have a match do we consider it a duplicate
                bool matchesPrev = std::all_of(keyColumns.begin(), keyColumns.end(),
                        [&prev, &tuple](std::size_t column) { return tuple[column] == prev[column]; });
                if (matchesPrev) {
                    ++duplicates;
                }
            }
            prev = tuple;
            ++total;
        }
    }
    double joinSize = (onlyConstants ? total : total / std::max(1.0, (total - duplicates)));

    std::stringstream columnsStream;
    columnsStream << cur.getKeyColumns();
    std::string columns = columnsStream.str();

    std::stringstream constantsStream;
    constantsStream << "{";
    bool first = true;
    for (auto& [k, constant] : cur.getConstantsMap()) {
        if (first) {
            first = false;
        } else {
            constantsStream << ",";
        }
        constantsStream << k << "->" << *constant;
    }
    constantsStream << "}";

    std::string constants = stringify(constantsStream.str());

    if (cur.isRecursiveRelation()) {
        std::string txt =
                "@recursive-estimate-join-size;" + cur.getRelation() + ";" + columns + ";" + constants;
        ProfileEventSingleton::instance().makeRecursiveCountEvent(txt, joinSize, getIterationNumber());
    } else {
        std::string txt =
                "@non-recursive-estimate-join-size;" + cur.getRelation() + ";" + columns + ";" + constants;
        ProfileEventSingleton::instance().makeNonRecursiveCountEvent(txt, joinSize);
    }
    return true;
}

template <typename Rel>
RamDomain Engine::evalIndexScan(const ram::IndexScan& cur, const IndexScan& shadow, Context& ctxt) {
    constexpr std::size_t Arity = Rel::Arity;
    // create pattern tuple for range query
    const auto& superInfo = shadow.getSuperInst();
    souffle::Tuple<RamDomain, Arity> low;
    souffle::Tuple<RamDomain, Arity> high;
    CAL_SEARCH_BOUND(superInfo, low, high);

    std::size_t viewId = shadow.getViewId();
    auto view = Rel::castView(ctxt.getView(viewId));
    // conduct range query
    for (const auto& tuple : view->range(low, high)) {
        ctxt[cur.getTupleId()] = tuple.data();
        if (!execute(shadow.getNestedOperation(), ctxt)) {
            break;
        }
    }
    return true;
}

template <typename Rel>
RamDomain Engine::evalParallelIndexScan(
        const Rel& rel, const ram::ParallelIndexScan& cur, const ParallelIndexScan& shadow, Context& ctxt) {
    auto viewContext = shadow.getViewContext();

    // create pattern tuple for range query
    constexpr std::size_t Arity = Rel::Arity;
    const auto& superInfo = shadow.getSuperInst();
    souffle::Tuple<RamDomain, Arity> low;
    souffle::Tuple<RamDomain, Arity> high;
    CAL_SEARCH_BOUND(superInfo, low, high);

    std::size_t indexPos = shadow.getViewId();
    auto pStream = rel.partitionRange(indexPos, low, high, numOfThreads * 20);
    PARALLEL_START
        Context newCtxt(ctxt);
        auto viewInfo = viewContext->getViewInfoForNested();
        for (const auto& info : viewInfo) {
            newCtxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
        }
#if defined _OPENMP && _OPENMP < 200805
        auto count = std::distance(pStream.begin(), pStream.end());
        auto b = pStream.begin();
        pfor(int i = 0; i < count; i++) {
            auto it = b + i;
#else
        pfor(auto it = pStream.begin(); it < pStream.end(); it++) {
#endif
            for (const auto& tuple : *it) {
                newCtxt[cur.getTupleId()] = tuple.data();
                if (!execute(shadow.getNestedOperation(), newCtxt)) {
                    break;
                }
            }
        }
    PARALLEL_END
    return true;
}

template <typename Rel>
RamDomain Engine::evalIfExists(
        const Rel& rel, const ram::IfExists& cur, const IfExists& shadow, Context& ctxt) {
    // use simple iterator
    for (const auto& tuple : rel.scan()) {
        ctxt[cur.getTupleId()] = tuple.data();
        if (execute(shadow.getCondition(), ctxt)) {
            execute(shadow.getNestedOperation(), ctxt);
            break;
        }
    }
    return true;
}

template <typename Rel>
RamDomain Engine::evalParallelIfExists(
        const Rel& rel, const ram::ParallelIfExists& cur, const ParallelIfExists& shadow, Context& ctxt) {
    auto viewContext = shadow.getViewContext();

    auto pStream = rel.partitionScan(numOfThreads * 20);
    auto viewInfo = viewContext->getViewInfoForNested();
    PARALLEL_START
        Context newCtxt(ctxt);
        for (const auto& info : viewInfo) {
            newCtxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
        }
#if defined _OPENMP && _OPENMP < 200805
        auto count = std::distance(pStream.begin(), pStream.end());
        auto b = pStream.begin();
        pfor(int i = 0; i < count; i++) {
            auto it = b + i;
#else
        pfor(auto it = pStream.begin(); it < pStream.end(); it++) {
#endif
            for (const auto& tuple : *it) {
                newCtxt[cur.getTupleId()] = tuple.data();
                if (execute(shadow.getCondition(), newCtxt)) {
                    execute(shadow.getNestedOperation(), newCtxt);
                    break;
                }
            }
        }
    PARALLEL_END
    return true;
}

template <typename Rel>
RamDomain Engine::evalIndexIfExists(
        const ram::IndexIfExists& cur, const IndexIfExists& shadow, Context& ctxt) {
    constexpr std::size_t Arity = Rel::Arity;
    const auto& superInfo = shadow.getSuperInst();
    souffle::Tuple<RamDomain, Arity> low;
    souffle::Tuple<RamDomain, Arity> high;
    CAL_SEARCH_BOUND(superInfo, low, high);

    std::size_t viewId = shadow.getViewId();
    auto view = Rel::castView(ctxt.getView(viewId));

    for (const auto& tuple : view->range(low, high)) {
        ctxt[cur.getTupleId()] = tuple.data();
        if (execute(shadow.getCondition(), ctxt)) {
            execute(shadow.getNestedOperation(), ctxt);
            break;
        }
    }
    return true;
}

template <typename Rel>
RamDomain Engine::evalParallelIndexIfExists(const Rel& rel, const ram::ParallelIndexIfExists& cur,
        const ParallelIndexIfExists& shadow, Context& ctxt) {
    auto viewContext = shadow.getViewContext();

    auto viewInfo = viewContext->getViewInfoForNested();

    // create pattern tuple for range query
    constexpr std::size_t Arity = Rel::Arity;
    const auto& superInfo = shadow.getSuperInst();
    souffle::Tuple<RamDomain, Arity> low;
    souffle::Tuple<RamDomain, Arity> high;
    CAL_SEARCH_BOUND(superInfo, low, high);

    std::size_t indexPos = shadow.getViewId();
    auto pStream = rel.partitionRange(indexPos, low, high, numOfThreads * 20);

    PARALLEL_START
        Context newCtxt(ctxt);
        for (const auto& info : viewInfo) {
            newCtxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
        }
#if defined _OPENMP && _OPENMP < 200805
        auto count = std::distance(pStream.begin(), pStream.end());
        auto b = pStream.begin();
        pfor(int i = 0; i < count; i++) {
            auto it = b + i;
#else
        pfor(auto it = pStream.begin(); it < pStream.end(); it++) {
#endif
            for (const auto& tuple : *it) {
                newCtxt[cur.getTupleId()] = tuple.data();
                if (execute(shadow.getCondition(), newCtxt)) {
                    execute(shadow.getNestedOperation(), newCtxt);
                    break;
                }
            }
        }
    PARALLEL_END

    return true;
}

template <typename Shadow>
RamDomain Engine::initValue(const ram::Aggregator& aggregator, const Shadow& shadow, Context& ctxt) {
    if (const auto* ia = as<ram::IntrinsicAggregator>(aggregator)) {
        switch (ia->getFunction()) {
            case AggregateOp::MIN: return ramBitCast(MAX_RAM_SIGNED);
            case AggregateOp::UMIN: return ramBitCast(MAX_RAM_UNSIGNED);
            case AggregateOp::FMIN: return ramBitCast(MAX_RAM_FLOAT);
            case AggregateOp::MAX: return ramBitCast(MIN_RAM_SIGNED);
            case AggregateOp::UMAX: return ramBitCast(MIN_RAM_UNSIGNED);
            case AggregateOp::FMAX: return ramBitCast(MIN_RAM_FLOAT);
            case AggregateOp::SUM: return ramBitCast(static_cast<RamSigned>(0));
            case AggregateOp::USUM: return ramBitCast(static_cast<RamUnsigned>(0));
            case AggregateOp::FSUM: return ramBitCast(static_cast<RamFloat>(0));
            case AggregateOp::MEAN: return 0;
            case AggregateOp::COUNT: return 0;
        }
    } else if (isA<ram::UserDefinedAggregator>(aggregator)) {
        return execute(shadow.getInit(), ctxt);
    }
    fatal("Unhandled aggregator");
}

bool runNested(const ram::Aggregator& aggregator) {
    if (const auto* ia = as<ram::IntrinsicAggregator>(aggregator)) {
        switch (ia->getFunction()) {
            case AggregateOp::COUNT:
            case AggregateOp::FSUM:
            case AggregateOp::USUM:
            case AggregateOp::SUM: return true;
            default: return false;
        }
    } else if (isA<ram::UserDefinedAggregator>(aggregator)) {
        return true;
    }
    return false;
}

void ifIntrinsic(const ram::Aggregator& aggregator, AggregateOp op, std::function<void()> fn) {
    if (const auto* ia = as<ram::IntrinsicAggregator>(aggregator)) {
        if (ia->getFunction() == op) {
            fn();
        };
    }
}

template <typename Aggregate, typename Shadow, typename Iter>
RamDomain Engine::evalAggregate(
        const Aggregate& aggregate, const Shadow& shadow, const Iter& ranges, Context& ctxt) {
    bool shouldRunNested = false;

    const Node& filter = *shadow.getCondition();
    const Node* expression = shadow.getExpr();
    const Node& nestedOperation = *shadow.getNestedOperation();
    // initialize result
    RamDomain res = 0;

    // Use for calculating mean.
    std::pair<RamFloat, RamFloat> accumulateMean = {0, 0};

    const ram::Aggregator& aggregator = aggregate.getAggregator();
    res = initValue(aggregator, shadow, ctxt);
    shouldRunNested = runNested(aggregator);

    for (const auto& tuple : ranges) {
        ctxt[aggregate.getTupleId()] = tuple.data();

        if (!execute(&filter, ctxt)) {
            continue;
        }

        shouldRunNested = true;

        bool isCount = false;
        ifIntrinsic(aggregator, AggregateOp::COUNT, [&]() { isCount = true; });

        // count is a special case.
        if (isCount) {
            ++res;
            continue;
        }

        // eval target expression
        assert(expression);  // only case where this is null is `COUNT`
        RamDomain val = execute(expression, ctxt);

        if (const auto* ia = as<ram::IntrinsicAggregator>(aggregator)) {
            switch (ia->getFunction()) {
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
        } else if (const auto* uda = as<ram::UserDefinedAggregator>(aggregator)) {
            auto userFunctorPtr = reinterpret_cast<void (*)()>(shadow.getFunctionPointer());
            if (uda->isStateful() && userFunctorPtr) {
                res = callStatefulAggregate(userFunctorPtr, &getSymbolTable(), &getRecordTable(), res, val);
            } else {
                fatal("stateless functors not supported in user-defined aggregates");
            }
        } else {
            fatal("Unhandled aggregator");
        }
    }

    ifIntrinsic(aggregator, AggregateOp::MEAN, [&]() {
        if (accumulateMean.second != 0) {
            res = ramBitCast(accumulateMean.first / accumulateMean.second);
        }
    });

    // write result to environment
    souffle::Tuple<RamDomain, 1> tuple;
    tuple[0] = res;
    ctxt[aggregate.getTupleId()] = tuple.data();

    if (!shouldRunNested) {
        return true;
    } else {
        return execute(&nestedOperation, ctxt);
    }
}
template <typename Rel>
RamDomain Engine::evalParallelAggregate(
        const Rel& rel, const ram::ParallelAggregate& cur, const ParallelAggregate& shadow, Context& ctxt) {
    // TODO (rdowavic): make parallel
    auto viewContext = shadow.getViewContext();

    Context newCtxt(ctxt);
    auto viewInfo = viewContext->getViewInfoForNested();
    for (const auto& info : viewInfo) {
        newCtxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
    }
    return evalAggregate(cur, shadow, rel.scan(), newCtxt);
}

template <typename Rel>
RamDomain Engine::evalParallelIndexAggregate(
        const ram::ParallelIndexAggregate& cur, const ParallelIndexAggregate& shadow, Context& ctxt) {
    // TODO (rdowavic): make parallel
    auto viewContext = shadow.getViewContext();

    Context newCtxt(ctxt);
    auto viewInfo = viewContext->getViewInfoForNested();
    for (const auto& info : viewInfo) {
        newCtxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
    }
    // init temporary tuple for this level
    constexpr std::size_t Arity = Rel::Arity;
    const auto& superInfo = shadow.getSuperInst();
    // get lower and upper boundaries for iteration
    souffle::Tuple<RamDomain, Arity> low;
    souffle::Tuple<RamDomain, Arity> high;
    CAL_SEARCH_BOUND(superInfo, low, high);

    std::size_t viewId = shadow.getViewId();
    auto view = Rel::castView(newCtxt.getView(viewId));

    return evalAggregate(cur, shadow, view->range(low, high), newCtxt);
}

template <typename Rel>
RamDomain Engine::evalIndexAggregate(
        const ram::IndexAggregate& cur, const IndexAggregate& shadow, Context& ctxt) {
    // init temporary tuple for this level
    const std::size_t Arity = Rel::Arity;
    const auto& superInfo = shadow.getSuperInst();
    souffle::Tuple<RamDomain, Arity> low;
    souffle::Tuple<RamDomain, Arity> high;
    CAL_SEARCH_BOUND(superInfo, low, high);

    std::size_t viewId = shadow.getViewId();
    auto view = Rel::castView(ctxt.getView(viewId));

    return evalAggregate(cur, shadow, view->range(low, high), ctxt);
}

template <typename Rel>
RamDomain Engine::evalInsert(Rel& rel, const Insert& shadow, Context& ctxt) {
    constexpr std::size_t Arity = Rel::Arity;
    const auto& superInfo = shadow.getSuperInst();
    souffle::Tuple<RamDomain, Arity> tuple;
    TUPLE_COPY_FROM(tuple, superInfo.first);

    /* TupleElement */
    for (const auto& tupleElement : superInfo.tupleFirst) {
        tuple[tupleElement[0]] = ctxt[tupleElement[1]][tupleElement[2]];
    }
    /* Generic */
    for (const auto& expr : superInfo.exprFirst) {
        tuple[expr.first] = execute(expr.second.get(), ctxt);
    }

    // insert in target relation
    rel.insert(tuple);
    return true;
}

template <typename Rel>
RamDomain Engine::evalErase(Rel& rel, const Erase& shadow, Context& ctxt) {
    constexpr std::size_t Arity = Rel::Arity;
    const auto& superInfo = shadow.getSuperInst();
    souffle::Tuple<RamDomain, Arity> tuple;
    TUPLE_COPY_FROM(tuple, superInfo.first);

    /* TupleElement */
    for (const auto& tupleElement : superInfo.tupleFirst) {
        tuple[tupleElement[0]] = ctxt[tupleElement[1]][tupleElement[2]];
    }
    /* Generic */
    for (const auto& expr : superInfo.exprFirst) {
        tuple[expr.first] = execute(expr.second.get(), ctxt);
    }

    // insert in target relation
    rel.erase(tuple);
    return true;
}

template <typename Rel>
RamDomain Engine::evalGuardedInsert(Rel& rel, const GuardedInsert& shadow, Context& ctxt) {
    if (!execute(shadow.getCondition(), ctxt)) {
        return true;
    }

    constexpr std::size_t Arity = Rel::Arity;
    const auto& superInfo = shadow.getSuperInst();
    souffle::Tuple<RamDomain, Arity> tuple;
    TUPLE_COPY_FROM(tuple, superInfo.first);

    /* TupleElement */
    for (const auto& tupleElement : superInfo.tupleFirst) {
        tuple[tupleElement[0]] = ctxt[tupleElement[1]][tupleElement[2]];
    }
    /* Generic */
    for (const auto& expr : superInfo.exprFirst) {
        tuple[expr.first] = execute(expr.second.get(), ctxt);
    }

    // insert in target relation
    rel.insert(tuple);
    return true;
}

}  // namespace souffle::interpreter
