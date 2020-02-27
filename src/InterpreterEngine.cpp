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

#include "InterpreterEngine.h"
#include "IOSystem.h"
#include "InterpreterGenerator.h"
#include "Logger.h"
#include "RamTypes.h"
#include "RecordTable.h"
#include "SignalHandler.h"
#include <cassert>
#include <csignal>
#include <regex>
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

    RamStatement& program = tUnit.getProgram().getMain();
    auto entry = generator.generateTree(program);
    InterpreterContext ctxt;

    if (!profileEnabled) {
        InterpreterContext ctxt;
        execute(entry.get(), ctxt);
    } else {
        ProfileEventSingleton::instance().setOutputFile(Global::config().get("profile"));
        // Prepare the frequency table for threaded use
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
        execute(entry.get(), ctxt);
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
void InterpreterEngine::executeSubroutine(
        const std::string& name, const std::vector<RamDomain>& args, std::vector<RamDomain>& ret) {
    InterpreterContext ctxt;
    ctxt.setReturnValues(ret);
    ctxt.setArguments(args);

    auto entry = generator.generateTree(tUnit.getProgram().getSubroutine(name));
    execute(entry.get(), ctxt);
}

RamDomain InterpreterEngine::execute(const InterpreterNode* node, InterpreterContext& ctxt) {
#define DEBUG(Kind) std::cout << "Running Node: " << #Kind << "\n";

#define CASE(Kind)     \
    case (I_##Kind): { \
        return [&]() -> RamDomain { \
            const auto& cur = *static_cast<const Ram##Kind*>(node->getShadow());
#define CASE_NO_CAST(Kind) \
    case (I_##Kind): {     \
        return [&]() -> RamDomain {
#define ESAC(Kind) \
    }              \
    ();            \
    }

    switch (node->getType()) {
        CASE(Constant)
            return cur.getConstant();
        ESAC(Constant)

        CASE(TupleElement)
            return ctxt[cur.getTupleId()][cur.getElement()];
        ESAC(TupleElement)

        CASE_NO_CAST(AutoIncrement)
            return incCounter();
        ESAC(AutoIncrement)

        CASE(IntrinsicOperator)
#define BINARY_OP(op) return execute(node->getChild(0), ctxt) op execute(node->getChild(1), ctxt)
// clang-format off
#define BINARY_OP_CAST(ty, op) return ramBitCast(       \
    ramBitCast<ty>(execute(node->getChild(0), ctxt)) op \
    ramBitCast<ty>(execute(node->getChild(1), ctxt)) )
#define BINARY_OP_INTEGRAL(opcode, op)                              \
    case FunctorOp::   opcode: { BINARY_OP_CAST(RamSigned  , op); } \
    case FunctorOp::U##opcode: { BINARY_OP_CAST(RamUnsigned, op); }
#define BINARY_OP_NUMERIC(opcode, op)                         \
    BINARY_OP_INTEGRAL(opcode, op)                            \
    case FunctorOp::F##opcode: BINARY_OP_CAST(RamFloat, op);
#define BINARY_OP_CAST_SHIFT_MASK(ty, op) return ramBitCast(                \
    ramBitCast<ty>(execute(node->getChild(0), ctxt)) op                     \
    ramBitCast<ty>(execute(node->getChild(1), ctxt) & RAM_BIT_SHIFT_MASK) )
#define BINARY_OP_INTEGRAL_SHIFT(opcode, op, tySigned, tyUnsigned)              \
    case FunctorOp::   opcode: { BINARY_OP_CAST_SHIFT_MASK(tySigned   , op); }  \
    case FunctorOp::U##opcode: { BINARY_OP_CAST_SHIFT_MASK(tyUnsigned , op); }
            // clang-format on

            const auto& args = cur.getArguments();
            switch (cur.getOperator()) {
                /** Unary Functor Operators */
                case FunctorOp::ORD:
                    return execute(node->getChild(0), ctxt);
                case FunctorOp::STRLEN:
                    return getSymbolTable().resolve(execute(node->getChild(0), ctxt)).size();
                case FunctorOp::NEG:
                    return -execute(node->getChild(0), ctxt);
                case FunctorOp::FNEG: {
                    RamDomain result = execute(node->getChild(0), ctxt);
                    return ramBitCast(-ramBitCast<RamFloat>(result));
                }
                case FunctorOp::BNOT:
                    return ~execute(node->getChild(0), ctxt);
                case FunctorOp::UBNOT: {
                    RamDomain result = execute(node->getChild(0), ctxt);
                    return ramBitCast(~ramBitCast<RamUnsigned>(result));
                }
                case FunctorOp::LNOT:
                    return !execute(node->getChild(0), ctxt);

                case FunctorOp::ULNOT: {
                    RamDomain result = execute(node->getChild(0), ctxt);
                    // Casting is a bit tricky here, since ! returns a boolean.
                    return ramBitCast(static_cast<RamUnsigned>(!ramBitCast<RamUnsigned>(result)));
                }
                case FunctorOp::TONUMBER: {
                    RamDomain result = 0;
                    try {
                        result = stord(getSymbolTable().resolve(execute(node->getChild(0), ctxt)));
                    } catch (...) {
                        std::cerr << "error: wrong string provided by to_number(\"";
                        std::cerr << getSymbolTable().resolve(execute(node->getChild(0), ctxt));
                        std::cerr << "\") functor.\n";
                        raise(SIGFPE);
                    }
                    return result;
                }
                case FunctorOp::TOSTRING:
                    return getSymbolTable().lookup(std::to_string(execute(node->getChild(0), ctxt)));

                /** The following are the default C++ conversions. */
                case FunctorOp::ITOU: {
                    auto result = execute(node->getChild(0), ctxt);
                    return ramBitCast(coerceSafe<RamUnsigned>(result));
                }
                case FunctorOp::UTOI: {
                    auto result = ramBitCast<RamUnsigned>(execute(node->getChild(0), ctxt));
                    return ramBitCast(coerceSafe<RamSigned>(result));
                }
                case FunctorOp::ITOF: {
                    auto result = execute(node->getChild(0), ctxt);
                    return ramBitCast(coerceSafe<RamFloat>(result));
                }
                case FunctorOp::FTOI: {
                    auto result = ramBitCast<RamFloat>(execute(node->getChild(0), ctxt));
                    return ramBitCast(coerceSafe<RamSigned>(result));
                }
                case FunctorOp::UTOF: {
                    auto result = ramBitCast<RamUnsigned>(execute(node->getChild(0), ctxt));
                    return ramBitCast(coerceSafe<RamFloat>(result));
                }
                case FunctorOp::FTOU: {
                    auto result = ramBitCast<RamFloat>(execute(node->getChild(0), ctxt));
                    return ramBitCast(coerceSafe<RamUnsigned>(result));
                }

                    /** Binary Functor Operators */
                    // clang-format off
                BINARY_OP_NUMERIC(ADD, +)
                BINARY_OP_NUMERIC(SUB, -)
                BINARY_OP_NUMERIC(MUL, *)
                BINARY_OP_NUMERIC(DIV, /)
                    // clang-format on

                case FunctorOp::EXP: {
                    return ramBitCast(coerceSafe<RamSigned>(
                            std::pow(execute(node->getChild(0), ctxt), execute(node->getChild(1), ctxt))));
                }

                case FunctorOp::UEXP: {
                    auto first = ramBitCast<RamUnsigned>(execute(node->getChild(0), ctxt));
                    auto second = ramBitCast<RamUnsigned>(execute(node->getChild(1), ctxt));
                    return ramBitCast(coerceSafe<RamUnsigned>(std::pow(first, second)));
                }

                case FunctorOp::FEXP: {
                    auto first = ramBitCast<RamFloat>(execute(node->getChild(0), ctxt));
                    auto second = ramBitCast<RamFloat>(execute(node->getChild(1), ctxt));
                    return ramBitCast(coerceSafe<RamFloat>(std::pow(first, second)));
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
                    // clang-format on

                case FunctorOp::LAND: {
                    BINARY_OP(&&);
                }

                case FunctorOp::ULAND: {
                    auto first = ramBitCast<RamUnsigned>(execute(node->getChild(0), ctxt));
                    auto second = ramBitCast<RamUnsigned>(execute(node->getChild(1), ctxt));
                    // Extra casting required (from bool)
                    return ramBitCast(static_cast<RamUnsigned>(first && second));
                }

                case FunctorOp::LOR: {
                    BINARY_OP(||);
                }

                case FunctorOp::ULOR: {
                    auto first = ramBitCast<RamUnsigned>(execute(node->getChild(0), ctxt));
                    auto second = ramBitCast<RamUnsigned>(execute(node->getChild(1), ctxt));
                    // Extra casting required (from bool)
                    return ramBitCast(static_cast<RamUnsigned>(first || second));
                }

                case FunctorOp::MAX: {
                    auto result = execute(node->getChild(0), ctxt);
                    for (size_t i = 1; i < args.size(); i++) {
                        result = std::max(result, execute(node->getChild(i), ctxt));
                    }
                    return result;
                }

                case FunctorOp::UMAX: {
                    auto result = ramBitCast<RamUnsigned>(execute(node->getChild(0), ctxt));
                    for (size_t i = 1; i < args.size(); i++) {
                        auto element = ramBitCast<RamUnsigned>(execute(node->getChild(i), ctxt));
                        result = std::max(result, element);
                    }
                    return ramBitCast(result);
                }

                case FunctorOp::FMAX: {
                    auto result = ramBitCast<RamFloat>(execute(node->getChild(0), ctxt));
                    for (size_t i = 1; i < args.size(); i++) {
                        auto element = ramBitCast<RamFloat>(execute(node->getChild(i), ctxt));
                        result = std::max(result, element);
                    }
                    return ramBitCast(result);
                }

                case FunctorOp::MIN: {
                    auto result = execute(node->getChild(0), ctxt);
                    for (size_t i = 1; i < args.size(); i++) {
                        result = std::min(result, execute(node->getChild(i), ctxt));
                    }
                    return result;
                }

                case FunctorOp::UMIN: {
                    auto result = ramBitCast<RamUnsigned>(execute(node->getChild(0), ctxt));
                    for (size_t i = 1; i < args.size(); i++) {
                        auto element = ramBitCast<RamUnsigned>(execute(node->getChild(i), ctxt));
                        result = std::min(result, element);
                    }
                    return ramBitCast(result);
                }

                case FunctorOp::FMIN: {
                    auto result = ramBitCast<RamFloat>(execute(node->getChild(0), ctxt));
                    for (size_t i = 1; i < args.size(); i++) {
                        auto element = ramBitCast<RamFloat>(execute(node->getChild(i), ctxt));
                        result = std::min(result, element);
                    }
                    return ramBitCast(result);
                }

                case FunctorOp::CAT: {
                    std::stringstream ss;
                    for (size_t i = 0; i < args.size(); i++) {
                        ss << getSymbolTable().resolve(execute(node->getChild(i), ctxt));
                    }
                    return getSymbolTable().lookup(ss.str());
                }
                /** Ternary Functor Operators */
                case FunctorOp::SUBSTR: {
                    auto symbol = execute(node->getChild(0), ctxt);
                    const std::string& str = getSymbolTable().resolve(symbol);
                    auto idx = execute(node->getChild(1), ctxt);
                    auto len = execute(node->getChild(2), ctxt);
                    std::string sub_str;
                    try {
                        sub_str = str.substr(idx, len);
                    } catch (...) {
                        std::cerr << "warning: wrong index position provided by substr(\"";
                        std::cerr << str << "\"," << (int32_t)idx << "," << (int32_t)len << ") functor.\n";
                    }
                    return getSymbolTable().lookup(sub_str);
                }
                /** Undefined */
                default: {
                    assert(false && "unsupported operator");
                    return 0;
                }
            }
        ESAC(IntrinsicOperator)

        CASE(UserDefinedOperator)
            // get name and type
            const std::string& name = cur.getName();
            const std::vector<TypeAttribute>& type = cur.getArgsTypes();

            auto fn = reinterpret_cast<void (*)()>(getMethodHandle(name));
            if (fn == nullptr) {
                std::cerr << "Cannot find user-defined operator " << name << std::endl;
                exit(1);
            }
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
                RamDomain arg = execute(node->getChild(i), ctxt);
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
                    case TypeAttribute::Record:
                        assert(false && "Record support is not implemented");
                }
            }

            // Get codomain.
            auto codomain = &FFI_RamSigned;
            switch (cur.getReturnType()) {
                // initialize for string value.
                case TypeAttribute::Symbol:
                    codomain = &FFI_Symbol;
                    break;
                case TypeAttribute::Signed:
                    codomain = &FFI_RamSigned;
                    break;
                case TypeAttribute::Unsigned:
                    codomain = &FFI_RamUnsigned;
                    break;
                case TypeAttribute::Float:
                    codomain = &FFI_RamFloat;
                    break;
                default:
                    assert(false && "Not implemented");
            }

            // Call the external function.
            if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, arity, codomain, args) != FFI_OK) {
                std::cerr << "Failed to prepare CIF for user-defined operator ";
                std::cerr << name << std::endl;
                exit(1);
            }
            ffi_call(&cif, fn, &rc, values);

            RamDomain result;
            switch (cur.getReturnType()) {
                case TypeAttribute::Signed:
                    result = static_cast<RamDomain>(rc);
                    break;
                case TypeAttribute::Symbol:
                    result = getSymbolTable().lookup(reinterpret_cast<const char*>(rc));
                    break;
                case TypeAttribute::Unsigned:
                    result = ramBitCast(static_cast<RamUnsigned>(rc));
                    break;
                case TypeAttribute::Float:
                    result = ramBitCast(static_cast<RamFloat>(rc));
                    break;
                default:
                    assert(false && "Not implemented");
            }

            return result;
        ESAC(UserDefinedOperator)

        CASE(PackRecord)
            auto values = cur.getArguments();
            size_t arity = values.size();
            RamDomain data[arity];
            for (size_t i = 0; i < arity; ++i) {
                data[i] = execute(node->getChild(i), ctxt);
            }
            return getRecordTable().pack(data, arity);
        ESAC(PackRecord)

        CASE(SubroutineArgument)
            return ctxt.getArgument(cur.getArgument());
        ESAC(SubroutineArgument)

        CASE_NO_CAST(True)
            return true;
        ESAC(True)

        CASE_NO_CAST(False)
            return false;
        ESAC(False)

        CASE_NO_CAST(Conjunction)
            return execute(node->getChild(0), ctxt) && execute(node->getChild(1), ctxt);
        ESAC(Conjunction)

        CASE_NO_CAST(Negation)
            return !execute(node->getChild(0), ctxt);
        ESAC(Negation)

        CASE_NO_CAST(EmptinessCheck)
            return node->getRelation()->empty();
        ESAC(EmptinessCheck)

        CASE(ExistenceCheck)
            // construct the pattern tuple
            size_t arity = cur.getRelation().getArity();

            size_t viewPos = node->getData(0);

            if (profileEnabled && !cur.getRelation().isTemp()) {
                reads[cur.getRelation().getName()]++;
            }
            // for total we use the exists test
            if (isa->isTotalSignature(&cur)) {
                RamDomain tuple[arity];
                for (size_t i = 0; i < arity; i++) {
                    tuple[i] = execute(node->getChild(i), ctxt);
                }
                return ctxt.getView(viewPos)->contains(TupleRef(tuple, arity));
            }

            // for partial we search for lower and upper boundaries
            RamDomain low[arity];
            RamDomain high[arity];
            for (size_t i = 0; i < node->getChildren().size(); ++i) {
                low[i] = node->getChild(i) != nullptr ? execute(node->getChild(i), ctxt) : MIN_RAM_DOMAIN;
                high[i] = node->getChild(i) != nullptr ? low[i] : MAX_RAM_DOMAIN;
            }
            return ctxt.getView(viewPos)->contains(TupleRef(low, arity), TupleRef(high, arity));
        ESAC(ExistenceCheck)

        CASE(ProvenanceExistenceCheck)
            // construct the pattern tuple
            size_t arity = cur.getRelation().getArity();

            // for partial we search for lower and upper boundaries
            RamDomain low[arity];
            RamDomain high[arity];
            for (size_t i = 0; i < arity - 2; i++) {
                low[i] = node->getChild(i) ? execute(node->getChild(i), ctxt) : MIN_RAM_DOMAIN;
                high[i] = node->getChild(i) ? low[i] : MAX_RAM_DOMAIN;
            }

            low[arity - 2] = MIN_RAM_DOMAIN;
            low[arity - 1] = MIN_RAM_DOMAIN;
            high[arity - 2] = MAX_RAM_DOMAIN;
            high[arity - 1] = MAX_RAM_DOMAIN;

            // obtain view
            size_t viewPos = node->getData(0);
            return ctxt.getView(viewPos)->contains(TupleRef(low, arity), TupleRef(high, arity));
        ESAC(ProvenanceExistenceCheck)

        CASE(Constraint)
            switch (cur.getOperator()) {
                case BinaryConstraintOp::EQ:
                    BINARY_OP(==);
                case BinaryConstraintOp::NE:
                    BINARY_OP(!=);
                case BinaryConstraintOp::LT:
                    BINARY_OP(<);
                case BinaryConstraintOp::ULT: {
                    auto left = ramBitCast<RamUnsigned>(execute(node->getChild(0), ctxt));
                    auto right = ramBitCast<RamUnsigned>(execute(node->getChild(1), ctxt));
                    return left < right;
                }
                case BinaryConstraintOp::FLT: {
                    auto left = ramBitCast<RamFloat>(execute(node->getChild(0), ctxt));
                    auto right = ramBitCast<RamFloat>(execute(node->getChild(1), ctxt));
                    return left < right;
                }
                case BinaryConstraintOp::LE:
                    BINARY_OP(<=);
                case BinaryConstraintOp::ULE: {
                    auto left = ramBitCast<RamUnsigned>(execute(node->getChild(0), ctxt));
                    auto right = ramBitCast<RamUnsigned>(execute(node->getChild(1), ctxt));
                    return left <= right;
                }
                case BinaryConstraintOp::FLE: {
                    auto left = ramBitCast<RamFloat>(execute(node->getChild(0), ctxt));
                    auto right = ramBitCast<RamFloat>(execute(node->getChild(1), ctxt));
                    return left <= right;
                }
                case BinaryConstraintOp::GT:
                    BINARY_OP(>);
                case BinaryConstraintOp::UGT: {
                    auto left = ramBitCast<RamUnsigned>(execute(node->getChild(0), ctxt));
                    auto right = ramBitCast<RamUnsigned>(execute(node->getChild(1), ctxt));
                    return left > right;
                }
                case BinaryConstraintOp::FGT: {
                    auto left = ramBitCast<RamFloat>(execute(node->getChild(0), ctxt));
                    auto right = ramBitCast<RamFloat>(execute(node->getChild(1), ctxt));
                    return left > right;
                }
                case BinaryConstraintOp::GE:
                    BINARY_OP(>=);
                case BinaryConstraintOp::UGE: {
                    auto left = ramBitCast<RamUnsigned>(execute(node->getChild(0), ctxt));
                    auto right = ramBitCast<RamUnsigned>(execute(node->getChild(1), ctxt));
                    return left >= right;
                }
                case BinaryConstraintOp::FGE: {
                    auto left = ramBitCast<RamFloat>(execute(node->getChild(0), ctxt));
                    auto right = ramBitCast<RamFloat>(execute(node->getChild(1), ctxt));
                    return left >= right;
                }
                case BinaryConstraintOp::MATCH: {
                    RamDomain left = execute(node->getChild(0), ctxt);
                    RamDomain right = execute(node->getChild(1), ctxt);
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
                    RamDomain left = execute(node->getChild(0), ctxt);
                    RamDomain right = execute(node->getChild(1), ctxt);
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
                    RamDomain left = execute(node->getChild(0), ctxt);
                    RamDomain right = execute(node->getChild(1), ctxt);
                    const std::string& pattern = getSymbolTable().resolve(left);
                    const std::string& text = getSymbolTable().resolve(right);
                    return text.find(pattern) != std::string::npos;
                }
                case BinaryConstraintOp::NOT_CONTAINS: {
                    RamDomain left = execute(node->getChild(0), ctxt);
                    RamDomain right = execute(node->getChild(1), ctxt);
                    const std::string& pattern = getSymbolTable().resolve(left);
                    const std::string& text = getSymbolTable().resolve(right);
                    return text.find(pattern) == std::string::npos;
                }
                default:
                    assert(false && "unsupported operator");
                    return false;
            }
        ESAC(Constraint)

        CASE(TupleOperation)
            bool result = execute(node->getChild(0), ctxt);

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
                if (!execute(node->getChild(0), ctxt)) {
                    break;
                }
            }
            return true;
        ESAC(Scan)

        CASE(ParallelScan)
            auto preamble = node->getPreamble();
            auto& rel = *node->getRelation();

            auto pStream = rel.partitionScan(numOfThreads);

            PARALLEL_START
                ;
                InterpreterContext newCtxt(ctxt);
                auto viewInfo = preamble->getViewInfoForNested();
                for (const auto& info : viewInfo) {
                    newCtxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
                }
                pfor(auto it = pStream.begin(); it < pStream.end(); it++) {
                    for (const TupleRef& val : *it) {
                        newCtxt[cur.getTupleId()] = val.getBase();
                        if (!execute(node->getChild(0), newCtxt)) {
                            break;
                        }
                    }
                }
            PARALLEL_END;
            return true;
        ESAC(ParallelScan)

        CASE(IndexScan)
            // create pattern tuple for range query
            size_t arity = cur.getRelation().getArity();
            RamDomain low[arity];
            RamDomain hig[arity];
            for (size_t i = 0; i < arity; i++) {
                if (node->getChild(i) != nullptr) {
                    low[i] = execute(node->getChild(i), ctxt);
                    hig[i] = low[i];
                } else {
                    low[i] = MIN_RAM_DOMAIN;
                    hig[i] = MAX_RAM_DOMAIN;
                }
            }

            size_t viewId = node->getData(0);
            auto& view = ctxt.getView(viewId);
            // conduct range query
            for (auto data : view->range(TupleRef(low, arity), TupleRef(hig, arity))) {
                ctxt[cur.getTupleId()] = &data[0];
                if (!execute(node->getChild(arity), ctxt)) {
                    break;
                }
            }
            return true;
        ESAC(IndexScan)

        CASE(ParallelIndexScan)
            auto preamble = node->getPreamble();
            auto& rel = *node->getRelation();

            // create pattern tuple for range query
            size_t arity = rel.getArity();
            RamDomain low[arity];
            RamDomain hig[arity];
            for (size_t i = 0; i < arity; i++) {
                if (node->getChild(i)) {
                    low[i] = execute(node->getChild(i), ctxt);
                    hig[i] = low[i];
                } else {
                    low[i] = MIN_RAM_DOMAIN;
                    hig[i] = MAX_RAM_DOMAIN;
                }
            }

            size_t indexPos = node->getData(0);
            auto pStream =
                    rel.partitionRange(indexPos, TupleRef(low, arity), TupleRef(hig, arity), numOfThreads);

            PARALLEL_START
                ;
                InterpreterContext newCtxt(ctxt);
                auto viewInfo = preamble->getViewInfoForNested();
                for (const auto& info : viewInfo) {
                    newCtxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
                }
                pfor(auto it = pStream.begin(); it < pStream.end(); it++) {
                    for (const TupleRef& val : *it) {
                        newCtxt[cur.getTupleId()] = val.getBase();
                        if (!execute(node->getChild(arity), newCtxt)) {
                            break;
                        }
                    }
                }
            PARALLEL_END;

            return true;
        ESAC(ParallelIndexScan)

        CASE(Choice)
            // get the targeted relation
            auto& rel = *node->getRelation();

            // use simple iterator
            for (const RamDomain* tuple : rel) {
                ctxt[cur.getTupleId()] = tuple;
                if (execute(node->getChild(0), ctxt)) {
                    execute(node->getChild(1), ctxt);
                    break;
                }
            }
            return true;
        ESAC(Choice)

        CASE(ParallelChoice)
            auto preamble = node->getPreamble();
            auto& rel = *node->getRelation();

            auto pStream = rel.partitionScan(numOfThreads);
            auto viewInfo = preamble->getViewInfoForNested();
            PARALLEL_START
                ;
                InterpreterContext newCtxt(ctxt);
                for (const auto& info : viewInfo) {
                    newCtxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
                }
                pfor(auto it = pStream.begin(); it < pStream.end(); it++) {
                    for (const TupleRef& val : *it) {
                        newCtxt[cur.getTupleId()] = val.getBase();
                        if (execute(node->getChild(0), newCtxt)) {
                            execute(node->getChild(1), newCtxt);
                            break;
                        }
                    }
                }
            PARALLEL_END;
            return true;
        ESAC(ParallelChoice)

        CASE(IndexChoice)
            // create pattern tuple for range query
            size_t arity = cur.getRelation().getArity();
            RamDomain low[arity];
            RamDomain hig[arity];
            for (size_t i = 0; i < arity; i++) {
                if (node->getChild(i) != nullptr) {
                    low[i] = execute(node->getChild(i), ctxt);
                    hig[i] = low[i];
                } else {
                    low[i] = MIN_RAM_DOMAIN;
                    hig[i] = MAX_RAM_DOMAIN;
                }
            }

            size_t viewId = node->getData(0);
            auto& view = ctxt.getView(viewId);

            for (auto ip : view->range(TupleRef(low, arity), TupleRef(hig, arity))) {
                const RamDomain* data = &ip[0];
                ctxt[cur.getTupleId()] = data;
                if (execute(node->getChild(arity), ctxt)) {
                    execute(node->getChild(arity + 1), ctxt);
                    break;
                }
            }
            return true;
        ESAC(IndexChoice)

        CASE(ParallelIndexChoice)
            auto preamble = node->getPreamble();
            auto& rel = *node->getRelation();

            auto viewInfo = preamble->getViewInfoForNested();

            // create pattern tuple for range query
            size_t arity = rel.getArity();
            RamDomain low[arity];
            RamDomain hig[arity];
            for (size_t i = 0; i < arity; i++) {
                if (node->getChild(i) != nullptr) {
                    low[i] = execute(node->getChild(i), ctxt);
                    hig[i] = low[i];
                } else {
                    low[i] = MIN_RAM_DOMAIN;
                    hig[i] = MAX_RAM_DOMAIN;
                }
            }

            size_t indexPos = node->getData(0);
            auto pStream =
                    rel.partitionRange(indexPos, TupleRef(low, arity), TupleRef(hig, arity), numOfThreads);

            PARALLEL_START
                ;
                InterpreterContext newCtxt(ctxt);
                for (const auto& info : viewInfo) {
                    newCtxt.createView(*getRelationHandle(info[0]), info[1], info[2]);
                }
                pfor(auto it = pStream.begin(); it < pStream.end(); it++) {
                    for (const TupleRef& val : *it) {
                        newCtxt[cur.getTupleId()] = val.getBase();
                        if (execute(node->getChild(arity), newCtxt)) {
                            execute(node->getChild(arity + 1), newCtxt);
                            break;
                        }
                    }
                }
            PARALLEL_END;

            return true;
        ESAC(ParallelIndexChoice)

        CASE(UnpackRecord)
            RamDomain ref = execute(node->getChild(0), ctxt);

            // check for nil
            if (getRecordTable().isNil(ref)) {
                return true;
            }

            // update environment variable
            size_t arity = cur.getArity();
            const RamDomain* tuple = getRecordTable().unpack(ref, arity);

            // save reference to temporary value
            ctxt[cur.getTupleId()] = tuple;

            // run nested part - using base class visitor
            return execute(node->getChild(1), ctxt);
        ESAC(UnpackRecord)

        CASE(Aggregate)
            // get the targeted relation
            const InterpreterRelation& rel = *node->getRelation();

            // initialize result
            RamDomain res = 0;
            switch (cur.getFunction()) {
                case souffle::MIN:
                    res = MAX_RAM_DOMAIN;
                    break;
                case souffle::MAX:
                    res = MIN_RAM_DOMAIN;
                    break;
                case souffle::COUNT:
                    res = 0;
                    break;
                case souffle::SUM:
                    res = 0;
                    break;
            }

            for (const RamDomain* data : rel) {
                ctxt[cur.getTupleId()] = data;

                if (!execute(node->getChild(0), ctxt)) {
                    continue;
                }

                // count is easy
                if (cur.getFunction() == souffle::COUNT) {
                    ++res;
                    continue;
                }

                // aggregation is a bit more difficult

                // eval target expression
                RamDomain val = execute(node->getChild(1), ctxt);

                switch (cur.getFunction()) {
                    case souffle::MIN:
                        res = std::min(res, val);
                        break;
                    case souffle::MAX:
                        res = std::max(res, val);
                        break;
                    case souffle::COUNT:
                        res = 0;
                        break;
                    case souffle::SUM:
                        res += val;
                        break;
                }
            }

            // write result to environment
            RamDomain tuple[1];
            tuple[0] = res;
            ctxt[cur.getTupleId()] = tuple;

            if (cur.getFunction() == souffle::MAX && res == MIN_RAM_DOMAIN) {
                // no maximum found
                return true;
            } else if (cur.getFunction() == souffle::MIN && res == MAX_RAM_DOMAIN) {
                // no minimum found
                return true;
            } else {
                // run nested part - using base class visitor
                return execute(node->getChild(2), ctxt);
            }
        ESAC(Aggregate)

        CASE(IndexAggregate)
            // initialize result
            RamDomain res = 0;
            switch (cur.getFunction()) {
                case souffle::MIN:
                    res = MAX_RAM_DOMAIN;
                    break;
                case souffle::MAX:
                    res = MIN_RAM_DOMAIN;
                    break;
                case souffle::COUNT:
                    res = 0;
                    break;
                case souffle::SUM:
                    res = 0;
                    break;
            }

            // init temporary tuple for this level
            size_t arity = cur.getRelation().getArity();

            // get lower and upper boundaries for iteration
            RamDomain low[arity];
            RamDomain hig[arity];

            for (size_t i = 0; i < arity; i++) {
                if (node->getChild(i) != nullptr) {
                    low[i] = execute(node->getChild(i), ctxt);
                    hig[i] = low[i];
                } else {
                    low[i] = MIN_RAM_DOMAIN;
                    hig[i] = MAX_RAM_DOMAIN;
                }
            }

            size_t viewId = node->getData(0);
            auto& view = ctxt.getView(viewId);

            for (auto ip : view->range(TupleRef(low, arity), TupleRef(hig, arity))) {
                // link tuple
                const RamDomain* data = &ip[0];
                ctxt[cur.getTupleId()] = data;

                if (!execute(node->getChild(arity), ctxt)) {
                    continue;
                }

                // count is easy
                if (cur.getFunction() == souffle::COUNT) {
                    ++res;
                    continue;
                }

                // aggregation is a bit more difficult

                // eval target expression
                RamDomain val = execute(node->getChild(arity + 1), ctxt);

                switch (cur.getFunction()) {
                    case souffle::MIN:
                        res = std::min(res, val);
                        break;
                    case souffle::MAX:
                        res = std::max(res, val);
                        break;
                    case souffle::COUNT:
                        res = 0;
                        break;
                    case souffle::SUM:
                        res += val;
                        break;
                }
            }

            // write result to environment
            RamDomain tuple[1];
            tuple[0] = res;
            ctxt[cur.getTupleId()] = tuple;

            // run nested part - using base class visitor
            if (cur.getFunction() == souffle::MAX && res == MIN_RAM_DOMAIN) {
                // no maximum found
                return true;
            } else if (cur.getFunction() == souffle::MIN && res == MAX_RAM_DOMAIN) {
                // no minimum found
                return true;
            } else {
                // run nested part - using base class visitor
                return execute(node->getChild(arity + 2), ctxt);
            }
        ESAC(IndexAggregate)

        CASE_NO_CAST(Break)
            // check condition
            if (execute(node->getChild(0), ctxt)) {
                return false;
            }
            return execute(node->getChild(1), ctxt);
        ESAC(Break)

        CASE(Filter)
            bool result = true;
            // check condition
            if (execute(node->getChild(0), ctxt)) {
                // process nested
                result = execute(node->getChild(1), ctxt);
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
            size_t arity = cur.getRelation().getArity();
            RamDomain tuple[arity];
            for (size_t i = 0; i < arity; i++) {
                tuple[i] = execute(node->getChild(i), ctxt);
            }

            // insert in target relation
            InterpreterRelation& rel = *node->getRelation();
            rel.insert(tuple);
            return true;
        ESAC(Project)

        CASE(SubroutineReturnValue)
            for (size_t i = 0; i < cur.getValues().size(); ++i) {
                if (node->getChild(i) == nullptr) {
                    ctxt.addReturnValue(0);
                } else {
                    ctxt.addReturnValue(execute(node->getChild(i), ctxt));
                }
            }
            return true;
        ESAC(SubroutineReturnValue)

        CASE_NO_CAST(Sequence)
            for (const auto& child : node->getChildren()) {
                if (!execute(child.get(), ctxt)) {
                    return false;
                }
            }
            return true;
        ESAC(Sequence)

        CASE_NO_CAST(Parallel)
            for (const auto& child : node->getChildren()) {
                if (!execute(child.get(), ctxt)) {
                    return false;
                }
            }
            return true;
        ESAC(Parallel)

        CASE_NO_CAST(Loop)
            resetIterationNumber();
            while (execute(node->getChild(0), ctxt)) {
                incIterationNumber();
            }
            resetIterationNumber();
            return true;
        ESAC(Loop)

        CASE_NO_CAST(Exit)
            return !execute(node->getChild(0), ctxt);
        ESAC(Exit)

        CASE(LogRelationTimer)
            Logger logger(cur.getMessage(), getIterationNumber(),
                    std::bind(&InterpreterRelation::size, node->getRelation()));
            return execute(node->getChild(0), ctxt);
        ESAC(LogRelationTimer)

        CASE(LogTimer)
            Logger logger(cur.getMessage(), getIterationNumber());
            return execute(node->getChild(0), ctxt);
        ESAC(LogTimer)

        CASE(DebugInfo)
            SignalHandler::instance()->setMsg(cur.getMessage().c_str());
            return execute(node->getChild(0), ctxt);
        ESAC(DebugInfo)

        CASE_NO_CAST(Clear)
            node->getRelation()->purge();
            return true;
        ESAC(Clear)

        CASE(LogSize)
            const InterpreterRelation& rel = *node->getRelation();
            ProfileEventSingleton::instance().makeQuantityEvent(
                    cur.getMessage(), rel.size(), getIterationNumber());
            return true;
        ESAC(LogSize)

        CASE(Load)
            try {
                for (IODirectives ioDirectives : cur.getIODirectives()) {
                    InterpreterRelation& relation = *node->getRelation();
                    IOSystem::getInstance()
                            .getReader(ioDirectives, getSymbolTable(), getRecordTable())
                            ->readAll(relation);
                }
            } catch (std::exception& e) {
                std::cerr << "Error loading data: " << e.what() << "\n";
            }
            return true;
        ESAC(Load)

        CASE(Store)
            try {
                for (IODirectives ioDirectives : cur.getIODirectives()) {
                    IOSystem::getInstance()
                            .getWriter(ioDirectives, getSymbolTable(), getRecordTable())
                            ->writeAll(*node->getRelation());
                }
            } catch (std::exception& e) {
                std::cerr << e.what();
                exit(1);
            }
            return true;
        ESAC(Store)

        CASE_NO_CAST(Query)
            InterpreterPreamble* preamble = node->getPreamble();

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
            execute(node->getChild(0), ctxt);
            return true;
        ESAC(Query)

        CASE_NO_CAST(Extend)
            InterpreterRelation& src = *getRelationHandle(node->getData(0));
            InterpreterRelation& trg = *getRelationHandle(node->getData(1));
            src.extend(trg);
            trg.insert(src);
            return true;
        ESAC(Extend)

        CASE_NO_CAST(Swap)
            swapRelation(node->getData(0), node->getData(1));
            return true;
        ESAC(Swap)

        default:
            assert(false && "Unhandled\n");
    }
}

}  // namespace souffle
