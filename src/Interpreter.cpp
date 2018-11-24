/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Interpreter.cpp
 *
 * Implementation of Souffle's interpreter.
 *
 ***********************************************************************/

#include "Interpreter.h"
#include "BinaryConstraintOps.h"
#include "BinaryFunctorOps.h"
#include "Global.h"
#include "IODirectives.h"
#include "IOSystem.h"
#include "InterpreterIndex.h"
#include "InterpreterRecords.h"
#include "LogStatement.h"
#include "Logger.h"
#include "ParallelUtils.h"
#include "ProfileEvent.h"
#include "RamNode.h"
#include "RamOperation.h"
#include "RamValue.h"
#include "RamVisitor.h"
#include "ReadStream.h"
#include "SignalHandler.h"
#include "SymbolTable.h"
#include "TernaryFunctorOps.h"
#include "UnaryFunctorOps.h"
#include "WriteStream.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <stdexcept>
#include <typeinfo>
#include <utility>
#include <ffi.h>

namespace souffle {

/** Evaluate RAM Value */
RamDomain Interpreter::evalVal(const RamValue& value, const InterpreterContext& ctxt) {
    class ValueEvaluator : public RamVisitor<RamDomain> {
        Interpreter& interpreter;
        const InterpreterContext& ctxt;

    public:
        ValueEvaluator(Interpreter& interp, const InterpreterContext& ctxt)
                : interpreter(interp), ctxt(ctxt) {}

        RamDomain visitNumber(const RamNumber& num) override {
            return num.getConstant();
        }

        RamDomain visitElementAccess(const RamElementAccess& access) override {
            return ctxt[access.getLevel()][access.getElement()];
        }

        RamDomain visitAutoIncrement(const RamAutoIncrement&) override {
            return interpreter.incCounter();
        }

        // unary operators
        RamDomain visitUnaryOperator(const RamUnaryOperator& op) override {
            RamDomain arg = visit(op.getValue());
            switch (op.getOperator()) {
                case UnaryOp::NEG:
                    return -arg;
                case UnaryOp::BNOT:
                    return ~arg;
                case UnaryOp::LNOT:
                    return !arg;
                case UnaryOp::ORD:
                    return arg;
                case UnaryOp::STRLEN:
                    return interpreter.getSymbolTable().resolve(arg).size();
                case UnaryOp::TONUMBER: {
                    RamDomain result = 0;
                    try {
                        result = stord(interpreter.getSymbolTable().resolve(arg));
                    } catch (...) {
                        std::cerr << "error: wrong string provided by to_number(\"";
                        std::cerr << interpreter.getSymbolTable().resolve(arg);
                        std::cerr << "\") functor.\n";
                        raise(SIGFPE);
                    }
                    return result;
                }
                case UnaryOp::TOSTRING:
                    return interpreter.getSymbolTable().lookup(std::to_string(arg));
                default:
                    assert(false && "unsupported operator");
                    return 0;
            }
        }

        RamDomain visitUserDefinedOperator(const RamUserDefinedOperator& op) override {
            // get name and type
            const std::string& name = op.getName();
            const std::string& type = op.getType();

            // load DLL (if not done yet)
            void* handle = interpreter.loadDLL();
            void (*fn)() = (void (*)())dlsym(handle, name.c_str());
            if (fn == nullptr) {
                std::cerr << "Cannot find user-defined operator " << name << " in " << SOUFFLE_DLL
                          << std::endl;
                exit(1);
            }

            // prepare dynamic call environment
            size_t arity = op.getArgCount();
            ffi_cif cif;
            ffi_type* args[arity];
            void* values[arity];
            RamDomain intVal[arity];
            const char* strVal[arity];
            ffi_arg rc;

            /* Initialize arguments for ffi-call */
            for (size_t i = 0; i < arity; i++) {
                RamDomain arg = visit(op.getArg(i));
                if (type[i] == 'S') {
                    args[i] = &ffi_type_pointer;
                    strVal[i] = interpreter.getSymbolTable().resolve(arg).c_str();
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
                result = interpreter.getSymbolTable().lookup(((const char*)rc));
            }

            return result;
        }

        // binary functors
        RamDomain visitBinaryOperator(const RamBinaryOperator& op) override {
            RamDomain lhs = visit(op.getLHS());
            RamDomain rhs = visit(op.getRHS());
            switch (op.getOperator()) {
                case BinaryOp::ADD: {
                    return lhs + rhs;
                }
                case BinaryOp::SUB: {
                    return lhs - rhs;
                }
                case BinaryOp::MUL: {
                    return lhs * rhs;
                }
                case BinaryOp::DIV: {
                    return lhs / rhs;
                }
                case BinaryOp::EXP: {
                    return std::pow(lhs, rhs);
                }
                case BinaryOp::MOD: {
                    return lhs % rhs;
                }
                case BinaryOp::BAND: {
                    return lhs & rhs;
                }
                case BinaryOp::BOR: {
                    return lhs | rhs;
                }
                case BinaryOp::BXOR: {
                    return lhs ^ rhs;
                }
                case BinaryOp::LAND: {
                    return lhs && rhs;
                }
                case BinaryOp::LOR: {
                    return lhs || rhs;
                }
                case BinaryOp::MAX: {
                    return std::max(lhs, rhs);
                }
                case BinaryOp::MIN: {
                    return std::min(lhs, rhs);
                }
                case BinaryOp::CAT: {
                    return interpreter.getSymbolTable().lookup(interpreter.getSymbolTable().resolve(lhs) +
                                                               interpreter.getSymbolTable().resolve(rhs));
                }
                default:
                    assert(false && "unsupported operator");
                    return 0;
            }
        }

        // ternary operators
        RamDomain visitTernaryOperator(const RamTernaryOperator& op) override {
            switch (op.getOperator()) {
                case TernaryOp::SUBSTR: {
                    auto symbol = visit(op.getArg(0));
                    const std::string& str = interpreter.getSymbolTable().resolve(symbol);
                    auto idx = visit(op.getArg(1));
                    auto len = visit(op.getArg(2));
                    std::string sub_str;
                    try {
                        sub_str = str.substr(idx, len);
                    } catch (...) {
                        std::cerr << "warning: wrong index position provided by substr(\"";
                        std::cerr << str << "\"," << (int32_t)idx << "," << (int32_t)len << ") functor.\n";
                    }
                    return interpreter.getSymbolTable().lookup(sub_str);
                }
                default:
                    assert(false && "unsupported operator");
                    return 0;
            }
        }

        // -- records --
        RamDomain visitPack(const RamPack& op) override {
            auto values = op.getValues();
            auto arity = values.size();
            RamDomain data[arity];
            for (size_t i = 0; i < arity; ++i) {
                data[i] = visit(values[i]);
            }
            return pack(data, arity);
        }

        // -- subroutine argument
        RamDomain visitArgument(const RamArgument& arg) override {
            return ctxt.getArgument(arg.getArgCount());
        }

        // -- safety net --

        RamDomain visitNode(const RamNode& node) override {
            std::cerr << "Unsupported node type: " << typeid(node).name() << "\n";
            assert(false && "Unsupported Node Type!");
            return 0;
        }
    };

    // create and run evaluator
    return ValueEvaluator(*this, ctxt)(value);
}

/** Evaluate RAM Condition */
bool Interpreter::evalCond(const RamCondition& cond, const InterpreterContext& ctxt) {
    class ConditionEvaluator : public RamVisitor<bool> {
        Interpreter& interpreter;
        const InterpreterContext& ctxt;

    public:
        ConditionEvaluator(Interpreter& interp, const InterpreterContext& ctxt)
                : interpreter(interp), ctxt(ctxt) {}

        // -- connectors operators --

        bool visitAnd(const RamAnd& a) override {
            return visit(a.getLHS()) && visit(a.getRHS());
        }

        // -- relation operations --

        bool visitEmpty(const RamEmpty& empty) override {
            const InterpreterRelation& rel = interpreter.getRelation(empty.getRelation());
            return rel.empty();
        }

        bool visitNotExists(const RamNotExists& ne) override {
            const InterpreterRelation& rel = interpreter.getRelation(ne.getRelation());

            // construct the pattern tuple
            auto arity = rel.getArity();
            auto values = ne.getValues();

            if (Global::config().has("profile") && !ne.getRelation().isTemp()) {
                interpreter.reads[ne.getRelation().getName()]++;
            }
            // for total we use the exists test
            if (ne.isTotal()) {
                RamDomain tuple[arity];
                for (size_t i = 0; i < arity; i++) {
                    tuple[i] = (values[i]) ? interpreter.evalVal(*values[i], ctxt) : MIN_RAM_DOMAIN;
                }

                return !rel.exists(tuple);
            }

            // for partial we search for lower and upper boundaries
            RamDomain low[arity];
            RamDomain high[arity];
            for (size_t i = 0; i < arity; i++) {
                low[i] = (values[i]) ? interpreter.evalVal(*values[i], ctxt) : MIN_RAM_DOMAIN;
                high[i] = (values[i]) ? low[i] : MAX_RAM_DOMAIN;
            }

            // obtain index
            auto idx = rel.getIndex(ne.getKey());
            auto range = idx->lowerUpperBound(low, high);
            return range.first == range.second;  // if there are none => done
        }

        bool visitProvenanceNotExists(const RamProvenanceNotExists& ne) override {
            const InterpreterRelation& rel = interpreter.getRelation(ne.getRelation());

            // construct the pattern tuple
            auto arity = rel.getArity();
            auto values = ne.getValues();

            // for partial we search for lower and upper boundaries
            RamDomain low[arity];
            RamDomain high[arity];
            for (size_t i = 0; i < arity - 2; i++) {
                low[i] = (values[i]) ? interpreter.evalVal(*values[i], ctxt) : MIN_RAM_DOMAIN;
                high[i] = (values[i]) ? low[i] : MAX_RAM_DOMAIN;
            }

            low[arity - 2] = MIN_RAM_DOMAIN;
            low[arity - 1] = MIN_RAM_DOMAIN;
            high[arity - 2] = MAX_RAM_DOMAIN;
            high[arity - 1] = MAX_RAM_DOMAIN;

            // obtain index
            auto idx = rel.getIndex(ne.getKey());
            auto range = idx->lowerUpperBound(low, high);
            return range.first == range.second;  // if there are none => done
        }

        // -- comparison operators --
        bool visitBinaryRelation(const RamBinaryRelation& relOp) override {
            RamDomain lhs = interpreter.evalVal(*relOp.getLHS(), ctxt);
            RamDomain rhs = interpreter.evalVal(*relOp.getRHS(), ctxt);
            switch (relOp.getOperator()) {
                case BinaryConstraintOp::EQ:
                    return lhs == rhs;
                case BinaryConstraintOp::NE:
                    return lhs != rhs;
                case BinaryConstraintOp::LT:
                    return lhs < rhs;
                case BinaryConstraintOp::LE:
                    return lhs <= rhs;
                case BinaryConstraintOp::GT:
                    return lhs > rhs;
                case BinaryConstraintOp::GE:
                    return lhs >= rhs;
                case BinaryConstraintOp::MATCH: {
                    RamDomain l = interpreter.evalVal(*relOp.getLHS(), ctxt);
                    RamDomain r = interpreter.evalVal(*relOp.getRHS(), ctxt);
                    const std::string& pattern = interpreter.getSymbolTable().resolve(l);
                    const std::string& text = interpreter.getSymbolTable().resolve(r);
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
                    RamDomain l = interpreter.evalVal(*relOp.getLHS(), ctxt);
                    RamDomain r = interpreter.evalVal(*relOp.getRHS(), ctxt);
                    const std::string& pattern = interpreter.getSymbolTable().resolve(l);
                    const std::string& text = interpreter.getSymbolTable().resolve(r);
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
                    RamDomain l = interpreter.evalVal(*relOp.getLHS(), ctxt);
                    RamDomain r = interpreter.evalVal(*relOp.getRHS(), ctxt);
                    const std::string& pattern = interpreter.getSymbolTable().resolve(l);
                    const std::string& text = interpreter.getSymbolTable().resolve(r);
                    return text.find(pattern) != std::string::npos;
                }
                case BinaryConstraintOp::NOT_CONTAINS: {
                    RamDomain l = interpreter.evalVal(*relOp.getLHS(), ctxt);
                    RamDomain r = interpreter.evalVal(*relOp.getRHS(), ctxt);
                    const std::string& pattern = interpreter.getSymbolTable().resolve(l);
                    const std::string& text = interpreter.getSymbolTable().resolve(r);
                    return text.find(pattern) == std::string::npos;
                }
                default:
                    assert(false && "unsupported operator");
                    return false;
            }
        }
        bool visitNode(const RamNode& node) override {
            std::cerr << "Unsupported node type: " << typeid(node).name() << "\n";
            assert(false && "Unsupported Node Type!");
            return false;
        }
    };

    // run evaluator
    return ConditionEvaluator(*this, ctxt)(cond);
}

/** Evaluate RAM operation */
void Interpreter::evalOp(const RamOperation& op, const InterpreterContext& args) {
    class OperationEvaluator : public RamVisitor<void> {
        Interpreter& interpreter;
        InterpreterContext& ctxt;

    public:
        OperationEvaluator(Interpreter& interp, InterpreterContext& ctxt) : interpreter(interp), ctxt(ctxt) {}

        // -- Operations -----------------------------

        void visitSearch(const RamSearch& search) override {
            // check condition
            auto condition = search.getCondition();
            if (!condition || interpreter.evalCond(*condition, ctxt)) {
                // process nested
                visit(*search.getNestedOperation());
            }

            if (Global::config().has("profile") && !search.getProfileText().empty()) {
                interpreter.frequencies[search.getProfileText()][interpreter.getIterationNumber()]++;
            }
        }

        void visitScan(const RamScan& scan) override {
            // get the targeted relation
            const InterpreterRelation& rel = interpreter.getRelation(scan.getRelation());

            // process full scan if no index is given
            if (scan.getRangeQueryColumns() == 0) {
                // if scan is not binding anything => check for emptiness
                if (scan.isPureExistenceCheck() && !rel.empty()) {
                    visitSearch(scan);
                    return;
                }

                // if scan is unrestricted => use simple iterator
                for (const RamDomain* cur : rel) {
                    ctxt[scan.getLevel()] = cur;
                    visitSearch(scan);
                }
                return;
            }

            // create pattern tuple for range query
            auto arity = rel.getArity();
            RamDomain low[arity];
            RamDomain hig[arity];
            auto pattern = scan.getRangePattern();
            for (size_t i = 0; i < arity; i++) {
                if (pattern[i] != nullptr) {
                    low[i] = interpreter.evalVal(*pattern[i], ctxt);
                    hig[i] = low[i];
                } else {
                    low[i] = MIN_RAM_DOMAIN;
                    hig[i] = MAX_RAM_DOMAIN;
                }
            }

            // obtain index
            auto idx = rel.getIndex(scan.getRangeQueryColumns(), nullptr);

            // get iterator range
            auto range = idx->lowerUpperBound(low, hig);

            // if this scan is not binding anything ...
            if (scan.isPureExistenceCheck()) {
                if (range.first != range.second) {
                    visitSearch(scan);
                }
                if (Global::config().has("profile") && !scan.getProfileText().empty()) {
                    interpreter.frequencies[scan.getProfileText()][interpreter.getIterationNumber()]++;
                }
                return;
            }

            // conduct range query
            for (auto ip = range.first; ip != range.second; ++ip) {
                const RamDomain* data = *(ip);
                ctxt[scan.getLevel()] = data;
                visitSearch(scan);
            }
        }

        void visitLookup(const RamLookup& lookup) override {
            // get reference
            RamDomain ref = ctxt[lookup.getReferenceLevel()][lookup.getReferencePosition()];

            // check for null
            if (isNull(ref)) {
                return;
            }

            // update environment variable
            auto arity = lookup.getArity();
            const RamDomain* tuple = unpack(ref, arity);

            // save reference to temporary value
            ctxt[lookup.getLevel()] = tuple;

            // run nested part - using base class visitor
            visitSearch(lookup);
        }

        void visitAggregate(const RamAggregate& aggregate) override {
            // get the targeted relation
            const InterpreterRelation& rel = interpreter.getRelation(aggregate.getRelation());

            // initialize result
            RamDomain res = 0;
            switch (aggregate.getFunction()) {
                case RamAggregate::MIN:
                    res = MAX_RAM_DOMAIN;
                    break;
                case RamAggregate::MAX:
                    res = MIN_RAM_DOMAIN;
                    break;
                case RamAggregate::COUNT:
                    res = 0;
                    break;
                case RamAggregate::SUM:
                    res = 0;
                    break;
            }

            // init temporary tuple for this level
            auto arity = rel.getArity();

            // get lower and upper boundaries for iteration
            const auto& pattern = aggregate.getPattern();
            RamDomain low[arity];
            RamDomain hig[arity];

            for (size_t i = 0; i < arity; i++) {
                if (pattern[i] != nullptr) {
                    low[i] = interpreter.evalVal(*pattern[i], ctxt);
                    hig[i] = low[i];
                } else {
                    low[i] = MIN_RAM_DOMAIN;
                    hig[i] = MAX_RAM_DOMAIN;
                }
            }

            // obtain index
            auto idx = rel.getIndex(aggregate.getRangeQueryColumns());

            // get iterator range
            auto range = idx->lowerUpperBound(low, hig);

            // check for emptiness
            if (aggregate.getFunction() != RamAggregate::COUNT) {
                if (range.first == range.second) {
                    return;  // no elements => no min/max
                }
            }

            // iterate through values
            for (auto ip = range.first; ip != range.second; ++ip) {
                // link tuple
                const RamDomain* data = *(ip);
                ctxt[aggregate.getLevel()] = data;

                // count is easy
                if (aggregate.getFunction() == RamAggregate::COUNT) {
                    res++;
                    continue;
                }

                // aggregation is a bit more difficult

                // eval target expression
                RamDomain cur = interpreter.evalVal(*aggregate.getTargetExpression(), ctxt);

                switch (aggregate.getFunction()) {
                    case RamAggregate::MIN:
                        res = std::min(res, cur);
                        break;
                    case RamAggregate::MAX:
                        res = std::max(res, cur);
                        break;
                    case RamAggregate::COUNT:
                        res = 0;
                        break;
                    case RamAggregate::SUM:
                        res += cur;
                        break;
                }
            }

            // write result to environment
            RamDomain tuple[1];
            tuple[0] = res;
            ctxt[aggregate.getLevel()] = tuple;

            // check whether result is used in a condition
            auto condition = aggregate.getCondition();
            if (condition && !interpreter.evalCond(*condition, ctxt)) {
                return;  // condition not valid => skip nested
            }

            // run nested part - using base class visitor
            visitSearch(aggregate);
        }

        void visitProject(const RamProject& project) override {
            // check constraints
            RamCondition* condition = project.getCondition();
            if (condition && !interpreter.evalCond(*condition, ctxt)) {
                return;  // condition violated => skip insert
            }

            // create a tuple of the proper arity (also supports arity 0)
            auto arity = project.getRelation().getArity();
            const auto& values = project.getValues();
            RamDomain tuple[arity];
            for (size_t i = 0; i < arity; i++) {
                assert(values[i]);
                tuple[i] = interpreter.evalVal(*values[i], ctxt);
            }

            // insert in target relation
            InterpreterRelation& rel = interpreter.getRelation(project.getRelation());
            rel.insert(tuple);
        }

        // -- return from subroutine --
        void visitReturn(const RamReturn& ret) override {
            for (auto val : ret.getValues()) {
                if (val == nullptr) {
                    ctxt.addReturnValue(0, true);
                } else {
                    ctxt.addReturnValue(interpreter.evalVal(*val, ctxt));
                }
            }
        }

        // -- safety net --
        void visitNode(const RamNode& node) override {
            std::cerr << "Unsupported node type: " << typeid(node).name() << "\n";
            assert(false && "Unsupported Node Type!");
        }
    };

    // create and run interpreter for operations
    InterpreterContext ctxt(op.getDepth());
    ctxt.setReturnValues(args.getReturnValues());
    ctxt.setReturnErrors(args.getReturnErrors());
    ctxt.setArguments(args.getArguments());
    OperationEvaluator(*this, ctxt).visit(op);
}

/** Evaluate RAM statement */
void Interpreter::evalStmt(const RamStatement& stmt) {
    class StatementEvaluator : public RamVisitor<bool> {
        Interpreter& interpreter;

    public:
        StatementEvaluator(Interpreter& interp) : interpreter(interp) {}

        // -- Statements -----------------------------

        bool visitSequence(const RamSequence& seq) override {
            // process all statements in sequence
            for (const auto& cur : seq.getStatements()) {
                if (!visit(cur)) {
                    return false;
                }
            }

            // all processed successfully
            return true;
        }

        bool visitParallel(const RamParallel& parallel) override {
            // get statements to be processed in parallel
            const auto& stmts = parallel.getStatements();

            // special case: empty
            if (stmts.empty()) {
                return true;
            }

            // special handling for a single child
            if (stmts.size() == 1) {
                return visit(stmts[0]);
            }

            // parallel execution
            bool cond = true;
#pragma omp parallel for reduction(&& : cond)
            for (size_t i = 0; i < stmts.size(); i++) {
                cond = cond && visit(stmts[i]);
            }
            return cond;
        }

        bool visitLoop(const RamLoop& loop) override {
            interpreter.resetIterationNumber();
            while (visit(loop.getBody())) {
                interpreter.incIterationNumber();
            }
            interpreter.resetIterationNumber();
            return true;
        }

        bool visitExit(const RamExit& exit) override {
            return !interpreter.evalCond(exit.getCondition());
        }

        bool visitLogTimer(const RamLogTimer& timer) override {
            if (timer.getRelation() == nullptr) {
                Logger logger(timer.getMessage().c_str(), interpreter.getIterationNumber());
                return visit(timer.getStatement());
            } else {
                const InterpreterRelation& rel = interpreter.getRelation(*timer.getRelation());
                Logger logger(timer.getMessage().c_str(), interpreter.getIterationNumber(),
                        std::bind(&InterpreterRelation::size, &rel));
                return visit(timer.getStatement());
            }
        }

        bool visitDebugInfo(const RamDebugInfo& dbg) override {
            SignalHandler::instance()->setMsg(dbg.getMessage().c_str());
            return visit(dbg.getStatement());
        }

        bool visitStratum(const RamStratum& stratum) override {
            // TODO (lyndonhenry): should enable strata as subprograms for interpreter here

            // Record relations created in each stratum
            if (Global::config().has("profile")) {
                std::map<std::string, size_t> relNames;
                visitDepthFirst(stratum, [&](const RamCreate& create) {
                    relNames[create.getRelation().getName()] = create.getRelation().getArity();
                });
                for (const auto& cur : relNames) {
                    // Skip temporary relations, marked with '@'
                    if (cur.first[0] == '@') {
                        continue;
                    }
                    ProfileEventSingleton::instance().makeStratumRecord(
                            stratum.getIndex(), "relation", cur.first, "arity", std::to_string(cur.second));
                }
            }
            return visit(stratum.getBody());
        }

        bool visitCreate(const RamCreate& create) override {
            interpreter.createRelation(create.getRelation());
            return true;
        }

        bool visitClear(const RamClear& clear) override {
            InterpreterRelation& rel = interpreter.getRelation(clear.getRelation());
            rel.purge();
            return true;
        }

        bool visitDrop(const RamDrop& drop) override {
            interpreter.dropRelation(drop.getRelation());
            return true;
        }

        bool visitPrintSize(const RamPrintSize& print) override {
            auto lease = getOutputLock().acquire();
            (void)lease;
            const InterpreterRelation& rel = interpreter.getRelation(print.getRelation());
            std::cout << print.getMessage() << rel.size() << "\n";
            return true;
        }

        bool visitLogSize(const RamLogSize& size) override {
            const InterpreterRelation& rel = interpreter.getRelation(size.getRelation());
            ProfileEventSingleton::instance().makeQuantityEvent(
                    size.getMessage(), rel.size(), interpreter.getIterationNumber());
            return true;
        }

        bool visitLoad(const RamLoad& load) override {
            for (IODirectives ioDirectives : load.getIODirectives()) {
                try {
                    InterpreterRelation& relation = interpreter.getRelation(load.getRelation());
                    IOSystem::getInstance()
                            .getReader(load.getRelation().getSymbolMask(), interpreter.getSymbolTable(),
                                    ioDirectives, Global::config().has("provenance"))
                            ->readAll(relation);
                } catch (std::exception& e) {
                    std::cerr << "Error loading data: " << e.what() << "\n";
                }
            }
            return true;
        }
        bool visitStore(const RamStore& store) override {
            for (IODirectives ioDirectives : store.getIODirectives()) {
                try {
                    IOSystem::getInstance()
                            .getWriter(store.getRelation().getSymbolMask(), interpreter.getSymbolTable(),
                                    ioDirectives, Global::config().has("provenance"))
                            ->writeAll(interpreter.getRelation(store.getRelation()));
                } catch (std::exception& e) {
                    std::cerr << e.what();
                    exit(1);
                }
            }
            return true;
        }

        bool visitFact(const RamFact& fact) override {
            auto arity = fact.getRelation().getArity();
            RamDomain tuple[arity];
            auto values = fact.getValues();

            for (size_t i = 0; i < arity; ++i) {
                tuple[i] = interpreter.evalVal(*values[i]);
            }

            interpreter.getRelation(fact.getRelation()).insert(tuple);
            return true;
        }

        bool visitInsert(const RamInsert& insert) override {
            // run generic query executor

            const RamCondition* c = insert.getCondition();
            if (c != nullptr) {
                if (interpreter.evalCond(*insert.getCondition())) {
                    interpreter.evalOp(insert.getOperation());
                }
            } else {
                interpreter.evalOp(insert.getOperation());
            }
            return true;
        }

        bool visitMerge(const RamMerge& merge) override {
            // get involved relation
            InterpreterRelation& src = interpreter.getRelation(merge.getSourceRelation());
            InterpreterRelation& trg = interpreter.getRelation(merge.getTargetRelation());

            if (dynamic_cast<InterpreterEqRelation*>(&trg)) {
                // expand src with the new knowledge generated by insertion.
                src.extend(trg);
            }
            // merge in all elements
            trg.insert(src);

            // done
            return true;
        }

        bool visitSwap(const RamSwap& swap) override {
            interpreter.swapRelation(swap.getFirstRelation(), swap.getSecondRelation());
            return true;
        }

        // -- safety net --

        bool visitNode(const RamNode& node) override {
            auto lease = getOutputLock().acquire();
            (void)lease;
            std::cerr << "Unsupported node type: " << typeid(node).name() << "\n";
            assert(false && "Unsupported Node Type!");
            return false;
        }
    };

    // create and run interpreter for statements
    StatementEvaluator(*this).visit(stmt);
}

/** Execute main program of a translation unit */
void Interpreter::executeMain() {
    SignalHandler::instance()->set();
    if (Global::config().has("verbose")) {
        SignalHandler::instance()->enableLogging();
    }
    const RamStatement& main = *translationUnit.getP().getMain();

    if (!Global::config().has("profile")) {
        evalStmt(main);
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
        visitDepthFirst(main, [&](const RamInsert& rule) { ++ruleCount; });
        ProfileEventSingleton::instance().makeConfigRecord("ruleCount", std::to_string(ruleCount));

        evalStmt(main);
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

/** Execute subroutine */
void Interpreter::executeSubroutine(const RamStatement& stmt, const std::vector<RamDomain>& arguments,
        std::vector<RamDomain>& returnValues, std::vector<bool>& returnErrors) {
    InterpreterContext ctxt;
    ctxt.setReturnValues(returnValues);
    ctxt.setReturnErrors(returnErrors);
    ctxt.setArguments(arguments);

    // run subroutine
    const RamOperation& op = static_cast<const RamInsert&>(stmt).getOperation();
    evalOp(op, ctxt);
}

}  // end of namespace souffle
