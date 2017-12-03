/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamInterpreter.cpp
 *
 * Implementation of the RAM interpreter.
 *
 ***********************************************************************/

#include "RamInterpreter.h"
#include "AstRelation.h"
#include "AstTranslator.h"
#include "AstVisitor.h"
#include "BinaryConstraintOps.h"
#include "BinaryFunctorOps.h"
#include "Global.h"
#include "IOSystem.h"
#include "Macro.h"
#include "RamAutoIndex.h"
#include "RamData.h"
#include "RamLogger.h"
#include "RamVisitor.h"
#include "RuleScheduler.h"
#include "SignalHandler.h"
#include "TypeSystem.h"
#include "UnaryFunctorOps.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <regex>
#include <utility>

#include <unistd.h>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace souffle {

class EvalContext {
    std::vector<const RamDomain*> data;
    std::vector<RamDomain>* returnValues = nullptr;
    std::vector<bool>* returnErrors = nullptr;
    const std::vector<RamDomain>* args = nullptr;

public:
    EvalContext(size_t size = 0) : data(size) {}

    const RamDomain*& operator[](size_t index) {
        return data[index];
    }

    const RamDomain* const& operator[](size_t index) const {
        return data[index];
    }

    std::vector<RamDomain>& getReturnValues() const {
        return *returnValues;
    }

    void setReturnValues(std::vector<RamDomain>& retVals) {
        returnValues = &retVals;
    }

    void addReturnValue(RamDomain val, bool err = false) {
        assert(returnValues != nullptr && returnErrors != nullptr);
        returnValues->push_back(val);
        returnErrors->push_back(err);
    }

    std::vector<bool>& getReturnErrors() const {
        return *returnErrors;
    }

    void setReturnErrors(std::vector<bool>& retErrs) {
        returnErrors = &retErrs;
    }

    const std::vector<RamDomain>& getArguments() const {
        return *args;
    }

    void setArguments(const std::vector<RamDomain>& a) {
        args = &a;
    }

    RamDomain getArgument(size_t i) const {
        assert(args != nullptr && i < args->size() && "argument out of range");
        return (*args)[i];
    }
};

namespace {

RamDomain eval(const RamValue& value, RamEnvironment& env, const EvalContext& ctxt = EvalContext()) {
    class Evaluator : public RamVisitor<RamDomain> {
        RamEnvironment& env;
        const EvalContext& ctxt;

    public:
        Evaluator(RamEnvironment& env, const EvalContext& ctxt) : env(env), ctxt(ctxt) {}

        // -- basics --
        RamDomain visitNumber(const RamNumber& num) override {
            return num.getConstant();
        }

        RamDomain visitElementAccess(const RamElementAccess& access) override {
            return ctxt[access.getLevel()][access.getElement()];
        }

        RamDomain visitAutoIncrement(const RamAutoIncrement& /*inc*/) override {
            return env.incCounter();
        }

        // unary functions

        RamDomain visitUnaryOperator(const RamUnaryOperator& op) override {
            switch (op.getOperator()) {
                case UnaryOp::NEG:
                    return -visit(op.getValue());
                case UnaryOp::BNOT:
                    return ~visit(op.getValue());
                case UnaryOp::LNOT:
                    return !visit(op.getValue());
                case UnaryOp::ORD:
                    return visit(op.getValue());
                case UnaryOp::STRLEN:
                    return strlen(env.getSymbolTable().resolve(visit(op.getValue())));
                case UnaryOp::SIN:
                    return sin(visit(op.getValue()));
                case UnaryOp::COS:
                    return cos(visit(op.getValue()));
                case UnaryOp::TAN:
                    return tan(visit(op.getValue()));
                case UnaryOp::ASIN:
                    return asin(visit(op.getValue()));
                case UnaryOp::ACOS:
                    return acos(visit(op.getValue()));
                case UnaryOp::ATAN:
                    return atan(visit(op.getValue()));
                case UnaryOp::SINH:
                    return sinh(visit(op.getValue()));
                case UnaryOp::COSH:
                    return cosh(visit(op.getValue()));
                case UnaryOp::TANH:
                    return tanh(visit(op.getValue()));
                case UnaryOp::ASINH:
                    return asinh(visit(op.getValue()));
                case UnaryOp::ACOSH:
                    return acosh(visit(op.getValue()));
                case UnaryOp::ATANH:
                    return atanh(visit(op.getValue()));
                case UnaryOp::LOG:
                    return log(visit(op.getValue()));
                case UnaryOp::EXP:
                    return exp(visit(op.getValue()));
                default:
                    assert(0 && "unsupported operator");
                    return 0;
            }
        }

        // binary functions

        RamDomain visitBinaryOperator(const RamBinaryOperator& op) override {
            switch (op.getOperator()) {
                // arithmetic
                case BinaryOp::ADD: {
                    return visit(op.getLHS()) + visit(op.getRHS());
                }
                case BinaryOp::SUB: {
                    return visit(op.getLHS()) - visit(op.getRHS());
                }
                case BinaryOp::MUL: {
                    return visit(op.getLHS()) * visit(op.getRHS());
                }
                case BinaryOp::DIV: {
                    RamDomain rhs = visit(op.getRHS());
                    return visit(op.getLHS()) / rhs;
                }
                case BinaryOp::EXP: {
                    return std::pow(visit(op.getLHS()), visit(op.getRHS()));
                }
                case BinaryOp::MOD: {
                    RamDomain rhs = visit(op.getRHS());
                    return visit(op.getLHS()) % rhs;
                }
                case BinaryOp::BAND: {
                    return visit(op.getLHS()) & visit(op.getRHS());
                }
                case BinaryOp::BOR: {
                    return visit(op.getLHS()) | visit(op.getRHS());
                }
                case BinaryOp::BXOR: {
                    return visit(op.getLHS()) ^ visit(op.getRHS());
                }
                case BinaryOp::LAND: {
                    return visit(op.getLHS()) && visit(op.getRHS());
                }
                case BinaryOp::LOR: {
                    return visit(op.getLHS()) || visit(op.getRHS());
                }
                case BinaryOp::MAX: {
                    return std::max(visit(op.getLHS()), visit(op.getRHS()));
                }
                case BinaryOp::MIN: {
                    return std::min(visit(op.getLHS()), visit(op.getRHS()));
                }

                // strings
                case BinaryOp::CAT: {
                    return env.getSymbolTable().lookup((
                            std::string(env.getSymbolTable().resolve(visit(op.getLHS()))) +
                            std::string(env.getSymbolTable().resolve(
                                    visit(op.getRHS())))).c_str());
                }
                default:
                    assert(0 && "unsupported operator");
                    return 0;
            }
        }

        // ternary functions

        RamDomain visitTernaryOperator(const RamTernaryOperator& op) override {
            switch (op.getOperator()) {
                case TernaryOp::SUBSTR: {
                    auto symbol = visit(op.getArg(0));
                    std::string str = env.getSymbolTable().resolve(symbol);
                    auto idx = visit(op.getArg(1));
                    auto len = visit(op.getArg(2));
                    std::string sub_str;
                    try {
                        sub_str = str.substr(idx, len);
                    } catch (...) {
                        std::cerr << "warning: wrong index position provided by substr(\"";
                        std::cerr << str << "\"," << (int32_t)idx << "," << (int32_t)len << ") functor.\n";
                    }
                    return env.getSymbolTable().lookup(sub_str.c_str());
                }
                default:
                    assert(0 && "unsupported operator");
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
            return ctxt.getArgument(arg.getNumber());
        }

        // -- safety net --

        RamDomain visitNode(const RamNode& node) override {
            std::cerr << "Unsupported node type: " << typeid(node).name() << "\n";
            assert(false && "Unsupported Node Type!");
            return 0;
        }
    };

    // create and run evaluator
    return Evaluator(env, ctxt)(value);
}

RamDomain eval(const RamValue* value, RamEnvironment& env, const EvalContext& ctxt = EvalContext()) {
    return eval(*value, env, ctxt);
}

bool eval(const RamCondition& cond, RamEnvironment& env, const EvalContext& ctxt = EvalContext()) {
    class Evaluator : public RamVisitor<bool> {
        RamEnvironment& env;
        const EvalContext& ctxt;

    public:
        Evaluator(RamEnvironment& env, const EvalContext& ctxt) : env(env), ctxt(ctxt) {}

        // -- connectors operators --

        bool visitAnd(const RamAnd& a) override {
            return visit(a.getLHS()) && visit(a.getRHS());
        }

        // -- relation operations --

        bool visitEmpty(const RamEmpty& empty) override {
            return env.getRelation(empty.getRelation()).empty();
        }

        bool visitNotExists(const RamNotExists& ne) override {
            const RamRelation& rel = env.getRelation(ne.getRelation());

            // construct the pattern tuple
            auto arity = rel.getArity();
            auto values = ne.getValues();

            // for total we use the exists test
            if (ne.isTotal()) {
                RamDomain tuple[arity];
                for (size_t i = 0; i < arity; i++) {
                    tuple[i] = (values[i]) ? eval(values[i], env, ctxt) : MIN_RAM_DOMAIN;
                }

                return !rel.exists(tuple);
            }

            // for partial we search for lower and upper boundaries
            RamDomain low[arity];
            RamDomain high[arity];
            for (size_t i = 0; i < arity; i++) {
                low[i] = (values[i]) ? eval(values[i], env, ctxt) : MIN_RAM_DOMAIN;
                high[i] = (values[i]) ? low[i] : MAX_RAM_DOMAIN;
            }

            // obtain index
            auto idx = ne.getIndex();
            auto idxRelationName = ne.getIndexRelationName();
            if (idxRelationName != rel.getID().getName() || !idx) {
                idx = rel.getIndex(ne.getKey());
                ne.setIndex(idx);
                ne.setIndexRelationName(rel.getID().getName());
            }

            auto range = idx->lowerUpperBound(low, high);
            return range.first == range.second;  // if there are none => done
        }

        // -- comparison operators --

        bool visitBinaryRelation(const RamBinaryRelation& relOp) override {
            switch (relOp.getOperator()) {
                // comparison operators
                case BinaryConstraintOp::EQ:
                    return eval(relOp.getLHS(), env, ctxt) == eval(relOp.getRHS(), env, ctxt);
                case BinaryConstraintOp::NE:
                    return eval(relOp.getLHS(), env, ctxt) != eval(relOp.getRHS(), env, ctxt);
                case BinaryConstraintOp::LT:
                    return eval(relOp.getLHS(), env, ctxt) < eval(relOp.getRHS(), env, ctxt);
                case BinaryConstraintOp::LE:
                    return eval(relOp.getLHS(), env, ctxt) <= eval(relOp.getRHS(), env, ctxt);
                case BinaryConstraintOp::GT:
                    return eval(relOp.getLHS(), env, ctxt) > eval(relOp.getRHS(), env, ctxt);
                case BinaryConstraintOp::GE:
                    return eval(relOp.getLHS(), env, ctxt) >= eval(relOp.getRHS(), env, ctxt);

                // strings
                case BinaryConstraintOp::MATCH: {
                    RamDomain l = eval(relOp.getLHS(), env, ctxt);
                    RamDomain r = eval(relOp.getRHS(), env, ctxt);
                    const std::string& pattern = env.getSymbolTable().resolve(l);
                    const std::string& text = env.getSymbolTable().resolve(r);
                    bool result = false;
                    try {
                        result = std::regex_match(text, std::regex(pattern));
                    } catch (...) {
                        std::cerr << "warning: wrong pattern provided for match(\"" << pattern << "\",\""
                                  << text << "\")\n";
                    }
                    return result;
                }
                case BinaryConstraintOp::NOT_MATCH: {
                    RamDomain l = eval(relOp.getLHS(), env, ctxt);
                    RamDomain r = eval(relOp.getRHS(), env, ctxt);
                    const std::string& pattern = env.getSymbolTable().resolve(l);
                    const std::string& text = env.getSymbolTable().resolve(r);
                    bool result = false;
                    try {
                        result = !std::regex_match(text, std::regex(pattern));
                    } catch (...) {
                        std::cerr << "warning: wrong pattern provided for !match(\"" << pattern << "\",\""
                                  << text << "\")\n";
                    }
                    return result;
                }
                case BinaryConstraintOp::CONTAINS: {
                    RamDomain l = eval(relOp.getLHS(), env, ctxt);
                    RamDomain r = eval(relOp.getRHS(), env, ctxt);
                    const std::string& pattern = env.getSymbolTable().resolve(l);
                    const std::string& text = env.getSymbolTable().resolve(r);
                    return text.find(pattern) != std::string::npos;
                }
                case BinaryConstraintOp::NOT_CONTAINS: {
                    RamDomain l = eval(relOp.getLHS(), env, ctxt);
                    RamDomain r = eval(relOp.getRHS(), env, ctxt);
                    const std::string& pattern = env.getSymbolTable().resolve(l);
                    const std::string& text = env.getSymbolTable().resolve(r);
                    return text.find(pattern) == std::string::npos;
                }
                default:
                    assert(0 && "unsupported operator");
                    return 0;
            }
        }

        // -- safety net --

        bool visitNode(const RamNode& node) override {
            std::cerr << "Unsupported node type: " << typeid(node).name() << "\n";
            assert(false && "Unsupported Node Type!");
            return 0;
        }
    };

    // run evaluator
    return Evaluator(env, ctxt)(cond);
}

void apply(const RamOperation& op, RamEnvironment& env, const EvalContext& args = EvalContext()) {
    class Interpreter : public RamVisitor<void> {
        RamEnvironment& env;
        EvalContext& ctxt;

    public:
        Interpreter(RamEnvironment& env, EvalContext& ctxt) : env(env), ctxt(ctxt) {}

        // -- Operations -----------------------------

        void visitSearch(const RamSearch& search) override {
            // check condition
            auto condition = search.getCondition();
            if (condition && !eval(*condition, env, ctxt)) {
                return;  // condition not valid => skip nested
            }

            // process nested
            visit(*search.getNestedOperation());
        }

        void visitScan(const RamScan& scan) override {
            // get the targeted relation
            const RamRelation& rel = env.getRelation(scan.getRelation());

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
                    low[i] = eval(pattern[i], env, ctxt);
                    hig[i] = low[i];
                } else {
                    low[i] = MIN_RAM_DOMAIN;
                    hig[i] = MAX_RAM_DOMAIN;
                }
            }

            // obtain index
            auto idx = scan.getIndex();
            if (!idx || rel.getID().isTemp()) {
                idx = rel.getIndex(scan.getRangeQueryColumns(), idx);
                scan.setIndex(idx);
            }

            // get iterator range
            auto range = idx->lowerUpperBound(low, hig);

            // if this scan is not binding anything ...
            if (scan.isPureExistenceCheck()) {
                if (range.first != range.second) {
                    visitSearch(scan);
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
            const RamRelation& rel = env.getRelation(aggregate.getRelation());

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
                    low[i] = eval(pattern[i], env, ctxt);
                    hig[i] = low[i];
                } else {
                    low[i] = MIN_RAM_DOMAIN;
                    hig[i] = MAX_RAM_DOMAIN;
                }
            }

            // obtain index
            auto idx = aggregate.getIndex();
            if (!idx) {
                idx = rel.getIndex(aggregate.getRangeQueryColumns());
                aggregate.setIndex(idx);
            }

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
                RamDomain cur = eval(aggregate.getTargetExpression(), env, ctxt);

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
            if (condition && !eval(*condition, env, ctxt)) {
                return;  // condition not valid => skip nested
            }

            // run nested part - using base class visitor
            visitSearch(aggregate);
        }

        void visitProject(const RamProject& project) override {
            // check constraints
            RamCondition* condition = project.getCondition();
            if (condition && !eval(*condition, env, ctxt)) {
                return;  // condition violated => skip insert
            }

            // create a tuple of the proper arity (also supports arity 0)
            auto arity = project.getRelation().getArity();
            const auto& values = project.getValues();
            RamDomain tuple[arity];
            for (size_t i = 0; i < arity; i++) {
                tuple[i] = eval(values[i], env, ctxt);
            }

            // check filter relation
            if (project.hasFilter() && env.getRelation(project.getFilter()).exists(tuple)) {
                return;
            }

            // insert in target relation
            env.getRelation(project.getRelation()).insert(tuple);
        }

        // -- return from subroutine --
        void visitReturn(const RamReturn& ret) override {
            for (auto val : ret.getValues()) {
                if (val == nullptr) {
                    ctxt.addReturnValue(0, true);
                } else {
                    ctxt.addReturnValue(eval(val, env, ctxt));
                }
            }
        }

        // -- safety net --
        void visitNode(const RamNode& node) override {
            std::cerr << "Unsupported node type: " << typeid(node).name() << "\n";
            assert(false && "Unsupported Node Type!");
        }
    };

    // create and run interpreter
    EvalContext ctxt(op.getDepth());
    ctxt.setReturnValues(args.getReturnValues());
    ctxt.setReturnErrors(args.getReturnErrors());
    ctxt.setArguments(args.getArguments());
    Interpreter(env, ctxt).visit(op);
}

void run(const QueryExecutionStrategy& executor, std::ostream* report, std::ostream* profile,
        const RamStatement& stmt, RamEnvironment& env, RamData* data) {
    class Interpreter : public RamVisitor<bool> {
        RamEnvironment& env;
        const QueryExecutionStrategy& queryExecutor;
        std::ostream* report;
        std::ostream* profile;

    public:
        Interpreter(RamEnvironment& env, const QueryExecutionStrategy& executor, std::ostream* report,
                std::ostream* profile, RamData* data)
                : env(env), queryExecutor(executor), report(report), profile(profile) {}

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
            while (visit(loop.getBody())) {
            }
            return true;
        }

        bool visitExit(const RamExit& exit) override {
            return !eval(exit.getCondition(), env);
        }

        bool visitLogTimer(const RamLogTimer& timer) override {
            RamLogger logger(timer.getLabel().c_str(), *profile);
            return visit(timer.getNested());
        }

        bool visitDebugInfo(const RamDebugInfo& dbg) override {
            SignalHandler::instance()->setMsg(dbg.getLabel().c_str());
            return visit(dbg.getNested());
        }

        bool visitCreate(const RamCreate& create) override {
            env.getRelation(create.getRelation());
            return true;
        }

        bool visitClear(const RamClear& clear) override {
            env.getRelation(clear.getRelation()).purge();
            return true;
        }

        bool visitDrop(const RamDrop& drop) override {
            env.dropRelation(drop.getRelation());
            return true;
        }

        bool visitPrintSize(const RamPrintSize& print) override {
            auto lease = getOutputLock().acquire();
            (void)lease;
            std::cout << print.getLabel() << env.getRelation(print.getRelation()).size() << "\n";
            return true;
        }

        bool visitLogSize(const RamLogSize& print) override {
            auto lease = getOutputLock().acquire();
            (void)lease;
            *profile << print.getLabel() << env.getRelation(print.getRelation()).size() << "\n";
            return true;
        }

        bool visitLoad(const RamLoad& load) override {
            try {
                RamRelation& relation = env.getRelation(load.getRelation());
                std::unique_ptr<ReadStream> reader = IOSystem::getInstance().getReader(
                        load.getRelation().getSymbolMask(), env.getSymbolTable(),
                        load.getRelation().getInputDirectives(), Global::config().has("provenance"));
                reader->readAll(relation);
            } catch (std::exception& e) {
                std::cerr << e.what();
                return false;
            }
            return true;
        }

        bool visitStore(const RamStore& store) override {
            for (IODirectives ioDirectives : store.getRelation().getOutputDirectives()) {
                try {
                    IOSystem::getInstance()
                            .getWriter(store.getRelation().getSymbolMask(), env.getSymbolTable(),
                                    ioDirectives, Global::config().has("provenance"))
                            ->writeAll(env.getRelation(store.getRelation()));
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
                tuple[i] = eval(values[i], env);
            }

            env.getRelation(fact.getRelation()).insert(tuple);
            return true;
        }

        bool visitInsert(const RamInsert& insert) override {
            // run generic query executor
            queryExecutor(insert, env, report);
            return true;
        }

        bool visitMerge(const RamMerge& merge) override {
            // get involved relation
            RamRelation& src = env.getRelation(merge.getSourceRelation());
            RamRelation& trg = env.getRelation(merge.getTargetRelation());

            // merge in all elements
            trg.insert(src);

            // done
            return true;
        }

        bool visitSwap(const RamSwap& swap) override {
            std::swap(env.getRelation(swap.getFirstRelation()), env.getRelation(swap.getSecondRelation()));
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

    // create and run interpreter
    Interpreter(env, executor, report, profile, data).visit(stmt);
}
}  // namespace

void RamInterpreter::applyOn(const RamProgram& prog, RamEnvironment& env, RamData* data) const {
    SignalHandler::instance()->set();
    if (Global::config().has("profile")) {
        std::string fname = Global::config().get("profile");
        // open output stream
        std::ofstream os(fname);
        if (!os.is_open()) {
            throw std::invalid_argument("Cannot open profile log file <" + fname + ">");
        }
        os << "@start-debug\n";
        run(queryStrategy, report, &os, *(prog.getMain()), env, data);
    } else {
        run(queryStrategy, report, nullptr, *(prog.getMain()), env, data);
    }
    SignalHandler::instance()->reset();
}

/**
 * Runs a subroutine of a RamProgram
 */
void RamInterpreter::executeSubroutine(RamEnvironment& env, const RamStatement& stmt,
        const std::vector<RamDomain>& arguments, std::vector<RamDomain>& returnValues,
        std::vector<bool>& returnErrors) const {
    EvalContext ctxt;
    ctxt.setReturnValues(returnValues);
    ctxt.setReturnErrors(returnErrors);
    ctxt.setArguments(arguments);

    // run subroutine
    const RamOperation& op = static_cast<const RamInsert&>(stmt).getOperation();
    apply(op, env, ctxt);
}

namespace {

using namespace scheduler;

Order scheduleByModel(AstClause& clause, RamEnvironment& env, std::ostream* report) {
    assert(!clause.isFact());

    // check whether schedule is fixed
    if (clause.hasFixedExecutionPlan()) {
        if (report) {
            *report << "   Skipped due to fixed execution plan!\n";
        }
        return Order::getIdentity(clause.getAtoms().size());
    }

    // check whether there is actually something to schedule
    if (clause.getAtoms().size() < 2) {
        return Order::getIdentity(clause.getAtoms().size());
    }

    // TODO: provide alternative scheduling approach for larger rules
    //  8 atoms require   ~200ms to schedule
    //  9 atoms require  ~2400ms to schedule
    // 10 atoms require ~29000ms to schedule
    // 11 atoms => out of memory
    if (clause.getAtoms().size() > 8) {
        return Order::getIdentity(clause.getAtoms().size());
    }

    // get atom list
    std::vector<AstAtom*> atoms = clause.getAtoms();

    // a utility for mapping variable names to ids
    std::map<std::string, int> varIDs;
    auto getID = [&](const AstVariable& var) -> int {
        auto pos = varIDs.find(var.getName());
        if (pos != varIDs.end()) {
            return pos->second;
        }
        int id = varIDs.size();
        varIDs[var.getName()] = id;
        return id;
    };

    // fix scheduling strategy
    typedef Problem<SimpleComputationalCostModel> Problem;
    typedef typename Problem::atom_type Atom;

    // create an optimization problem
    Problem p;

    // create atoms
    for (unsigned i = 0; i < atoms.size(); i++) {
        // convert pattern of arguments
        std::vector<Argument> args;

        for (const AstArgument* arg : atoms[i]->getArguments()) {
            if (const AstVariable* var = dynamic_cast<const AstVariable*>(arg)) {
                args.push_back(Argument::createVar(getID(*var)));
            } else if (dynamic_cast<const AstUnnamedVariable*>(arg)) {
                args.push_back(Argument::createUnderscore());
            } else if (dynamic_cast<const AstConstant*>(arg)) {
                args.push_back(Argument::createConst());
            } else {
                args.push_back(Argument::createOther());
            }
        }

        // add new atom
        AstTranslator translator;
        p.addAtom(
                Atom(i, args, env.getRelation(translator.translateRelationName(atoms[i]->getName())).size()));
    }

    // solve the optimization problem
    auto schedule = p.solve();

    // log problem and solution
    if (report) {
        *report << "Scheduling Problem: " << p << "\n";
        *report << "          Schedule: " << schedule << "\n";
    }

    // extract order
    Order res;
    for (const auto& cur : schedule) {
        res.append(cur.getID());
    }

    // re-order atoms
    clause.reorderAtoms(res.getOrder());

    // done
    return res;
}
}  // namespace

/** With this strategy queries will be processed as they are stated by the user */
const QueryExecutionStrategy DirectExecution = [](
        const RamInsert& insert, RamEnvironment& env, std::ostream*) -> ExecutionSummary {
    // measure the time
    auto start = now();

    // simplest strategy of all - just apply the nested operation
    apply(insert.getOperation(), env);

    // create report
    auto end = now();
    return ExecutionSummary(
            {Order::getIdentity(insert.getOrigin().getAtoms().size()), duration_in_ms(start, end)});
};

/** With this strategy queries will be dynamically rescheduled before each execution */
const QueryExecutionStrategy ScheduledExecution = [](
        const RamInsert& insert, RamEnvironment& env, std::ostream* report) -> ExecutionSummary {

    // Report scheduling
    // TODO: only re-schedule atoms (avoid cloning entire clause)
    std::unique_ptr<AstClause> clause(insert.getOrigin().clone());

    Order order;

    // (re-)schedule clause
    if (report) {
        *report << "\nScheduling clause @ " << clause->getSrcLoc() << "\n";
    }
    {
        auto start = now();
        order = scheduleByModel(*clause, env, report);
        auto end = now();
        if (report) {
            *report << "    Original Query: " << insert.getOrigin() << "\n";
        }
        if (report) {
            *report << "       Rescheduled: " << *clause << "\n";
        }
        if (!equal_targets(insert.getOrigin().getAtoms(), clause->getAtoms())) {
            if (report) {
                *report << "            Order has Changed!\n";
            }
        }
        if (report) {
            *report << "   Scheduling Time: " << duration_in_ms(start, end) << "ms\n";
        }
    }

    // create operation
    std::unique_ptr<RamStatement> stmt =
            AstTranslator(Global::config().has("profile")).translateClause(*clause, nullptr, nullptr);
    assert(dynamic_cast<RamInsert*>(stmt.get()));

    // run rescheduled node
    auto start = now();
    apply(static_cast<RamInsert*>(stmt.get())->getOperation(), env);
    auto end = now();
    auto runtime = duration_in_ms(start, end);
    if (report) {
        *report << "           Runtime: " << runtime << "ms\n";
    }

    return ExecutionSummary({order, runtime});
};

}  // end of namespace souffle
