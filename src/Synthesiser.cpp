/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Synthesiser.cpp
 *
 * Implementation of the C++ synthesiser for RAM programs.
 *
 ***********************************************************************/

#include "Synthesiser.h"
#include "AstRelation.h"
#include "AstVisitor.h"
#include "AutoIndex.h"
#include "BinaryConstraintOps.h"
#include "BinaryFunctorOps.h"
#include "Global.h"
#include "IOSystem.h"
#include "Logger.h"
#include "Macro.h"
#include "RamRelation.h"
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

/**
 * A singleton which provides a mapping from strings to unique valid CPP identifiers.
 */
class CPPIdentifierMap {
public:
    /**
     * Obtains the singleton instance.
     */
    static CPPIdentifierMap& getInstance() {
        if (instance == nullptr) {
            instance = new CPPIdentifierMap();
        }
        return *instance;
    }

    /**
     * Given a string, returns its corresponding unique valid identifier;
     */
    static std::string getIdentifier(const std::string& name) {
        return getInstance().identifier(name);
    }

    ~CPPIdentifierMap() = default;

private:
    CPPIdentifierMap() {}

    static CPPIdentifierMap* instance;

    /**
     * Instance method for getIdentifier above.
     */
    const std::string identifier(const std::string& name) {
        auto it = identifiers.find(name);
        if (it != identifiers.end()) {
            return it->second;
        }
        // strip leading numbers
        unsigned int i;
        for (i = 0; i < name.length(); ++i) {
            if (isalnum(name.at(i)) || name.at(i) == '_') {
                break;
            }
        }
        std::string id;
        for (auto ch : std::to_string(identifiers.size() + 1) + '_' + name.substr(i)) {
            // alphanumeric characters are allowed
            if (isalnum(ch)) {
                id += ch;
            }
            // all other characters are replaced by an underscore, except when
            // the previous character was an underscore as double underscores
            // in identifiers are reserved by the standard
            else if (id.size() == 0 || id.back() != '_') {
                id += '_';
            }
        }
        // most compilers have a limit of 2048 characters (if they have a limit at all) for
        // identifiers; we use half of that for safety
        id = id.substr(0, 1024);
        identifiers.insert(std::make_pair(name, id));
        return id;
    }

    // The map of identifiers.
    std::map<const std::string, const std::string> identifiers;
};

// See the CPPIdentifierMap, (it is a singleton class).
CPPIdentifierMap* CPPIdentifierMap::instance = nullptr;

namespace {

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

// Static wrapper to get relation names without going directly though the CPPIdentifierMap.
static const std::string getRelationName(const RamRelation& rel) {
    return "rel_" + CPPIdentifierMap::getIdentifier(rel.getName());
}

// Static wrapper to get op context names without going directly though the CPPIdentifierMap.
static const std::string getOpContextName(const RamRelation& rel) {
    return getRelationName(rel) + "_op_ctxt";
}

class IndexMap {
    typedef std::map<RamRelation, AutoIndex> data_t;
    typedef typename data_t::iterator iterator;

    std::map<RamRelation, AutoIndex> data;

public:
    AutoIndex& operator[](const RamRelation& rel) {
        return data[rel];
    }

    const AutoIndex& operator[](const RamRelation& rel) const {
        const static AutoIndex empty;
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

std::string getRelationType(const RamRelation& rel, std::size_t arity, const AutoIndex& indices) {
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

std::set<RamRelation> getReferencedRelations(const RamOperation& op) {
    std::set<RamRelation> res;
    visitDepthFirst(op, [&](const RamNode& node) {
        if (auto scan = dynamic_cast<const RamScan*>(&node)) {
            res.insert(scan->getRelation());
        } else if (auto agg = dynamic_cast<const RamAggregate*>(&node)) {
            res.insert(agg->getRelation());
        } else if (auto notExist = dynamic_cast<const RamNotExists*>(&node)) {
            res.insert(notExist->getRelation());
        } else if (auto project = dynamic_cast<const RamProject*>(&node)) {
            res.insert(project->getRelation());
            if (project->hasFilter()) {
                res.insert(project->getFilter());
            }
        }
    });
    return res;
}

class Printer : public RamVisitor<void, std::ostream&> {
// macros to add comments to generated code for debugging
#ifndef PRINT_BEGIN_COMMENT
#define PRINT_BEGIN_COMMENT(os)                                                  \
    if (Global::config().has("debug-report") || Global::config().has("verbose")) \
    os << "/* BEGIN " << __FUNCTION__ << " @" << __FILE__ << ":" << __LINE__ << " */\n"
#endif

#ifndef PRINT_END_COMMENT
#define PRINT_END_COMMENT(os)                                                    \
    if (Global::config().has("debug-report") || Global::config().has("verbose")) \
    os << "/* END " << __FUNCTION__ << " @" << __FILE__ << ":" << __LINE__ << " */\n"
#endif

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

    void visitCreate(const RamCreate& /*create*/, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        PRINT_END_COMMENT(out);
    }

    void visitFact(const RamFact& fact, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        out << getRelationName(fact.getRelation()) << "->"
            << "insert(" << join(fact.getValues(), ",", rec) << ");\n";
        PRINT_END_COMMENT(out);
    }

    void visitLoad(const RamLoad& load, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        out << "if (performIO) {\n";
        // get some table details
        out << "try {";
        out << "std::map<std::string, std::string> directiveMap(";
        out << load.getRelation().getInputDirectives() << ");\n";
        out << "if (!inputDirectory.empty() && directiveMap[\"IO\"] == \"file\" && ";
        out << "directiveMap[\"filename\"].front() != '/') {";
        out << "directiveMap[\"filename\"] = inputDirectory + \"/\" + directiveMap[\"filename\"];";
        out << "}\n";
        out << "IODirectives ioDirectives(directiveMap);\n";
        out << "IOSystem::getInstance().getReader(";
        out << "SymbolMask({" << load.getRelation().getSymbolMask() << "})";
        out << ", symTable, ioDirectives";
        out << ", " << Global::config().has("provenance");
        out << ")->readAll(*" << getRelationName(load.getRelation());
        out << ");\n";
        out << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
        out << "}\n";
        PRINT_END_COMMENT(out);
    }

    void visitStore(const RamStore& store, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        out << "if (performIO) {\n";
        for (IODirectives ioDirectives : store.getRelation().getOutputDirectives()) {
            out << "try {";
            out << "std::map<std::string, std::string> directiveMap(" << ioDirectives << ");\n";
            out << "if (!outputDirectory.empty() && directiveMap[\"IO\"] == \"file\" && ";
            out << "directiveMap[\"filename\"].front() != '/') {";
            out << "directiveMap[\"filename\"] = outputDirectory + \"/\" + directiveMap[\"filename\"];";
            out << "}\n";
            out << "IODirectives ioDirectives(directiveMap);\n";
            out << "IOSystem::getInstance().getWriter(";
            out << "SymbolMask({" << store.getRelation().getSymbolMask() << "})";
            out << ", symTable, ioDirectives";
            out << ", " << Global::config().has("provenance");
            out << ")->writeAll(*" << getRelationName(store.getRelation()) << ");\n";
            out << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
        }
        out << "}\n";
        PRINT_END_COMMENT(out);
    }

    void visitInsert(const RamInsert& insert, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        // enclose operation with a check for an empty relation
        std::set<RamRelation> input_relations;
        visitDepthFirst(insert, [&](const RamScan& scan) { input_relations.insert(scan.getRelation()); });
        if (!input_relations.empty()) {
            out << "if (" << join(input_relations, "&&", [&](std::ostream& out, const RamRelation& rel) {
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
        for (const RamRelation& rel : getReferencedRelations(insert.getOperation())) {
            // TODO (#467): this causes bugs for subprogram compilation for record types if artificial
            // dependencies are introduces in the precedence graph
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
            const RamRelation* rel = nullptr;
            visitDepthFirst(insert, [&](const RamProject& project) { rel = &project.getRelation(); });

            // build log message
            auto& clause = insert.getOrigin();
            std::string clauseText = toString(clause);
            replace(clauseText.begin(), clauseText.end(), '"', '\'');
            replace(clauseText.begin(), clauseText.end(), '\n', ' ');

            std::ostringstream line;
            line << "p-proof-counter;" << rel->getName() << ";" << clause.getSrcLoc() << ";" << clauseText
                 << ";";
            std::string label = line.str();

            // print log entry
            out << "{ auto lease = getOutputLock().acquire(); ";
            out << "(void)lease;\n";
            out << "profile << R\"(#" << label << ";)\" << num_failed_proofs << std::endl;\n";
            out << "}";
        }

        out << "}\n";  // end lambda
        // out << "();";  // call lambda
        PRINT_END_COMMENT(out);
    }

    void visitMerge(const RamMerge& merge, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        if (merge.getTargetRelation().isEqRel()) {
            out << getRelationName(merge.getSourceRelation()) << "->"
                << "extend("
                << "*" << getRelationName(merge.getTargetRelation()) << ");\n";
        }
        out << getRelationName(merge.getTargetRelation()) << "->"
            << "insertAll("
            << "*" << getRelationName(merge.getSourceRelation()) << ");\n";
        PRINT_END_COMMENT(out);
    }

    void visitClear(const RamClear& clear, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        out << getRelationName(clear.getRelation()) << "->"
            << "purge();\n";
        PRINT_END_COMMENT(out);
    }

    void visitDrop(const RamDrop& drop, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        out << "if (!isHintsProfilingEnabled() && (performIO || " << drop.getRelation().isTemp() << ")) ";
        out << getRelationName(drop.getRelation()) << "->"
            << "purge();\n";
        PRINT_END_COMMENT(out);
    }

    void visitPrintSize(const RamPrintSize& print, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        out << "if (performIO) {\n";
        out << "{ auto lease = getOutputLock().acquire(); \n";
        out << "(void)lease;\n";
        out << "std::cout << R\"(" << print.getMessage() << ")\" <<  ";
        out << getRelationName(print.getRelation()) << "->"
            << "size() << std::endl;\n";
        out << "}";
        out << "}\n";
        PRINT_END_COMMENT(out);
    }

    void visitLogSize(const RamLogSize& print, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        out << "{ auto lease = getOutputLock().acquire(); \n";
        out << "(void)lease;\n";
        out << "profile << R\"(" << print.getMessage() << ")\" <<  ";
        out << getRelationName(print.getRelation());
        out << "->"
            << "size() << std::endl;\n"
            << "}";
        PRINT_END_COMMENT(out);
    }

    // -- control flow statements --

    void visitSequence(const RamSequence& seq, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        for (const auto& cur : seq.getStatements()) {
            out << print(cur);
        }
        PRINT_END_COMMENT(out);
    }

    void visitParallel(const RamParallel& parallel, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        auto stmts = parallel.getStatements();

        // special handling cases
        if (stmts.empty()) {
            return;
            PRINT_END_COMMENT(out);
        }

        // a single statement => save the overhead
        if (stmts.size() == 1) {
            out << print(stmts[0]);
            return;
            PRINT_END_COMMENT(out);
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
        PRINT_END_COMMENT(out);
    }

    void visitLoop(const RamLoop& loop, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        out << "for(;;) {\n" << print(loop.getBody()) << "}\n";
        PRINT_END_COMMENT(out);
    }

    void visitSwap(const RamSwap& swap, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        const std::string tempKnowledge = "rel_0";
        const std::string& deltaKnowledge = getRelationName(swap.getFirstRelation());
        const std::string& newKnowledge = getRelationName(swap.getSecondRelation());

        // perform a triangular swap of pointers
        out << "{\nauto " << tempKnowledge << " = " << deltaKnowledge << ";\n"
            << deltaKnowledge << " = " << newKnowledge << ";\n"
            << newKnowledge << " = " << tempKnowledge << ";\n"
            << "}\n";
        PRINT_END_COMMENT(out);
    }

    void visitExit(const RamExit& exit, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        out << "if(" << print(exit.getCondition()) << ") break;\n";
        PRINT_END_COMMENT(out);
    }

    void visitLogTimer(const RamLogTimer& timer, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        // create local scope for name resolution
        out << "{\n";

        // create local timer
        out << "\tLogger logger(R\"(" << timer.getMessage() << ")\",profile);\n";

        // insert statement to be measured
        visit(timer.getStatement(), out);

        // done
        out << "}\n";
        PRINT_END_COMMENT(out);
    }

    void visitDebugInfo(const RamDebugInfo& dbg, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        out << "SignalHandler::instance()->setMsg(R\"_(";
        out << dbg.getMessage();
        out << ")_\");\n";

        // insert statements of the rule
        visit(dbg.getStatement(), out);
        PRINT_END_COMMENT(out);
    }

    // -- operations --

    void visitSearch(const RamSearch& search, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        auto condition = search.getCondition();
        if (condition) {
            out << "if( " << print(condition) << ") {\n" << print(search.getNestedOperation()) << "}\n";
            if (Global::config().has("profile")) {
                out << " else { ++private_num_failed_proofs; }";
            }
        } else {
            out << print(search.getNestedOperation());
        }
        PRINT_END_COMMENT(out);
    }

    void visitScan(const RamScan& scan, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
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
            PRINT_END_COMMENT(out);
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
        PRINT_END_COMMENT(out);
    }

    void visitLookup(const RamLookup& lookup, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
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
        PRINT_END_COMMENT(out);
    }

    void visitAggregate(const RamAggregate& aggregate, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        // get some properties
        const auto& rel = aggregate.getRelation();
        auto arity = rel.getArity();
        auto relName = getRelationName(rel);
        auto ctxName = "READ_OP_CONTEXT(" + getOpContextName(rel) + ")";
        auto level = aggregate.getLevel();

        // get the tuple type working with
        std::string tuple_type = "ram::Tuple<RamDomain," + toString(std::max(1u, arity)) + ">";

        // declare environment variable
        out << tuple_type << " env" << level << ";\n";

        // special case: counting of number elements in a full relation
        if (aggregate.getFunction() == RamAggregate::COUNT && aggregate.getRangeQueryColumns() == 0) {
            // shortcut: use relation size
            out << "env" << level << "[0] = " << relName << "->"
                << "size();\n";
            visitSearch(aggregate, out);
            PRINT_END_COMMENT(out);
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
        PRINT_END_COMMENT(out);
    }

    void visitProject(const RamProject& project, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
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
        PRINT_END_COMMENT(out);
    }

    // -- conditions --

    void visitAnd(const RamAnd& c, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        out << "((" << print(c.getLHS()) << ") && (" << print(c.getRHS()) << "))";
        PRINT_END_COMMENT(out);
    }

    void visitBinaryRelation(const RamBinaryRelation& rel, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
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
        PRINT_END_COMMENT(out);
    }

    void visitEmpty(const RamEmpty& empty, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        out << getRelationName(empty.getRelation()) << "->"
            << "empty()";
        PRINT_END_COMMENT(out);
    }

    void visitNotExists(const RamNotExists& ne, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
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
            PRINT_END_COMMENT(out);
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
        PRINT_END_COMMENT(out);
    }

    // -- values --
    void visitNumber(const RamNumber& num, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        out << num.getConstant();
        PRINT_END_COMMENT(out);
    }

    void visitElementAccess(const RamElementAccess& access, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        out << "env" << access.getLevel() << "[" << access.getElement() << "]";
        PRINT_END_COMMENT(out);
    }

    void visitAutoIncrement(const RamAutoIncrement& /*inc*/, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        out << "(ctr++)";
        PRINT_END_COMMENT(out);
    }

    void visitUnaryOperator(const RamUnaryOperator& op, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
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
        PRINT_END_COMMENT(out);
    }

    void visitBinaryOperator(const RamBinaryOperator& op, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
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
            case BinaryOp::MAX: {
                out << "(AstDomain)(std::max((AstDomain)" << print(op.getLHS()) << ","
                    << "(AstDomain)" << print(op.getRHS()) << "))";
                break;
            }
            case BinaryOp::MIN: {
                out << "(AstDomain)(std::min((AstDomain)" << print(op.getLHS()) << ","
                    << "(AstDomain)" << print(op.getRHS()) << "))";
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
        PRINT_END_COMMENT(out);
    }

    void visitTernaryOperator(const RamTernaryOperator& op, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
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
        PRINT_END_COMMENT(out);
    }

    // -- records --

    void visitPack(const RamPack& pack, std::ostream& out) override {
        PRINT_BEGIN_COMMENT(out);
        out << "pack("
            << "ram::Tuple<RamDomain," << pack.getValues().size() << ">({" << join(pack.getValues(), ",", rec)
            << "})"
            << ")";
        PRINT_END_COMMENT(out);
    }

    // -- subroutine argument --

    void visitArgument(const RamArgument& arg, std::ostream& out) override {
        out << "(args)[" << arg.getArgNumber() << "]";
    }

    // -- subroutine return --

    void visitReturn(const RamReturn& ret, std::ostream& out) override {
        for (auto val : ret.getValues()) {
            if (val == nullptr) {
                out << "ret.push_back(0);\n";
                out << "err.push_back(true);\n";
            } else {
                out << "ret.push_back(" << print(val) << ");\n";
                out << "err.push_back(false);\n";
            }
        }
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

std::string Synthesiser::generateCode(const SymbolTable& symTable, const RamProgram& prog,
        const std::string& filename, const int index) const {
    // ---------------------------------------------------------------
    //                      Auto-Index Generation
    // ---------------------------------------------------------------

    // collect all used indices
    IndexMap indices;
    visitDepthFirst(prog, [&](const RamNode& node) {
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

    const std::string nameNoExtension = simpleName((filename != "") ? filename : tempFile());
    const std::string indexStr = ((index != -1) ? "_" + std::to_string(index) : "");
    const std::string id = identifier(baseName(nameNoExtension));

    std::string classname = "Sf_" + id + indexStr;

    // add filename extension
    std::string sourceFilename = nameNoExtension + indexStr + ".cpp";

    // open output stream for header file
    std::ofstream os(sourceFilename);

    // generate C++ program
    os << "#include \"souffle/CompiledSouffle.h\"\n";
    if (Global::config().has("provenance") || Global::config().has("record-provenance")) {
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
    os << "     std::cerr << str << \"\\\",\" << (int32_t)idx << \",\" << (int32_t)len << \") "
          "functor.\\n\";\n";
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
    visitDepthFirst(*(prog.getMain()), [&](const RamCreate& create) {

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
        if ((rel.isInput() || rel.isComputed() || Global::config().has("provenance")) && !rel.isTemp()) {
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
            os << ",\n" << initCons;
        }
    } else {
        os << "()";
        if (initCons.size() > 0) {
            os << " : " << initCons;
        }
    }
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

    os << "private:\ntemplate <bool performIO> void runFunction(std::string inputDirectory = \".\", "
          "std::string outputDirectory = \".\") {\n";

    os << "SignalHandler::instance()->set();\n";

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
        genCode(os, *(prog.getMain()), indices);
    } else {
        genCode(os, *(prog.getMain()), indices);
    }
    // add code printing hint statistics
    os << "\n// -- relation hint statistics --\n";
    os << "if(isHintsProfilingEnabled()) {\n";
    os << "std::cout << \" -- Operation Hint Statistics --\\n\";\n";
    visitDepthFirst(*(prog.getMain()), [&](const RamCreate& create) {
        auto name = getRelationName(create.getRelation());
        os << "std::cout << \"Relation " << name << ":\\n\";\n";
        os << name << "->printHintStatistics(std::cout,\"  \");\n";
        os << "std::cout << \"\\n\";\n";
    });
    os << "}\n";

    os << "SignalHandler::instance()->reset();\n";

    os << "}\n";  // end of runFunction() method

    // add methods to run with and without performing IO (mainly for the interface)
    os << "public:\nvoid run() { runFunction<false>(); }\n";
    os << "public:\nvoid runAll(std::string inputDirectory = \".\", std::string outputDirectory = \".\") { "
          "runFunction<true>(inputDirectory, outputDirectory); }\n";

    // issue printAll method
    os << "public:\n";
    os << "void printAll(std::string outputDirectory = \".\") {\n";
    visitDepthFirst(*(prog.getMain()), [&](const RamStatement& node) {
        if (auto store = dynamic_cast<const RamStore*>(&node)) {
            for (IODirectives ioDirectives : store->getRelation().getOutputDirectives()) {
                os << "try {";
                os << "std::map<std::string, std::string> directiveMap(" << ioDirectives << ");\n";
                os << "if (!outputDirectory.empty() && directiveMap[\"IO\"] == \"file\" && ";
                os << "directiveMap[\"filename\"].front() != '/') {";
                os << "directiveMap[\"filename\"] = outputDirectory + \"/\" + directiveMap[\"filename\"];";
                os << "}\n";
                os << "IODirectives ioDirectives(directiveMap);\n";
                os << "IOSystem::getInstance().getWriter(";
                os << "SymbolMask({" << store->getRelation().getSymbolMask() << "})";
                os << ", symTable, ioDirectives, " << Global::config().has("provenance");
                os << ")->writeAll(*" << getRelationName(store->getRelation()) << ");\n";

                os << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
            }
        } else if (auto print = dynamic_cast<const RamPrintSize*>(&node)) {
            os << "{ auto lease = getOutputLock().acquire(); \n";
            os << "(void)lease;\n";
            os << "std::cout << R\"(" << print->getMessage() << ")\" <<  ";
            os << getRelationName(print->getRelation()) << "->"
               << "size() << std::endl;\n";
            os << "}";
        }
    });
    os << "}\n";  // end of printAll() method

    // issue loadAll method
    os << "public:\n";
    os << "void loadAll(std::string inputDirectory = \".\") {\n";
    visitDepthFirst(*(prog.getMain()), [&](const RamLoad& load) {
        IODirectives ioDirectives = load.getRelation().getInputDirectives();
        // get some table details
        os << "try {";
        os << "std::map<std::string, std::string> directiveMap(";
        os << load.getRelation().getInputDirectives() << ");\n";
        os << "if (!inputDirectory.empty() && directiveMap[\"IO\"] == \"file\" && ";
        os << "directiveMap[\"filename\"].front() != '/') {";
        os << "directiveMap[\"filename\"] = inputDirectory + \"/\" + directiveMap[\"filename\"];";
        os << "}\n";
        os << "IODirectives ioDirectives(directiveMap);\n";
        os << "IOSystem::getInstance().getReader(";
        os << "SymbolMask({" << load.getRelation().getSymbolMask() << "})";
        os << ", symTable, ioDirectives";
        os << ", " << Global::config().has("provenance");
        os << ")->readAll(*" << getRelationName(load.getRelation());
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
        os << ", symTable, ioDirectives, " << Global::config().has("provenance");
        os << ")->writeAll(*" << relName << ");\n";
        os << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
    };

    // dump inputs
    os << "public:\n";
    os << "void dumpInputs(std::ostream& out = std::cout) {\n";
    visitDepthFirst(*(prog.getMain()), [&](const RamLoad& load) {
        auto& name = getRelationName(load.getRelation());
        auto& mask = load.getRelation().getSymbolMask();
        size_t arity = load.getRelation().getArity();
        dumpRelation(name, mask, arity);
    });
    os << "}\n";  // end of dumpInputs() method

    // dump outputs
    os << "public:\n";
    os << "void dumpOutputs(std::ostream& out = std::cout) {\n";
    visitDepthFirst(*(prog.getMain()), [&](const RamStore& store) {
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

    // TODO: generate code for subroutines
    if (Global::config().has("provenance")) {
        // generate subroutine adapter
        os << "void executeSubroutine(std::string name, const std::vector<RamDomain>& args, "
              "std::vector<RamDomain>& ret, std::vector<bool>& err) override {\n";

        // subroutine number
        size_t subroutineNum = 0;
        for (auto& sub : prog.getSubroutines()) {
            os << "if (name == \"" << sub.first << "\") {\n"
               << "subproof_" << subroutineNum
               << "(args, ret, err);\n"  // subproof_i to deal with special characters in relation names
               << "}\n";
            subroutineNum++;
        }
        os << "}\n";  // end of executeSubroutine

        // generate method for each subroutine
        subroutineNum = 0;
        for (auto& sub : prog.getSubroutines()) {
            // method header
            os << "void "
               << "subproof_" << subroutineNum << "(const std::vector<RamDomain>& args, "
                                                  "std::vector<RamDomain>& ret, std::vector<bool>& err) {\n";

            // generate code for body
            genCode(os, *sub.second, indices);

            os << "return;\n";
            os << "}\n";  // end of subroutine
            subroutineNum++;
        }
    }

    os << "};\n";  // end of class declaration

    // hidden hooks
    os << "SouffleProgram *newInstance_" << id << "(){return new " << classname << ";}\n";
    os << "SymbolTable *getST_" << id << "(SouffleProgram *p){return &reinterpret_cast<" << classname
       << "*>(p)->symTable;}\n";

    os << "#ifdef __EMBEDDED_SOUFFLE__\n";
    os << "class factory_" << classname << ": public souffle::ProgramFactory {\n";
    os << "SouffleProgram *newInstance() {\n";
    os << "return new " << classname << "();\n";
    os << "};\n";
    os << "public:\n";
    os << "factory_" << classname << "() : ProgramFactory(\"" << id << "\"){}\n";
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

    os << "obj.runAll(opt.getInputFileDir(), opt.getOutputFileDir());\n";
    if (Global::config().get("provenance") == "1") {
        os << "explain(obj, true, false);\n";
    } else if (Global::config().get("provenance") == "2") {
        os << "explain(obj, true, true);\n";
    }

    if (Global::config().get("record-provenance") == "1") {
        os << "explain(obj, false, false);\n";
    } else if (Global::config().get("record-provenance") == "2") {
        os << "explain(obj, false, true);\n";
    }

    os << "return 0;\n";
    os << "} catch(std::exception &e) { souffle::SignalHandler::instance()->error(e.what());}\n";
    os << "}\n";
    os << "#endif\n";

    // close source file
    os.close();

    // return the filename
    return sourceFilename;
}

std::string Synthesiser::compileToBinary(const SymbolTable& symTable, const RamProgram& prog,
        const std::string& filename, const int index) const {
    // ---------------------------------------------------------------
    //                       Code Generation
    // ---------------------------------------------------------------

    std::string sourceFilename = generateCode(symTable, prog, filename, index);

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
    cmd += sourceFilename;

    // separate souffle output form executable output
    if (Global::config().has("profile")) {
        std::cout.flush();
    }

    // run executable
    if (system(cmd.c_str()) != 0) {
        throw std::invalid_argument("failed to compile C++ source <" + sourceFilename + ">");
    }

    // done
    return sourceFilename;
}

std::string Synthesiser::executeBinary(const SymbolTable& symTable, const RamProgram& prog,
        const std::string& filename, const int index) const {
    // compile statement
    std::string sourceFilename = compileToBinary(symTable, prog, filename, index);
    std::string binaryFilename = "./" + simpleName(sourceFilename);

    // separate souffle output form executable output
    if (Global::config().has("profile")) {
        std::cout.flush();
    }

    // check whether the executable exists
    if (!isExecutable(binaryFilename)) {
        throw std::invalid_argument("Generated executable <" + binaryFilename + "> could not be found");
    }

    // run executable
    int result = system(binaryFilename.c_str());
    if (Global::config().get("dl-program").empty()) {
        remove(binaryFilename.c_str());
        remove((binaryFilename + ".cpp").c_str());
    }
    if (result != 0) {
        exit(result);
    }
    return sourceFilename;
}

}  // end of namespace souffle
