/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamExecutor.cpp
 *
 * Defines entities capable of executing a RAM program.
 *
 ***********************************************************************/

#include "RamExecutor.h"
#include "AstRelation.h"
#include "AstVisitor.h"
#include "BinaryConstraintOps.h"
#include "BinaryFunctorOps.h"
#include "Global.h"
#include "IOSystem.h"
#include "RamAutoIndex.h"
#include "RamData.h"
#include "RamLogger.h"
#include "RamTranslator.h"
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

static const char ENV_NO_INDEX[] = "SOUFFLE_USE_NO_INDEX";
bool useNoIndex() {
    static bool flag = std::getenv(ENV_NO_INDEX);
    static bool first = true;
    if (first && flag) {
        std::cout << "WARNING: indices are ignored!\n";
        first = false;
    }
    return flag;
}

// See the CPPIdentifierMap, (it is a singleton class).
CPPIdentifierMap* CPPIdentifierMap::instance = nullptr;

// Static wrapper to get relation names without going directly though the CPPIdentifierMap.
static const std::string getRelationName(const RamRelationIdentifier& rel) {
    return "rel_" + CPPIdentifierMap::getIdentifier(rel.getName());
}

// Static wrapper to get op context names without going directly though the CPPIdentifierMap.
static const std::string getOpContextName(const RamRelationIdentifier& rel) {
    return getRelationName(rel) + "_op_ctxt";
}

namespace {

class EvalContext {
    std::vector<const RamDomain*> data;

public:
    EvalContext(size_t size = 0) : data(size) {}

    const RamDomain*& operator[](size_t index) {
        return data[index];
    }

    const RamDomain* const& operator[](size_t index) const {
        return data[index];
    }
};

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

                // strings
                case BinaryOp::CAT: {
                    return env.getSymbolTable().lookup(
                            (std::string(env.getSymbolTable().resolve(visit(op.getLHS()))) +
                                    std::string(env.getSymbolTable().resolve(visit(op.getRHS()))))
                                    .c_str());
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
                        std::cerr << str << "\"," << idx << "," << len << ") functor.\n";
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
            if (!idx) {
                idx = rel.getIndex(ne.getKey());
                ne.setIndex(idx);
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
                case BinaryConstraintOp::CONTAINS: {
                    RamDomain l = eval(relOp.getLHS(), env, ctxt);
                    RamDomain r = eval(relOp.getRHS(), env, ctxt);
                    const std::string& pattern = env.getSymbolTable().resolve(l);
                    const std::string& text = env.getSymbolTable().resolve(r);
                    return text.find(pattern) != std::string::npos;
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

void apply(const RamOperation& op, RamEnvironment& env) {
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

        // -- safety net --
        void visitNode(const RamNode& node) override {
            std::cerr << "Unsupported node type: " << typeid(node).name() << "\n";
            assert(false && "Unsupported Node Type!");
        }
    };

    // create and run interpreter
    EvalContext ctxt(op.getDepth());
    Interpreter(env, ctxt).visit(op);
}

void run(const QueryExecutionStrategy& executor, std::ostream* report, std::ostream* profile,
        const RamStatement& stmt, RamEnvironment& env, RamData* data) {
    class Interpreter : public RamVisitor<bool> {
        RamEnvironment& env;
        const QueryExecutionStrategy& queryExecutor;
        std::ostream* report;
        std::ostream* profile;
        RamData* data;

    public:
        Interpreter(RamEnvironment& env, const QueryExecutionStrategy& executor, std::ostream* report,
                std::ostream* profile, RamData* data)
                : env(env), queryExecutor(executor), report(report), profile(profile), data(data) {}

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

#ifdef _OPENMP
            if (std::stoi(Global::config().get("jobs")) != 0) {
                omp_set_num_threads(std::stoi(Global::config().get("jobs")));
            }
#endif

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
            std::cout << print.getLabel() << env.getRelation(print.getRelation()).size() << "\n";
            return true;
        }

        bool visitLogSize(const RamLogSize& print) override {
            *profile << print.getLabel() << env.getRelation(print.getRelation()).size() << "\n";
            return true;
        }

        bool visitLoad(const RamLoad& load) override {
#ifdef USE_JAVAI
            if (load.getRelation().isData()) {
                // Load from mem
                std::string name = load.getRelation().getName();
                if (data == nullptr) {
                    std::cout << "data is null\n";
                    return false;
                }
                PrimData* pd = data->getTuples(name);
                if (pd == nullptr || pd->data.empty()) {
                    std::cout << "relation " << name << " is empty\n";
                    return true;
                }

                bool err = env.getRelation(load.getRelation())
                                   .load(pd->data, env.getSymbolTable(), load.getRelation().getSymbolMask());
                return !err;
            }
#endif
            try {
                RamRelation& relation = env.getRelation(load.getRelation());
                std::unique_ptr<ReadStream> reader =
                        IOSystem::getInstance().getReader(load.getRelation().getSymbolMask(),
                                env.getSymbolTable(), load.getRelation().getInputDirectives());
                reader->readAll(relation);
            } catch (std::exception& e) {
                std::cerr << e.what();
                return false;
            }
            return true;
        }

        bool visitStore(const RamStore& store) override {
#ifdef USE_JAVAI
            if (store.getRelation().isData()) {
                return true;
            }
#endif
            auto& rel = env.getRelation(store.getRelation());
            for (IODirectives ioDirectives : store.getRelation().getOutputDirectives()) {
                try {
                    IOSystem::getInstance()
                            .getWriter(
                                    store.getRelation().getSymbolMask(), env.getSymbolTable(), ioDirectives)
                            ->writeAll(rel);
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
            std::cerr << "Unsupported node type: " << typeid(node).name() << "\n";
            assert(false && "Unsupported Node Type!");
            return false;
        }
    };

    // create and run interpreter
    Interpreter(env, executor, report, profile, data).visit(stmt);
}
}  // namespace

void RamGuidedInterpreter::applyOn(const RamStatement& stmt, RamEnvironment& env, RamData* data) const {
    if (Global::config().has("profile")) {
        std::string fname = Global::config().get("profile");
        // open output stream
        std::ofstream os(fname);
        if (!os.is_open()) {
            // TODO: use different error reporting here!!
            std::cerr << "Cannot open fact file " << fname << " for profiling\n";
        }
        os << "@start-debug\n";
        run(queryStrategy, report, &os, stmt, env, data);
    } else {
        run(queryStrategy, report, nullptr, stmt, env, data);
    }
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
        RamTranslator translator;
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
            RamTranslator(Global::config().has("profile")).translateClause(*clause, nullptr, nullptr);
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

namespace {

class IndexMap {
    typedef std::map<RamRelationIdentifier, RamAutoIndex> data_t;
    typedef typename data_t::iterator iterator;

    std::map<RamRelationIdentifier, RamAutoIndex> data;

public:
    RamAutoIndex& operator[](const RamRelationIdentifier& rel) {
        return data[rel];
    }

    const RamAutoIndex& operator[](const RamRelationIdentifier& rel) const {
        const static RamAutoIndex empty;
        auto pos = data.find(rel);
        return (pos != data.end()) ? pos->second : empty;
    }

    iterator begin() {
        return data.begin();
    }

    iterator end() {
        return data.end();
    }
};

std::string getRelationType(
        const RamRelationIdentifier& rel, std::size_t arity, const RamAutoIndex& indices) {
    std::stringstream res;
    res << "ram::Relation";
    res << "<";

    if (rel.isBTree()) {
        res << "BTree,";
    } else if (rel.isBrie()) {
        res << "Brie,";
    } else if (rel.isEqRel()) {
        res << "EqRel,";
    } else {
        res << "Auto,";
    }

    res << arity;
    if (!useNoIndex()) {
        for (auto& cur : indices.getAllOrders()) {
            res << ", ram::index<";
            res << join(cur, ",");
            res << ">";
        }
    }
    res << ">";
    return res.str();
}

std::string toIndex(SearchColumns key) {
    std::stringstream tmp;
    tmp << "<";
    int i = 0;
    while (key != 0) {
        if (key % 2) {
            tmp << i;
            if (key > 1) {
                tmp << ",";
            }
        }
        key >>= 1;
        i++;
    }

    tmp << ">";
    return tmp.str();
}

std::set<RamRelationIdentifier> getReferencedRelations(const RamOperation& op) {
    std::set<RamRelationIdentifier> res;
    visitDepthFirst(op, [&](const RamNode& node) {
        if (auto scan = dynamic_cast<const RamScan*>(&node)) {
            res.insert(scan->getRelation());
        } else if (auto agg = dynamic_cast<const RamAggregate*>(&node)) {
            res.insert(agg->getRelation());
        } else if (auto project = dynamic_cast<const RamProject*>(&node)) {
            res.insert(project->getRelation());
            if (project->hasFilter()) {
                res.insert(project->getFilter());
            }
        } else if (auto notExist = dynamic_cast<const RamNotExists*>(&node)) {
            res.insert(notExist->getRelation());
        }
    });
    return res;
}

class Printer : public RamVisitor<void, std::ostream&> {
    // const IndexMap& indices;

    std::function<void(std::ostream&, const RamNode*)> rec;

    struct printer {
        Printer& p;
        const RamNode& node;
        printer(Printer& p, const RamNode& n) : p(p), node(n) {}
        printer(const printer& other) = default;
        friend std::ostream& operator<<(std::ostream& out, const printer& p) {
            p.p.visit(p.node, out);
            return out;
        }
    };

public:
    Printer(const IndexMap& /*indexMap*/) {
        rec = [&](std::ostream& out, const RamNode* node) { this->visit(*node, out); };
    }

    // -- relation statements --

    void visitCreate(const RamCreate& /*create*/, std::ostream& /*out*/) override {}

    void visitFact(const RamFact& fact, std::ostream& out) override {
        out << getRelationName(fact.getRelation()) << "->"
            << "insert(" << join(fact.getValues(), ",", rec) << ");\n";
    }

    void visitLoad(const RamLoad& /*load*/, std::ostream& /*out*/) override {}

    void visitStore(const RamStore& /*store*/, std::ostream& /*out*/) override {}

    void visitInsert(const RamInsert& insert, std::ostream& out) override {
        // enclose operation with a check for an empty relation
        std::set<RamRelationIdentifier> input_relations;
        visitDepthFirst(insert, [&](const RamScan& scan) { input_relations.insert(scan.getRelation()); });
        if (!input_relations.empty()) {
            out << "if (" << join(input_relations, "&&", [&](std::ostream& out,
                                                                 const RamRelationIdentifier& rel) {
                out << "!" << getRelationName(rel) << "->"
                    << "empty()";
            }) << ") ";
        }

        // outline each search operation to improve compilation time
        // Disabled to work around issue #345 with clang 3.7-3.9 & omp.
        // out << "[&]()";

        // enclose operation in its own scope
        out << "{\n";

        // create proof counters
        if (Global::config().has("profile")) {
            out << "std::atomic<uint64_t> num_failed_proofs(0);\n";
        }

        // check whether loop nest can be parallelized
        bool parallel = false;
        if (const RamScan* scan = dynamic_cast<const RamScan*>(&insert.getOperation())) {
            // if it is a full scan
            if (scan->getRangeQueryColumns() == 0 && !scan->isPureExistenceCheck()) {
                // yes it can!
                parallel = true;

                // partition outermost relation
                out << "auto part = " << getRelationName(scan->getRelation()) << "->"
                    << "partition();\n";

                // build a parallel block around this loop nest
                out << "PARALLEL_START;\n";
            }
        }

        // add local counters
        if (Global::config().has("profile")) {
            out << "uint64_t private_num_failed_proofs = 0;\n";
        }

        // create operation contexts for this operation
        for (const RamRelationIdentifier& rel : getReferencedRelations(insert.getOperation())) {
            out << "CREATE_OP_CONTEXT(" << getOpContextName(rel) << "," << getRelationName(rel) << "->"
                << "createContext());\n";
        }

        out << print(insert.getOperation());

        // aggregate proof counters
        if (Global::config().has("profile")) {
            out << "num_failed_proofs += private_num_failed_proofs;\n";
        }

        if (parallel) {
            out << "PARALLEL_END;\n";  // end parallel

            // aggregate proof counters
        }
        if (Global::config().has("profile")) {
            // get target relation
            RamRelationIdentifier rel;
            visitDepthFirst(insert, [&](const RamProject& project) { rel = project.getRelation(); });

            // build log message
            auto& clause = insert.getOrigin();
            std::string clauseText = toString(clause);
            replace(clauseText.begin(), clauseText.end(), '"', '\'');
            replace(clauseText.begin(), clauseText.end(), '\n', ' ');

            std::ostringstream line;
            line << "p-proof-counter;" << rel.getName() << ";" << clause.getSrcLoc() << ";" << clauseText
                 << ";";
            std::string label = line.str();

            // print log entry
            out << "{ auto lease = getOutputLock().acquire(); ";
            out << "profile << R\"(#" << label << ";)\" << num_failed_proofs << \"\\n\";\n";
            out << "}";
        }

        out << "}\n";  // end lambda
        // out << "();";  // call lambda
    }

    void visitMerge(const RamMerge& merge, std::ostream& out) override {
        out << getRelationName(merge.getTargetRelation()) << "->"
            << "insertAll("
            << "*" << getRelationName(merge.getSourceRelation()) << ");\n";
    }

    void visitClear(const RamClear& clear, std::ostream& out) override {
        out << getRelationName(clear.getRelation()) << "->"
            << "purge();\n";
    }

    void visitDrop(const RamDrop& drop, std::ostream& out) override {
        if (drop.getRelation().isTemp()) {
            out << getRelationName(drop.getRelation()) << "->"
                << "purge();\n";
        }
    }

    void visitPrintSize(const RamPrintSize& /*print*/, std::ostream& /*out*/) override {}

    void visitLogSize(const RamLogSize& print, std::ostream& out) override {
        out << "{ auto lease = getOutputLock().acquire(); \n";
        out << "profile << R\"(" << print.getLabel() << ")\" <<  ";
        out << getRelationName(print.getRelation());
        out << "->"
            << "size() << \"\\n\";\n"
            << "}";
    }

    // -- control flow statements --

    void visitSequence(const RamSequence& seq, std::ostream& out) override {
        for (const auto& cur : seq.getStatements()) {
            out << print(cur);
        }
    }

    void visitParallel(const RamParallel& parallel, std::ostream& out) override {
        auto stmts = parallel.getStatements();

        // special handling cases
        if (stmts.empty()) {
            return;
        }

        // a single statement => save the overhead
        if (stmts.size() == 1) {
            out << print(stmts[0]);
            return;
        }

        // more than one => parallel sections

        // start parallel section
        out << "SECTIONS_START;\n";

        // put each thread in another section
        for (const auto& cur : stmts) {
            out << "SECTION_START;\n";
            out << print(cur);
            out << "SECTION_END\n";
        }

        // done
        out << "SECTIONS_END;\n";
    }

    void visitLoop(const RamLoop& loop, std::ostream& out) override {
        out << "for(;;) {\n" << print(loop.getBody()) << "}\n";
    }

    void visitSwap(const RamSwap& swap, std::ostream& out) override {
        const std::string tempKnowledge = "rel_0";
        const std::string& deltaKnowledge = getRelationName(swap.getFirstRelation());
        const std::string& newKnowledge = getRelationName(swap.getSecondRelation());

        // perform a triangular swap of pointers
        out << "{\nauto " << tempKnowledge << " = " << deltaKnowledge << ";\n"
            << deltaKnowledge << " = " << newKnowledge << ";\n"
            << newKnowledge << " = " << tempKnowledge << ";\n"
            << "}\n";
    }

    void visitExit(const RamExit& exit, std::ostream& out) override {
        out << "if(" << print(exit.getCondition()) << ") break;\n";
    }

    void visitLogTimer(const RamLogTimer& timer, std::ostream& out) override {
        // create local scope for name resolution
        out << "{\n";

        // create local timer
        out << "\tRamLogger logger(R\"(" << timer.getLabel() << ")\",profile);\n";

        // insert statement to be measured
        visit(timer.getNested(), out);

        // done
        out << "}\n";
    }

    void visitDebugInfo(const RamDebugInfo& dbg, std::ostream& out) override {
        out << "SignalHandler::instance()->setMsg(R\"_(";
        out << dbg.getLabel();
        out << ")_\");\n";

        // insert statements of the rule
        visit(dbg.getNested(), out);
    }

    // -- operations --

    void visitSearch(const RamSearch& search, std::ostream& out) override {
        auto condition = search.getCondition();
        if (condition) {
            out << "if( " << print(condition) << ") {\n" << print(search.getNestedOperation()) << "}\n";
            if (Global::config().has("profile")) {
                out << " else { ++private_num_failed_proofs; }";
            }
        } else {
            out << print(search.getNestedOperation());
        }
    }

    void visitScan(const RamScan& scan, std::ostream& out) override {
        // get relation name
        const auto& rel = scan.getRelation();
        auto relName = getRelationName(rel);
        auto ctxName = "READ_OP_CONTEXT(" + getOpContextName(rel) + ")";
        auto level = scan.getLevel();

        // if this search is a full scan
        if (scan.getRangeQueryColumns() == 0) {
            if (scan.isPureExistenceCheck()) {
                out << "if(!" << relName << "->"
                    << "empty()) {\n";
                visitSearch(scan, out);
                out << "}\n";
            } else if (scan.getLevel() == 0) {
                // make this loop parallel
                out << "pfor(auto it = part.begin(); it<part.end(); ++it) \n";
                out << "try{";
                out << "for(const auto& env0 : *it) {\n";
                visitSearch(scan, out);
                out << "}\n";
                out << "} catch(std::exception &e) { SignalHandler::instance()->error(e.what());}\n";
            } else {
                out << "for(const auto& env" << level << " : "
                    << "*" << relName << ") {\n";
                visitSearch(scan, out);
                out << "}\n";
            }
            return;
        }

        // check list of keys
        auto arity = rel.getArity();
        const auto& rangePattern = scan.getRangePattern();

        // a lambda for printing boundary key values
        auto printKeyTuple = [&]() {
            for (size_t i = 0; i < arity; i++) {
                if (rangePattern[i] != nullptr) {
                    out << this->print(rangePattern[i]);
                } else {
                    out << "0";
                }
                if (i + 1 < arity) {
                    out << ",";
                }
            }
        };

        // get index to be queried
        auto keys = scan.getRangeQueryColumns();
        auto index = toIndex(keys);

        // if it is a equality-range query
        out << "const Tuple<RamDomain," << arity << "> key({";
        printKeyTuple();
        out << "});\n";
        out << "auto range = " << relName << "->"
            << "equalRange" << index << "(key," << ctxName << ");\n";
        if (Global::config().has("profile")) {
            out << "if (range.empty()) ++private_num_failed_proofs;\n";
        }
        if (scan.isPureExistenceCheck()) {
            out << "if(!range.empty()) {\n";
        } else {
            out << "for(const auto& env" << level << " : range) {\n";
        }
        visitSearch(scan, out);
        out << "}\n";
        return;
    }

    void visitLookup(const RamLookup& lookup, std::ostream& out) override {
        auto arity = lookup.getArity();

        // get the tuple type working with
        std::string tuple_type = "ram::Tuple<RamDomain," + toString(arity) + ">";

        // look up reference
        out << "auto ref = env" << lookup.getReferenceLevel() << "[" << lookup.getReferencePosition()
            << "];\n";
        out << "if (isNull<" << tuple_type << ">(ref)) continue;\n";
        out << tuple_type << " env" << lookup.getLevel() << " = unpack<" << tuple_type << ">(ref);\n";

        out << "{\n";

        // continue with condition checks and nested body
        visitSearch(lookup, out);

        out << "}\n";
    }

    void visitAggregate(const RamAggregate& aggregate, std::ostream& out) override {
        // get some properties
        const auto& rel = aggregate.getRelation();
        auto arity = rel.getArity();
        auto relName = getRelationName(rel);
        auto ctxName = "READ_OP_CONTEXT(" + getOpContextName(rel) + ")";
        auto level = aggregate.getLevel();

        // get the tuple type working with
        std::string tuple_type = "ram::Tuple<RamDomain," + toString(arity) + ">";

        // declare environment variable
        out << tuple_type << " env" << level << ";\n";

        // special case: counting of number elements in a full relation
        if (aggregate.getFunction() == RamAggregate::COUNT && aggregate.getRangeQueryColumns() == 0) {
            // shortcut: use relation size
            out << "env" << level << "[0] = " << relName << "->"
                << "size();\n";
            visitSearch(aggregate, out);
            return;
        }

        // init result
        std::string init;
        switch (aggregate.getFunction()) {
            case RamAggregate::MIN:
                init = "MAX_RAM_DOMAIN";
                break;
            case RamAggregate::MAX:
                init = "MIN_RAM_DOMAIN";
                break;
            case RamAggregate::COUNT:
                init = "0";
                break;
            case RamAggregate::SUM:
                init = "0";
                break;
        }
        out << "RamDomain res = " << init << ";\n";

        // get range to aggregate
        auto keys = aggregate.getRangeQueryColumns();

        // check whether there is an index to use
        if (keys == 0) {
            // no index => use full relation
            out << "auto& range = "
                << "*" << relName << ";\n";
        } else {
            // a lambda for printing boundary key values
            auto printKeyTuple = [&]() {
                for (size_t i = 0; i < arity; i++) {
                    if (aggregate.getPattern()[i] != nullptr) {
                        out << this->print(aggregate.getPattern()[i]);
                    } else {
                        out << "0";
                    }
                    if (i + 1 < arity) {
                        out << ",";
                    }
                }
            };

            // get index
            auto index = toIndex(keys);
            out << "const " << tuple_type << " key({";
            printKeyTuple();
            out << "});\n";
            out << "auto range = " << relName << "->"
                << "equalRange" << index << "(key," << ctxName << ");\n";
        }

        // add existence check
        if (aggregate.getFunction() != RamAggregate::COUNT) {
            out << "if(!range.empty()) {\n";
        }

        // aggregate result
        out << "for(const auto& cur : range) {\n";

        // create aggregation code
        if (aggregate.getFunction() == RamAggregate::COUNT) {
            // count is easy
            out << "++res\n;";
        } else if (aggregate.getFunction() == RamAggregate::SUM) {
            out << "env" << level << " = cur;\n";
            out << "res += ";
            visit(*aggregate.getTargetExpression(), out);
            out << ";\n";
        } else {
            // pick function
            std::string fun = "min";
            switch (aggregate.getFunction()) {
                case RamAggregate::MIN:
                    fun = "std::min";
                    break;
                case RamAggregate::MAX:
                    fun = "std::max";
                    break;
                case RamAggregate::COUNT:
                    assert(false);
                case RamAggregate::SUM:
                    assert(false);
            }

            out << "env" << level << " = cur;\n";
            out << "res = " << fun << "(res,";
            visit(*aggregate.getTargetExpression(), out);
            out << ");\n";
        }

        // end aggregator loop
        out << "}\n";

        // write result into environment tuple
        out << "env" << level << "[0] = res;\n";

        // continue with condition checks and nested body
        out << "{\n";

        auto condition = aggregate.getCondition();
        if (condition) {
            out << "if( " << print(condition) << ") {\n";
            visitSearch(aggregate, out);
            out << "}\n";
            if (Global::config().has("profile")) {
                out << " else { ++private_num_failed_proofs; }";
            }
        } else {
            visitSearch(aggregate, out);
        }

        out << "}\n";

        // end conditional nested block
        if (aggregate.getFunction() != RamAggregate::COUNT) {
            out << "}\n";
        }
    }

    void visitProject(const RamProject& project, std::ostream& out) override {
        const auto& rel = project.getRelation();
        auto arity = rel.getArity();
        auto relName = getRelationName(rel);
        auto ctxName = "READ_OP_CONTEXT(" + getOpContextName(rel) + ")";

        // check condition
        auto condition = project.getCondition();
        if (condition) {
            out << "if (" << print(condition) << ") {\n";
        }

        // create projected tuple
        if (project.getValues().empty()) {
            out << "Tuple<RamDomain," << arity << "> tuple({});\n";
        } else {
            out << "Tuple<RamDomain," << arity << "> tuple({(RamDomain)("
                << join(project.getValues(), "),(RamDomain)(", rec) << ")});\n";

            // check filter
        }
        if (project.hasFilter()) {
            auto relFilter = getRelationName(project.getFilter());
            auto ctxFilter = "READ_OP_CONTEXT(" + getOpContextName(project.getFilter()) + ")";
            out << "if (!" << relFilter << ".contains(tuple," << ctxFilter << ")) {";
        }

        // insert tuple
        if (Global::config().has("profile")) {
            out << "if (!(" << relName << "->"
                << "insert(tuple," << ctxName << "))) { ++private_num_failed_proofs; }\n";
        } else {
            out << relName << "->"
                << "insert(tuple," << ctxName << ");\n";
        }

        // end filter
        if (project.hasFilter()) {
            out << "}";

            // add fail counter
            if (Global::config().has("profile")) {
                out << " else { ++private_num_failed_proofs; }";
            }
        }

        // end condition
        if (condition) {
            out << "}\n";

            // add fail counter
            if (Global::config().has("profile")) {
                out << " else { ++private_num_failed_proofs; }";
            }
        }
    }

    // -- conditions --

    void visitAnd(const RamAnd& c, std::ostream& out) override {
        out << "((" << print(c.getLHS()) << ") && (" << print(c.getRHS()) << "))";
    }

    void visitBinaryRelation(const RamBinaryRelation& rel, std::ostream& out) override {
        switch (rel.getOperator()) {
            // comparison operators
            case BinaryConstraintOp::EQ:
                out << "((" << print(rel.getLHS()) << ") == (" << print(rel.getRHS()) << "))";
                break;
            case BinaryConstraintOp::NE:
                out << "((" << print(rel.getLHS()) << ") != (" << print(rel.getRHS()) << "))";
                break;
            case BinaryConstraintOp::LT:
                out << "((" << print(rel.getLHS()) << ") < (" << print(rel.getRHS()) << "))";
                break;
            case BinaryConstraintOp::LE:
                out << "((" << print(rel.getLHS()) << ") <= (" << print(rel.getRHS()) << "))";
                break;
            case BinaryConstraintOp::GT:
                out << "((" << print(rel.getLHS()) << ") > (" << print(rel.getRHS()) << "))";
                break;
            case BinaryConstraintOp::GE:
                out << "((" << print(rel.getLHS()) << ") >= (" << print(rel.getRHS()) << "))";
                break;

            // strings
            case BinaryConstraintOp::MATCH: {
                out << "regex_wrapper(symTable.resolve((size_t)";
                out << print(rel.getLHS());
                out << "),symTable.resolve((size_t)";
                out << print(rel.getRHS());
                out << "))";
                break;
            }
            case BinaryConstraintOp::NOT_MATCH: {
                out << "!regex_wrapper(symTable.resolve((size_t)";
                out << print(rel.getLHS());
                out << "),symTable.resolve((size_t)";
                out << print(rel.getRHS());
                out << "))";
                break;
            }
            case BinaryConstraintOp::CONTAINS: {
                out << "(std::string(symTable.resolve((size_t)";
                out << print(rel.getRHS());
                out << ")).find(symTable.resolve((size_t)";
                out << print(rel.getLHS());
                out << "))!=std::string::npos)";
                break;
            }
            case BinaryConstraintOp::NOT_CONTAINS: {
                out << "(std::string(symTable.resolve((size_t)";
                out << print(rel.getRHS());
                out << ")).find(symTable.resolve((size_t)";
                out << print(rel.getLHS());
                out << "))==std::string::npos)";
                break;
            }
            default:
                assert(0 && "Unsupported Operation!");
                break;
        }
    }

    void visitEmpty(const RamEmpty& empty, std::ostream& out) override {
        out << getRelationName(empty.getRelation()) << "->"
            << "empty()";
    }

    void visitNotExists(const RamNotExists& ne, std::ostream& out) override {
        // get some details
        const auto& rel = ne.getRelation();
        auto relName = getRelationName(rel);
        auto ctxName = "READ_OP_CONTEXT(" + getOpContextName(rel) + ")";
        auto arity = rel.getArity();

        // if it is total we use the contains function
        if (ne.isTotal()) {
            out << "!" << relName << "->"
                << "contains(Tuple<RamDomain," << arity << ">({" << join(ne.getValues(), ",", rec) << "}),"
                << ctxName << ")";
            return;
        }

        // else we conduct a range query
        out << relName << "->"
            << "equalRange";
        out << toIndex(ne.getKey());
        out << "(Tuple<RamDomain," << arity << ">({";
        out << join(ne.getValues(), ",", [&](std::ostream& out, RamValue* value) {
            if (!value) {
                out << "0";
            } else {
                visit(*value, out);
            }
        });
        out << "})," << ctxName << ").empty()";
    }

    // -- values --
    void visitNumber(const RamNumber& num, std::ostream& out) override {
        out << num.getConstant();
    }

    void visitElementAccess(const RamElementAccess& access, std::ostream& out) override {
        out << "env" << access.getLevel() << "[" << access.getElement() << "]";
    }

    void visitAutoIncrement(const RamAutoIncrement& /*inc*/, std::ostream& out) override {
        out << "(ctr++)";
    }

    void visitUnaryOperator(const RamUnaryOperator& op, std::ostream& out) override {
        switch (op.getOperator()) {
            case UnaryOp::ORD:
                out << print(op.getValue());
                break;
            case UnaryOp::STRLEN:
                out << "strlen(symTable.resolve((size_t)" << print(op.getValue()) << "))";
                break;
            case UnaryOp::NEG:
                out << "(-(" << print(op.getValue()) << "))";
                break;
            case UnaryOp::BNOT:
                out << "(~(" << print(op.getValue()) << "))";
                break;
            case UnaryOp::LNOT:
                out << "(!(" << print(op.getValue()) << "))";
                break;
            case UnaryOp::SIN:
                out << "sin((" << print(op.getValue()) << "))";
                break;
            case UnaryOp::COS:
                out << "cos((" << print(op.getValue()) << "))";
                break;
            case UnaryOp::TAN:
                out << "tan((" << print(op.getValue()) << "))";
                break;
            case UnaryOp::ASIN:
                out << "asin((" << print(op.getValue()) << "))";
                break;
            case UnaryOp::ACOS:
                out << "acos((" << print(op.getValue()) << "))";
                break;
            case UnaryOp::ATAN:
                out << "atan((" << print(op.getValue()) << "))";
                break;
            case UnaryOp::SINH:
                out << "sinh((" << print(op.getValue()) << "))";
                break;
            case UnaryOp::COSH:
                out << "cosh((" << print(op.getValue()) << "))";
                break;
            case UnaryOp::TANH:
                out << "tanh((" << print(op.getValue()) << "))";
                break;
            case UnaryOp::ASINH:
                out << "asinh((" << print(op.getValue()) << "))";
                break;
            case UnaryOp::ACOSH:
                out << "acosh((" << print(op.getValue()) << "))";
                break;
            case UnaryOp::ATANH:
                out << "atanh((" << print(op.getValue()) << "))";
                break;
            case UnaryOp::LOG:
                out << "log((" << print(op.getValue()) << "))";
                break;
            case UnaryOp::EXP:
                out << "exp((" << print(op.getValue()) << "))";
                break;
            default:
                assert(0 && "Unsupported Operation!");
                break;
        }
    }

    void visitBinaryOperator(const RamBinaryOperator& op, std::ostream& out) override {
        switch (op.getOperator()) {
            // arithmetic
            case BinaryOp::ADD: {
                out << "(" << print(op.getLHS()) << ") + (" << print(op.getRHS()) << ")";
                break;
            }
            case BinaryOp::SUB: {
                out << "(" << print(op.getLHS()) << ") - (" << print(op.getRHS()) << ")";
                break;
            }
            case BinaryOp::MUL: {
                out << "(" << print(op.getLHS()) << ") * (" << print(op.getRHS()) << ")";
                break;
            }
            case BinaryOp::DIV: {
                out << "(" << print(op.getLHS()) << ") / (" << print(op.getRHS()) << ")";
                break;
            }
            case BinaryOp::EXP: {
                out << "(AstDomain)(std::pow((AstDomain)" << print(op.getLHS()) << ","
                    << "(AstDomain)" << print(op.getRHS()) << "))";
                break;
            }
            case BinaryOp::MOD: {
                out << "(" << print(op.getLHS()) << ") % (" << print(op.getRHS()) << ")";
                break;
            }
            case BinaryOp::BAND: {
                out << "(" << print(op.getLHS()) << ") & (" << print(op.getRHS()) << ")";
                break;
            }
            case BinaryOp::BOR: {
                out << "(" << print(op.getLHS()) << ") | (" << print(op.getRHS()) << ")";
                break;
            }
            case BinaryOp::BXOR: {
                out << "(" << print(op.getLHS()) << ") ^ (" << print(op.getRHS()) << ")";
                break;
            }
            case BinaryOp::LAND: {
                out << "(" << print(op.getLHS()) << ") && (" << print(op.getRHS()) << ")";
                break;
            }
            case BinaryOp::LOR: {
                out << "(" << print(op.getLHS()) << ") || (" << print(op.getRHS()) << ")";
                break;
            }

            // strings
            case BinaryOp::CAT: {
                out << "(RamDomain)symTable.lookup(";
                out << "(std::string(symTable.resolve((size_t)";
                out << print(op.getLHS());
                out << ")) + std::string(symTable.resolve((size_t)";
                out << print(op.getRHS());
                out << "))).c_str())";
                break;
            }
            default:
                assert(0 && "Unsupported Operation!");
        }
    }

    void visitTernaryOperator(const RamTernaryOperator& op, std::ostream& out) override {
        switch (op.getOperator()) {
            case TernaryOp::SUBSTR:
                out << "(RamDomain)symTable.lookup(";
                out << "(substr_wrapper(symTable.resolve((size_t)";
                out << print(op.getArg(0));
                out << "),(";
                out << print(op.getArg(1));
                out << "),(";
                out << print(op.getArg(2));
                out << ")).c_str()))";
                break;
            default:
                assert(0 && "Unsupported Operation!");
        }
    }

    // -- records --

    void visitPack(const RamPack& pack, std::ostream& out) override {
        out << "pack("
            << "ram::Tuple<RamDomain," << pack.getValues().size() << ">({" << join(pack.getValues(), ",", rec)
            << "})"
            << ")";
    }

    // -- safety net --

    void visitNode(const RamNode& node, std::ostream& /*out*/) override {
        std::cerr << "Unsupported node type: " << typeid(node).name() << "\n";
        assert(false && "Unsupported Node Type!");
    }

private:
    printer print(const RamNode& node) {
        return printer(*this, node);
    }

    printer print(const RamNode* node) {
        return print(*node);
    }
};

void genCode(std::ostream& out, const RamStatement& stmt, const IndexMap& indices) {
    // use printer
    Printer(indices).visit(stmt, out);
}
}  // namespace

std::string RamCompiler::resolveFileName() const {
    if (Global::config().get("dl-program") == "") {
        // generate temporary file
        char templ[40] = "./souffleXXXXXX";
        close(mkstemp(templ));
        return templ;
    }
    return Global::config().get("dl-program");
}

std::string RamCompiler::generateCode(
        const SymbolTable& symTable, const RamStatement& stmt, const std::string& filename) const {
    // ---------------------------------------------------------------
    //                      Auto-Index Generation
    // ---------------------------------------------------------------

    // collect all used indices
    IndexMap indices;
    visitDepthFirst(stmt, [&](const RamNode& node) {
        if (const RamScan* scan = dynamic_cast<const RamScan*>(&node)) {
            indices[scan->getRelation()].addSearch(scan->getRangeQueryColumns());
        }
        if (const RamAggregate* agg = dynamic_cast<const RamAggregate*>(&node)) {
            indices[agg->getRelation()].addSearch(agg->getRangeQueryColumns());
        }
        if (const RamNotExists* ne = dynamic_cast<const RamNotExists*>(&node)) {
            indices[ne->getRelation()].addSearch(ne->getKey());
        }
    });

    // compute smallest number of indices (and report)
    if (report) {
        *report << "------ Auto-Index-Generation Report -------\n";
    }
    for (auto& cur : indices) {
        cur.second.solve();
        if (report) {
            *report << "Relation " << cur.first.getName() << "\n";
            *report << "\tNumber of Scan Patterns: " << cur.second.getSearches().size() << "\n";
            for (auto& cols : cur.second.getSearches()) {
                *report << "\t\t";
                for (uint32_t i = 0; i < cur.first.getArity(); i++) {
                    if ((1UL << i) & cols) {
                        *report << cur.first.getArg(i) << " ";
                    }
                }
                *report << "\n";
            }
            *report << "\tNumber of Indexes: " << cur.second.getAllOrders().size() << "\n";
            for (auto& order : cur.second.getAllOrders()) {
                *report << "\t\t";
                for (auto& i : order) {
                    *report << cur.first.getArg(i) << " ";
                }
                *report << "\n";
            }
            *report << "------ End of Auto-Index-Generation Report -------\n";
        }
    }

    // ---------------------------------------------------------------
    //                      Code Generation
    // ---------------------------------------------------------------

    // generate class name
    std::string simplename = baseName(filename);
    // strip .h/.cpp, if present
    if (endsWith(simplename, ".h")) {
        simplename = simplename.substr(0, simplename.size() - 2);
    } else if (endsWith(simplename, ".cpp")) {
        simplename = simplename.substr(0, simplename.size() - 4);
    }
    // Remove invalid characters
    for (size_t i = 0; i < simplename.length(); i++) {
        if ((!isalpha(simplename[i]) && i == 0) || !isalnum(simplename[i])) {
            simplename[i] = '_';
        }
    }

    std::string classname = "Sf_" + simplename;

    // add filename extension
    std::string source = filename;
    if (!(endsWith(source, ".h") || endsWith(source, ".cpp"))) {
        source += ".cpp";
    }

    // open output stream for header file
    std::ofstream os(source);

    // generate C++ program
    os << "#include \"souffle/CompiledSouffle.h\"\n";
    if (Global::config().has("provenance")) {
        os << "#include \"souffle/Explain.h\"\n";
        os << "#include <ncurses.h>\n";
    }
    os << "\n";
    os << "namespace souffle {\n";
    os << "using namespace ram;\n";

    // print wrapper for regex
    os << "class " << classname << " : public SouffleProgram {\n";
    os << "private:\n";
    os << "static inline bool regex_wrapper(const char *pattern, const char *text) {\n";
    os << "   bool result = false; \n";
    os << "   try { result = std::regex_match(text, std::regex(pattern)); } catch(...) { \n";
    os << "     std::cerr << \"warning: wrong pattern provided for match(\\\"\" << pattern << \"\\\",\\\"\" "
          "<< text << \"\\\")\\n\";\n}\n";
    os << "   return result;\n";
    os << "}\n";
    os << "static inline std::string substr_wrapper(const char *str, size_t idx, size_t len) {\n";
    os << "   std::string sub_str, result; \n";
    os << "   try { result = std::string(str).substr(idx,len); } catch(...) { \n";
    os << "     std::cerr << \"warning: wrong index position provided by substr(\\\"\";\n";
    os << "     std::cerr << str << \"\\\",\" << idx << \",\" << len << \") functor.\\n\";\n";
    os << "   } return result;\n";
    os << "}\n";

    if (Global::config().has("profile")) {
        os << "std::string profiling_fname;\n";
    }

    // declare symbol table
    os << "public:\n";
    os << "SymbolTable symTable;\n";

    // print relation definitions
    std::string initCons;      // initialization of constructor
    std::string deleteForNew;  // matching deletes for each new, used in the destructor
    std::string registerRel;   // registration of relations
    int relCtr = 0;
    std::string tempType;  // string to hold the type of the temporary relations
    visitDepthFirst(stmt, [&](const RamCreate& create) {

        // get some table details
        const auto& rel = create.getRelation();
        int arity = rel.getArity();
        const std::string& raw_name = rel.getName();
        const std::string& name = getRelationName(rel);

        // ensure that the type of the new knowledge is the same as that of the delta knowledge
        tempType = (rel.isTemp() && raw_name.find("@delta") != std::string::npos)
                           ? getRelationType(rel, rel.getArity(), indices[rel])
                           : tempType;
        const std::string& type =
                (rel.isTemp()) ? tempType : getRelationType(rel, rel.getArity(), indices[rel]);

        // defining table
        os << "// -- Table: " << raw_name << "\n";
        os << type << "* " << name << ";\n";
        if (initCons.size() > 0) {
            initCons += ",\n";
        }
        initCons += name + "(new " + type + "())";
        deleteForNew += "delete " + name + ";\n";
        if ((rel.isInput() || rel.isComputed()) && !rel.isTemp()) {
            os << "souffle::RelationWrapper<";
            os << relCtr++ << ",";
            os << type << ",";
            os << "Tuple<RamDomain," << arity << ">,";
            os << arity << ",";
            os << (rel.isInput() ? "true" : "false") << ",";
            os << (rel.isComputed() ? "true" : "false");
            os << "> wrapper_" << name << ";\n";

            // construct types
            std::string tupleType = "std::array<const char *," + std::to_string(arity) + ">{";
            std::string tupleName = "std::array<const char *," + std::to_string(arity) + ">{";

            if (rel.getArity()) {
                tupleType += "\"" + rel.getArgTypeQualifier(0) + "\"";
                for (int i = 1; i < arity; i++) {
                    tupleType += ",\"" + rel.getArgTypeQualifier(i) + "\"";
                }

                tupleName += "\"" + rel.getArg(0) + "\"";
                for (int i = 1; i < arity; i++) {
                    tupleName += ",\"" + rel.getArg(i) + "\"";
                }
            }
            tupleType += "}";
            tupleName += "}";

            initCons += ",\nwrapper_" + name + "(" + "*" + name + ",symTable,\"" + raw_name + "\"," +
                        tupleType + "," + tupleName + ")";
            registerRel += "addRelation(\"" + raw_name + "\",&wrapper_" + name + "," +
                           std::to_string(rel.isInput()) + "," + std::to_string(rel.isOutput()) + ");\n";
        }
    });

    os << "public:\n";

    // -- constructor --

    os << classname;
    if (Global::config().has("profile")) {
        os << "(std::string pf=\"profile.log\") : profiling_fname(pf)";
        if (initCons.size() > 0) {
            os << ",\n";
        }
    } else {
        os << "() : \n";
    }
    os << initCons;
    os << "{\n";
    os << registerRel;

    if (symTable.size() > 0) {
        os << "// -- initialize symbol table --\n";
        os << "static const char *symbols[]={\n";
        for (size_t i = 0; i < symTable.size(); i++) {
            os << "\tR\"(" << symTable.resolve(i) << ")\",\n";
        }
        os << "};\n";
        os << "symTable.insert(symbols," << symTable.size() << ");\n";
        os << "\n";
    }

    os << "}\n";

    // -- destructor --

    os << "~" << classname << "() {\n";
    os << deleteForNew;
    os << "}\n";

    // -- run function --

    os << "void run() {\n";

    // initialize counter
    os << "// -- initialize counter --\n";
    os << "std::atomic<RamDomain> ctr(0);\n\n";

    // set default threads (in embedded mode)
    if (std::stoi(Global::config().get("jobs")) > 0) {
        os << "#if defined(__EMBEDDED_SOUFFLE__) && defined(_OPENMP)\n";
        os << "omp_set_num_threads(" << std::stoi(Global::config().get("jobs")) << ");\n";
        os << "#endif\n\n";
    }

    // add actual program body
    os << "// -- query evaluation --\n";
    if (Global::config().has("profile")) {
        os << "std::ofstream profile(profiling_fname);\n";
        os << "profile << \"@start-debug\\n\";\n";
        genCode(os, stmt, indices);
    } else {
        genCode(os, stmt, indices);
    }
    os << "}\n";  // end of run() method

    // issue printAll method
    os << "public:\n";
    os << "void printAll(std::string dirname) {\n";
    visitDepthFirst(stmt, [&](const RamStatement& node) {
        if (auto store = dynamic_cast<const RamStore*>(&node)) {
            for (IODirectives ioDirectives : store->getRelation().getOutputDirectives()) {
                os << "try {";
                os << "std::map<std::string, std::string> directiveMap(" << ioDirectives << ");\n";
                os << "if (!dirname.empty() && directiveMap[\"IO\"] == \"file\" && ";
                os << "directiveMap[\"filename\"].front() != '/') {";
                os << "directiveMap[\"filename\"] = dirname + \"/\" + directiveMap[\"filename\"];";
                os << "}\n";
                os << "IODirectives ioDirectives(directiveMap);\n";
                os << "IOSystem::getInstance().getWriter(";
                os << "SymbolMask({" << store->getRelation().getSymbolMask() << "})";
                os << ", symTable, ioDirectives";
                os << ")->writeAll(*" << getRelationName(store->getRelation()) << ");\n";

                os << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
            }
        } else if (auto print = dynamic_cast<const RamPrintSize*>(&node)) {
            os << "{ auto lease = getOutputLock().acquire(); \n";
            os << "std::cout << R\"(" << print->getLabel() << ")\" <<  ";
            os << getRelationName(print->getRelation()) << "->"
               << "size() << \"\\n\";\n";
            os << "}";
        }
    });
    os << "}\n";  // end of printAll() method

    // issue loadAll method
    os << "public:\n";
    os << "void loadAll(std::string dirname) {\n";
    visitDepthFirst(stmt, [&](const RamLoad& load) {
        IODirectives ioDirectives = load.getRelation().getInputDirectives();

        // get some table details
        os << "try {";
        os << "std::map<std::string, std::string> directiveMap(";
        os << load.getRelation().getInputDirectives() << ");\n";
        os << "if (!dirname.empty() && directiveMap[\"IO\"] == \"file\" && ";
        os << "directiveMap[\"filename\"].front() != '/') {";
        os << "directiveMap[\"filename\"] = dirname + \"/\" + directiveMap[\"filename\"];";
        os << "}\n";
        os << "IODirectives ioDirectives(directiveMap);\n";
        os << "IOSystem::getInstance().getReader(";
        os << "SymbolMask({" << load.getRelation().getSymbolMask() << "})";
        os << ", symTable, ioDirectives)->readAll(*" << getRelationName(load.getRelation());
        os << ");\n";
        os << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
    });
    os << "}\n";  // end of loadAll() method

    // issue dump methods
    auto dumpRelation = [&](const std::string& name, const SymbolMask& mask, size_t arity) {
        auto relName = name;

        os << "try {";
        os << "IODirectives ioDirectives;\n";
        os << "ioDirectives.setIOType(\"stdout\");\n";
        os << "ioDirectives.setRelationName(\"" << name << "\");\n";
        os << "IOSystem::getInstance().getWriter(";
        os << "SymbolMask({" << mask << "})";
        os << ", symTable, ioDirectives";
        os << ")->writeAll(*" << relName << ");\n";
        os << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
    };

    // dump inputs
    os << "public:\n";
    os << "void dumpInputs(std::ostream& out = std::cout) {\n";
    visitDepthFirst(stmt, [&](const RamLoad& load) {
        auto& name = getRelationName(load.getRelation());
        auto& mask = load.getRelation().getSymbolMask();
        size_t arity = load.getRelation().getArity();
        dumpRelation(name, mask, arity);
    });
    os << "}\n";  // end of dumpInputs() method

    // dump outputs
    os << "public:\n";
    os << "void dumpOutputs(std::ostream& out = std::cout) {\n";
    visitDepthFirst(stmt, [&](const RamStore& store) {
        auto& name = getRelationName(store.getRelation());
        auto& mask = store.getRelation().getSymbolMask();
        size_t arity = store.getRelation().getArity();
        dumpRelation(name, mask, arity);
    });
    os << "}\n";  // end of dumpOutputs() method

    os << "public:\n";
    os << "const SymbolTable &getSymbolTable() const {\n";
    os << "return symTable;\n";
    os << "}\n";  // end of getSymbolTable() method

    os << "};\n";  // end of class declaration

    // hidden hooks
    os << "SouffleProgram *newInstance_" << simplename << "(){return new " << classname << ";}\n";
    os << "SymbolTable *getST_" << simplename << "(SouffleProgram *p){return &reinterpret_cast<" << classname
       << "*>(p)->symTable;}\n";

    os << "#ifdef __EMBEDDED_SOUFFLE__\n";
    os << "class factory_" << classname << ": public souffle::ProgramFactory {\n";
    os << "SouffleProgram *newInstance() {\n";
    os << "return new " << classname << "();\n";
    os << "};\n";
    os << "public:\n";
    os << "factory_" << classname << "() : ProgramFactory(\"" << simplename << "\"){}\n";
    os << "};\n";
    os << "static factory_" << classname << " __factory_" << classname << "_instance;\n";
    os << "}\n";
    os << "#else\n";
    os << "}\n";
    os << "int main(int argc, char** argv)\n{\n";
    os << "try{\n";

    // parse arguments
    os << "souffle::CmdOptions opt(";
    os << "R\"(" << Global::config().get("") << ")\",\n";
    os << "R\"(.)\",\n";
    os << "R\"(.)\",\n";
    if (Global::config().has("profile")) {
        os << "true,\n";
        os << "R\"(" << Global::config().get("profile") << ")\",\n";
    } else {
        os << "false,\n";
        os << "R\"()\",\n";
    }
    os << std::stoi(Global::config().get("jobs")) << "\n";
    os << ");\n";

    os << "if (!opt.parse(argc,argv)) return 1;\n";

    os << "#if defined(_OPENMP) \n";
    os << "omp_set_nested(true);\n";
    os << "#endif\n";

    os << "souffle::";
    if (Global::config().has("profile")) {
        os << classname + " obj(opt.getProfileName());\n";
    } else {
        os << classname + " obj;\n";
    }

    os << "obj.loadAll(opt.getInputFileDir());\n";
    os << "obj.run();\n";
    os << "obj.printAll(opt.getOutputFileDir());\n";
    if (Global::config().get("provenance") == "1") {
        os << "explain(obj, true);\n";
    } else if (Global::config().get("provenance") == "2") {
        os << "explain(obj, false);\n";
    }
    os << "return 0;\n";
    os << "} catch(std::exception &e) { souffle::SignalHandler::instance()->error(e.what());}\n";
    os << "}\n";
    os << "#endif\n";

    // close source file
    os.close();

    // return the filename
    return source;
}

std::string RamCompiler::compileToLibrary(
        const SymbolTable& symTable, const RamStatement& stmt, const std::string& filename) const {
    std::string source = generateCode(symTable, stmt, filename + ".cpp");

    // execute shell script that compiles the generated C++ program
    std::string libCmd = "souffle-compilelib " + filename;

    // separate souffle output form executable output
    if (Global::config().has("profile")) {
        std::cout.flush();
    }

    // run executable
    if (system(libCmd.c_str()) != 0) {
        std::cerr << "failed to compile C++ source " << filename << "\n";
        std::cerr << "Have you installed souffle with java?\n";
        return "";
    }

    // done
    return filename;
}

std::string RamCompiler::compileToBinary(const SymbolTable& symTable, const RamStatement& stmt) const {
    // ---------------------------------------------------------------
    //                       Code Generation
    // ---------------------------------------------------------------

    std::string binary = resolveFileName();
    std::string source = generateCode(symTable, stmt, binary + ".cpp");

    // ---------------------------------------------------------------
    //                    Compilation & Execution
    // ---------------------------------------------------------------

    std::string cmd = compileCmd;

    // set up number of threads
    auto num_threads = std::stoi(Global::config().get("jobs"));
    if (num_threads == 1) {
        cmd += "-s ";
    }

    // add source code
    cmd += source;

    // separate souffle output form executable output
    if (Global::config().has("profile")) {
        std::cout.flush();
    }

    // run executable
    if (system(cmd.c_str()) != 0) {
        std::cerr << "failed to compile C++ source " << binary << "\n";
    }

    // done
    return binary;
}

void RamCompiler::applyOn(const RamStatement& stmt, RamEnvironment& env, RamData* /*data*/) const {
    // compile statement
    std::string binary = compileToBinary(env.getSymbolTable(), stmt);

    // separate souffle output form executable output
    if (Global::config().has("profile")) {
        std::cout.flush();
    }

    // check whether the executable exists
    if (!isExecutable(binary)) {
        std::cerr << "failed to run executable " << binary << "\n";
    }

    // run executable
    int result = system(binary.c_str());
    if (Global::config().get("dl-program").empty()) {
        remove(binary.c_str());
        remove((binary + ".cpp").c_str());
    }
    if (result != 0) {
        exit(result);
    }
}

}  // end of namespace souffle
