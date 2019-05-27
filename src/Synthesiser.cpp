/**
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
#include "BinaryConstraintOps.h"
#include "FunctorOps.h"
#include "Global.h"
#include "IODirectives.h"
#include "RamCondition.h"
#include "RamExpression.h"
#include "RamIndexAnalysis.h"
#include "RamNode.h"
#include "RamOperation.h"
#include "RamProgram.h"
#include "RamRelation.h"
#include "RamTranslationUnit.h"
#include "RamVisitor.h"
#include "RelationRepresentation.h"
#include "SymbolTable.h"
#include "SynthesiserRelation.h"
#include "Util.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <sstream>
#include <typeinfo>
#include <utility>
#include <vector>

namespace souffle {

/** Lookup frequency counter */
unsigned Synthesiser::lookupFreqIdx(const std::string& txt) {
    static unsigned ctr;
    auto pos = idxMap.find(txt);
    if (pos == idxMap.end()) {
        return idxMap[txt] = ctr++;
    } else {
        return idxMap[txt];
    }
}

/** Lookup frequency counter */
size_t Synthesiser::lookupReadIdx(const std::string& txt) {
    std::string modifiedTxt = txt;
    for (auto& cur : modifiedTxt) {
        if (cur == '-') {
            cur = '.';
        }
    }
    static unsigned counter;
    auto pos = neIdxMap.find(modifiedTxt);
    if (pos == neIdxMap.end()) {
        return neIdxMap[modifiedTxt] = counter++;
    } else {
        return neIdxMap[modifiedTxt];
    }
}

/** Convert RAM identifier */
const std::string Synthesiser::convertRamIdent(const std::string& name) {
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
        else if (id.empty() || id.back() != '_') {
            id += '_';
        }
    }
    // most compilers have a limit of 2048 characters (if they have a limit at all) for
    // identifiers; we use half of that for safety
    id = id.substr(0, 1024);
    identifiers.insert(std::make_pair(name, id));
    return id;
}

/** check whether indexes are disabled */
bool Synthesiser::areIndexesDisabled() {
    bool flag = std::getenv("SOUFFLE_USE_NO_INDEX");
    static bool first = true;
    if (first && flag) {
        std::cout << "WARNING: indexes are ignored!\n";
        first = false;
    }
    return flag;
}

/** Get relation name */
const std::string Synthesiser::getRelationName(const RamRelation& rel) {
    return "rel_" + convertRamIdent(rel.getName());
}

/** Get relation name via string */
const std::string Synthesiser::getRelationName(const std::string& relName) {
    return "rel_" + convertRamIdent(relName);
}

/** Get context name */
const std::string Synthesiser::getOpContextName(const RamRelation& rel) {
    return getRelationName(rel) + "_op_ctxt";
}

/** Get relation type struct */
void Synthesiser::generateRelationTypeStruct(
        std::ostream& out, std::unique_ptr<SynthesiserRelation> relationType) {
    // If this type has been generated already, use the cached version
    if (typeCache.find(relationType->getTypeName()) != typeCache.end()) {
        return;
    }
    typeCache.insert(relationType->getTypeName());

    // Generate the type struct for the relation
    relationType->generateTypeStruct(out);
}

/* Convert SearchColums to a template index */
std::string Synthesiser::toIndex(SearchSignature key) {
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

/** Get referenced relations */
std::set<const RamRelation*> Synthesiser::getReferencedRelations(const RamOperation& op) {
    std::set<const RamRelation*> res;
    visitDepthFirst(op, [&](const RamNode& node) {
        if (auto scan = dynamic_cast<const RamRelationSearch*>(&node)) {
            res.insert(&scan->getRelation());
        } else if (auto agg = dynamic_cast<const RamAggregate*>(&node)) {
            res.insert(&agg->getRelation());
        } else if (auto exists = dynamic_cast<const RamExistenceCheck*>(&node)) {
            res.insert(&exists->getRelation());
        } else if (auto provExists = dynamic_cast<const RamProvenanceExistenceCheck*>(&node)) {
            res.insert(&provExists->getRelation());
        } else if (auto project = dynamic_cast<const RamProject*>(&node)) {
            res.insert(&project->getRelation());
        }
    });
    return res;
}

void Synthesiser::emitCode(std::ostream& out, const RamStatement& stmt) {
    class CodeEmitter : public RamVisitor<void, std::ostream&> {
    private:
        Synthesiser& synthesiser;
        RamIndexAnalysis* isa;

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
        std::ostringstream preamble;
        bool preambleIssued = false;

    public:
        CodeEmitter(Synthesiser& syn)
                : synthesiser(syn), isa(syn.getTranslationUnit().getAnalysis<RamIndexAnalysis>()) {
            rec = [&](std::ostream& out, const RamNode* node) { this->visit(*node, out); };
        }

        // -- relation statements --

        void visitCreate(const RamCreate& /*create*/, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            PRINT_END_COMMENT(out);
        }

        void visitFact(const RamFact& fact, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << synthesiser.getRelationName(fact.getRelation()) << "->"
                << "insert(" << join(fact.getValues(), ",", rec) << ");\n";
            PRINT_END_COMMENT(out);
        }

        void visitLoad(const RamLoad& load, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "if (performIO) {\n";
            std::vector<bool> symbolMask;
            for (auto& cur : load.getRelation().getAttributeTypeQualifiers()) {
                symbolMask.push_back(cur[0] == 's');
            }
            // get some table details
            for (IODirectives ioDirectives : load.getIODirectives()) {
                out << "try {";
                out << "std::map<std::string, std::string> directiveMap(";
                out << ioDirectives << ");\n";
                out << R"_(if (!inputDirectory.empty() && directiveMap["IO"] == "file" && )_";
                out << "directiveMap[\"filename\"].front() != '/') {";
                out << R"_(directiveMap["filename"] = inputDirectory + "/" + directiveMap["filename"];)_";
                out << "}\n";
                out << "IODirectives ioDirectives(directiveMap);\n";
                out << "IOSystem::getInstance().getReader(";
                out << "std::vector<bool>({" << join(symbolMask) << "})";
                out << ", symTable, ioDirectives";
                out << ", " << (Global::config().has("provenance") ? "true" : "false");
                out << ")->readAll(*" << synthesiser.getRelationName(load.getRelation());
                out << ");\n";
                out << "} catch (std::exception& e) {std::cerr << \"Error loading data: \" << e.what() << "
                       "'\\n';}\n";
            }
            out << "}\n";
            PRINT_END_COMMENT(out);
        }

        void visitStore(const RamStore& store, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "if (performIO) {\n";
            std::vector<bool> symbolMask;
            for (auto& cur : store.getRelation().getAttributeTypeQualifiers()) {
                symbolMask.push_back(cur[0] == 's');
            }
            for (IODirectives ioDirectives : store.getIODirectives()) {
                out << "try {";
                out << "std::map<std::string, std::string> directiveMap(" << ioDirectives << ");\n";
                out << R"_(if (!outputDirectory.empty() && directiveMap["IO"] == "file" && )_";
                out << "directiveMap[\"filename\"].front() != '/') {";
                out << R"_(directiveMap["filename"] = outputDirectory + "/" + directiveMap["filename"];)_";
                out << "}\n";
                out << "IODirectives ioDirectives(directiveMap);\n";
                out << "IOSystem::getInstance().getWriter(";
                out << "std::vector<bool>({" << join(symbolMask) << "})";
                out << ", symTable, ioDirectives";
                out << ", " << (Global::config().has("provenance") ? "true" : "false");
                out << ")->writeAll(*" << synthesiser.getRelationName(store.getRelation()) << ");\n";
                out << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
            }
            out << "}\n";
            PRINT_END_COMMENT(out);
        }

        void visitQuery(const RamQuery& query, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);

            // split terms of conditions of outer filter operation
            // into terms that require a context and terms that
            // do not require a context
            const RamOperation* next = &query.getOperation();
            std::vector<std::unique_ptr<RamCondition>> requireCtx;
            std::vector<std::unique_ptr<RamCondition>> freeOfCtx;
            if (const auto* filter = dynamic_cast<const RamFilter*>(&query.getOperation())) {
                next = &filter->getOperation();
                // Check terms of outer filter operation whether they can be pushed before
                // the context-generation for speed imrovements
                auto conditions = toConjunctionList(&filter->getCondition());
                for (auto const& cur : conditions) {
                    bool needContext = false;
                    visitDepthFirst(*cur, [&](const RamExistenceCheck& exists) { needContext = true; });
                    if (needContext) {
                        requireCtx.push_back(std::unique_ptr<RamCondition>(cur->clone()));
                    } else {
                        freeOfCtx.push_back(std::unique_ptr<RamCondition>(cur->clone()));
                    }
                }
                // discharge conditions that do not require a context
                if (freeOfCtx.size() > 0) {
                    out << "if(";
                    visit(*toCondition(toConstPtrVector(freeOfCtx)), out);
                    out << ") {\n";
                }
            }

            // outline each search operation to improve compilation time
#ifdef __clang__
#if __clang_major > 3
            out << "[&]()";
#endif
#else
            out << "[&]()";
#endif
            // enclose operation in its own scope
            out << "{\n";

            // check whether loop nest can be parallelized
            bool isParallel = false;
            visitDepthFirst(*next, [&](const RamAbstractParallel& node) { isParallel = true; });

            // reset preamble
            preamble.str("");
            preamble.clear();
            preambleIssued = false;

            // create operation contexts for this operation
            for (const RamRelation* rel : synthesiser.getReferencedRelations(query.getOperation())) {
                preamble << "CREATE_OP_CONTEXT(" << synthesiser.getOpContextName(*rel);
                preamble << "," << synthesiser.getRelationName(*rel);
                preamble << "->createContext());\n";
            }

            // discharge conditions that require a context
            if (isParallel) {
                if (requireCtx.size() > 0) {
                    preamble << "if(";
                    visit(*toCondition(toConstPtrVector(requireCtx)), preamble);
                    preamble << ") {\n";
                    visit(*next, out);
                    out << "}\n";
                } else {
                    visit(*next, out);
                }
            } else {
                out << preamble.str();
                if (requireCtx.size() > 0) {
                    out << "if(";
                    visit(*toCondition(toConstPtrVector(requireCtx)), out);
                    out << ") {\n";
                    visit(*next, out);
                    out << "}\n";
                } else {
                    visit(*next, out);
                }
            }

            if (isParallel) {
                out << "PARALLEL_END;\n";  // end parallel
            }

            out << "}\n";
#ifdef __clang__
#if __clang_major > 3
            out << "();";  // call lambda
#endif
#else
            out << "();";  // call lambda
#endif
            if (freeOfCtx.size() > 0) {
                out << "}\n";
            }

            PRINT_END_COMMENT(out);
        }

        void visitMerge(const RamMerge& merge, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            if (merge.getTargetRelation().getRepresentation() == RelationRepresentation::EQREL) {
                out << synthesiser.getRelationName(merge.getSourceRelation()) << "->"
                    << "extend("
                    << "*" << synthesiser.getRelationName(merge.getTargetRelation()) << ");\n";
            }
            out << synthesiser.getRelationName(merge.getTargetRelation()) << "->"
                << "insertAll("
                << "*" << synthesiser.getRelationName(merge.getSourceRelation()) << ");\n";
            PRINT_END_COMMENT(out);
        }

        void visitClear(const RamClear& clear, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << synthesiser.getRelationName(clear.getRelation()) << "->"
                << "purge();\n";
            PRINT_END_COMMENT(out);
        }

        void visitDrop(const RamDrop& drop, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);

            out << "if (!isHintsProfilingEnabled()"
                << (drop.getRelation().isTemp() ? ") " : "&& performIO) ");
            out << synthesiser.getRelationName(drop.getRelation()) << "->"
                << "purge();\n";

            PRINT_END_COMMENT(out);
        }

        void visitLogSize(const RamLogSize& size, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "ProfileEventSingleton::instance().makeQuantityEvent( R\"(";
            out << size.getMessage() << ")\",";
            out << synthesiser.getRelationName(size.getRelation()) << "->size(),iter);";
            PRINT_END_COMMENT(out);
        }

        // -- control flow statements --

        void visitSequence(const RamSequence& seq, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            for (const auto& cur : seq.getStatements()) {
                visit(cur, out);
            }
            PRINT_END_COMMENT(out);
        }

        void visitParallel(const RamParallel& parallel, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            auto stmts = parallel.getStatements();

            // special handling cases
            if (stmts.empty()) {
                PRINT_END_COMMENT(out);
                return;
            }

            // a single statement => save the overhead
            if (stmts.size() == 1) {
                visit(stmts[0], out);
                PRINT_END_COMMENT(out);
                return;
            }

            // more than one => parallel sections

            // start parallel section
            out << "SECTIONS_START;\n";

            // put each thread in another section
            for (const auto& cur : stmts) {
                out << "SECTION_START;\n";
                visit(cur, out);
                out << "SECTION_END\n";
            }

            // done
            out << "SECTIONS_END;\n";
            PRINT_END_COMMENT(out);
        }

        void visitLoop(const RamLoop& loop, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "iter = 0;\n";
            out << "for(;;) {\n";
            visit(loop.getBody(), out);
            out << "iter++;\n";
            out << "}\n";
            out << "iter = 0;\n";
            PRINT_END_COMMENT(out);
        }

        void visitSwap(const RamSwap& swap, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            const std::string& deltaKnowledge = synthesiser.getRelationName(swap.getFirstRelation());
            const std::string& newKnowledge = synthesiser.getRelationName(swap.getSecondRelation());

            out << "std::swap(" << deltaKnowledge << ", " << newKnowledge << ");\n";
            PRINT_END_COMMENT(out);
        }

        void visitExit(const RamExit& exit, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "if(";
            visit(exit.getCondition(), out);
            out << ") break;\n";
            PRINT_END_COMMENT(out);
        }

        void visitLogRelationTimer(const RamLogRelationTimer& timer, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            // create local scope for name resolution
            out << "{\n";

            const std::string ext = fileExtension(Global::config().get("profile"));

            const auto& rel = timer.getRelation();
            auto relName = synthesiser.getRelationName(rel);

            out << "\tLogger logger(R\"_(" << timer.getMessage() << ")_\",iter, [&](){return " << relName
                << "->size();});\n";
            // insert statement to be measured
            visit(timer.getStatement(), out);

            // done
            out << "}\n";
            PRINT_END_COMMENT(out);
        }

        void visitLogTimer(const RamLogTimer& timer, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            // create local scope for name resolution
            out << "{\n";

            const std::string ext = fileExtension(Global::config().get("profile"));

            // create local timer
            out << "\tLogger logger(R\"_(" << timer.getMessage() << ")_\",iter);\n";
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

        void visitStratum(const RamStratum& stratum, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            PRINT_END_COMMENT(out);
        }

        // -- operations --

        void visitNestedOperation(const RamNestedOperation& nested, std::ostream& out) override {
            visit(nested.getOperation(), out);
            if (Global::config().has("profile") && !nested.getProfileText().empty()) {
                out << "freqs[" << synthesiser.lookupFreqIdx(nested.getProfileText()) << "]++;\n";
            }
        }

        void visitSearch(const RamSearch& search, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            visitNestedOperation(search, out);
            PRINT_END_COMMENT(out);
        }

        void visitParallelScan(const RamParallelScan& pscan, std::ostream& out) override {
            const auto& rel = pscan.getRelation();
            const auto& relName = synthesiser.getRelationName(rel);

            assert(pscan.getTupleId() == 0 && "not outer-most loop");

            assert(rel.getArity() > 0 && "AstTranslator failed/no parallel scans for nullaries");

            assert(!preambleIssued && "only first loop can be made parallel");
            preambleIssued = true;

            PRINT_BEGIN_COMMENT(out);

            out << "auto part = " << relName << "->partition();\n";
            out << "PARALLEL_START;\n";
            out << preamble.str();
            out << "pfor(auto it = part.begin(); it<part.end();++it){\n";
            out << "try{\n";
            out << "for(const auto& env0 : *it) {\n";

            visitSearch(pscan, out);

            out << "}\n";
            out << "} catch(std::exception &e) { SignalHandler::instance()->error(e.what());}\n";
            out << "}\n";

            PRINT_END_COMMENT(out);
        }

        void visitScan(const RamScan& scan, std::ostream& out) override {
            const auto& rel = scan.getRelation();
            auto relName = synthesiser.getRelationName(rel);
            auto id = scan.getTupleId();

            PRINT_BEGIN_COMMENT(out);

            assert(rel.getArity() > 0 && "AstTranslator failed/no scans for nullaries");

            out << "for(const auto& env" << id << " : "
                << "*" << relName << ") {\n";

            visitSearch(scan, out);

            out << "}\n";

            PRINT_END_COMMENT(out);
        }

        void visitChoice(const RamChoice& choice, std::ostream& out) override {
            const auto& rel = choice.getRelation();
            auto relName = synthesiser.getRelationName(rel);
            auto identifier = choice.getTupleId();

            assert(rel.getArity() > 0 && "AstTranslator failed/no choice for nullaries");

            PRINT_BEGIN_COMMENT(out);

            out << "for(const auto& env" << identifier << " : "
                << "*" << relName << ") {\n";
            out << "if( ";

            visit(choice.getCondition(), out);

            out << ") {\n";

            visitSearch(choice, out);

            out << "break;\n";
            out << "}\n";
            out << "}\n";

            PRINT_END_COMMENT(out);
        }

        void visitParallelChoice(const RamParallelChoice& pchoice, std::ostream& out) override {
            const auto& rel = pchoice.getRelation();
            auto relName = synthesiser.getRelationName(rel);

            assert(pchoice.getTupleId() == 0 && "not outer-most loop");

            assert(rel.getArity() > 0 && "AstTranslator failed/no parallel choice for nullaries");

            assert(!preambleIssued && "only first loop can be made parallel");
            preambleIssued = true;

            PRINT_BEGIN_COMMENT(out);

            out << "auto part = " << relName << "->partition();\n";
            out << "PARALLEL_START;\n";
            out << preamble.str();
            out << "pfor(auto it = part.begin(); it<part.end();++it){\n";
            out << "try{\n";
            out << "for(const auto& env0 : *it) {\n";
            out << "if( ";

            visit(pchoice.getCondition(), out);

            out << ") {\n";

            visitSearch(pchoice, out);

            out << "break;\n";
            out << "}\n";
            out << "}\n";
            out << "} catch(std::exception &e) { SignalHandler::instance()->error(e.what());}\n";
            out << "}\n";

            PRINT_END_COMMENT(out);
        }

        void visitIndexScan(const RamIndexScan& iscan, std::ostream& out) override {
            const auto& rel = iscan.getRelation();
            auto relName = synthesiser.getRelationName(rel);
            auto identifier = iscan.getTupleId();
            auto keys = isa->getSearchSignature(&iscan);
            auto arity = rel.getArity();
            const auto& rangePattern = iscan.getRangePattern();

            assert(arity > 0 && "AstTranslator failed/no index scans for nullaries");

            PRINT_BEGIN_COMMENT(out);

            out << "const Tuple<RamDomain," << arity << "> key({{";
            for (size_t i = 0; i < arity; i++) {
                if (!isRamUndefValue(rangePattern[i])) {
                    visit(rangePattern[i], out);
                } else {
                    out << "0";
                }
                if (i + 1 < arity) {
                    out << ",";
                }
            }
            out << "}});\n";

            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(rel) + ")";

            out << "auto range = " << relName << "->"
                << "equalRange_" << keys << "(key," << ctxName << ");\n";
            out << "for(const auto& env" << identifier << " : range) {\n";

            visitSearch(iscan, out);

            out << "}\n";
            PRINT_END_COMMENT(out);
        }

        void visitParallelIndexScan(const RamParallelIndexScan& piscan, std::ostream& out) override {
            const auto& rel = piscan.getRelation();
            auto relName = synthesiser.getRelationName(rel);
            auto arity = rel.getArity();
            auto keys = isa->getSearchSignature(&piscan);
            const auto& rangePattern = piscan.getRangePattern();

            assert(piscan.getTupleId() == 0 && "not outer-most loop");

            assert(arity > 0 && "AstTranslator failed/no parallel index scan for nullaries");

            assert(!preambleIssued && "only first loop can be made parallel");
            preambleIssued = true;

            PRINT_BEGIN_COMMENT(out);

            out << "const Tuple<RamDomain," << arity << "> key({{";
            for (size_t i = 0; i < arity; i++) {
                if (!isRamUndefValue(rangePattern[i])) {
                    visit(rangePattern[i], out);
                } else {
                    out << "0";
                }
                if (i + 1 < arity) {
                    out << ",";
                }
            }
            out << "}});\n";
            out << "auto range = " << relName
                << "->"
                // TODO (b-scholz): context may be missing here?
                << "equalRange_" << keys << "(key);\n";
            out << "auto part = range.partition();\n";
            out << "PARALLEL_START;\n";
            out << preamble.str();
            out << "pfor(auto it = part.begin(); it<part.end(); ++it) { \n";
            out << "try{\n";
            out << "for(const auto& env0 : *it) {\n";

            visitSearch(piscan, out);

            out << "}\n";
            out << "} catch(std::exception &e) { SignalHandler::instance()->error(e.what());}\n";
            out << "}\n";

            PRINT_END_COMMENT(out);
        }

        void visitIndexChoice(const RamIndexChoice& ichoice, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            const auto& rel = ichoice.getRelation();
            auto relName = synthesiser.getRelationName(rel);
            auto identifier = ichoice.getTupleId();
            auto arity = rel.getArity();
            const auto& rangePattern = ichoice.getRangePattern();
            auto keys = isa->getSearchSignature(&ichoice);

            // check list of keys
            assert(arity > 0 && "AstTranslator failed");

            out << "const Tuple<RamDomain," << arity << "> key({{";
            for (size_t i = 0; i < arity; i++) {
                if (!isRamUndefValue(rangePattern[i])) {
                    visit(rangePattern[i], out);
                } else {
                    out << "0";
                }
                if (i + 1 < arity) {
                    out << ",";
                }
            }
            out << "}});\n";

            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(rel) + ")";

            out << "auto range = " << relName << "->"
                << "equalRange_" << keys << "(key," << ctxName << ");\n";
            out << "for(const auto& env" << identifier << " : range) {\n";
            out << "if( ";

            visit(ichoice.getCondition(), out);

            out << ") {\n";

            visitSearch(ichoice, out);

            out << "break;\n";
            out << "}\n";
            out << "}\n";

            PRINT_END_COMMENT(out);
        }

        void visitParallelIndexChoice(const RamParallelIndexChoice& pichoice, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            const auto& rel = pichoice.getRelation();
            auto relName = synthesiser.getRelationName(rel);
            auto arity = rel.getArity();
            const auto& rangePattern = pichoice.getRangePattern();
            auto keys = isa->getSearchSignature(&pichoice);

            assert(pichoice.getTupleId() == 0 && "not outer-most loop");

            assert(arity > 0 && "AstTranslator failed");

            assert(!preambleIssued && "only first loop can be made parallel");
            preambleIssued = true;

            PRINT_BEGIN_COMMENT(out);

            out << "const Tuple<RamDomain," << arity << "> key({{";
            for (size_t i = 0; i < arity; i++) {
                if (!isRamUndefValue(rangePattern[i])) {
                    visit(rangePattern[i], out);
                } else {
                    out << "0";
                }
                if (i + 1 < arity) {
                    out << ",";
                }
            }
            out << "}});\n";
            out << "auto range = " << relName
                << "->"
                // TODO (b-scholz): context may be missing here?
                << "equalRange_" << keys << "(key);\n";
            out << "auto part = range.partition();\n";
            out << "PARALLEL_START;\n";
            out << preamble.str();
            out << "pfor(auto it = part.begin(); it<part.end(); ++it) { \n";
            out << "try{";
            out << "for(const auto& env0 : *it) {\n";
            out << "if( ";

            visit(pichoice.getCondition(), out);

            out << ") {\n";

            visitSearch(pichoice, out);

            out << "break;\n";
            out << "}\n";
            out << "}\n";
            out << "} catch(std::exception &e) { SignalHandler::instance()->error(e.what());}\n";
            out << "}\n";

            PRINT_END_COMMENT(out);
        }

        void visitUnpackRecord(const RamUnpackRecord& lookup, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            auto arity = lookup.getArity();

            // get the tuple type working with
            std::string tuple_type = "ram::Tuple<RamDomain," + toString(arity) + ">";

            // look up reference
            out << "auto ref = ";
            visit(lookup.getExpression(), out);
            out << ";\n";

            out << "if (isNull<" << tuple_type << ">(ref)) continue;\n";
            out << tuple_type << " env" << lookup.getTupleId() << " = unpack<" << tuple_type << ">(ref);\n";

            out << "{\n";

            // continue with condition checks and nested body
            visitSearch(lookup, out);

            out << "}\n";
            PRINT_END_COMMENT(out);
        }

        void visitIndexAggregate(const RamIndexAggregate& aggregate, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            // get some properties
            const auto& rel = aggregate.getRelation();
            auto arity = rel.getArity();
            auto relName = synthesiser.getRelationName(rel);
            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(rel) + ")";
            auto identifier = aggregate.getTupleId();

            // aggregate tuple storing the result of aggregate
            std::string tuple_type = "ram::Tuple<RamDomain," + toString(arity) + ">";

            // declare environment variable
            out << "ram::Tuple<RamDomain,1> env" << identifier << ";\n";

            // get range to aggregate
            auto keys = isa->getSearchSignature(&aggregate);

            // special case: counting number elements over an unrestricted predicate
            if (aggregate.getFunction() == souffle::COUNT && keys == 0 &&
                    dynamic_cast<const RamTrue*>(&aggregate.getCondition()) != nullptr) {
                // shortcut: use relation size
                out << "env" << identifier << "[0] = " << relName << "->"
                    << "size();\n";
                visitSearch(aggregate, out);
                PRINT_END_COMMENT(out);
                return;
            }

            // init result
            std::string init;
            switch (aggregate.getFunction()) {
                case souffle::MIN:
                    init = "MAX_RAM_DOMAIN";
                    break;
                case souffle::MAX:
                    init = "MIN_RAM_DOMAIN";
                    break;
                case souffle::COUNT:
                    init = "0";
                    break;
                case souffle::SUM:
                    init = "0";
                    break;
                default:
                    abort();
            }
            out << "RamDomain res" << identifier << " = " << init << ";\n";

            // check whether there is an index to use
            if (keys == 0) {
                out << "for(const auto& env" << identifier << " : "
                    << "*" << relName << ") {\n";
            } else {
                // a lambda for printing boundary key values
                auto printKeyTuple = [&]() {
                    for (size_t i = 0; i < arity; i++) {
                        if (!isRamUndefValue(aggregate.getRangePattern()[i])) {
                            visit(aggregate.getRangePattern()[i], out);
                        } else {
                            out << "0";
                        }
                        if (i + 1 < arity) {
                            out << ",";
                        }
                    }
                };

                // get index
                auto index = synthesiser.toIndex(keys);
                out << "const " << tuple_type << " key({{";
                printKeyTuple();
                out << "}});\n";
                out << "auto range = " << relName << "->"
                    << "equalRange_" << keys << "(key," << ctxName << ");\n";

                // aggregate result
                out << "for(const auto& env" << identifier << " : range) {\n";
            }

            // produce condition inside the loop
            out << "if( ";
            visit(aggregate.getCondition(), out);
            out << ") {\n";

            switch (aggregate.getFunction()) {
                case souffle::MIN:
                    out << "res" << identifier << " = std::min (res" << identifier << ",";
                    visit(aggregate.getExpression(), out);
                    out << ");\n";
                    break;
                case souffle::MAX:
                    out << "res" << identifier << " = std::max (res" << identifier << ",";
                    visit(aggregate.getExpression(), out);
                    out << ");\n";
                    break;
                case souffle::COUNT:
                    // count is easy
                    out << "++res" << identifier << "\n;";
                    break;
                case souffle::SUM:
                    out << "res" << identifier << " += ";
                    visit(aggregate.getExpression(), out);
                    out << ";\n";
                    break;
                default:
                    abort();
            }

            out << "}\n";

            // end aggregator loop
            out << "}\n";

            // write result into environment tuple
            out << "env" << identifier << "[0] = res" << identifier << ";\n";

            if (aggregate.getFunction() == souffle::MIN || aggregate.getFunction() == souffle::MAX) {
                // check whether there exists a min/max first before next loop
                out << "if(res" << identifier << " != " << init << "){\n";
                visitSearch(aggregate, out);
                out << "}\n";
            } else {
                visitSearch(aggregate, out);
            }

            PRINT_END_COMMENT(out);
        }

        void visitAggregate(const RamAggregate& aggregate, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            // get some properties
            const auto& rel = aggregate.getRelation();
            auto relName = synthesiser.getRelationName(rel);
            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(rel) + ")";
            auto identifier = aggregate.getTupleId();

            // declare environment variable
            out << "ram::Tuple<RamDomain,1> env" << identifier << ";\n";

            // special case: counting number elements over an unrestricted predicate
            if (aggregate.getFunction() == souffle::COUNT &&
                    dynamic_cast<const RamTrue*>(&aggregate.getCondition()) != nullptr) {
                // shortcut: use relation size
                out << "env" << identifier << "[0] = " << relName << "->"
                    << "size();\n";
                visitSearch(aggregate, out);
                PRINT_END_COMMENT(out);
                return;
            }

            // init result
            std::string init;
            switch (aggregate.getFunction()) {
                case souffle::MIN:
                    init = "MAX_RAM_DOMAIN";
                    break;
                case souffle::MAX:
                    init = "MIN_RAM_DOMAIN";
                    break;
                case souffle::COUNT:
                    init = "0";
                    break;
                case souffle::SUM:
                    init = "0";
                    break;
                default:
                    abort();
            }
            out << "RamDomain res" << identifier << " = " << init << ";\n";

            // check whether there is an index to use
            out << "for(const auto& env" << identifier << " : "
                << "*" << relName << ") {\n";

            // produce condition inside the loop
            out << "if( ";
            visit(aggregate.getCondition(), out);
            out << ") {\n";

            // pick function
            switch (aggregate.getFunction()) {
                case souffle::MIN:
                    out << "res" << identifier << " = std::min(res" << identifier << ",";
                    visit(aggregate.getExpression(), out);
                    out << ");\n";
                    break;
                case souffle::MAX:
                    out << "res" << identifier << " = std::max(res" << identifier << ",";
                    visit(aggregate.getExpression(), out);
                    out << ");\n";
                    break;
                case souffle::COUNT:
                    out << "++res" << identifier << "\n;";
                    break;
                case souffle::SUM:
                    out << "res" << identifier << " += ";
                    visit(aggregate.getExpression(), out);
                    out << ";\n";
                    break;
                default:
                    abort();
            }

            out << "}\n";

            // end aggregator loop
            out << "}\n";

            // write result into environment tuple
            out << "env" << identifier << "[0] = res" << identifier << ";\n";

            if (aggregate.getFunction() == souffle::MIN || aggregate.getFunction() == souffle::MAX) {
                // check whether there exists a min/max first before next loop
                out << "if(res" << identifier << " != " << init << "){\n";
                visitSearch(aggregate, out);
                out << "}\n";
            } else {
                visitSearch(aggregate, out);
            }

            PRINT_END_COMMENT(out);
        }

        void visitFilter(const RamFilter& filter, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "if( ";
            visit(filter.getCondition(), out);
            out << ") {\n";
            visitNestedOperation(filter, out);
            out << "}\n";
            PRINT_END_COMMENT(out);
        }

        void visitBreak(const RamBreak& breakOp, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "if( ";
            visit(breakOp.getCondition(), out);
            out << ") break;\n";
            visitNestedOperation(breakOp, out);
            PRINT_END_COMMENT(out);
        }

        void visitProject(const RamProject& project, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            const auto& rel = project.getRelation();
            auto arity = rel.getArity();
            auto relName = synthesiser.getRelationName(rel);
            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(rel) + ")";

            // create projected tuple
            if (project.getValues().empty()) {
                out << "Tuple<RamDomain," << arity << "> tuple({{}});\n";
            } else {
                out << "Tuple<RamDomain," << arity << "> tuple({{static_cast<RamDomain>("
                    << join(project.getValues(), "),static_cast<RamDomain>(", rec) << ")}});\n";
            }

            // insert tuple
            out << relName << "->"
                << "insert(tuple," << ctxName << ");\n";

            PRINT_END_COMMENT(out);
        }

        // -- conditions --

        void visitTrue(const RamTrue& ltrue, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "true";
            PRINT_END_COMMENT(out);
        }

        void visitFalse(const RamFalse& lfalse, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "false";
            PRINT_END_COMMENT(out);
        }

        void visitConjunction(const RamConjunction& conj, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "((";
            visit(conj.getLHS(), out);
            out << ") && (";
            visit(conj.getRHS(), out);
            out << "))";
            PRINT_END_COMMENT(out);
        }

        void visitNegation(const RamNegation& neg, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "!(";
            visit(neg.getOperand(), out);
            out << ")";
            PRINT_END_COMMENT(out);
        }

        void visitConstraint(const RamConstraint& rel, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            switch (rel.getOperator()) {
                // comparison operators
                case BinaryConstraintOp::EQ:
                    out << "((";
                    visit(rel.getLHS(), out);
                    out << ") == (";
                    visit(rel.getRHS(), out);
                    out << "))";
                    break;
                case BinaryConstraintOp::NE:
                    out << "((";
                    visit(rel.getLHS(), out);
                    out << ") != (";
                    visit(rel.getRHS(), out);
                    out << "))";
                    break;
                case BinaryConstraintOp::LT:
                    out << "((";
                    visit(rel.getLHS(), out);
                    out << ") < (";
                    visit(rel.getRHS(), out);
                    out << "))";
                    break;
                case BinaryConstraintOp::LE:
                    out << "((";
                    visit(rel.getLHS(), out);
                    out << ") <= (";
                    visit(rel.getRHS(), out);
                    out << "))";
                    break;
                case BinaryConstraintOp::GT:
                    out << "((";
                    visit(rel.getLHS(), out);
                    out << ") > (";
                    visit(rel.getRHS(), out);
                    out << "))";
                    break;
                case BinaryConstraintOp::GE:
                    out << "((";
                    visit(rel.getLHS(), out);
                    out << ") >= (";
                    visit(rel.getRHS(), out);
                    out << "))";
                    break;

                // strings
                case BinaryConstraintOp::MATCH: {
                    out << "regex_wrapper(symTable.resolve(";
                    visit(rel.getLHS(), out);
                    out << "),symTable.resolve(";
                    visit(rel.getRHS(), out);
                    out << "))";
                    break;
                }
                case BinaryConstraintOp::NOT_MATCH: {
                    out << "!regex_wrapper(symTable.resolve(";
                    visit(rel.getLHS(), out);
                    out << "),symTable.resolve(";
                    visit(rel.getRHS(), out);
                    out << "))";
                    break;
                }
                case BinaryConstraintOp::CONTAINS: {
                    out << "(symTable.resolve(";
                    visit(rel.getRHS(), out);
                    out << ").find(symTable.resolve(";
                    visit(rel.getLHS(), out);
                    out << ")) != std::string::npos)";
                    break;
                }
                case BinaryConstraintOp::NOT_CONTAINS: {
                    out << "(symTable.resolve(";
                    visit(rel.getRHS(), out);
                    out << ").find(symTable.resolve(";
                    visit(rel.getLHS(), out);
                    out << ")) == std::string::npos)";
                    break;
                }
                default:
                    assert(false && "Unsupported Operation!");
                    break;
            }
            PRINT_END_COMMENT(out);
        }

        void visitEmptinessCheck(const RamEmptinessCheck& emptiness, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << synthesiser.getRelationName(emptiness.getRelation()) << "->"
                << "empty()";
            PRINT_END_COMMENT(out);
        }

        void visitExistenceCheck(const RamExistenceCheck& exists, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            // get some details
            const auto& rel = exists.getRelation();
            auto relName = synthesiser.getRelationName(rel);
            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(rel) + ")";
            auto arity = rel.getArity();
            assert(arity > 0 && "AstTranslator failed");
            std::string before, after;
            if (Global::config().has("profile") && !exists.getRelation().isTemp()) {
                out << R"_((reads[)_" << synthesiser.lookupReadIdx(rel.getName()) << R"_(]++,)_";
                after = ")";
            }

            // if it is total we use the contains function
            if (isa->isTotalSignature(&exists)) {
                out << relName << "->"
                    << "contains(Tuple<RamDomain," << arity << ">({{" << join(exists.getValues(), ",", rec)
                    << "}})," << ctxName << ")" << after;
                PRINT_END_COMMENT(out);
                return;
            }

            // else we conduct a range query
            out << "!" << relName << "->"
                << "equalRange";
            out << "_" << isa->getSearchSignature(&exists);
            out << "(Tuple<RamDomain," << arity << ">({{";
            out << join(exists.getValues(), ",", [&](std::ostream& out, RamExpression* value) {
                if (!isRamUndefValue(value)) {
                    visit(*value, out);
                } else {
                    out << "0";
                }
            });
            out << "}})," << ctxName << ").empty()" << after;
            PRINT_END_COMMENT(out);
        }

        void visitProvenanceExistenceCheck(
                const RamProvenanceExistenceCheck& provExists, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            // get some details
            const auto& rel = provExists.getRelation();
            auto relName = synthesiser.getRelationName(rel);
            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(rel) + ")";
            auto arity = rel.getArity();

            // provenance not exists is never total, conduct a range query
            out << "[&]() -> bool {\n";
            out << "auto existenceCheck = " << relName << "->"
                << "equalRange";
            // out << synthesiser.toIndex(ne.getSearchSignature());
            out << "_" << isa->getSearchSignature(&provExists);
            out << "(Tuple<RamDomain," << arity << ">({{";
            for (size_t i = 0; i < provExists.getValues().size() - 1; i++) {
                RamExpression* val = provExists.getValues()[i];
                if (!isRamUndefValue(val)) {
                    visit(*val, out);
                } else {
                    out << "0";
                }
                out << ",";
            }
            // extra 0 for provenance height annotation
            out << "0";

            out << "}})," << ctxName << ");\n";
            out << "if (existenceCheck.empty()) return false; else return (*existenceCheck.begin())["
                << arity - 1 << "] <= ";
            visit(*(provExists.getValues()[arity - 1]), out);
            out << ";}()\n";
            PRINT_END_COMMENT(out);
        }

        // -- values --
        void visitNumber(const RamNumber& num, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "RamDomain(" << num.getConstant() << ")";
            PRINT_END_COMMENT(out);
        }

        void visitElementAccess(const RamElementAccess& access, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "env" << access.getTupleId() << "[" << access.getElement() << "]";
            PRINT_END_COMMENT(out);
        }

        void visitAutoIncrement(const RamAutoIncrement& /*inc*/, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "(ctr++)";
            PRINT_END_COMMENT(out);
        }

        void visitIntrinsicOperator(const RamIntrinsicOperator& op, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);

            switch (op.getOperator()) {
                /** Unary Functor Operators */
                case FunctorOp::ORD: {
                    visit(op.getArgument(0), out);
                    break;
                }
                case FunctorOp::STRLEN: {
                    out << "static_cast<RamDomain>(symTable.resolve(";
                    visit(op.getArgument(0), out);
                    out << ").size())";
                    break;
                }
                case FunctorOp::NEG: {
                    out << "(-(";
                    visit(op.getArgument(0), out);
                    out << "))";
                    break;
                }
                case FunctorOp::BNOT: {
                    out << "(~(";
                    visit(op.getArgument(0), out);
                    out << "))";
                    break;
                }
                case FunctorOp::LNOT: {
                    out << "(!(";
                    visit(op.getArgument(0), out);
                    out << "))";
                    break;
                }
                case FunctorOp::TOSTRING: {
                    out << "symTable.lookup(std::to_string(";
                    visit(op.getArgument(0), out);
                    out << "))";
                    break;
                }
                case FunctorOp::TONUMBER: {
                    out << "(wrapper_tonumber(symTable.resolve((size_t)";
                    visit(op.getArgument(0), out);
                    out << ")))";
                    break;
                }

                /** Binary Functor Operators */
                // arithmetic
                case FunctorOp::ADD: {
                    out << "(";
                    visit(op.getArgument(0), out);
                    out << ") + (";
                    visit(op.getArgument(1), out);
                    out << ")";
                    break;
                }
                case FunctorOp::SUB: {
                    out << "(";
                    visit(op.getArgument(0), out);
                    out << ") - (";
                    visit(op.getArgument(1), out);
                    out << ")";
                    break;
                }
                case FunctorOp::MUL: {
                    out << "(";
                    visit(op.getArgument(0), out);
                    out << ") * (";
                    visit(op.getArgument(1), out);
                    out << ")";
                    break;
                }
                case FunctorOp::DIV: {
                    out << "(";
                    visit(op.getArgument(0), out);
                    out << ") / (";
                    visit(op.getArgument(1), out);
                    out << ")";
                    break;
                }
                case FunctorOp::EXP: {
                    // Cast as int64, then back to RamDomain of int32 to avoid wrapping to negative
                    // when using int32 RamDomains
                    out << "static_cast<int64_t>(std::pow(";
                    visit(op.getArgument(0), out);
                    out << ",";
                    visit(op.getArgument(1), out);
                    out << "))";
                    break;
                }
                case FunctorOp::MOD: {
                    out << "(";
                    visit(op.getArgument(0), out);
                    out << ") % (";
                    visit(op.getArgument(1), out);
                    out << ")";
                    break;
                }
                case FunctorOp::BAND: {
                    out << "(";
                    visit(op.getArgument(0), out);
                    out << ") & (";
                    visit(op.getArgument(1), out);
                    out << ")";
                    break;
                }
                case FunctorOp::BOR: {
                    out << "(";
                    visit(op.getArgument(0), out);
                    out << ") | (";
                    visit(op.getArgument(1), out);
                    out << ")";
                    break;
                }
                case FunctorOp::BXOR: {
                    out << "(";
                    visit(op.getArgument(0), out);
                    out << ") ^ (";
                    visit(op.getArgument(1), out);
                    out << ")";
                    break;
                }
                case FunctorOp::LAND: {
                    out << "(";
                    visit(op.getArgument(0), out);
                    out << ") && (";
                    visit(op.getArgument(1), out);
                    out << ")";
                    break;
                }
                case FunctorOp::LOR: {
                    out << "(";
                    visit(op.getArgument(0), out);
                    out << ") || (";
                    visit(op.getArgument(1), out);
                    out << ")";
                    break;
                }
                case FunctorOp::MAX: {
                    out << "std::max({";
                    for (auto& cur : op.getArguments()) {
                        visit(cur, out);
                        out << ", ";
                    }
                    out << "})";
                    break;
                }
                case FunctorOp::MIN: {
                    out << "std::min({";
                    for (auto& cur : op.getArguments()) {
                        visit(cur, out);
                        out << ", ";
                    }
                    out << "})";
                    break;
                }

                // strings
                case FunctorOp::CAT: {
                    out << "symTable.lookup(";
                    for (size_t i = 0; i < op.getArgCount() - 1; i++) {
                        out << "symTable.resolve(";
                        visit(op.getArgument(i), out);
                        out << ") + ";
                    }
                    out << "symTable.resolve(";
                    visit(op.getArgument(op.getArgCount() - 1), out);
                    out << "))";
                    break;
                }

                /** Ternary Functor Operators */
                case FunctorOp::SUBSTR: {
                    out << "symTable.lookup(";
                    out << "substr_wrapper(symTable.resolve(";
                    visit(op.getArgument(0), out);
                    out << "),(";
                    visit(op.getArgument(1), out);
                    out << "),(";
                    visit(op.getArgument(2), out);
                    out << ")))";
                    break;
                }

                /** Undefined */
                default: {
                    assert(false && "Unsupported Operation!");
                    break;
                }
            }
            PRINT_END_COMMENT(out);
        }

        void visitUserDefinedOperator(const RamUserDefinedOperator& op, std::ostream& out) override {
            const std::string& name = op.getName();
            const std::string& type = op.getType();
            size_t arity = type.length() - 1;

            if (type[arity] == 'S') {
                out << "symTable.lookup(";
            }
            out << name << "(";

            for (size_t i = 0; i < arity; i++) {
                if (i > 0) {
                    out << ",";
                }
                if (type[i] == 'N') {
                    out << "((RamDomain)";
                    visit(op.getArgument(i), out);
                    out << ")";
                } else {
                    out << "symTable.resolve((RamDomain)";
                    visit(op.getArgument(i), out);
                    out << ").c_str()";
                }
            }
            out << ")";
            if (type[arity] == 'S') {
                out << ")";
            }
        }

        // -- records --

        void visitPackRecord(const RamPackRecord& pack, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "pack("
                << "ram::Tuple<RamDomain," << pack.getArguments().size() << ">({"
                << join(pack.getArguments(), ",", rec) << "})"
                << ")";
            PRINT_END_COMMENT(out);
        }

        // -- subroutine argument --

        void visitArgument(const RamArgument& arg, std::ostream& out) override {
            out << "(args)[" << arg.getArgument() << "]";
        }

        // -- subroutine return --

        void visitReturnValue(const RamReturnValue& ret, std::ostream& out) override {
            out << "std::lock_guard<std::mutex> guard(lock);\n";
            for (auto val : ret.getValues()) {
                if (isRamUndefValue(val)) {
                    out << "ret.push_back(0);\n";
                    out << "err.push_back(true);\n";
                } else {
                    out << "ret.push_back(";
                    visit(val, out);
                    out << ");\n";
                    out << "err.push_back(false);\n";
                }
            }
        }

#ifdef USE_MPI

        // -- mpi statements --

        void visitRecv(const RamRecv& recv, std::ostream& os) override {
            os << "\n#ifdef USE_MPI\n";
            os << "{";
            os << "auto status = souffle::mpi::probe(";
            // source
            os << recv.getSourceStratum() + 1 << ", ";
            // tag
            os << "tag_" << synthesiser.getRelationName(recv.getRelation());
            os << ");";
            os << "souffle::mpi::recv<RamDomain>(";
            // data
            os << "*" << synthesiser.getRelationName(recv.getRelation()) << ", ";
            // arity
            os << recv.getRelation().getArity() << ", ";
            // status
            os << "status";
            os << ");";
            os << "}";
            os << "\n#endif\n";
        }

        void visitSend(const RamSend& send, std::ostream& os) override {
            os << "\n#ifdef USE_MPI\n";
            os << "{";
            os << "souffle::mpi::send<RamDomain>(";
            // data
            os << "*" << synthesiser.getRelationName(send.getRelation()) << ", ";
            // arity
            os << send.getRelation().getArity() << ", ";
            // destinations
            const auto& destinationStrata = send.getDestinationStrata();
            auto it = destinationStrata.begin();
            os << "std::set<int>(";
            if (it != destinationStrata.end()) {
                os << "{" << *it + 1;
                ++it;
                while (it != destinationStrata.end()) {
                    os << ", " << *it + 1;
                    ++it;
                }
                os << "}";
            } else {
                os << "0";
            }
            os << "), ";
            // tag
            os << "tag_" << synthesiser.getRelationName(send.getRelation());
            os << ");";
            os << "}";
            os << "\n#endif\n";
        }

        void visitNotify(const RamNotify&, std::ostream& os) override {
            os << "\n#ifdef USE_MPI\n";
            os << "mpi::send(0, SymbolTable::exitTag());";
            os << "mpi::recv(0, SymbolTable::exitTag());";
            os << "\n#endif\n";
        }

        void visitWait(const RamWait& wait, std::ostream& os) override {
            os << "\n#ifdef USE_MPI\n";
            os << "symTable.handleMpiMessages(" << wait.getCount() << ");";
            os << "\n#endif\n";
        }

#endif
        // -- safety net --

        void visitUndefValue(const RamUndefValue& undef, std::ostream& /*out*/) override {
            assert(false && "Compilation error");
        }

        void visitNode(const RamNode& node, std::ostream& /*out*/) override {
            std::cerr << "Unsupported node type: " << typeid(node).name() << "\n";
            assert(false && "Unsupported Node Type!");
        }
    };

    // emit code
    CodeEmitter(*this).visit(stmt, out);
}

void Synthesiser::generateCode(std::ostream& os, const std::string& id, bool& withSharedLibrary) {
    // ---------------------------------------------------------------
    //                      Auto-Index Generation
    // ---------------------------------------------------------------
    const SymbolTable& symTable = translationUnit.getSymbolTable();
    const RamProgram& prog = *translationUnit.getProgram();
    auto* idxAnalysis = translationUnit.getAnalysis<RamIndexAnalysis>();

    // ---------------------------------------------------------------
    //                      Code Generation
    // ---------------------------------------------------------------

    withSharedLibrary = false;

    std::string classname = "Sf_" + id;

#ifdef USE_MPI
    // turn off mpi support if not enabled as the execution engine
    if (Global::config().get("engine") != "mpi") {
        os << "#undef USE_MPI\n";
    }
#endif

    // generate C++ program
    os << "\n#include \"souffle/CompiledSouffle.h\"\n";
    if (Global::config().has("provenance")) {
        os << "#include <mutex>\n";
        os << "#include \"souffle/Explain.h\"\n";
    }

    if (Global::config().has("live-profile")) {
        os << "#include <thread>\n";
        os << "#include \"souffle/profile/Tui.h\"\n";
    }
    os << "\n";
    // produce external definitions for user-defined functors
    std::map<std::string, std::string> functors;
    visitDepthFirst(prog, [&](const RamUserDefinedOperator& op) {
        if (functors.find(op.getName()) == functors.end())
            functors.insert(std::make_pair(op.getName(), op.getType()));
        withSharedLibrary = true;
    });
    os << "extern \"C\" {\n";
    for (const auto& f : functors) {
        size_t arity = f.second.length() - 1;
        const std::string& type = f.second;
        const std::string& name = f.first;
        if (type[arity] == 'N') {
            os << "souffle::RamDomain ";
        } else if (type[arity] == 'S') {
            os << "const char * ";
        }
        os << name << "(";
        std::vector<std::string> args;
        for (size_t i = 0; i < arity; i++) {
            if (type[i] == 'N') {
                args.push_back("souffle::RamDomain");
            } else {
                args.push_back("const char *");
            }
        }
        os << join(args, ",");
        os << ");\n";
    }
    os << "}\n";
    os << "\n";
    os << "namespace souffle {\n";
    os << "using namespace ram;\n";

    visitDepthFirst(*(prog.getMain()), [&](const RamCreate& create) {
        // get some table details
        const RamRelation& rel = create.getRelation();
        const std::string& raw_name = rel.getName();

        bool isProvInfo = raw_name.find("@info") != std::string::npos;
        auto relationType = SynthesiserRelation::getSynthesiserRelation(
                rel, idxAnalysis->getIndexes(rel), Global::config().has("provenance") && !isProvInfo);

        generateRelationTypeStruct(os, std::move(relationType));
    });
    os << '\n';

    os << "class " << classname << " : public SouffleProgram {\n";

    // regex wrapper
    os << "private:\n";
    os << "static inline bool regex_wrapper(const std::string& pattern, const std::string& text) {\n";
    os << "   bool result = false; \n";
    os << "   try { result = std::regex_match(text, std::regex(pattern)); } catch(...) { \n";
    os << "     std::cerr << \"warning: wrong pattern provided for match(\\\"\" << pattern << \"\\\",\\\"\" "
          "<< text << \"\\\").\\n\";\n}\n";
    os << "   return result;\n";
    os << "}\n";

    // substring wrapper
    os << "private:\n";
    os << "static inline std::string substr_wrapper(const std::string& str, size_t idx, size_t len) {\n";
    os << "   std::string result; \n";
    os << "   try { result = str.substr(idx,len); } catch(...) { \n";
    os << "     std::cerr << \"warning: wrong index position provided by substr(\\\"\";\n";
    os << "     std::cerr << str << \"\\\",\" << (int32_t)idx << \",\" << (int32_t)len << \") "
          "functor.\\n\";\n";
    os << "   } return result;\n";
    os << "}\n";

    // to number wrapper
    os << "private:\n";
    os << "static inline RamDomain wrapper_tonumber(const std::string& str) {\n";
    os << "   RamDomain result=0; \n";
    os << "   try { result = stord(str); } catch(...) { \n";
    os << "     std::cerr << \"error: wrong string provided by to_number(\\\"\";\n";
    os << R"(     std::cerr << str << "\") )";
    os << "functor.\\n\";\n";
    os << "     raise(SIGFPE);\n";
    os << "   } return result;\n";
    os << "}\n";

// if using mpi...
#ifdef USE_MPI
    if (Global::config().get("engine") == "mpi") {
        os << "\n#ifdef USE_MPI\n";

        // create an enum of message tags, one for each relation
        {
            os << "private:\n";
            os << "enum {";
            {
                int tag = SymbolTable::numberOfTags();
                visitDepthFirst(*(prog.getMain()), [&](const RamCreate& create) {
                    if (tag != SymbolTable::numberOfTags()) {
                        os << ", ";
                    }
                    os << "tag_" << getRelationName(create.getRelation()) << " = " << tag;
                    ++tag;
                });
            }
            os << "};";
        }
        os << "\n#endif\n";
    }
#endif

    if (Global::config().has("profile")) {
        os << "std::string profiling_fname;\n";
    }

    os << "public:\n";

    // declare symbol table
    os << "// -- initialize symbol table --\n";
    {
        os << "SymbolTable symTable\n";
        if (symTable.size() > 0) {
            os << "{\n";
            for (size_t i = 0; i < symTable.size(); i++) {
                os << "\tR\"_(" << symTable.resolve(i) << ")_\",\n";
            }
            os << "}";
        }
        os << ";";
    }
    if (Global::config().has("profile")) {
        os << "private:\n";
        size_t numFreq = 0;
        visitDepthFirst(*(prog.getMain()), [&](const RamStatement& node) { numFreq++; });
        os << "  size_t freqs[" << numFreq << "]{};\n";
        size_t numRead = 0;
        visitDepthFirst(*(prog.getMain()), [&](const RamCreate& node) {
            if (!node.getRelation().isTemp()) numRead++;
        });
        os << "  size_t reads[" << numRead << "]{};\n";
    }

    // print relation definitions
    std::string initCons;     // initialization of constructor
    std::string registerRel;  // registration of relations
    int relCtr = 0;
    std::string tempType;  // string to hold the type of the temporary relations
    std::set<std::string> storeRelations;
    std::set<std::string> loadRelations;
    visitDepthFirst(*(prog.getMain()),
            [&](const RamStore& store) { storeRelations.insert(store.getRelation().getName()); });
    visitDepthFirst(*(prog.getMain()),
            [&](const RamLoad& load) { loadRelations.insert(load.getRelation().getName()); });
    visitDepthFirst(*(prog.getMain()), [&](const RamCreate& create) {
        // get some table details
        const auto& rel = create.getRelation();
        int arity = rel.getArity();
        const std::string& raw_name = rel.getName();
        const std::string& name = getRelationName(rel);

        // TODO: make this correct
        // ensure that the type of the new knowledge is the same as that of the delta knowledge
        bool isDelta = rel.isTemp() && raw_name.find("@delta") != std::string::npos;
        bool isProvInfo = raw_name.find("@info") != std::string::npos;
        auto relationType = SynthesiserRelation::getSynthesiserRelation(
                rel, idxAnalysis->getIndexes(rel), Global::config().has("provenance") && !isProvInfo);
        tempType = isDelta ? relationType->getTypeName() : tempType;
        const std::string& type = (rel.isTemp()) ? tempType : relationType->getTypeName();

        // defining table
        os << "// -- Table: " << raw_name << "\n";

        os << "std::unique_ptr<" << type << "> " << name << " = std::make_unique<" << type << ">();\n";
        if (!rel.isTemp()) {
            os << "souffle::RelationWrapper<";
            os << relCtr++ << ",";
            os << type << ",";
            os << "Tuple<RamDomain," << arity << ">,";
            os << arity;
            os << "> wrapper_" << name << ";\n";

            // construct types
            std::string tupleType = "std::array<const char *," + std::to_string(arity) + ">{{";
            std::string tupleName = "std::array<const char *," + std::to_string(arity) + ">{{";

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
            tupleType += "}}";
            tupleName += "}}";

            if (!initCons.empty()) {
                initCons += ",\n";
            }
            initCons += "\nwrapper_" + name + "(" + "*" + name + ",symTable,\"" + raw_name + "\"," +
                        tupleType + "," + tupleName + ")";
            registerRel += "addRelation(\"" + raw_name + "\",&wrapper_" + name + ",";
            registerRel += (loadRelations.count(rel.getName()) > 0) ? "true" : "false";
            registerRel += ",";
            registerRel += (storeRelations.count(rel.getName()) > 0) ? "true" : "false";
            registerRel += ");\n";
        }
    });

    os << "public:\n";

    // -- constructor --

    os << classname;
    if (Global::config().has("profile")) {
        os << "(std::string pf=\"profile.log\") : profiling_fname(pf)";
        if (!initCons.empty()) {
            os << ",\n" << initCons;
        }
    } else {
        os << "()";
        if (!initCons.empty()) {
            os << " : " << initCons;
        }
    }
    os << "{\n";
    if (Global::config().has("profile")) {
        os << "ProfileEventSingleton::instance().setOutputFile(profiling_fname);\n";
    }
    os << registerRel;
    os << "}\n";
    // -- destructor --

    os << "~" << classname << "() {\n";
    os << "}\n";

    // -- run function --
    os << "private:\nvoid runFunction(std::string inputDirectory = \".\", "
          "std::string outputDirectory = \".\", size_t stratumIndex = (size_t) -1, bool performIO = false) "
          "{\n";

    os << "SignalHandler::instance()->set();\n";
    if (Global::config().has("verbose")) {
        os << "SignalHandler::instance()->enableLogging();\n";
    }

    bool hasIncrement = false;
    visitDepthFirst(*(prog.getMain()), [&](const RamAutoIncrement& inc) { hasIncrement = true; });
    // initialize counter
    if (hasIncrement) {
        os << "// -- initialize counter --\n";
        os << "std::atomic<RamDomain> ctr(0);\n\n";
    }
    os << "std::atomic<size_t> iter(0);\n\n";

    // set default threads (in embedded mode)
    if (std::stoi(Global::config().get("jobs")) > 1) {
        os << "#if defined(__EMBEDDED_SOUFFLE__) && defined(_OPENMP)\n";
        os << "omp_set_num_threads(" << std::stoi(Global::config().get("jobs")) << ");\n";
        os << "#endif\n\n";
    }

    // add actual program body
    os << "// -- query evaluation --\n";
    if (Global::config().has("profile")) {
        os << "ProfileEventSingleton::instance().startTimer();\n";
        os << R"_(ProfileEventSingleton::instance().makeTimeEvent("@time;starttime");)_" << '\n';
        os << "{\n"
           << R"_(Logger logger("@runtime;", 0);)_" << '\n';
        // Store count of relations
        size_t relationCount = 0;
        visitDepthFirst(*(prog.getMain()), [&](const RamCreate& create) {
            if (create.getRelation().getName()[0] != '@') ++relationCount;
        });
        // Store configuration
        os << R"_(ProfileEventSingleton::instance().makeConfigRecord("relationCount", std::to_string()_"
           << relationCount << "));";
        // Outline stratum records for faster compilation
        os << "[](){\n";

        // Record relations created in each stratum
        visitDepthFirst(*(prog.getMain()), [&](const RamStratum& stratum) {
            std::map<std::string, size_t> relNames;
            visitDepthFirst(stratum, [&](const RamCreate& create) {
                relNames[create.getRelation().getName()] = create.getRelation().getArity();
            });
            for (const auto& cur : relNames) {
                // Skip temporary relations, marked with '@'
                if (cur.first[0] == '@') {
                    continue;
                }
                os << "ProfileEventSingleton::instance().makeStratumRecord(" << stratum.getIndex()
                   << R"_(, "relation", ")_" << cur.first << R"_(", "arity", ")_" << cur.second << R"_(");)_"
                   << '\n';
            }
        });
        // End stratum record outlining
        os << "}();\n";
    }

    if (Global::config().has("engine")) {
        std::stringstream ss;
        bool hasAtLeastOneStrata = false;
        visitDepthFirst(*(prog.getMain()), [&](const RamStratum& stratum) {
            hasAtLeastOneStrata = true;
            // go to stratum of index in switch
            auto i = stratum.getIndex();
            ss << "case " << i << ":\ngoto STRATUM_" << i << ";\nbreak;\n";
        });
        if (hasAtLeastOneStrata) {
            os << "switch (stratumIndex) {\n";
            {
                // otherwise use stratum 0 if index is -1
                os << "case (size_t) -1:\ngoto STRATUM_0;\nbreak;\n";
            }
            os << ss.str();
            os << "}\n";
        }
    }

    // Set up stratum
    visitDepthFirst(*(prog.getMain()), [&](const RamStratum& stratum) {
        os << "/* BEGIN STRATUM " << stratum.getIndex() << " */\n";
        if (Global::config().has("engine")) {
            // go to the stratum with the max value for int as a suffix if calling the master stratum
            auto i = stratum.getIndex();
            os << "STRATUM_" << i << ":\n";
        }
        os << "[&]() {\n";
        emitCode(os, stratum.getBody());
        os << "}();\n";
        if (Global::config().has("engine")) {
            os << "if (stratumIndex != (size_t) -1) goto EXIT;\n";
        }
        os << "/* END STRATUM " << stratum.getIndex() << " */\n";
    });

    if (Global::config().has("engine")) {
        os << "EXIT:{}";
    }

    if (Global::config().has("profile")) {
        os << "}\n";
        os << "ProfileEventSingleton::instance().stopTimer();\n";
        os << "dumpFreqs();\n";
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
    os << "public:\nvoid run(size_t stratumIndex = (size_t) -1) override { runFunction(\".\", \".\", "
          "stratumIndex, false); }\n";
    os << "public:\nvoid runAll(std::string inputDirectory = \".\", std::string outputDirectory = \".\", "
          "size_t stratumIndex = (size_t) -1) "
          "override { ";
    if (Global::config().has("live-profile")) {
        os << "std::thread profiler([]() { profile::Tui().runProf(); });\n";
    }
    os << "runFunction(inputDirectory, outputDirectory, stratumIndex, true);\n";
    if (Global::config().has("live-profile")) {
        os << "if (profiler.joinable()) { profiler.join(); }\n";
    }
    os << "}\n";

    // issue printAll method
    os << "public:\n";
    os << "void printAll(std::string outputDirectory = \".\") override {\n";
    visitDepthFirst(*(prog.getMain()), [&](const RamStatement& node) {
        if (auto store = dynamic_cast<const RamStore*>(&node)) {
            std::vector<bool> symbolMask;
            for (auto& cur : store->getRelation().getAttributeTypeQualifiers()) {
                symbolMask.push_back(cur[0] == 's');
            }
            for (IODirectives ioDirectives : store->getIODirectives()) {
                os << "try {";
                os << "std::map<std::string, std::string> directiveMap(" << ioDirectives << ");\n";
                os << R"_(if (!outputDirectory.empty() && directiveMap["IO"] == "file" && )_";
                os << "directiveMap[\"filename\"].front() != '/') {";
                os << R"_(directiveMap["filename"] = outputDirectory + "/" + directiveMap["filename"];)_";
                os << "}\n";
                os << "IODirectives ioDirectives(directiveMap);\n";
                os << "IOSystem::getInstance().getWriter(";
                os << "std::vector<bool>({" << join(symbolMask) << "})";
                os << ", symTable, ioDirectives, " << (Global::config().has("provenance") ? "true" : "false");
                os << ")->writeAll(*" << getRelationName(store->getRelation()) << ");\n";

                os << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
            }
        }
    });
    os << "}\n";  // end of printAll() method

    // dumpFreqs method
    if (Global::config().has("profile")) {
        os << "private:\n";
        os << "void dumpFreqs() {\n";
        for (auto const& cur : idxMap) {
            os << "\tProfileEventSingleton::instance().makeQuantityEvent(R\"_(" << cur.first << ")_\", freqs["
               << cur.second << "],0);\n";
        }
        for (auto const& cur : neIdxMap) {
            os << "\tProfileEventSingleton::instance().makeQuantityEvent(R\"_(@relation-reads;" << cur.first
               << ")_\", reads[" << cur.second << "],0);\n";
        }
        os << "}\n";  // end of dumpFreqs() method
    }

    // issue loadAll method
    os << "public:\n";
    os << "void loadAll(std::string inputDirectory = \".\") override {\n";
    visitDepthFirst(*(prog.getMain()), [&](const RamLoad& load) {
        // get some table details
        std::vector<bool> symbolMask;
        for (auto& cur : load.getRelation().getAttributeTypeQualifiers()) {
            symbolMask.push_back(cur[0] == 's');
        }
        for (IODirectives ioDirectives : load.getIODirectives()) {
            os << "try {";
            os << "std::map<std::string, std::string> directiveMap(";
            os << ioDirectives << ");\n";
            os << R"_(if (!inputDirectory.empty() && directiveMap["IO"] == "file" && )_";
            os << "directiveMap[\"filename\"].front() != '/') {";
            os << R"_(directiveMap["filename"] = inputDirectory + "/" + directiveMap["filename"];)_";
            os << "}\n";
            os << "IODirectives ioDirectives(directiveMap);\n";
            os << "IOSystem::getInstance().getReader(";
            os << "std::vector<bool>({" << join(symbolMask) << "})";
            os << ", symTable, ioDirectives";
            os << ", " << (Global::config().has("provenance") ? "true" : "false");
            os << ")->readAll(*" << getRelationName(load.getRelation());
            os << ");\n";
            os << "} catch (std::exception& e) {std::cerr << \"Error loading data: \" << e.what() << "
                  "'\\n';}\n";
        }
    });
    os << "}\n";  // end of loadAll() method

    // issue dump methods
    auto dumpRelation = [&](const std::string& name, const std::vector<std::string>& mask, size_t arity) {
        auto relName = name;
        std::vector<bool> symbolMask;
        for (auto& cur : mask) {
            symbolMask.push_back(cur[0] == 's');
        }

        os << "try {";
        os << "IODirectives ioDirectives;\n";
        os << "ioDirectives.setIOType(\"stdout\");\n";
        os << "ioDirectives.setRelationName(\"" << name << "\");\n";
        os << "IOSystem::getInstance().getWriter(";
        os << "std::vector<bool>({" << join(symbolMask) << "})";
        os << ", symTable, ioDirectives, " << (Global::config().has("provenance") ? "true" : "false");
        os << ")->writeAll(*" << relName << ");\n";
        os << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
    };

    // dump inputs
    os << "public:\n";
    os << "void dumpInputs(std::ostream& out = std::cout) override {\n";
    visitDepthFirst(*(prog.getMain()), [&](const RamLoad& load) {
        auto& name = getRelationName(load.getRelation());
        auto& mask = load.getRelation().getAttributeTypeQualifiers();
        size_t arity = load.getRelation().getArity();
        dumpRelation(name, mask, arity);
    });
    os << "}\n";  // end of dumpInputs() method

    // dump outputs
    os << "public:\n";
    os << "void dumpOutputs(std::ostream& out = std::cout) override {\n";
    visitDepthFirst(*(prog.getMain()), [&](const RamStore& store) {
        auto& name = getRelationName(store.getRelation());
        auto& mask = store.getRelation().getAttributeTypeQualifiers();
        size_t arity = store.getRelation().getArity();
        dumpRelation(name, mask, arity);
    });
    os << "}\n";  // end of dumpOutputs() method

    os << "public:\n";
    os << "SymbolTable& getSymbolTable() override {\n";
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
               << "subproof_" << subroutineNum
               << "(const std::vector<RamDomain>& args, "
                  "std::vector<RamDomain>& ret, std::vector<bool>& err) {\n";

            // a lock is needed when filling the subroutine return vectors
            os << "std::mutex lock;\n";

            // generate code for body
            emitCode(os, *sub.second);

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

    os << "\n#ifdef __EMBEDDED_SOUFFLE__\n";
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
    os << std::stoi(Global::config().get("jobs")) << ",\n";
    os << "-1";
    os << ");\n";

    os << "if (!opt.parse(argc,argv)) return 1;\n";

    os << "#if defined(_OPENMP) \n";
    os << "omp_set_nested(true);\n";
    os << "\n#endif\n";

    os << "souffle::";
    if (Global::config().has("profile")) {
        os << classname + " obj(opt.getProfileName());\n";
    } else {
        os << classname + " obj;\n";
    }

    if (Global::config().has("profile")) {
        os << R"_(souffle::ProfileEventSingleton::instance().makeConfigRecord("", opt.getSourceFileName());)_"
           << '\n';
        os << R"_(souffle::ProfileEventSingleton::instance().makeConfigRecord("fact-dir", opt.getInputFileDir());)_"
           << '\n';
        os << R"_(souffle::ProfileEventSingleton::instance().makeConfigRecord("jobs", std::to_string(opt.getNumJobs()));)_"
           << '\n';
        os << R"_(souffle::ProfileEventSingleton::instance().makeConfigRecord("output-dir", opt.getOutputFileDir());)_"
           << '\n';
        os << R"_(souffle::ProfileEventSingleton::instance().makeConfigRecord("version", ")_"
           << Global::config().get("version") << R"_(");)_" << '\n';
    }
#ifdef USE_MPI
    if (Global::config().get("engine") == "mpi") {
        os << "\n#ifdef USE_MPI\n";
        os << "souffle::mpi::init(argc, argv);";
        os << "int rank = souffle::mpi::commRank();";
        os << "int stratum = (rank == 0) ? " << std::numeric_limits<int>::max() << " : rank - 1;";
        os << "obj.runAll(opt.getInputFileDir(), opt.getOutputFileDir(), stratum);\n";
        os << "souffle::mpi::finalize();";
        os << "\n#endif\n";
    } else
#endif
    {
        os << "obj.runAll(opt.getInputFileDir(), opt.getOutputFileDir(), opt.getStratumIndex());\n";
    }

    if (Global::config().get("provenance") == "explain") {
        os << "explain(obj, false);\n";
    } else if (Global::config().get("provenance") == "explore") {
        os << "explain(obj, true);\n";
    }
    os << "return 0;\n";
    os << "} catch(std::exception &e) { souffle::SignalHandler::instance()->error(e.what());}\n";
    os << "}\n";
    os << "\n#endif\n";
}

}  // end of namespace souffle
