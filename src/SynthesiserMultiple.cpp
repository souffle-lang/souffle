/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SynthesiserMultiple.cpp
 *
 * Implementation of the C++ synthesiser for RAM programs.
 *
 ***********************************************************************/

#include "SynthesiserMultiple.h"
#include "AstLogStatement.h"
#include "AstRelation.h"
#include "AstVisitor.h"
#include "BinaryConstraintOps.h"
#include "BinaryFunctorOps.h"
#include "Global.h"
#include "IOSystem.h"
#include "IndexSetAnalysis.h"
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
        std::cout << "WARNING: indexes are ignored!\n";
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

std::string getRelationType(const RamRelation& rel, std::size_t arity, const IndexSet& indexes) {
    std::stringstream res;
    res << "ram::Relation";
    res << "<";

    if (rel.isBTree()) {
        res << "BTree,";
    } else if (rel.isRbtset()) {
        res << "Rbtset,";
    } else if (rel.isHashset()) {
        res << "Hashset,";
    } else if (rel.isBrie()) {
        res << "Brie,";
    } else if (rel.isEqRel()) {
        res << "EqRel,";
    } else if (rel.isForall()) {
	res << "Forall,";
    } else {
        auto data_structure = Global::config().get("data-structure");
        if (data_structure == "btree") {
            res << "BTree,";
        } else if (data_structure == "rbtset") {
            res << "Rbtset,";
        } else if (data_structure == "hashset") {
            res << "Hashset,";
        } else if (data_structure == "brie") {
            res << "Brie,";
        } else if (data_structure == "eqrel") {
            res << "Eqrel,";
        } else {
            res << "Auto,";
        }
    }

    res << arity;
    if (!useNoIndex()) {
        for (auto& cur : indexes.getAllOrders()) {
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

class CodeEmitter : public RamVisitor<int, int> {
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

    std::function<void(std::ostream&, const RamNode*)> rec;

    int indentLevel = 0;
    int fileSize = 0;
    int fileCounter = 0;
public:
    std::string baseFilename;
    std::string mainFilename;
    std::string classname;
    std::ofstream* curr_os;
    static const int maxFileSize = 10;

    struct printer {
        CodeEmitter& p;
        const RamNode& node;
	std::ofstream* internal_curr_os;

        printer(CodeEmitter& p, const RamNode& n, std::ofstream* curr_os)
	    : p(p), node(n), internal_curr_os(curr_os) {}

        printer(const printer& other) = default;

        friend std::ostream& operator<<(std::ostream& out, const printer& p) {
            p.p.visit(p.node, 0);
            return *(p.internal_curr_os);
        }
    };

    CodeEmitter(std::string classname, std::string mainFile, std::string baseFile)
	: baseFilename(baseFile), mainFilename(mainFile), classname(classname) {
        rec = [&](std::ostream& out, const RamNode* node) { this->visit(*node, 0); };
	// updateFileStream();
	SplitFunction(true);
    }

    void SplitFunction(bool init = false) {
	std::ofstream* stream = curr_os;
	std::string newFunction = "runFunction" + std::to_string(fileCounter);
	if (!init) {
	    *stream << "this->" << newFunction << "<performIO>(inputDirectory, outputDirectory);\n";
	    *stream << "\n}\n";
	    *stream << "template void " << classname << "::runFunction" <<
		fileCounter - 1<< "<true>(std::string inputDirectory, std::string outputDirectory);\n";
	    *stream << "template void " << classname << "::runFunction" <<
		fileCounter - 1<< "<false>(std::string inputDirectory, std::string outputDirectory);\n";
	    *stream << "\n}\n";
	}
	stream = updateFileStream(init);

	*stream << "#include \"souffle/CompiledSouffle.h\"\n\n";
	*stream << "#include \"" << mainFilename << "\"\n\n";
	if (Global::config().has("provenance")) {
	    *stream << "#include \"souffle/Explain.h\"\n";
	    *stream << "#include \"<ncurses.h>\"\n";
	}
	*stream << "namespace souffle {\n";
	*stream << "using namespace ram;\n";
	*stream << "template <bool performIO> void " << classname << "::"
		<< newFunction << "(std::string inputDirectory = \".\", "
		<< "std::string outputDirectory = \".\") {\n";
	fileSize = 0;
	indentLevel = 0;
    }

    std::string nextFileName() {
	return baseFilename + "_run" + std::to_string(fileCounter++) + ".cpp";
    }

    std::ofstream* updateFileStream(bool init) {
	if (!init) {
	    curr_os->flush();
	    curr_os->close();
	    delete curr_os;
	}
        std::string filename = nextFileName();
	curr_os = new std::ofstream(filename);
	return curr_os;
    }

    void cleanUp() {
	*curr_os << "}\n";
	*curr_os << "template void " << classname << "::runFunction" <<
	    fileCounter - 1 << "<true>(std::string inputDirectory, std::string outputDirectory);\n";
	*curr_os << "template void " << classname << "::runFunction" <<
	    fileCounter - 1 << "<false>(std::string inputDirectory, std::string outputDirectory);\n";
	*curr_os << "}\n";
	curr_os->flush();
	curr_os->close();
	delete curr_os;
    }

public:
    // -- relation statements --

    int visitCreate(const RamCreate& /*create*/, int) override {
	std::ostream& out = *(curr_os);
        PRINT_BEGIN_COMMENT(out);
        PRINT_END_COMMENT(out);
	return fileCounter;
    }

    int visitFact(const RamFact& fact, int) override {
	std::ostream& out = *(curr_os);
        PRINT_BEGIN_COMMENT(out);
        out << getRelationName(fact.getRelation()) << "->"
            << "insert(" << join(fact.getValues(), ",", rec) << ");\n";
        PRINT_END_COMMENT(out);
        return fileCounter;
    }

    int visitLoad(const RamLoad& load, int) override {
	std::ostream& out = *(curr_os);
        PRINT_BEGIN_COMMENT(out);
        out << "if (performIO) {\n";
        // get some table details
        out << "try {";
        out << "std::map<std::string, std::string> directiveMap(";
        out << load.getIODirectives() << ");\n";
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

	// Code splitting
	++fileSize;
	if (fileSize > maxFileSize && indentLevel == 0) SplitFunction();
	//
        return fileCounter;
    }

    int visitStore(const RamStore& store, int) override {
	std::ostream& out = *(curr_os);
        PRINT_BEGIN_COMMENT(out);
        out << "if (performIO) {\n";
        for (IODirectives ioDirectives : store.getIODirectives()) {
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

	// Code splitting
	++fileSize;
	if (fileSize > maxFileSize && indentLevel == 0) SplitFunction();
	//
        return fileCounter;
    }

    int visitInsert(const RamInsert& insert, int) override {
	std::ostream& out = *(curr_os);
 	// Code splitting
	++indentLevel;
	//

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
            // TODO: this should be moved to AstTranslator, as all other such logging is done there

            // get target relation
            const RamRelation* rel = nullptr;
            visitDepthFirst(insert, [&](const RamProject& project) { rel = &project.getRelation(); });

            // build log message
            auto& clause = insert.getOrigin();
            std::string clauseText = toString(clause);
            replace(clauseText.begin(), clauseText.end(), '"', '\'');
            replace(clauseText.begin(), clauseText.end(), '\n', ' ');

            // print log entry
            out << "{ auto lease = getOutputLock().acquire(); ";
            out << "(void)lease;\n";
            out << "profile << R\"(";
            out << AstLogStatement::pProofCounter(rel->getName(), clause.getSrcLoc(), clauseText);
            out << ")\" << num_failed_proofs << ";
            if (fileExtension(Global::config().get("profile")) == "json") {
                out << "\"},\" << ";
            }
            out << "std::endl;\n";
            out << "}";
        }

        out << "}\n";  // end lambda
        // out << "();";  // call lambda
        PRINT_END_COMMENT(out);

	// Code splitting
	++fileSize;
	--indentLevel;
	if (fileSize > maxFileSize && indentLevel == 0) SplitFunction();
	//
        return fileCounter;
    }

    int visitMerge(const RamMerge& merge, int) override {
	std::ostream& out = *(curr_os);
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
        return fileCounter;
    }

    int visitClear(const RamClear& clear, int) override {
	std::ostream& out = *(curr_os);
        PRINT_BEGIN_COMMENT(out);
        out << getRelationName(clear.getRelation()) << "->"
            << "purge();\n";
        PRINT_END_COMMENT(out);
        return fileCounter;
    }

    int visitDrop(const RamDrop& drop, int) override {
	std::ostream& out = *(curr_os);
        PRINT_BEGIN_COMMENT(out);
        out << "if (!isHintsProfilingEnabled() && (performIO || " << drop.getRelation().isTemp() << ")) ";
        out << getRelationName(drop.getRelation()) << "->"
            << "purge();\n";
        PRINT_END_COMMENT(out);
        return fileCounter;
    }

    int visitPrintSize(const RamPrintSize& print, int) override {
	std::ostream& out = *(curr_os);
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
        return fileCounter;
    }

    int visitLogSize(const RamLogSize& print, int) override {
	std::ostream& out = *(curr_os);
        PRINT_BEGIN_COMMENT(out);
        const std::string ext = fileExtension(Global::config().get("profile"));
        out << "{ auto lease = getOutputLock().acquire(); \n";
        out << "(void)lease;\n";
        out << "profile << R\"(" << print.getMessage() << ")\" << ";
        out << getRelationName(print.getRelation()) << "->size() << ";
        if (ext == "json") {
            out << "\"},\" << ";
        }
        out << "std::endl;\n";
        out << "}";
        PRINT_END_COMMENT(out);
        return fileCounter;
    }

    // -- control flow statements --

    int visitSequence(const RamSequence& seq, int) override {
	std::ostream& out = *(curr_os);
        PRINT_BEGIN_COMMENT(out);
        for (const auto& cur : seq.getStatements()) {
            out << print(cur);
        }
        PRINT_END_COMMENT(out);
        return fileCounter;
    }

    int visitParallel(const RamParallel& parallel, int) override {
	std::ostream& out = *(curr_os);
        PRINT_BEGIN_COMMENT(out);
        auto stmts = parallel.getStatements();

        // special handling cases
        if (stmts.empty()) {
            return fileCounter;
            PRINT_END_COMMENT(out);
        }

        // a single statement => save the overhead
        if (stmts.size() == 1) {
            out << print(stmts[0]);
            return fileCounter;
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
        return fileCounter;
    }

    int visitLoop(const RamLoop& loop, int) override {
	std::ostream& out = *(curr_os);
	// Code splitting
	++indentLevel;

        PRINT_BEGIN_COMMENT(out);
        out << "for(;;) {\n" << print(loop.getBody()) << "}\n";
        PRINT_END_COMMENT(out);

	// Code splitting
	--indentLevel;
        return fileCounter;
    }

    int visitSwap(const RamSwap& swap, int) override {
	std::ostream& out = *(curr_os);
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
        return fileCounter;
    }

    int visitExit(const RamExit& exit, int) override {
	std::ostream& out = *(curr_os);
        PRINT_BEGIN_COMMENT(out);
        out << "if(" << print(exit.getCondition()) << ") break;\n";
        PRINT_END_COMMENT(out);
        return fileCounter;
    }

    int visitLogTimer(const RamLogTimer& timer, int) override {
	std::ostream& out = *(curr_os);
        PRINT_BEGIN_COMMENT(out);
        // create local scope for name resolution
        out << "{\n";

        const std::string ext = fileExtension(Global::config().get("profile"));

        // create local timer
        out << "\tLogger logger(R\"(" << timer.getMessage() << ")\",profile, \"" << ext << "\");\n";

        // insert statement to be measured
        visit(timer.getStatement(), 0);

        // done
        out << "}\n";
        PRINT_END_COMMENT(out);
        return fileCounter;
    }

    int visitDebugInfo(const RamDebugInfo& dbg, int) override {
	std::ostream& out = *(curr_os);
        PRINT_BEGIN_COMMENT(out);
        out << "SignalHandler::instance()->setMsg(R\"_(";
        out << dbg.getMessage();
        out << ")_\");\n";

        // insert statements of the rule
        visit(dbg.getStatement(), 0);
        PRINT_END_COMMENT(out);
        return fileCounter;
    }

    // -- operations --

    int visitSearch(const RamSearch& search, int) override {
	std::ostream& out = *(curr_os);
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
        return fileCounter;
    }

    int visitScan(const RamScan& scan, int) override {
	std::ostream& out = *(curr_os);
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
                visitSearch(scan, 0);
                out << "}\n";
            } else if (scan.getLevel() == 0) {
                // make this loop parallel
                out << "pfor(auto it = part.begin(); it<part.end(); ++it) \n";
                out << "try{";
                out << "for(const auto& env0 : *it) {\n";
                visitSearch(scan, 0);
                out << "}\n";
                out << "} catch(std::exception &e) { SignalHandler::instance()->error(e.what());}\n";
            } else {
                out << "for(const auto& env" << level << " : "
                    << "*" << relName << ") {\n";
                visitSearch(scan, 0);
                out << "}\n";
            }
            return fileCounter;
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
        visitSearch(scan, 0);
        out << "}\n";
        PRINT_END_COMMENT(out);
        return fileCounter;
    }

    int visitLookup(const RamLookup& lookup, int) override {
	std::ostream& out = *(curr_os);
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
        visitSearch(lookup, 0);

        out << "}\n";
        PRINT_END_COMMENT(out);
        return fileCounter;
    }

    int visitAggregate(const RamAggregate& aggregate, int) override {
	std::ostream& out = *(curr_os);
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
            visitSearch(aggregate, 0);
            PRINT_END_COMMENT(out);
            return fileCounter;
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
            visit(*aggregate.getTargetExpression(), 0);
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
            visit(*aggregate.getTargetExpression(), 0);
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
            visitSearch(aggregate, 0);
            out << "}\n";
            if (Global::config().has("profile")) {
                out << " else { ++private_num_failed_proofs; }";
            }
        } else {
            visitSearch(aggregate, 0);
        }

        out << "}\n";

        // end conditional nested block
        if (aggregate.getFunction() != RamAggregate::COUNT) {
            out << "}\n";
        }
        PRINT_END_COMMENT(out);
        return fileCounter;
    }

    int visitProject(const RamProject& project, int) override {
	std::ostream& out = *(curr_os);
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
        return fileCounter;
    }

    // -- conditions --

    int visitAnd(const RamAnd& c, int) override {
	std::ostream& out = *(curr_os);
        PRINT_BEGIN_COMMENT(out);
        out << "((" << print(c.getLHS()) << ") && (" << print(c.getRHS()) << "))";
        PRINT_END_COMMENT(out);
        return fileCounter;
    }

    int visitBinaryRelation(const RamBinaryRelation& rel, int) override {
	std::ostream& out = *(curr_os);
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
        return fileCounter;
    }

    int visitEmpty(const RamEmpty& empty, int) override {
	std::ostream& out = *(curr_os);
        PRINT_BEGIN_COMMENT(out);
        out << getRelationName(empty.getRelation()) << "->"
            << "empty()";
        PRINT_END_COMMENT(out);
        return fileCounter;
    }

    int visitNotExists(const RamNotExists& ne, int) override {
	std::ostream& out = *(curr_os);
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
            return fileCounter;
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
                visit(*value, 0);
            }
        });
        out << "})," << ctxName << ").empty()";
        PRINT_END_COMMENT(out);
        return fileCounter;
    }

    // -- values --
    int visitNumber(const RamNumber& num, int) override {
	std::ostream& out = *(curr_os);
        PRINT_BEGIN_COMMENT(out);
        out << "RamDomain(" << num.getConstant() << ")";
        PRINT_END_COMMENT(out);
        return fileCounter;
    }

    int visitElementAccess(const RamElementAccess& access, int) override {
	std::ostream& out = *(curr_os);
        PRINT_BEGIN_COMMENT(out);
        out << "env" << access.getLevel() << "[" << access.getElement() << "]";
        PRINT_END_COMMENT(out);
        return fileCounter;
    }

    int visitAutoIncrement(const RamAutoIncrement& /*inc*/, int) override {
	std::ostream& out = *(curr_os);
        PRINT_BEGIN_COMMENT(out);
        out << "(ctr++)";
        PRINT_END_COMMENT(out);
        return fileCounter;
    }

    int visitUnaryOperator(const RamUnaryOperator& op, int) override {
	std::ostream& out = *(curr_os);
        PRINT_BEGIN_COMMENT(out);
        switch (op.getOperator()) {
            case UnaryOp::STOI:
                out << "std::stoi(symTable.resolve((size_t)" << print(op.getValue()) << "))";
                break;
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
        return fileCounter;
    }

    int visitBinaryOperator(const RamBinaryOperator& op, int) override {
	std::ostream& out = *(curr_os);
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
        return fileCounter;
    }

    int visitTernaryOperator(const RamTernaryOperator& op, int) override {
	std::ostream& out = *(curr_os);
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
        return fileCounter;
    }

    // -- records --

    int visitPack(const RamPack& pack, int) override {
	std::ostream& out = *(curr_os);
        PRINT_BEGIN_COMMENT(out);
        out << "pack("
            << "ram::Tuple<RamDomain," << pack.getValues().size() << ">({" << join(pack.getValues(), ",", rec)
            << "})"
            << ")";
        PRINT_END_COMMENT(out);
        return fileCounter;
    }

    // -- subroutine argument --

    int visitArgument(const RamArgument& arg, int) override {
	std::ostream& out = *(curr_os);
        out << "(args)[" << arg.getArgNumber() << "]";
        return fileCounter;
    }

    // -- subroutine return --

    int visitReturn(const RamReturn& ret, int) override {
	std::ostream& out = *(curr_os);
        for (auto val : ret.getValues()) {
            if (val == nullptr) {
                out << "ret.push_back(0);\n";
                out << "err.push_back(true);\n";
            } else {
                out << "ret.push_back(" << print(val) << ");\n";
                out << "err.push_back(false);\n";
            }
        }
        return fileCounter;
    }

    // -- safety net --

    int visitNode(const RamNode& node, int /*out*/) override {
        std::cerr << "Unsupported node type: " << typeid(node).name() << "\n";
        assert(false && "Unsupported Node Type!");
        return fileCounter;
    }

private:
    printer print(const RamNode& node) {
        return printer(*this, node, curr_os);
    }

    printer print(const RamNode* node) {
        return print(*node);
    }
};

    int genCode(std::string classname, std::string mainFilename,
		std::string baseName, const RamStatement& stmt) {
    // use printer
	CodeEmitter ce(classname, mainFilename, baseName);
	int res = ce.visit(stmt, 0);
	ce.cleanUp();
	return res;
    }
}  // namespace

std::vector<std::string>* SynthesiserMultiple::generateCode(
    const RamTranslationUnit& unit, const std::string& id, const std::string& baseFilename) const {
    std::string cppFilename = baseFilename + ".cpp";
    std::string hppFilename = baseFilename + ".hpp";
    std::ofstream cppos(cppFilename);
    std::ofstream hppos(hppFilename);

    const SymbolTable& symTable = unit.getSymbolTable();
    const RamProgram& prog = unit.getP();

    // compute the set of all record arities
    std::set<int> recArities;
    visitDepthFirst(prog, [&](const RamNode& node) {
        if (const RamPack* pack = dynamic_cast<const RamPack*>(&node)) {
            recArities.insert(pack->getValues().size());
        }
    });

    // ---------------------------------------------------------------
    //                      Auto-Index Generation
    // ---------------------------------------------------------------
    IndexSetAnalysis* idxAnalysis = unit.getAnalysis<IndexSetAnalysis>();

    // ---------------------------------------------------------------
    //                      Code Generation
    // ---------------------------------------------------------------

    std::string classname = "Sf_" + id;

    // generate C++ program
    hppos << "#include \"souffle/CompiledSouffle.h\"\n";
    hppos << "#include <string>\n";
    if (Global::config().has("provenance")) {
        hppos << "#include \"souffle/Explain.h\"\n";
        hppos << "#include <ncurses.h>\n";
    }
    hppos << "\n";
    hppos << "namespace souffle {\n";
    hppos << "using namespace ram;\n";
    cppos << "#include \"" << hppFilename << "\"\n";
    cppos << "namespace souffle {\n";
    cppos << "using namespace ram;\n";

    // print wrapper for regex
    hppos << "class " << classname << " : public SouffleProgram {\n";
    hppos << "private:\n";
    hppos << "static inline bool regex_wrapper(const char *pattern, const char *text) {\n";
    hppos << "   bool result = false; \n";
    hppos << "   try { result = std::regex_match(text, std::regex(pattern)); } catch(...) { \n";
    hppos << "     std::cerr << \"warning: wrong pattern provided for match(\\\"\" << pattern << \"\\\",\\\"\" "
          "<< text << \"\\\")\\n\";\n}\n";
    hppos << "   return result;\n";
    hppos << "}\n";
    hppos << "static inline std::string substr_wrapper(const char *str, size_t idx, size_t len) {\n";
    hppos << "   std::string sub_str, result; \n";
    hppos << "   try { result = std::string(str).substr(idx,len); } catch(...) { \n";
    hppos << "     std::cerr << \"warning: wrong index position provided by substr(\\\"\";\n";
    hppos << "     std::cerr << str << \"\\\",\" << (int32_t)idx << \",\" << (int32_t)len << \") "
          "functor.\\n\";\n";
    hppos << "   } return result;\n";
    hppos << "}\n";

    if (Global::config().has("profile")) {
        hppos << "std::string profiling_fname;\n";
    }

    // declare symbol table
    hppos << "public:\n";
    hppos << "SymbolTable symTable;\n";

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
                           ? getRelationType(rel, rel.getArity(), idxAnalysis->getIndexes(rel))
                           : tempType;
        const std::string& type = (rel.isTemp()) ? tempType : getRelationType(rel, rel.getArity(),
                                                                      idxAnalysis->getIndexes(rel));

        // defining table
        hppos << "// -- Table: " << raw_name << "\n";
        hppos << type << "* " << name << ";\n";
        if (initCons.size() > 0) {
            initCons += ",\n";
        }
        initCons += name + "(new " + type + "())";
        deleteForNew += "delete " + name + ";\n";
        if ((rel.isInput() || rel.isComputed() || Global::config().has("provenance")) && !rel.isTemp()) {
            hppos << "souffle::RelationWrapper<";
            hppos << relCtr++ << ",";
            hppos << type << ",";
            hppos << "Tuple<RamDomain," << arity << ">,";
            hppos << arity << ",";
            hppos << (rel.isInput() ? "true" : "false") << ",";
            hppos << (rel.isComputed() ? "true" : "false");
            hppos << "> wrapper_" << name << ";\n";

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

    hppos << "public:\n";

    // -- constructor --
    // declare the constructor in the header
    hppos << classname;
    cppos << classname << "::" << classname;
    if (Global::config().has("profile")) {
	hppos << "(std::string pf, std::string inputDirectory);\n";
        cppos << "(std::string pf=\"profile.log\", std::string inputDirectory = \".\") : profiling_fname(pf)";
        if (initCons.size() > 0) {
            cppos << ",\n" << initCons;
        }
    } else {
        hppos << "(std::string inputDirectory);\n";
        cppos << "(std::string inputDirectory = \".\")";
        if (initCons.size() > 0) {
            cppos << " : " << initCons;
        }
    }
    cppos << "{\n";
    cppos << registerRel;

    cppos << "// -- initialize symbol table --\n";
    // Read symbol table
    cppos << "std::string symtab_filepath = inputDirectory + \"/\" + \"" + Global::getSymtabFilename() + "\";\n";
    cppos << "if (fileExists(symtab_filepath)) {\n";
	cppos << "std::map<std::string, std::string> readIODirectivesMap = {\n";
	cppos << "{\"IO\", \"file\"},\n";
	    cppos << "{\"filename\", symtab_filepath},\n";
	cppos << "{\"symtabfilename\", symtab_filepath},\n";
	    cppos << "{\"name\", \"souffle_records\"}\n";
	cppos << "};\n";
	cppos << "IODirectives readIODirectives(readIODirectivesMap);\n";
	cppos << "std::unique_ptr<RecordReadStream> reader = IOSystem::getInstance()\n";
	    cppos << ".getRecordReader(symTable, readIODirectives);\n";
	cppos << "reader->readIntoSymbolTable(symTable);\n";
    cppos << "}\n";
    if (symTable.size() > 0) {
        cppos << "static const char *symbols[]={\n";
        for (size_t i = 0; i < symTable.size(); i++) {
            cppos << "\tR\"(" << symTable.resolve(i) << ")\",\n";
        }
        cppos << "};\n";
        cppos << "symTable.insert(symbols," << symTable.size() << ");\n";
        cppos << "\n";
    }

    cppos << "std::string recordsInFilepath = inputDirectory + \"/\" + \"" + Global::getRecordFilename() + "\";\n";
    cppos << "if (fileExists(recordsInFilepath)) {\n";
    cppos << "std::map<std::string, std::string> readIODirectivesMap = {\n";
    cppos << "{\"IO\", \"file\"},\n";
    cppos << "{\"filename\", recordsInFilepath},\n";
    cppos << "{\"name\", \"souffle_records\"}\n";
    cppos << "};\n";
    cppos << "IODirectives readIODirectives(readIODirectivesMap);\n";
    cppos << "try {\n";
    cppos << "std::unique_ptr<RecordReadStream> reader = IOSystem::getInstance()\n";
    cppos << ".getRecordReader(symTable, readIODirectives);\n";
    cppos << "auto records = reader->readAllRecords();\n";
    cppos << "for (auto r_it = records->begin(); r_it != records->end(); ++r_it) {\n";
    cppos << "for (RamDomain* record: r_it->second) {\n";
    cppos << "switch(r_it->first) {";
    for (int arity: recArities) {
	cppos << "case " << arity << ": \n";
	cppos << "ram::Tuple<RamDomain, " << arity << "> tuple" << arity << ";\n";
	for (int i = 0; i < arity; ++i) {
	    cppos << "tuple" << arity << "["<< i << "] = record[" << i << "];\n";
	}
	cppos << "pack<ram::Tuple<RamDomain, " << arity << ">>(tuple" << arity << ");\n";
	cppos << "break;\n";
    }
    cppos << "default: break; \n";
    cppos << "}\n";
    cppos << "}\n";
    cppos << "}\n";
    cppos << "} catch (std::exception& e) {\n";
    cppos << "std::cerr << e.what();\n";
    cppos << "}\n";
    cppos << "}\n";

    cppos << "}\n";

    // -- destructor --

    hppos << "~" << classname << "();\n";
    cppos << classname << "::~" << classname << "() {\n";
    cppos << deleteForNew;
    cppos << "}\n";

    // -- run function --

    // generate code for the actual program body
    int numRunFuns = -1;
    if (Global::config().has("profile")) {
        cppos << "std::ofstream profile(profiling_fname);\n";
        cppos << "profile << \"" << AstLogStatement::startDebug() << "\" << std::endl;\n";
        numRunFuns = genCode(classname, hppFilename, baseFilename, *(prog.getMain()));
    } else {
        numRunFuns = genCode(classname, hppFilename, baseFilename, *(prog.getMain()));
    }

    // declare all the runFunctions in the header
    for (int i = 0; i < numRunFuns; ++i) {
	hppos << "private:\ntemplate <bool performIO> void runFunction" << i << "(std::string inputDirectory, "
	    "std::string outputDirectory);\n";
    }

    hppos << "private:\ntemplate <bool performIO> void runFunction(std::string inputDirectory, "
	"std::string outputDirectory);\n";
    cppos << "template <bool performIO> void " << classname << "::runFunction(std::string inputDirectory = \".\", "
	"std::string outputDirectory = \".\") {\n";

    cppos << "SignalHandler::instance()->set();\n";

    // initialize counter
    cppos << "// -- initialize counter --\n";
    cppos << "std::atomic<RamDomain> ctr(0);\n\n";

    // set default threads (in embedded mode)
    if (std::stoi(Global::config().get("jobs")) > 0) {
        cppos << "#if defined(__EMBEDDED_SOUFFLE__) && defined(_OPENMP)\n";
        cppos << "omp_set_num_threads(" << std::stoi(Global::config().get("jobs")) << ");\n";
        cppos << "#endif\n\n";
    }

    cppos << "// -- query evaluation --\n";
    cppos << "runFunction0<performIO>(inputDirectory, outputDirectory);\n";

    // add code printing hint statistics
    cppos << "\n// -- relation hint statistics --\n";
    cppos << "if(isHintsProfilingEnabled()) {\n";
    cppos << "std::cout << \" -- Operation Hint Statistics --\\n\";\n";
    visitDepthFirst(*(prog.getMain()), [&](const RamCreate& create) {
        auto name = getRelationName(create.getRelation());
        cppos << "std::cout << \"Relation " << name << ":\\n\";\n";
        cppos << name << "->printHintStatistics(std::cout,\"  \");\n";
        cppos << "std::cout << \"\\n\";\n";
    });
    cppos << "}\n";

    cppos << "SignalHandler::instance()->reset();\n";

    cppos << "}\n";  // end of runFunction() method

    // add methods to run with and without performing IO (mainly for the interface)
    hppos << "public:\nvoid run();\n";
    cppos << "void " << classname << "::run() { runFunction<false>(); }\n";
    hppos << "void runAll(std::string inputDirectory, std::string outputDirectory);\n";
    cppos << "void " << classname << "::runAll(std::string inputDirectory = \".\", std::string outputDirectory = \".\") { "
	"runFunction<true>(inputDirectory, outputDirectory); }\n";

    // issue printAll method
    hppos << "public:\n";
    hppos << "void printAll(std::string outputDirectory);\n";
    cppos << "void " << classname << "::printAll(std::string outputDirectory = \".\") {\n";
    visitDepthFirst(*(prog.getMain()), [&](const RamStatement& node) {
        if (auto store = dynamic_cast<const RamStore*>(&node)) {
            for (IODirectives ioDirectives : store->getIODirectives()) {
                cppos << "try {";
                cppos << "std::map<std::string, std::string> directiveMap(" << ioDirectives << ");\n";
                cppos << "if (!outputDirectory.empty() && directiveMap[\"IO\"] == \"file\" && ";
                cppos << "directiveMap[\"filename\"].front() != '/') {";
                cppos << "directiveMap[\"filename\"] = outputDirectory + \"/\" + directiveMap[\"filename\"];";
                cppos << "}\n";
                cppos << "IODirectives ioDirectives(directiveMap);\n";
                cppos << "IOSystem::getInstance().getWriter(";
                cppos << "SymbolMask({" << store->getRelation().getSymbolMask() << "})";
                cppos << ", symTable, ioDirectives, " << Global::config().has("provenance");
                cppos << ")->writeAll(*" << getRelationName(store->getRelation()) << ");\n";

                cppos << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
            }
        } else if (auto print = dynamic_cast<const RamPrintSize*>(&node)) {
            cppos << "{ auto lease = getOutputLock().acquire(); \n";
            cppos << "(void)lease;\n";
            cppos << "std::cout << R\"(" << print->getMessage() << ")\" <<  ";
            cppos << getRelationName(print->getRelation()) << "->"
               << "size() << std::endl;\n";
            cppos << "}";
        }
    });
    cppos << "}\n";  // end of printAll() method

    // Print record tables and symtab
    // issue printAllRecords method
    hppos << "public:\n";
    hppos << "void printAllRecords(std::string outputDirectory);\n";
    cppos << "void " << classname << "::printAllRecords(std::string outputDirectory = \".\") {\n";
    cppos << "std::string recordsOutFilepath = outputDirectory + \"/\" + \"" + Global::getRecordFilename() + "\";\n";
    cppos << "std::string symtabOutFilepath = outputDirectory + \"/\" + \"" + Global::getSymtabFilename() + "\";\n";
    cppos << "std::map<std::string, std::string> writeIODirectivesMap = {\n";
    cppos << "{\"IO\", \"file\"},\n";
    cppos << "{\"filename\", recordsOutFilepath},\n";
    cppos << "{\"symtabfilename\", symtabOutFilepath},\n";
    cppos << "{\"name\", \"souffle_records\"}\n";
    cppos << "};\n";
    cppos << "IODirectives writeIODirectives(writeIODirectivesMap);\n";
    cppos << "try {\n";
    cppos << "auto writer = IOSystem::getInstance().getRecordWriter(symTable, writeIODirectives);\n";
    for (int arity: recArities) {
	cppos << "printRecords<ram::Tuple<RamDomain," << arity << ">>(writer);\n";
    }
    cppos << "writer->writeSymbolTable();\n";
    cppos << "} catch (std::exception& e) {\n";
    cppos << "std::cerr << e.what();\n";
    cppos << "exit(1);\n";
    cppos << "}\n";
    cppos << "}\n";  // end of printAll() method

    // issue loadAll method
    hppos << "public:\n";
    hppos << "void loadAll(std::string inputDirectory);\n";
    cppos << "void " << classname << "::loadAll(std::string inputDirectory = \".\") {\n";
    visitDepthFirst(*(prog.getMain()), [&](const RamLoad& load) {
        // get some table details
        cppos << "try {";
        cppos << "std::map<std::string, std::string> directiveMap(";
        cppos << load.getIODirectives() << ");\n";
        cppos << "if (!inputDirectory.empty() && directiveMap[\"IO\"] == \"file\" && ";
        cppos << "directiveMap[\"filename\"].front() != '/') {";
        cppos << "directiveMap[\"filename\"] = inputDirectory + \"/\" + directiveMap[\"filename\"];";
        cppos << "}\n";
        cppos << "IODirectives ioDirectives(directiveMap);\n";
        cppos << "IOSystem::getInstance().getReader(";
        cppos << "SymbolMask({" << load.getRelation().getSymbolMask() << "})";
        cppos << ", symTable, ioDirectives";
        cppos << ", " << Global::config().has("provenance");
        cppos << ")->readAll(*" << getRelationName(load.getRelation());
        cppos << ");\n";
        cppos << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
    });
    cppos << "}\n";  // end of loadAll() method

    // issue dump methods
    auto dumpRelation = [&](const std::string& name, const SymbolMask& mask, size_t arity) {
        auto relName = name;

        cppos << "try {";
        cppos << "IODirectives ioDirectives;\n";
        cppos << "ioDirectives.setIOType(\"stdout\");\n";
        cppos << "ioDirectives.setRelationName(\"" << name << "\");\n";
        cppos << "IOSystem::getInstance().getWriter(";
        cppos << "SymbolMask({" << mask << "})";
        cppos << ", symTable, ioDirectives, " << Global::config().has("provenance");
        cppos << ")->writeAll(*" << relName << ");\n";
        cppos << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
    };

    // dump inputs
    hppos << "public:\n";
    hppos << "void dumpInputs(std::ostream& out);\n";
    cppos << "void " << classname << "::dumpInputs(std::ostream& out = std::cout) {\n";
    visitDepthFirst(*(prog.getMain()), [&](const RamLoad& load) {
        auto& name = getRelationName(load.getRelation());
        auto& mask = load.getRelation().getSymbolMask();
        size_t arity = load.getRelation().getArity();
        dumpRelation(name, mask, arity);
    });
    cppos << "}\n";  // end of dumpInputs() method

    // dump outputs
    hppos << "public:\n";
    hppos << "void dumpOutputs(std::ostream& out);\n";
    cppos << "void " << classname << "::dumpOutputs(std::ostream& out = std::cout) {\n";
    visitDepthFirst(*(prog.getMain()), [&](const RamStore& store) {
        auto& name = getRelationName(store.getRelation());
        auto& mask = store.getRelation().getSymbolMask();
        size_t arity = store.getRelation().getArity();
        dumpRelation(name, mask, arity);
    });
    cppos << "}\n";  // end of dumpOutputs() method

    hppos << "public:\n";
    hppos << "const SymbolTable& getSymbolTable() const;\n";
    cppos << "const SymbolTable& " << classname << "::getSymbolTable() const {\n";
    cppos << "return symTable;\n";
    cppos << "}\n";  // end of getSymbolTable() method

    // TODO: generate code for subroutines
    if (Global::config().has("provenance")) {
        // generate subroutine adapter
        hppos << "void executeSubroutine(std::string name, const std::vector<RamDomain>& args, "
              "std::vector<RamDomain>& ret, std::vector<bool>& err) override;\n";
        cppos << "void " << classname << "::executeSubroutine(std::string name, const std::vector<RamDomain>& args, "
              "std::vector<RamDomain>& ret, std::vector<bool>& err) override {\n";

        // subroutine number
        size_t subroutineNum = 0;
        for (auto& sub : prog.getSubroutines()) {
            cppos << "if (name == \"" << sub.first << "\") {\n"
               << "subproof_" << subroutineNum
               << "(args, ret, err);\n"  // subproof_i to deal with special characters in relation names
               << "}\n";
            subroutineNum++;
        }
        cppos << "}\n";  // end of executeSubroutine

        // generate method for each subroutine
        subroutineNum = 0;
        for (auto& sub : prog.getSubroutines()) {
            // method header
            hppos << "void "
               << "subproof_" << subroutineNum << "(const std::vector<RamDomain>& args, "
                                                  "std::vector<RamDomain>& ret, std::vector<bool>& err);\n";
            cppos << "void " << classname
               << "::subproof_" << subroutineNum << "(const std::vector<RamDomain>& args, "
                                                  "std::vector<RamDomain>& ret, std::vector<bool>& err) {\n";

            // generate code for body
            genCode(classname, hppFilename, baseFilename, *sub.second);

            cppos << "return;\n";
            cppos << "}\n";  // end of subroutine
            subroutineNum++;
        }
    }

    hppos << "};\n";  // end of class declaration
    hppos << "}\n";   // end of namespace declaration

    // hidden hooks
    cppos << "SouffleProgram *newInstance_" << id << "(){return new " << classname << ";}\n";
    cppos << "SymbolTable *getST_" << id << "(SouffleProgram *p){return &reinterpret_cast<" << classname
       << "*>(p)->symTable;}\n";

    cppos << "#ifdef __EMBEDDED_SOUFFLE__\n";
    cppos << "class factory_" << classname << ": public souffle::ProgramFactory {\n";
    cppos << "SouffleProgram *newInstance() {\n";
    cppos << "return new " << classname << "();\n";
    cppos << "};\n";
    cppos << "public:\n";
    cppos << "factory_" << classname << "() : ProgramFactory(\"" << id << "\"){}\n";
    cppos << "};\n";
    cppos << "static factory_" << classname << " __factory_" << classname << "_instance;\n";
    // cppos << "}\n";
    cppos << "#else\n";
    cppos << "}\n";
    cppos << "int main(int argc, char** argv)\n{\n";
    cppos << "try{\n";

    // parse arguments
    cppos << "souffle::CmdOptions opt(";
    cppos << "R\"(" << Global::config().get("") << ")\",\n";
    cppos << "R\"(.)\",\n";
    cppos << "R\"(.)\",\n";
    if (Global::config().has("profile")) {
        cppos << "true,\n";
        cppos << "R\"(" << Global::config().get("profile") << ")\",\n";
    } else {
        cppos << "false,\n";
        cppos << "R\"()\",\n";
    }
    cppos << std::stoi(Global::config().get("jobs")) << "\n";
    cppos << ");\n";

    cppos << "if (!opt.parse(argc,argv)) return 1;\n";

    cppos << "#if defined(_OPENMP) \n";
    cppos << "omp_set_nested(true);\n";
    cppos << "#endif\n";

    cppos << "souffle::";
    if (Global::config().has("profile")) {
        cppos << classname + " obj(opt.getProfileName(), opt.getInputFileDir());\n";
    } else {
        cppos << classname + " obj(opt.getInputFileDir());\n";
    }

    cppos << "obj.runAll(opt.getInputFileDir(), opt.getOutputFileDir());\n";
    cppos << "obj.printAllRecords(opt.getOutputFileDir());\n";
    if (Global::config().get("provenance") == "1") {
        cppos << "explain(obj, true, false);\n";
    } else if (Global::config().get("provenance") == "2") {
        cppos << "explain(obj, true, true);\n";
    }

    cppos << "return 0;\n";
    cppos << "} catch(std::exception &e) { souffle::SignalHandler::instance()->error(e.what());}\n";
    cppos << "}\n";
    cppos << "#endif\n";
    hppos.flush();
    hppos.close();
    cppos.flush();
    cppos.close();

    std::vector<std::string>* allCppFiles = new std::vector<std::string>();
    allCppFiles->push_back(cppFilename);
    for (int i = 0; i < numRunFuns; ++i) {
	allCppFiles->push_back(baseFilename + "_run" + std::to_string(i) + ".cpp");
    }
    return allCppFiles;
}
}  // end of namespace souffle
