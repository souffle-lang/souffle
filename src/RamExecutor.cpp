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

#include <chrono>
#include <regex>
#include <unistd.h>
#include <algorithm>
#include <cmath>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "RamTranslator.h"
#include "RamExecutor.h"
#include "RamVisitor.h"
#include "RamAutoIndex.h"
#include "RamLogger.h"
#include "AstRelation.h"
#include "BinaryOperator.h"

#include "AstVisitor.h"
#include "RuleScheduler.h"
#include "TypeSystem.h"
#include "RamData.h"

namespace souffle {

namespace {

    class EvalContext {

        std::vector<const RamDomain*> data;

    public:

        EvalContext(size_t size = 0) : data(size) {}

        const RamDomain*& operator[](size_t index) {
            return data[index];
        }

        const RamDomain* const & operator[](size_t index) const {
            return data[index];
        }

    };


    RamDomain eval(const RamValue& value, RamEnvironment& env, const EvalContext& ctxt = EvalContext()) {

        class Evaluator : public RamVisitor<RamDomain> {

            RamEnvironment& env;
            const EvalContext& ctxt;

        public:

            Evaluator(RamEnvironment& env, const EvalContext& ctxt)
                : env(env), ctxt(ctxt) {}

            // -- basics --
            RamDomain visitNumber(const RamNumber& num) {
                return num.getConstant();
            }

            RamDomain visitElementAccess(const RamElementAccess& access) {
                return ctxt[access.getLevel()][access.getElement()];
            }

            RamDomain visitAutoIncrement(const RamAutoIncrement& inc) {
                return env.incCounter();
            }

            RamDomain visitBinaryOperator(const RamBinaryOperator& op) {
                switch(op.getOperator()) {

                // arithmetic
                case BinaryOp::ADD: return visit(op.getLHS()) + visit(op.getRHS());
                case BinaryOp::SUB: return visit(op.getLHS()) - visit(op.getRHS());
                case BinaryOp::MUL: return visit(op.getLHS()) * visit(op.getRHS());
                case BinaryOp::DIV: return visit(op.getLHS()) / visit(op.getRHS());
                case BinaryOp::EXP: return std::pow(visit(op.getLHS()), visit(op.getRHS()));
                case BinaryOp::MOD: return visit(op.getLHS()) % visit(op.getRHS());
                case BinaryOp::BAND: return visit(op.getLHS()) & visit(op.getRHS());
                case BinaryOp::BOR: return visit(op.getLHS()) | visit(op.getRHS());
                case BinaryOp::BXOR: return visit(op.getLHS()) ^ visit(op.getRHS());
                case BinaryOp::LAND: return visit(op.getLHS()) && visit(op.getRHS());
                case BinaryOp::LOR: return visit(op.getLHS()) || visit(op.getRHS());

                // strings
                case BinaryOp::CAT: {
                    RamDomain a = visit(op.getLHS());
                    RamDomain b = visit(op.getRHS());
                    const std::string& l = env.getSymbolTable().resolve(a);
                    const std::string& r = env.getSymbolTable().resolve(b);
                    return env.getSymbolTable().lookup((l + r).c_str());
                }
                default:
                    assert(0 && "unsupported operator");
                    return 0;
                }
            }

            RamDomain visitNegation(const RamNegation& op) {
                return -visit(op.getValue());
            }

            RamDomain visitComplement(const RamComplement& op) {
                return ~visit(op.getValue());
            }

            RamDomain visitNot(const RamNot& op) {
                return !visit(op.getValue());
            }

            RamDomain visitOrd(const RamOrd& op) {
                return visit(op.getSymbol());
            }


            // -- records --

            RamDomain visitPack(const RamPack& op) {
                auto values = op.getValues();
                auto arity = values.size();
                RamDomain data[arity];
                for(size_t i=0; i<arity; ++i) {
                    data[i] = visit(values[i]);
                }
                return pack(data, arity);
            }


            // -- safety net --

            RamDomain visitNode(const RamNode& node) {
                std::cout << "Unsupported node Type: " << typeid(node).name() << "\n";
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

            Evaluator(RamEnvironment& env, const EvalContext& ctxt)
                : env(env), ctxt(ctxt) {}

            // -- connectors operators --

            bool visitAnd(const RamAnd& a) {
                return visit(a.getLHS()) && visit(a.getRHS());
            }

            // -- relation operations --

            bool visitEmpty(const RamEmpty& empty) {
                return env.getRelation(empty.getRelation()).empty();
            }

            bool visitNotExists(const RamNotExists& ne) {
                const RamRelation& rel = env.getRelation(ne.getRelation());

                // construct the pattern tuple
                auto arity = rel.getArity();
                auto values = ne.getValues();

                // for total we use the exists test
                if (ne.isTotal()) {
                    RamDomain tuple[arity];
					for(size_t i=0;i<arity;i++) {
						tuple[i]= (values[i]) ? eval(values[i],env,ctxt) : MIN_RAM_DOMAIN;
					}

                    return !rel.exists(tuple);
                }

                // for partial we search for lower and upper boundaries
                RamDomain low[arity];
                RamDomain high[arity];
                for(size_t i=0;i<arity;i++) {
                    low[i]= (values[i]) ? eval(values[i],env,ctxt) : MIN_RAM_DOMAIN;
                    high[i]= (values[i]) ? low[i] : MAX_RAM_DOMAIN;
                }

                // obtain index
                auto idx = ne.getIndex();
                if (!idx) {
                    idx = rel.getIndex(ne.getKey());
                    ne.setIndex(idx);
                }

                auto range = idx->lowerUpperBound(low,high);
                return range.first == range.second;     // if there are none => done
            }

            // -- comparison operators --

            bool visitBinaryRelation(const RamBinaryRelation& relOp) {
                switch (relOp.getOperator()) {

                // comparison operators
                case BinaryRelOp::EQ: return eval(relOp.getLHS(),env,ctxt) == eval(relOp.getRHS(),env,ctxt);
                case BinaryRelOp::NE: return eval(relOp.getLHS(),env,ctxt) != eval(relOp.getRHS(),env,ctxt);
                case BinaryRelOp::LT: return eval(relOp.getLHS(),env,ctxt) < eval(relOp.getRHS(),env,ctxt);
                case BinaryRelOp::LE: return eval(relOp.getLHS(),env,ctxt) <= eval(relOp.getRHS(),env,ctxt);
                case BinaryRelOp::GT: return eval(relOp.getLHS(),env,ctxt) > eval(relOp.getRHS(),env,ctxt);
                case BinaryRelOp::GE: return eval(relOp.getLHS(),env,ctxt) >= eval(relOp.getRHS(),env,ctxt);

                // strings
                case BinaryRelOp::MATCH: {
                    RamDomain l = eval(relOp.getLHS(), env, ctxt);
                    RamDomain r = eval(relOp.getRHS(), env, ctxt);
                    const std::string& pattern = env.getSymbolTable().resolve(l);
                    const std::string& text = env.getSymbolTable().resolve(r);
                    bool result = false;
                    try {
                        result = std::regex_match(text, std::regex(pattern));
                    } catch(...) {
                        std::cerr << "warning: wrong pattern provided for match(\"" << pattern << "\",\"" << text << "\")\n";
                    }
                    return result;
                }
                case BinaryRelOp::CONTAINS: {
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

            bool visitNode(const RamNode& node) {
                std::cout << "Unsupported node Type: " << typeid(node).name() << "\n";
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

            Interpreter(RamEnvironment& env, EvalContext& ctxt)
                : env(env), ctxt(ctxt) {}

            // -- Operations -----------------------------

            void visitSearch(const RamSearch& search) {

                // check condition
                auto condition = search.getCondition();
                if (condition && !eval(*condition, env, ctxt)) {
                    return;    // condition not valid => skip nested
                }

                // process nested
                visit(*search.getNestedOperation());
            }

            void visitScan(const RamScan& scan) {

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
                    for(const RamDomain* cur : rel) {
                        ctxt[scan.getLevel()]=cur;
                        visitSearch(scan);
                    }
                    return;
                }

                // create pattern tuple for range query
                auto arity = rel.getArity();
                RamDomain low[arity];
                RamDomain hig[arity];
                auto pattern = scan.getRangePattern();
                for(size_t i=0; i<arity; i++) {
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
                if (!idx) {
                    idx = rel.getIndex(scan.getRangeQueryColumns());
                    scan.setIndex(idx);
                }

                // get iterator range
                auto range = idx->lowerUpperBound(low,hig);

                // if this scan is not binding anything ...
                if (scan.isPureExistenceCheck()) {
                    if (range.first != range.second) {
                        visitSearch(scan);
                    }
                    return;
                }

                // conduct range query
                for(auto ip=range.first;ip!=range.second;++ip) {
                    const RamDomain* data = *(ip);
                    ctxt[scan.getLevel()] = data;
                    visitSearch(scan);
                }
            }

            void visitLookup(const RamLookup& lookup) {

                // get reference
                RamDomain ref = ctxt[lookup.getReferenceLevel()][lookup.getReferencePosition()];

                // check for null
                if (isNull(ref)) return;

                // update environment variable
                auto arity = lookup.getArity();
                const RamDomain* tuple = unpack(ref,arity);

                // save reference to temporary value
                ctxt[lookup.getLevel()] = tuple;

                // run nested part - using base class visitor
                visitSearch(lookup);
            }

            void visitAggregate(const RamAggregate& aggregate) {
                // get the targeted relation
                const RamRelation& rel = env.getRelation(aggregate.getRelation());

                // initialize result
                RamDomain res = 0;
                switch(aggregate.getFunction()) {
                case RamAggregate::MIN: res = MAX_RAM_DOMAIN; break;
                case RamAggregate::MAX: res = MIN_RAM_DOMAIN; break;
                case RamAggregate::COUNT: res = 0; break;
                }

                // init temporary tuple for this level
                auto arity = rel.getArity();

                // get lower and upper boundaries for iteration
                const auto& pattern = aggregate.getPattern();
                RamDomain low[arity];
                RamDomain hig[arity];

                for(size_t i=0; i<arity; i++) {
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
                auto range = idx->lowerUpperBound(low,hig);

                // check for emptiness
                if (aggregate.getFunction() != RamAggregate::COUNT) {
                    if (range.first == range.second) return;        // no elements => no min/max
                }

                // iterate through values
                for(auto ip=range.first;ip!=range.second;++ip) {

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

                    switch(aggregate.getFunction()) {
                    case RamAggregate::MIN: res = std::min(res,cur); break;
                    case RamAggregate::MAX: res = std::max(res,cur); break;
                    case RamAggregate::COUNT: res = 0; break;
                    }
                }

                // write result to environment
                RamDomain tuple[1];
                tuple[0] = res;
                ctxt[aggregate.getLevel()] = tuple;

                // check whether result is used in a condition
                auto condition = aggregate.getCondition();
                if (condition && !eval(*condition, env, ctxt)) {
                    return;    // condition not valid => skip nested
                }

                // run nested part - using base class visitor
                visitSearch(aggregate);
            }

            void visitProject(const RamProject& project) {
                // check constraints
                RamCondition* condition = project.getCondition();
                if (condition && !eval(*condition, env, ctxt)) {
                    return;        // condition violated => skip insert
                }

                // create a tuple of the proper arity (also supports arity 0)
                auto arity = project.getRelation().getArity();
                const auto& values = project.getValues();
                RamDomain tuple[arity];
                for(size_t i=0;i<arity;i++) {
                    tuple[i] = eval(values[i], env, ctxt);
                }

                // check filter relation
                if(project.hasFilter() && env.getRelation(project.getFilter()).exists(tuple)) {
                    return;
                }

                // insert in target relation
                env.getRelation(project.getRelation()).insert(tuple);
            }

            // -- safety net --
            void visitNode(const RamNode& node) {
                std::cout << "Unsupported node Type: " << typeid(node).name() << "\n";
                assert(false && "Unsupported Node Type!");
            }

        };

        // create and run interpreter
        EvalContext ctxt(op.getDepth());
        Interpreter(env, ctxt).visit(op);
    }


    void run(const RamExecutorConfig& config, const QueryExecutionStrategy& executor, std::ostream* report, std::ostream* profile, const RamStatement& stmt, RamEnvironment& env, RamData* data) {

        class Interpreter : public RamVisitor<bool> {

            const RamExecutorConfig& config;
            RamEnvironment& env;
            const QueryExecutionStrategy& queryExecutor;
            std::ostream* report;
            std::ostream* profile;
            RamData* data;

        public:

            Interpreter(
                    const RamExecutorConfig& config, RamEnvironment& env,
                    const QueryExecutionStrategy& executor, std::ostream* report,
                    std::ostream* profile,
                    RamData* data
            )
                : config(config), env(env), queryExecutor(executor), report(report), profile(profile), data(data) {}


            // -- Statements -----------------------------

            bool visitSequence(const RamSequence& seq) {
                return visit(seq.getFirst()) && visit(seq.getSecond());
            }

            bool visitParallel(const RamParallel& parallel) {

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
                    if (config.getNumThreads() != 0) {
                        omp_set_num_threads(config.getNumThreads());
                    }
                #endif

                // parallel execution
                bool cond = true;
                #pragma omp parallel for reduction(&&:cond)
                for(size_t i=0;i<stmts.size();i++) {
                    cond = cond && visit(stmts[i]);
                }
                return cond;
            }

            bool visitLoop(const RamLoop& loop) {
                while(visit(loop.getBody())) {}
                return true;
            }

            bool visitExit(const RamExit& exit) {
                return !eval(exit.getCondition(), env);
            }

            bool visitLogTimer(const RamLogTimer& timer) {
                RamLogger logger(timer.getLabel().c_str(), *profile);
                return visit(timer.getNested());
            }

            bool visitCreate(const RamCreate& create) {
                env.getRelation(create.getRelation());
                return true;
            }

            bool visitClear(const RamClear& clear) {
                env.getRelation(clear.getRelation()).purge();
                return true;
            }

            bool visitDrop(const RamDrop& drop) {
                env.dropRelation(drop.getRelation());
                return true;
            }

            bool visitPrintSize(const RamPrintSize& print) {
                std::cout << print.getLabel() << env.getRelation(print.getRelation()).size() << "\n";
                return true;
            }

            bool visitLogSize(const RamLogSize& print) {
                *profile << print.getLabel() << env.getRelation(print.getRelation()).size() << "\n";
                return true;
            }

            bool visitLoad(const RamLoad& load) {
                if (load.getRelation().isData()) {
                  // Load from mem
                  env.getRelation(load.getRelation()).load(data->getTuples(
                  load.getRelation().getName()), 
                  env.getSymbolTable(), 
                  load.getRelation().getSymbolMask());
                  return true;
                }

                // load facts from file
                std::ifstream csvfile;
                std::string fname = config.getFactFileDir() + "/" + load.getFileName();
                csvfile.open(fname.c_str());
                if (!csvfile.is_open()) {
                    // TODO: use different error reporting here!!
                    std::cerr << "Cannot open fact file " << baseName(fname) << "\n";
                    return false; 
                }
                if(env.getRelation(load.getRelation()).load(csvfile, env.getSymbolTable(), load.getRelation().getSymbolMask())) {
                    char *bname = strdup(fname.c_str());
                    std::string simplename = basename(bname);
                    std::cerr << "cannot parse fact file " << simplename << "!\n";
                    return false;
                };
                return true;
            }

            bool visitStore(const RamStore& store) {
                if(store.getRelation().isData()){
                  return true;
                }

                auto& rel = env.getRelation(store.getRelation());
                if (config.getOutputDir() == "-") {
                    std::cout << "---------------\n" << rel.getName() << "\n===============\n";
                    rel.store(std::cout, env.getSymbolTable(), store.getRelation().getSymbolMask());
                    std::cout << "===============\n";
                    return true;
                }
                std::ofstream fout(config.getOutputDir() + "/" + store.getFileName());
                rel.store(fout, env.getSymbolTable(), store.getRelation().getSymbolMask());
                return true;
            }

            bool visitFact(const RamFact& fact) {
                auto arity = fact.getRelation().getArity();
                RamDomain tuple[arity];
                auto values = fact.getValues();

                for(size_t i = 0 ; i < arity ; ++i) {
                    tuple[i] = eval(values[i], env);
                }

                env.getRelation(fact.getRelation()).insert(tuple);
                return true;
            }

            bool visitInsert(const RamInsert& insert) {
                // run generic query executor
                queryExecutor(config, insert, env, report);
                return true;
            }

            bool visitMerge(const RamMerge& merge) {
                // get involved relation
                RamRelation& src = env.getRelation(merge.getSourceRelation());
                RamRelation& trg = env.getRelation(merge.getTargetRelation());

                // merge in all elements
                trg.insert(src);

                // done
                return true;
            }


            // -- safety net --

            bool visitNode(const RamNode& node) {
                std::cout << "Unsupported node Type: " << typeid(node).name() << "\n";
                assert(false && "Unsupported Node Type!");
                return false;
            }

        };

        // create and run interpreter
        Interpreter(config, env, executor, report, profile, data).visit(stmt);
    }

}


void RamGuidedInterpreter::applyOn(const RamStatement& stmt, RamEnvironment& env, RamData* data) const {

    if (getConfig().isLogging()) {
        std::string fname = getConfig().getProfileName();
        // open output stream
        std::ofstream os(fname);
        if (!os.is_open()) {
            // TODO: use different error reporting here!!
            std::cerr << "Cannot open fact file " << fname << " for profiling\n";
        }
        os << "@start-debug\n";
        run(getConfig(), queryStrategy, report, &os, stmt, env, data);
    } else {
        run(getConfig(), queryStrategy, report, nullptr, stmt, env, data);
    }

}


namespace {

    using namespace scheduler;

    Order scheduleByModel(AstClause& clause, RamEnvironment& env, std::ostream* report) {
        assert(!clause.isFact());

        // check whether schedule is fixed
        if (clause.hasFixedExecutionPlan()) {
            if(report) *report << "   Skipped due to fixed execution plan!\n";
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
        auto getID = [&](const AstVariable& var)->int {
            auto pos = varIDs.find(var.getName());
            if (pos != varIDs.end()) return pos->second;
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
        for(unsigned i = 0; i<atoms.size(); i++) {

            // convert pattern of arguments
            std::vector<Argument> args;

            for(const AstArgument* arg : atoms[i]->getArguments()) {
                if(const AstVariable* var = dynamic_cast<const AstVariable*>(arg)) {
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
            p.addAtom(Atom(i, args, env.getRelation(translator.translateRelationName(atoms[i]->getName())).size()));
        }

        // solve the optimization problem
        auto schedule = p.solve();

        // log problem and solution
        if(report) *report << "Scheduling Problem: " << p << "\n";
        if(report) *report << "          Schedule: " << schedule << "\n";

        // extract order
        Order res;
        for(const auto& cur : schedule) {
            res.append(cur.getID());
        }

        // re-order atoms
        clause.reorderAtoms(res.getOrder());

        // done
        return res;
    }

}


/** With this strategy queries will be processed as they are stated by the user */
const QueryExecutionStrategy DirectExecution =
        [](const RamExecutorConfig& config, const RamInsert& insert, RamEnvironment& env, std::ostream*)->ExecutionSummary {
            // measure the time
            auto start = now();

            // simplest strategy of all - just apply the nested operation
            apply(insert.getOperation(), env);

            // create report
            auto end = now();
            return ExecutionSummary({Order::getIdentity(insert.getOrigin().getAtoms().size()), duration_in_ms(start, end)});
        };

/** With this strategy queries will be dynamically rescheduled before each execution */
const QueryExecutionStrategy ScheduledExecution =
        [](const RamExecutorConfig& config, const RamInsert& insert, RamEnvironment& env, std::ostream* report)->ExecutionSummary {

            // Report scheduling
            // TODO: only re-schedule atoms (avoid cloning entire clause)
            std::unique_ptr<AstClause> clause(insert.getOrigin().clone());

            Order order;

            // (re-)schedule clause
            if (report) *report << "\nScheduling clause @ " << clause->getSrcLoc() << "\n";
            {
                auto start = now();
                order = scheduleByModel(*clause, env, report);
                auto end = now();
                if (report) *report << "    Original Query: " << insert.getOrigin() << "\n";
                if (report) *report << "       Rescheduled: " << *clause << "\n";
                if (!equal_targets(insert.getOrigin().getAtoms(), clause->getAtoms())) {
                    if (report) *report << "            Order has Changed!\n";
                }
                if (report) *report << "   Scheduling Time: " << duration_in_ms(start, end) << "ms\n";
            }


            // create operation
            std::unique_ptr<RamStatement> stmt = RamTranslator(config.isLogging()).translateClause(*clause, nullptr, nullptr);
            assert(dynamic_cast<RamInsert*>(stmt.get()));

            // run rescheduled node
            auto start = now();
            apply(static_cast<RamInsert*>(stmt.get())->getOperation(), env);
            auto end = now();
            auto runtime = duration_in_ms(start, end);
            if (report) *report << "           Runtime: " << runtime << "ms\n";

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

    std::string getRelationType(std::size_t arity, const RamAutoIndex& indices) {
        std::stringstream res;
        res << "ram::Relation<" << arity;
        for(auto &cur : indices.getAllOrders() ) {
            res << ",ram::index<";
            res << join(cur, ",");
            res << ">";
        }
        res << ">";
        return res.str();
    }

    std::string toIndex(SearchColumns key) {
        std::stringstream tmp;
        tmp << "<";
        int i =0;
        while(key != 0) {
            if (key % 2) {
                tmp << i;
                if (key > 1) tmp << ",";
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

        const RamExecutorConfig& config;

        std::function<void(std::ostream&,const RamNode*)> rec;

        struct printer {
            Printer& p;
            const RamNode& node;
            printer(Printer& p, const RamNode& n) : p(p), node(n) {}
            printer(const printer& other) : p(other.p), node(other.node) {}
            friend std::ostream& operator<<(std::ostream& out, const printer& p) {
                p.p.visit(p.node, out);
                return out;
            }
        };

    public:

        Printer(const RamExecutorConfig& config, const IndexMap&)
            : config(config) {
            rec = [&](std::ostream& out, const RamNode* node) {
              this->visit(*node, out);
            };
        }

        // -- relation statements --

        void visitCreate(const RamCreate& create, std::ostream& out) {
        }

        void visitFact(const RamFact& fact, std::ostream& out) {
            out << getRelationName(fact.getRelation()) << ".insert("
                << join(fact.getValues(), ",", rec)
                << ");\n";
        }

        void visitLoad(const RamLoad& load, std::ostream& out) {
        }

        void visitStore(const RamStore& store, std::ostream& out) {
        }

        void visitInsert(const RamInsert& insert, std::ostream& out) {

            // enclose operation with a check for an empty relation
            std::set<RamRelationIdentifier> input_relations;
            visitDepthFirst(insert, [&](const RamScan& scan) {
                input_relations.insert(scan.getRelation());
            });
            if (!input_relations.empty()) {
                out << "if (" << join(input_relations, "&&", [&](std::ostream& out, const RamRelationIdentifier& rel){
                    out << "!" << this->getRelationName(rel) << ".empty()";
                }) << ") ";
            }

            // outline each search operation to improve compilation time
            out << "[&]()";

            // enclose operation in its own scope
            out << "{\n";

            // create proof counters
            if (config.isLogging()) {
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
                    out << "auto part = " << getRelationName(scan->getRelation()) << ".partition();\n";

                    // build a parallel block around this loop nest
                    out << "PARALLEL_START;\n";

                }
            }

            // add local counters
            if (config.isLogging()) {
                out << "uint64_t private_num_failed_proofs = 0;\n";
            }

            // create operation contexts for this operation
            for(const RamRelationIdentifier& rel : getReferencedRelations(insert.getOperation())) {
                out << "CREATE_OP_CONTEXT(" << getOpContextName(rel) << ","<< getRelationName(rel) << ".createContext());\n";
            }

            out << print(insert.getOperation());

            // aggregate proof counters
            if (config.isLogging()) {
                out << "num_failed_proofs += private_num_failed_proofs;\n";
            }

            if (parallel) out << "PARALLEL_END;\n";       // end parallel

            // aggregate proof counters
            if (config.isLogging()) {

                // get target relation
                RamRelationIdentifier rel;
                visitDepthFirst(insert, [&](const RamProject& project) {
                    rel = project.getRelation();
                });

                // build log message
                auto &clause = insert.getOrigin();
                std::string clauseText = toString(clause);
                replace(clauseText.begin(), clauseText.end(), '"', '\'');
                replace(clauseText.begin(), clauseText.end(), '\n', ' ');

                std::ostringstream line;
                line << "p-proof-counter;" << rel.getName() << ";" << clause.getSrcLoc() << ";" << clauseText << ";";
                std::string label = line.str();

                // print log entry
                out << "{ auto lease = getOutputLock().acquire(); ";
                out << "profile << R\"(#" << label << ";)\" << num_failed_proofs << \"\\n\";\n";
                out << "}";
            }

            out << "}\n";       // end lambda
            out << "();";       // call lambda
        }

        void visitMerge(const RamMerge& merge, std::ostream& out) {
            out << getRelationName(merge.getTargetRelation()) << ".insertAll("
                << getRelationName(merge.getSourceRelation())
                << ");\n";
        }

        void visitClear(const RamClear& clear, std::ostream& out) {
            out << getRelationName(clear.getRelation()) << ".purge();\n";
        }

        void visitDrop(const RamDrop& drop, std::ostream& out) {
            std::string name = getRelationName(drop.getRelation());
            bool isTemp = (name.find("rel__temp1_")==0) || (name.find("rel__temp2_")==0);
            if (!config.isDebug() || isTemp) {
                out << name << ".purge();\n";
            }
        }

        void visitPrintSize(const RamPrintSize& print, std::ostream& out) {
        }

        void visitLogSize(const RamLogSize& print, std::ostream& out) {
            out << "{ auto lease = getOutputLock().acquire(); \n";
            out << "profile << R\"(" << print.getLabel() << ")\" <<  ";
            out << getRelationName(print.getRelation());
            out << ".size() << \"\\n\";\n" << "}";
        }

        // -- control flow statements --

        void visitSequence(const RamSequence& seq, std::ostream& out) {
            out << print(seq.getFirst());
            out << print(seq.getSecond());
        }

        void visitParallel(const RamParallel& parallel, std::ostream& out) {
            auto stmts = parallel.getStatements();

            // special handling cases
            if (stmts.empty()) return;

            // a single statement => save the overhead
            if (stmts.size() == 1) {
                out << print(stmts[0]);
                return;
            }

            // more than one => parallel sections

            // start parallel section
            out << "SECTIONS_START;\n";

            // put each thread in another section
            for(const auto& cur : stmts) {
                out << "SECTION_START;\n";
                out << print(cur);
                out << "SECTION_END\n";
            }

            // done
            out << "SECTIONS_END;\n";
        }

        void visitLoop(const RamLoop& loop, std::ostream& out) {
            out << "for(;;) {\n" << print(loop.getBody()) << "}\n";
        }

        void visitExit(const RamExit& exit, std::ostream& out) {
            out << "if(" << print(exit.getCondition()) << ") break;\n";
        }

        void visitLogTimer(const RamLogTimer& timer, std::ostream& out) {
            // create local scope for name resolution
            out << "{\n";

            // create local timer
            out << "\tRamLogger logger(R\"(" << timer.getLabel() << ")\",profile);\n";

            // insert statement to be measured
            visit(timer.getNested(), out);

            // done
            out << "}\n";
        }

        // -- operations --

        void visitSearch(const RamSearch& search, std::ostream& out) {
            auto condition = search.getCondition();
            if(condition) {
                out << "if( " << print(condition) << ") {\n"
                        << print(search.getNestedOperation())
                    << "}\n";
                if (config.isLogging()) {
                    out << " else { ++private_num_failed_proofs; }";
                }
            } else {
                out << print(search.getNestedOperation());
            }
        }

        void visitScan(const RamScan& scan, std::ostream& out) {

            // get relation name
            const auto& rel = scan.getRelation();
            auto relName = getRelationName(rel);
            auto ctxName = "READ_OP_CONTEXT(" + getOpContextName(rel) + ")";
            auto level = scan.getLevel();

            // if this search is a full scan
            if (scan.getRangeQueryColumns() == 0) {
                if (scan.isPureExistenceCheck()) {
                    out << "if(!" << relName << ".empty()) {\n";
                } else if (scan.getLevel() == 0) {
                    // make this loop parallel
                    out << "pfor(auto it = part.begin(); it<part.end(); ++it) \n";
                    out << "for(const auto& env0 : *it) {\n";
                } else {
                    out << "for(const auto& env" << level << " : " << relName << ") {\n";
                }
                visitSearch(scan, out);
                out << "}\n";
                return;
            }

            // check list of keys
            auto arity = rel.getArity();
            const auto& rangePattern = scan.getRangePattern();

            // a lambda for printing boundary key values
            auto printKeyTuple = [&]() {
                for(size_t i=0; i<arity; i++) {
                    if (rangePattern[i] != nullptr) {
                        out << this->print(rangePattern[i]);
                    } else {
                        out << "0";
                    }
                    if (i+1 < arity) {
                        out << ",";
                    }
                }
            };

            // get index to be queried
            auto keys = scan.getRangeQueryColumns();
            auto index = toIndex(keys);

            // if it is a equality-range query
            out << "const Tuple<RamDomain," << arity << "> key({"; printKeyTuple(); out << "});\n";
            out << "auto range = " << relName << ".equalRange" << index << "(key," << ctxName << ");\n";
            if (config.isLogging()) {
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

        void visitLookup(const RamLookup& lookup, std::ostream& out) {
            auto arity = lookup.getArity();

            // get the tuple type working with
            std::string tuple_type = "ram::Tuple<RamDomain," + toString(arity) + ">";

            // look up reference
            out << "auto ref = env" << lookup.getReferenceLevel() << "[" << lookup.getReferencePosition() << "];\n";
            out << "if (isNull<" << tuple_type << ">(ref)) continue;\n";
            out << tuple_type << " env" << lookup.getLevel() << " = unpack<" << tuple_type << ">(ref);\n";

            out << "{\n";

            // continue with condition checks and nested body
            visitSearch(lookup, out);

            out << "}\n";
        }

        void visitAggregate(const RamAggregate& aggregate, std::ostream& out) {

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
                out << "env" << level << "[0] = " << relName << ".size();\n";
                visitSearch(aggregate, out);
                return;
            }

            // init result
            std::string init;
            switch(aggregate.getFunction()){
                case RamAggregate::MIN: init = "MAX_RAM_DOMAIN"; break;
                case RamAggregate::MAX: init = "MIN_RAM_DOMAIN"; break;
                case RamAggregate::COUNT: init = "0"; break;
            }
            out << "RamDomain res = " << init << ";\n";

            // get range to aggregate
            auto keys = aggregate.getRangeQueryColumns();

            // check whether there is an index to use
            if (keys == 0) {

                // no index => use full relation
                out << "auto& range = " << relName << ";\n";

            } else {

                // a lambda for printing boundary key values
                auto printKeyTuple = [&]() {
                    for(size_t i=0; i<arity; i++) {
                        if (aggregate.getPattern()[i] != nullptr) {
                            out << this->print(aggregate.getPattern()[i]);
                        } else {
                            out << "0";
                        }
                        if (i+1 < arity) {
                            out << ",";
                        }
                    }
                };

                // get index
                auto index = toIndex(keys);
                out << "const " << tuple_type << " key({"; printKeyTuple();  out << "});\n";
                out << "auto range = " << relName << ".equalRange" << index << "(key," << ctxName << ");\n";

            }

            // add existence check
            if(aggregate.getFunction() != RamAggregate::COUNT) {
                out << "if(!range.empty()) {\n";
            }

            // aggregate result
            out << "for(const auto& cur : range) {\n";

            // create aggregation code
            if (aggregate.getFunction() == RamAggregate::COUNT) {

                // count is easy
                out << "++res\n;";

            } else {

                // pick function
                std::string fun = "min";
                switch(aggregate.getFunction()) {
                case RamAggregate::MIN: fun = "std::min"; break;
                case RamAggregate::MAX: fun = "std::max"; break;
                case RamAggregate::COUNT: assert(false);
                }

                out << "env" << level << " = cur;\n";
                out << "res = " << fun << "(res,"; visit(*aggregate.getTargetExpression(),out); out << ");\n";
            }

            // end aggregator loop
            out << "}\n";

            // write result into environment tuple
            out << "env" << level << "[0] = res;\n";

            // continue with condition checks and nested body
            out << "{\n";

            auto condition = aggregate.getCondition();
            if(condition) {
                out << "if( " << print(condition) << ") {\n";
                visitSearch(aggregate, out);
                out  << "}\n";
                if (config.isLogging()) {
                    out << " else { ++private_num_failed_proofs; }";
                }
            } else {
                visitSearch(aggregate, out);
            }

            out << "}\n";

            // end conditional nested block
            if(aggregate.getFunction() != RamAggregate::COUNT) {
                out << "}\n";
            }
        }

        void visitProject(const RamProject& project, std::ostream& out) {
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
			out << "Tuple<RamDomain," << arity << "> tuple({"
					<< join(project.getValues(), ",", rec)
				<< "});\n";

            // check filter
            if (project.hasFilter()) {
                auto relFilter = getRelationName(project.getFilter());
                auto ctxFilter = "READ_OP_CONTEXT(" + getOpContextName(project.getFilter()) + ")";
                out << "if (!" << relFilter << ".contains(tuple," << ctxFilter << ")) {";
            }

            // insert tuple
            if (config.isLogging()) {
                out << "if (!(" << relName << ".insert(tuple," << ctxName << "))) { ++private_num_failed_proofs; }\n";
            } else {
                out << relName << ".insert(tuple," << ctxName << ");\n";
            }

            // end filter
            if (project.hasFilter()) {
                out << "}";

                // add fail counter
                if (config.isLogging()) {
                    out << " else { ++private_num_failed_proofs; }";
                }
            }

            // end condition
            if (condition) {
                out << "}\n";

                // add fail counter
                if (config.isLogging()) {
                    out << " else { ++private_num_failed_proofs; }";
                }
            }

        }


        // -- conditions --

        void visitAnd(const RamAnd& c, std::ostream& out) {
            out << "((" << print(c.getLHS()) << ") && (" << print(c.getRHS()) << "))";
        }

        void visitBinaryRelation(const RamBinaryRelation& rel, std::ostream& out) {
            switch (rel.getOperator()) {

            // comparison operators
            case BinaryRelOp::EQ:
                out << "((" << print(rel.getLHS()) << ") == (" << print(rel.getRHS()) << "))";
                break;
            case BinaryRelOp::NE:
                out << "((" << print(rel.getLHS()) << ") != (" << print(rel.getRHS()) << "))";
                break;
            case BinaryRelOp::LT:
                out << "((" << print(rel.getLHS()) << ") < (" << print(rel.getRHS()) << "))";
                break;
            case BinaryRelOp::LE:
                out << "((" << print(rel.getLHS()) << ") <= (" << print(rel.getRHS()) << "))";
                break;
            case BinaryRelOp::GT:
                out << "((" << print(rel.getLHS()) << ") > (" << print(rel.getRHS()) << "))";
                break;
            case BinaryRelOp::GE:
                out << "((" << print(rel.getLHS()) << ") >= (" << print(rel.getRHS()) << "))";
                break;

                // strings
            case BinaryRelOp::MATCH: {
                out << "regex_wrapper(symTable.resolve((size_t)";
                out << print(rel.getLHS());
                out << "),symTable.resolve((size_t)";
                out << print(rel.getRHS());
                out << "))";
                break;
            }
            case BinaryRelOp::NOT_MATCH: {
				out << "!regex_wrapper(symTable.resolve((size_t)";
				out << print(rel.getLHS());
				out << "),symTable.resolve((size_t)";
				out << print(rel.getRHS());
				out << "))";
				break;
			}
            case BinaryRelOp::CONTAINS: {
                out << "(std::string(symTable.resolve((size_t)";
                out << print(rel.getRHS());
                out << ")).find(symTable.resolve((size_t)";
                out << print(rel.getLHS());
                out << "))!=std::string::npos)";
                break;
            }
            case BinaryRelOp::NOT_CONTAINS: {
				out << "(std::string(symTable.resolve((size_t)";
				out << print(rel.getRHS());
				out << ")).find(symTable.resolve((size_t)";
				out << print(rel.getLHS());
				out << "))==std::string::npos)";
				break;
			}
//            default:
//                assert(0 && "unsupported operation");
//                break;
            }
        }

        void visitEmpty(const RamEmpty& empty, std::ostream& out) {
            out << getRelationName(empty.getRelation()) << ".empty()";
        }

        void visitNotExists(const RamNotExists& ne, std::ostream& out) {

            // get some details
            const auto& rel = ne.getRelation();
            auto relName = getRelationName(rel);
            auto ctxName = "READ_OP_CONTEXT(" + getOpContextName(rel) + ")";
            auto arity = rel.getArity();

            // if it is total we use the contains function
            if (ne.isTotal()) {
                out << "!" << relName << ".contains(Tuple<RamDomain," << arity << ">({"
                            << join(ne.getValues(),",",rec)
                        << "})," << ctxName << ")";
                return;
            }

            // else we conduct a range query
            out << relName << ".equalRange";
            out << toIndex(ne.getKey());
            out << "(Tuple<RamDomain," << arity << ">({";
            out << join(ne.getValues(), ",", [&](std::ostream& out, RamValue* value) {
                if (!value) out << "0";
                else visit(*value, out);
            });
            out << "})," << ctxName << ").empty()";
        }

        // -- values --
        void visitNumber(const RamNumber& num, std::ostream& out) {
            out << num.getConstant();
        }

        void visitElementAccess(const RamElementAccess& access, std::ostream& out) {
            out << "env" << access.getLevel() << "[" << access.getElement() << "]";
        }

        void visitAutoIncrement(const RamAutoIncrement& inc, std::ostream& out) {
            out << "(ctr++)";
        }

        void visitBinaryOperator(const RamBinaryOperator& op, std::ostream& out) {
            switch (op.getOperator()) {

            // arithmetic
            case BinaryOp::ADD:
                out << "(" << print(op.getLHS()) << ") + (" << print(op.getRHS()) << ")";
                break;
            case BinaryOp::SUB:
                out << "(" << print(op.getLHS()) << ") - (" << print(op.getRHS()) << ")";
                break;
            case BinaryOp::MUL:
                out << "(" << print(op.getLHS()) << ") * (" << print(op.getRHS()) << ")";
                break;
            case BinaryOp::DIV:
                out << "(" << print(op.getLHS()) << ") / (" << print(op.getRHS()) << ")";
                break;
            case BinaryOp::EXP: {
                out << "(RamDomain)(std::pow((long)" << print(op.getLHS()) << "," << "(long)" << print(op.getRHS()) << "))";
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
                assert(0 && "unsupported operation");

            }
        }

        // -- records --

        void visitPack(const RamPack& pack, std::ostream& out) {
            out << "pack("
                    << "ram::Tuple<RamDomain," << pack.getValues().size() << ">({"
                        << join(pack.getValues(),",",rec)
                    << "})"
                << ")";
        }

        void visitOrd(const RamOrd& op, std::ostream& out) {
            out << print(op.getSymbol());
        }

        void visitNegation(const RamNegation& op, std::ostream& out) {
            out << "(-(" << print(op.getValue()) << "))";
        }

        void visitComplement(const RamComplement& op, std::ostream& out) {
            out << "(~(" << print(op.getValue()) << "))";
        }

        void visitNot(const RamNot& op, std::ostream& out) {
            out << "(!(" << print(op.getValue()) << "))";
        }

        // -- safety net --

        void visitNode(const RamNode& node, std::ostream&) {
            std::cout << "Unsupported node Type: " << typeid(node).name() << "\n";
            assert(false && "Unsupported Node Type!");
        }

    private:

        printer print(const RamNode& node) {
            return printer(*this, node);
        }

        printer print(const RamNode* node) {
            return print(*node);
        }

        std::string getRelationName(const RamRelationIdentifier& rel) const {
            return "rel_" + CPPIdentifierMap::getIdentifier(rel.getName());
        }

        std::string getOpContextName(const RamRelationIdentifier& rel) const {
            return getRelationName(rel) + "_op_ctxt";
        }
    };


    void genCode(std::ostream& out, const RamStatement& stmt, const RamExecutorConfig& config, const IndexMap& indices) {
        // use printer
        Printer(config, indices).visit(stmt,out);
    }


}


CPPIdentifierMap* CPPIdentifierMap::instance = 0;

CPPIdentifierMap& CPPIdentifierMap::getInstance() {
    if (instance == NULL) {
        instance = new CPPIdentifierMap();
    }
    return *instance;
}


std::string CPPIdentifierMap::getIdentifier(std::string rel_name) {
    return getInstance().identifier(rel_name);
}

std::string CPPIdentifierMap::identifier(std::string rel_name) {
    if (name_id_map.count(rel_name)) {
        return name_id_map.find(rel_name)->second;
    }
    
    std::string unique_id = uniqueIdentifier(rel_name);
    name_id_map.insert(std::make_pair(rel_name, unique_id));
    return unique_id;
}

std::string CPPIdentifierMap::uniqueIdentifier(std::string name) {

    // Remove digits, invalid characters.
    std::string new_id;
    bool start_digits = true;
    for (size_t i = 0; i < name.size(); ++i) {
        if (start_digits && !isdigit(name[i])) {
            start_digits = false;
        }
        if (!start_digits && isValidChar(name[i])) {
            new_id.push_back(name[i]);
        }
    }

    // Truncate the string if too long.
    if (new_id.length() > prefix_len) new_id = new_id.substr(0, prefix_len);
    else if (new_id.length() == 0) new_id = "identifier";
    
    // Make the identifier unique.
    if (used_ids.count(new_id)) {
        size_t n = 0;
        for (; used_ids.count(new_id + std::to_string(n)); ++n);
        new_id = new_id + std::to_string(n);
    }

    used_ids.insert(new_id);
    return new_id;
}

bool CPPIdentifierMap::isValidChar(char c) {
    return isalnum(c) || c == '_';
}

std::string RamCompiler::resolveFileName() const {
	if (getBinaryFile() == "") {
		// generate temporary file
		char templ[40] = "./fileXXXXXX";
		close(mkstemp(templ));
		return templ;
	}
	return getBinaryFile();
}


std::string RamCompiler::generateCode(const SymbolTable& symTable, const RamStatement& stmt, const std::string& filename) const {

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
    if (report) *report << "------ Auto-Index-Generation Report -------\n";
    for(auto& cur : indices) {
        cur.second.solve();
        if (report) {
            *report << "Relation " << cur.first.getName() << "\n";
            *report << "\tNumber of Scan Patterns: " << cur.second.getSearches().size() << "\n";
            for(auto& cols : cur.second.getSearches()) {
                *report << "\t\t";
                for(uint32_t i=0;i<cur.first.getArity();i++) {
                    if ((1UL<<i) & cols) {
                       *report << cur.first.getArg(i) << " ";
                    }
                }
                *report << "\n";
            }
            *report << "\tNumber of Indexes: " << cur.second.getAllOrders().size() << "\n";
            for(auto& order : cur.second.getAllOrders()) {
                *report << "\t\t";
                for(auto& i : order) {
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


    // open output file
    std::string fname = filename;
    if(fname == "") {
    	fname = resolveFileName();
    }

    // generate class name
    std::string classname = fname;
    if (endsWith(classname,".h")) {
    	classname = classname.substr(0,classname.size() - 2);
    } else if(endsWith(classname,".cpp")) {
    	classname = classname.substr(0,classname.size() - 4);
    }
    char *bname = strdup(classname.c_str());
    std::string simplename = basename(bname);
    free(bname);
    for(size_t i=0;i<simplename.length();i++) {
        if((!isalpha(simplename[i]) && i == 0) || !isalnum(simplename[i]))  {
            simplename[i]='_';
        }
    }
    classname = "Sf_" + simplename;

    // add filename extension
    std::string source = fname;
    if (!(endsWith(fname,".h") || endsWith(fname,".cpp"))) {
    	source += ".cpp";
    }

    // open output stream for header file
    std::ofstream os(source);

    // generate C++ program
    os << "#include \"souffle/CompiledSouffle.h\"\n";
    os << "\n";
    os << "namespace souffle {\n";
    os << "using namespace ram;\n";

    // print wrapper for regex
    os << "class " << classname << " : public SouffleProgram {\n";
    os << "private:\n";
    os << "static bool regex_wrapper(const char *pattern, const char *text) {\n";
    os << "   bool result = false; \n";
    os << "   try { result = std::regex_match(text, std::regex(pattern)); } catch(...) { \n";
    os << "     std::cerr << \"warning: wrong pattern provided for match(\\\"\" << pattern << \"\\\",\\\"\" << text << \"\\\")\\n\";\n}\n";
    os << "   return result;\n";
    os << "}\n";

    if (getConfig().isLogging()) {
        os << "std::string profiling_fname;\n";
    }

    // declare symbol table
    os << "public:\n";
    os << "SymbolTable symTable;\n";

    // print relation definitions
    std::string initCons; // initialization of constructor
    std::string registerRel; // registration of relations
    int relCtr=0;
    visitDepthFirst(stmt, [&](const RamCreate& create) {
        // get some table details
        const auto& rel = create.getRelation();
        auto type = getRelationType(rel.getArity(), indices[rel]);
        int arity = rel.getArity();
        const std::string &raw_name = rel.getName();
        const std::string &name = CPPIdentifierMap::getIdentifier(raw_name);

        // defining table
        os << "// -- Table: " << raw_name << "\n";
        os << type << " rel_" << name << ";\n";
        bool isTemp = (name.find("_temp1_")==0) || (name.find("_temp2_")==0);
        if ((rel.isInput() || rel.isComputed() || getConfig().isDebug()) && !isTemp) {
           os << "souffle::RelationWrapper<";
           os << relCtr++ << ",";
           os << type << ",";
           os << "Tuple<RamDomain," << arity << ">,";
           os << arity << ",";
           os << (rel.isInput()?"true":"false") << ",";
           os << (rel.isComputed()?"true":"false");
           os << "> wrapper_" << name << ";\n";

           // construct types
           std::string tupleType = "std::array<const char *," + std::to_string(arity) + ">{";
           std::string tupleName = "std::array<const char *," + std::to_string(arity) + ">{";
           
           if (rel.getArity()) {
               tupleType += "\"" + rel.getArgTypeQualifier(0) + "\"";
               for(int i=1; i<arity; i++) {
                   tupleType += ",\"" + rel.getArgTypeQualifier(i) + "\"";
               }

               tupleName += "\"" + rel.getArg(0) + "\"";
               for (int i=1; i<arity; i++) {
                   tupleName += ",\"" + rel.getArg(i) + "\"";
               }
           }
           tupleType += "}";
           tupleName += "}";

           if (initCons.size() > 0) {
               initCons += ",\n";
           }
           initCons += "wrapper_" + name + "(rel_" + name + ",symTable,\"" + raw_name + "\"," + tupleType + "," + tupleName + ")";
           registerRel += "addRelation(\"" + raw_name + "\",&wrapper_" + name + "," + std::to_string(rel.isInput()) + "," + std::to_string(rel.isOutput()) + ");\n";
        }
    });

    os << "public:\n";

    // -- constructor --

    os << classname;
    if (getConfig().isLogging()) {
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
        for(size_t i=0;i<symTable.size();i++) {
            os << "\tR\"(" << symTable.resolve(i) << ")\",\n";
        }
        os << "};\n";
        os << "symTable.insert(symbols," << symTable.size() <<  ");\n";
        os << "\n";
    }

    os << "}\n";


    // -- run function --

    os << "void run() {\n";

    // initialize counter
    os << "// -- initialize counter --\n";
    os << "std::atomic<RamDomain> ctr(0);\n\n";

    // set default threads (in embedded mode)
    if (getConfig().getNumThreads() > 0) {
        os << "#if defined(__EMBEDDED_SOUFFLE__) && defined(_OPENMP)\n";
        os << "omp_set_num_threads(" << getConfig().getNumThreads() << ");\n";
        os << "#endif\n\n";
    }

    // add actual program body
    os << "// -- query evaluation --\n";
    if (getConfig().isLogging()) {
        os << "std::ofstream profile(profiling_fname);\n";
        os << "profile << \"@start-debug\\n\";\n";
        genCode(os, stmt, getConfig(), indices);
    } else {
        genCode(os, stmt, getConfig(), indices);
    }
    os << "}\n"; // end of run() method

    // issue printAll method
    os << "public:\n";
    os << "void printAll(std::string dirname=\"" << getConfig().getOutputDir() << "\") {\n";
    bool toConsole = (getConfig().getOutputDir() == "-");
    visitDepthFirst(stmt, [&](const RamStatement& node) {
        if (auto store = dynamic_cast<const RamStore*>(&node)) {
            auto name = store->getRelation().getName();
            auto relName = "rel_" + CPPIdentifierMap::getIdentifier(name);

            // pick target
            std::string fname = "dirname + \"/" + store->getFileName() + "\"";
            auto target = (toConsole) ? "nullptr" : fname;

            if (toConsole) {
                os << "std::cout << \"---------------\\n" << name << "\\n===============\\n\";\n";
            }

            // create call
            os << relName << ".printCSV(" << target;
            os << ",symTable";

            // add format parameters
            const SymbolMask& mask = store->getRelation().getSymbolMask();
            for(size_t i=0; i<store->getRelation().getArity(); i++) {
                os << ((mask.isSymbol(i)) ? ",1" : ",0");
            }

            os << ");\n";

            if (toConsole) os << "std::cout << \"===============\\n\";\n";
        } else if (auto print = dynamic_cast<const RamPrintSize*>(&node)) {
            auto relName = "rel_" + CPPIdentifierMap::getIdentifier(print->getRelation().getName());
            os << "{ auto lease = getOutputLock().acquire(); \n";
            os << "std::cout << R\"(" << print->getLabel() << ")\" <<  " << relName << ".size() << \"\\n\";\n";
            os << "}";
        }
    });
    os << "}\n";  // end of printAll() method

    // issue loadAll method
    os << "public:\n";
    os << "void loadAll(std::string dirname=\"" << getConfig().getOutputDir() << "\") {\n";
    visitDepthFirst(stmt, [&](const RamLoad& load) {
        // get some table details
        os << "rel_" <<  CPPIdentifierMap::getIdentifier(load.getRelation().getName());
        os << ".loadCSV(dirname + \"/";
        os << load.getFileName() << "\"";
        os << ",symTable";
        for(size_t i=0;i<load.getRelation().getArity();i++) {
            os << (load.getRelation().getSymbolMask().isSymbol(i) ? ",1" : ",0");
        }
        os << ");\n";
    });
    os << "}\n";  // end of loadAll() method

    // issue dump methods
	auto dumpRelation = [&](const std::string& name, const SymbolMask& mask, size_t arity) {
		auto relName = "rel_" + CPPIdentifierMap::getIdentifier(name);

		os << "out << \"---------------\\n" << name << "\\n===============\\n\";\n";

		// create call
		os << relName << ".printCSV(out,symTable";

		// add format parameters
		for(size_t i=0; i<arity; i++) {
			os << ((mask.isSymbol(i)) ? ",1" : ",0");
		}

		os << ");\n";

		os << "out << \"===============\\n\";\n";
	};

    // dump inputs
	os << "public:\n";
	os << "void dumpInputs(std::ostream& out = std::cout) {\n";
	visitDepthFirst(stmt, [&](const RamLoad& load) {
		auto& name = load.getRelation().getName();
		auto& mask = load.getRelation().getSymbolMask();
		size_t arity = load.getRelation().getArity();
		dumpRelation(name,mask,arity);
	});
	os << "}\n";  // end of dumpInputs() method

	// dump outputs
	os << "public:\n";
	os << "void dumpOutputs(std::ostream& out = std::cout) {\n";
	visitDepthFirst(stmt, [&](const RamStore& store) {
		auto& name = store.getRelation().getName();
		auto& mask = store.getRelation().getSymbolMask();
		size_t arity = store.getRelation().getArity();
		dumpRelation(name,mask,arity);
	});
	os << "}\n";  // end of dumpOutputs() method

    // issue dumpDB() method
    os << "public:\n";
    os << "void dumpDB(std::string filename, bool outputRelationsOnly) {\n";
    os << "writeRelationsToSqlite(filename, this, outputRelationsOnly);\n";
    os << "}\n"; // end of dumpDB() method

    os << "public:\n";
    os << "const SymbolTable &getSymbolTable() const {\n";
    os << "return symTable;\n";
    os << "}\n"; // end of getSymbolTable() method

    os << "};\n"; // end of class declaration

    // hidden hooks
    os << "SouffleProgram *newInstance_" << simplename << "(){return new " << classname << ";}\n";
    os << "SymbolTable *getST_" << simplename << "(SouffleProgram *p){return &reinterpret_cast<"
        << classname << "*>(p)->symTable;}\n";

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

    // parse arguments
    os << "souffle::CmdOptions opt(" ;
    os << "R\"(" << getConfig().getSourceFileName() << ")\",\n";
    os << "R\"(" << getConfig().getFactFileDir() << ")\",\n";
    os << "R\"(" << getConfig().getOutputDir() << ")\",\n";
    if (getConfig().isLogging()) {
       os << "true,\n";
       os << "R\"(" << getConfig().getProfileName() << ")\",\n";
    } else {
       os << "false,\n";
       os << "R\"()\",\n";
    }
    os << "R\"(" << getConfig().getOutputDatabaseName() << ")\",\n";
    os << getConfig().getNumThreads() << ",\n";
    os << ((getConfig().isDebug())?"R\"(true)\"":"R\"(false)\"");
    os << ");\n";

    os << "if (!opt.parse(argc,argv)) return 1;\n";

    os << "#if defined(_OPENMP) \n";
    os << "omp_set_nested(true);\n";
    os << "#endif\n";

    os << "souffle::";
    if (getConfig().isLogging()) {
       os << classname + " obj(opt.getProfileName());\n";
    } else {
       os << classname + " obj;\n";
    }

    os << "obj.loadAll(opt.getInputFileDir());\n";
    os << "obj.run();\n";
    os << "if (!opt.getOutputFileDir().empty()) obj.printAll(opt.getOutputFileDir());\n";
    os << "if (!opt.getOutputDatabaseName().empty()) obj.dumpDB(opt.getOutputDatabaseName(), " << !getConfig().isDebug() << ");\n";

    os << "return 0;\n";
    os << "}\n";
    os << "#endif\n";

    // close source file
    os.close();

    // return the filename
    return source;

}

std::string RamCompiler::compileToLibrary(const SymbolTable& symTable, const RamStatement& stmt, const std::string& name) const {

    std::string source = generateCode(symTable, stmt, name + ".cpp");

    // execute shell script that compiles the generated C++ program
    std::string cmd = "./compilelib.sh " + name;

    // separate souffle output form executable output
    if (getConfig().isLogging()) {
        std::cout.flush();
    }

    // run executable
    if(system(cmd.c_str()) != 0) {
        std::cerr << "failed to compile C++ source " << name << "\n";
    }

    // done
    return name;
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

    // execute shell script that compiles the generated C++ program
    std::string cmd = getConfig().getCompileScript();

    // set up number of threads
    auto num_threads = getConfig().getNumThreads();
    if (num_threads == 1) {
        cmd+="-s ";
    }

    // add source code
    cmd += source;

    // separate souffle output form executable output
    if (getConfig().isLogging()) {
        std::cout.flush();
    }

    // run executable
    if(system(cmd.c_str()) != 0) {
        std::cerr << "failed to compile C++ source " << binary << "\n";
    }

    // done
    return binary;
}

void RamCompiler::applyOn(const RamStatement& stmt, RamEnvironment& env, RamData* data) const {

    // compile statement
    std::string binary = compileToBinary(env.getSymbolTable(), stmt);

    // separate souffle output form executable output
    if (getConfig().isLogging()) {
        std::cout.flush();
    }

    // check whether the executable exists
    if(!isExecutable(binary)) {
       std::cerr << "failed to run executable " << binary << "\n";
    }

    // run executable
    int result = system(binary.c_str());
    if (result !=0) { 
       exit(result);
    } 
}

} // end of namespace souffle

