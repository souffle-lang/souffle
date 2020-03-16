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
#include "AggregateOp.h"
#include "BinaryConstraintOps.h"
#include "FunctorOps.h"
#include "Global.h"
#include "RWOperation.h"
#include "RamCondition.h"
#include "RamExpression.h"
#include "RamIndexAnalysis.h"
#include "RamNode.h"
#include "RamOperation.h"
#include "RamProgram.h"
#include "RamRelation.h"
#include "RamTranslationUnit.h"
#include "RamTypes.h"
#include "RamUtils.h"
#include "RamVisitor.h"
#include "RelationTag.h"
#include "SymbolTable.h"
#include "SynthesiserRelation.h"
#include "Util.h"
#include "json11.h"
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

using json11::Json;

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
    std::replace(modifiedTxt.begin(), modifiedTxt.end(), '-', '.');
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
        if ((isalnum(name.at(i)) != 0) || name.at(i) == '_') {
            break;
        }
    }
    std::string id;
    for (auto ch : std::to_string(identifiers.size() + 1) + '_' + name.substr(i)) {
        // alphanumeric characters are allowed
        if (isalnum(ch) != 0) {
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

/** Get relation name */
const std::string Synthesiser::getRelationName(const RamRelation& rel) {
    return "rel_" + convertRamIdent(rel.getName());
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
        if ((key % 2) != 0u) {
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
        if (auto scan = dynamic_cast<const RamRelationOperation*>(&node)) {
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
        RamIndexAnalysis* const isa = synthesiser.getTranslationUnit().getAnalysis<RamIndexAnalysis>();

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

        // used to populate tuple literal init expressions
        std::function<void(std::ostream&, const RamExpression*)> rec;
        std::function<void(std::ostream&, const RamExpression*)> recWithDefault;

        std::ostringstream preamble;
        bool preambleIssued = false;

    public:
        CodeEmitter(Synthesiser& syn) : synthesiser(syn) {
            rec = [&](auto& out, const auto* value) {
                out << "ramBitCast(";
                visit(*value, out);
                out << ")";
            };
            recWithDefault = [&](auto& out, const auto* value) {
                if (!isRamUndefValue(&*value)) {
                    rec(out, value);
                } else {
                    out << "0";
                }
            };
        }

        // -- relation statements --

        void visitIO(const RamIO& io, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);

            // print directives as C++ initializers
            auto printDirectives = [&](const std::map<std::string, std::string>& registry) {
                auto cur = registry.begin();
                if (cur == registry.end()) {
                    return;
                }
                out << "{{\"" << cur->first << "\",\"" << escape(cur->second) << "\"}";
                ++cur;
                for (; cur != registry.end(); ++cur) {
                    out << ",{\"" << cur->first << "\",\"" << escape(cur->second) << "\"}";
                }
                out << '}';
            };

            const auto& directives = io.getDirectives();
            const std::string& op = io.get("operation");
            out << "if (performIO) {\n";

            // get some table details
            if (op == "input") {
                out << "try {";
                out << "std::map<std::string, std::string> directiveMap(";
                printDirectives(directives);
                out << ");\n";
                out << R"_(if (!inputDirectory.empty() && directiveMap["IO"] == "file" && )_";
                out << "directiveMap[\"filename\"].front() != '/') {";
                out << R"_(directiveMap["filename"] = inputDirectory + "/" + directiveMap["filename"];)_";
                out << "}\n";
                out << "RWOperation rwOperation(directiveMap);\n";
                out << "IOSystem::getInstance().getReader(";
                out << "rwOperation, symTable, recordTable";
                out << ")->readAll(*" << synthesiser.getRelationName(io.getRelation());
                out << ");\n";
                out << "} catch (std::exception& e) {std::cerr << \"Error loading data: \" << e.what() "
                       "<< "
                       "'\\n';}\n";
            } else if (op == "output" || op == "printsize") {
                out << "try {";
                out << "std::map<std::string, std::string> directiveMap(";
                printDirectives(directives);
                out << ");\n";
                out << R"_(if (!outputDirectory.empty() && directiveMap["IO"] == "file" && )_";
                out << "directiveMap[\"filename\"].front() != '/') {";
                out << R"_(directiveMap["filename"] = outputDirectory + "/" + directiveMap["filename"];)_";
                out << "}\n";
                out << "RWOperation rwOperation(directiveMap);\n";
                out << "IOSystem::getInstance().getWriter(";
                out << "rwOperation, symTable, recordTable";
                out << ")->writeAll(*" << synthesiser.getRelationName(io.getRelation()) << ");\n";
                out << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
            } else {
                assert("Wrong i/o operation");
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
                    visitDepthFirst(*cur, [&](const RamExistenceCheck&) { needContext = true; });
                    if (needContext) {
                        requireCtx.push_back(std::unique_ptr<RamCondition>(cur->clone()));
                    } else {
                        freeOfCtx.push_back(std::unique_ptr<RamCondition>(cur->clone()));
                    }
                }
                // discharge conditions that do not require a context
                if (freeOfCtx.size() > 0) {
                    out << "if(";
                    visit(*toCondition(freeOfCtx), out);
                    out << ") {\n";
                }
            }

            // outline each search operation to improve compilation time
            out << "[&]()";
            // enclose operation in its own scope
            out << "{\n";

            // check whether loop nest can be parallelized
            bool isParallel = false;
            visitDepthFirst(*next, [&](const RamAbstractParallel&) { isParallel = true; });

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
                    visit(*toCondition(requireCtx), preamble);
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
                    visit(*toCondition(requireCtx), out);
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
            out << "();";  // call lambda

            if (freeOfCtx.size() > 0) {
                out << "}\n";
            }

            PRINT_END_COMMENT(out);
        }

        void visitClear(const RamClear& clear, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);

            out << "if (!isHintsProfilingEnabled()"
                << (clear.getRelation().isTemp() ? ") " : "&& performIO) ");
            out << synthesiser.getRelationName(clear.getRelation()) << "->"
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

        void visitExtend(const RamExtend& extend, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << synthesiser.getRelationName(extend.getSourceRelation()) << "->"
                << "extend("
                << "*" << synthesiser.getRelationName(extend.getTargetRelation()) << ");\n";
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

        // -- operations --

        void visitNestedOperation(const RamNestedOperation& nested, std::ostream& out) override {
            visit(nested.getOperation(), out);
            if (Global::config().has("profile") && !nested.getProfileText().empty()) {
                out << "freqs[" << synthesiser.lookupFreqIdx(nested.getProfileText()) << "]++;\n";
            }
        }

        void visitTupleOperation(const RamTupleOperation& search, std::ostream& out) override {
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

            visitTupleOperation(pscan, out);

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

            visitTupleOperation(scan, out);

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

            visitTupleOperation(choice, out);

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

            visitTupleOperation(pchoice, out);

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

            out << "const Tuple<RamDomain," << arity << "> key{{";
            out << join(rangePattern.begin(), rangePattern.begin() + arity, ",", recWithDefault);
            out << "}};\n";

            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(rel) + ")";

            out << "auto range = " << relName << "->"
                << "equalRange_" << keys << "(key," << ctxName << ");\n";
            out << "for(const auto& env" << identifier << " : range) {\n";

            visitTupleOperation(iscan, out);

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

            out << "const Tuple<RamDomain," << arity << "> key{{";
            out << join(rangePattern.begin(), rangePattern.begin() + arity, ",", recWithDefault);
            out << "}};\n";
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

            visitTupleOperation(piscan, out);

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

            out << "const Tuple<RamDomain," << arity << "> key{{";
            out << join(rangePattern.begin(), rangePattern.begin() + arity, ",", recWithDefault);
            out << "}};\n";

            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(rel) + ")";

            out << "auto range = " << relName << "->"
                << "equalRange_" << keys << "(key," << ctxName << ");\n";
            out << "for(const auto& env" << identifier << " : range) {\n";
            out << "if( ";

            visit(ichoice.getCondition(), out);

            out << ") {\n";

            visitTupleOperation(ichoice, out);

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

            out << "const Tuple<RamDomain," << arity << "> key{{";
            out << join(rangePattern.begin(), rangePattern.begin() + arity, ",", recWithDefault);
            out << "}};\n";
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

            visitTupleOperation(pichoice, out);

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
            out << "RamDomain const ref = ";
            visit(lookup.getExpression(), out);
            out << ";\n";

            // Handle nil case.
            out << "if (recordTable.isNil(ref)) continue;\n";

            // Unpack tuple
            out << tuple_type << " "
                << "env" << lookup.getTupleId() << " = "
                << "recordTable.unpackTuple<RamDomain, " << arity << ">"
                << "(ref);"
                << "\n";

            out << "{\n";

            // continue with condition checks and nested body
            visitTupleOperation(lookup, out);

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
            if (aggregate.getFunction() == AggregateOp::count && keys == 0 &&
                    isRamTrue(&aggregate.getCondition())) {
                // shortcut: use relation size
                out << "env" << identifier << "[0] = " << relName << "->"
                    << "size();\n";
                visitTupleOperation(aggregate, out);
                PRINT_END_COMMENT(out);
                return;
            }

            // init result
            std::string init;
            switch (aggregate.getFunction()) {
                case AggregateOp::min:
                    init = "MAX_RAM_DOMAIN";
                    break;
                case AggregateOp::max:
                    init = "MIN_RAM_DOMAIN";
                    break;
                case AggregateOp::count:
                    init = "0";
                    break;
                case AggregateOp::sum:
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
                const auto& patterns = aggregate.getRangePattern();
                out << "const " << tuple_type << " key{{";
                out << join(patterns.begin(), patterns.begin() + arity, ",", recWithDefault);
                out << "}};\n";
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
                case AggregateOp::min:
                    out << "res" << identifier << " = std::min (res" << identifier << ",";
                    visit(aggregate.getExpression(), out);
                    out << ");\n";
                    break;
                case AggregateOp::max:
                    out << "res" << identifier << " = std::max (res" << identifier << ",";
                    visit(aggregate.getExpression(), out);
                    out << ");\n";
                    break;
                case AggregateOp::count:
                    // count is easy
                    out << "++res" << identifier << "\n;";
                    break;
                case AggregateOp::sum:
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

            if (aggregate.getFunction() == AggregateOp::min || aggregate.getFunction() == AggregateOp::max) {
                // check whether there exists a min/max first before next loop
                out << "if(res" << identifier << " != " << init << "){\n";
                visitTupleOperation(aggregate, out);
                out << "}\n";
            } else {
                visitTupleOperation(aggregate, out);
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
            if (aggregate.getFunction() == AggregateOp::count && isRamTrue(&aggregate.getCondition())) {
                // shortcut: use relation size
                out << "env" << identifier << "[0] = " << relName << "->"
                    << "size();\n";
                visitTupleOperation(aggregate, out);
                PRINT_END_COMMENT(out);
                return;
            }

            // init result
            std::string init;
            switch (aggregate.getFunction()) {
                case AggregateOp::min:
                    init = "MAX_RAM_DOMAIN";
                    break;
                case AggregateOp::max:
                    init = "MIN_RAM_DOMAIN";
                    break;
                case AggregateOp::count:
                    init = "0";
                    break;
                case AggregateOp::sum:
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
                case AggregateOp::min:
                    out << "res" << identifier << " = std::min(res" << identifier << ",";
                    visit(aggregate.getExpression(), out);
                    out << ");\n";
                    break;
                case AggregateOp::max:
                    out << "res" << identifier << " = std::max(res" << identifier << ",";
                    visit(aggregate.getExpression(), out);
                    out << ");\n";
                    break;
                case AggregateOp::count:
                    out << "++res" << identifier << "\n;";
                    break;
                case AggregateOp::sum:
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

            if (aggregate.getFunction() == AggregateOp::min || aggregate.getFunction() == AggregateOp::max) {
                // check whether there exists a min/max first before next loop
                out << "if(res" << identifier << " != " << init << "){\n";
                visitTupleOperation(aggregate, out);
                out << "}\n";
            } else {
                visitTupleOperation(aggregate, out);
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
            out << "Tuple<RamDomain," << arity << "> tuple{{" << join(project.getValues(), ",", rec)
                << "}};\n";

            // insert tuple
            out << relName << "->"
                << "insert(tuple," << ctxName << ");\n";

            PRINT_END_COMMENT(out);
        }

        // -- conditions --

        void visitTrue(const RamTrue&, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "true";
            PRINT_END_COMMENT(out);
        }

        void visitFalse(const RamFalse&, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "false";
            PRINT_END_COMMENT(out);
        }

        void visitConjunction(const RamConjunction& conj, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            visit(conj.getLHS(), out);
            out << " && ";
            visit(conj.getRHS(), out);
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
                case BinaryConstraintOp::ULT:
                case BinaryConstraintOp::FLT:
                case BinaryConstraintOp::LT:
                    out << "((";
                    visit(rel.getLHS(), out);
                    out << ") < (";
                    visit(rel.getRHS(), out);
                    out << "))";
                    break;
                case BinaryConstraintOp::ULE:
                case BinaryConstraintOp::FLE:
                case BinaryConstraintOp::LE:
                    out << "((";
                    visit(rel.getLHS(), out);
                    out << ") <= (";
                    visit(rel.getRHS(), out);
                    out << "))";
                    break;
                case BinaryConstraintOp::UGT:
                case BinaryConstraintOp::FGT:
                case BinaryConstraintOp::GT:
                    out << "((";
                    visit(rel.getLHS(), out);
                    out << ") > (";
                    visit(rel.getRHS(), out);
                    out << "))";
                    break;
                case BinaryConstraintOp::UGE:
                case BinaryConstraintOp::FGE:
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
            std::string after;
            if (Global::config().has("profile") && !exists.getRelation().isTemp()) {
                out << R"_((reads[)_" << synthesiser.lookupReadIdx(rel.getName()) << R"_(]++,)_";
                after = ")";
            }

            // if it is total we use the contains function
            if (isa->isTotalSignature(&exists)) {
                out << relName << "->"
                    << "contains(Tuple<RamDomain," << arity << ">{{" << join(exists.getValues(), ",", rec)
                    << "}}," << ctxName << ")" << after;
                PRINT_END_COMMENT(out);
                return;
            }

            // else we conduct a range query
            out << "!" << relName << "->"
                << "equalRange";
            out << "_" << isa->getSearchSignature(&exists);
            out << "(Tuple<RamDomain," << arity << ">{{";
            out << join(exists.getValues(), ",", recWithDefault);
            out << "}}," << ctxName << ").empty()" << after;
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
            auto auxiliaryArity = rel.getAuxiliaryArity();

            // provenance not exists is never total, conduct a range query
            out << "[&]() -> bool {\n";
            out << "auto existenceCheck = " << relName << "->"
                << "equalRange";
            // out << synthesiser.toIndex(ne.getSearchSignature());
            out << "_" << isa->getSearchSignature(&provExists);
            out << "(Tuple<RamDomain," << arity << ">{{";
            auto parts = provExists.getValues().size() - auxiliaryArity + 1;
            out << join(provExists.getValues().begin(), provExists.getValues().begin() + parts, ",",
                    recWithDefault);
            // extra 0 for provenance height annotations
            for (size_t i = 0; i < auxiliaryArity - 2; i++) {
                out << "0,";
            }
            out << "0";

            out << "}}," << ctxName << ");\n";
            out << "if (existenceCheck.empty()) return false; else return ((*existenceCheck.begin())["
                << arity - auxiliaryArity + 1 << "] <= ";

            visit(*(provExists.getValues()[arity - auxiliaryArity + 1]), out);
            out << ")";
            if (auxiliaryArity > 2) {
                out << " &&  !("
                    << "(*existenceCheck.begin())[" << arity - auxiliaryArity + 1 << "] == ";
                visit(*(provExists.getValues()[arity - auxiliaryArity + 1]), out);

                // out << ")";}
                out << " && (";

                out << "(*existenceCheck.begin())[" << arity - auxiliaryArity + 2 << "] > ";
                visit(*(provExists.getValues()[arity - auxiliaryArity + 2]), out);
                // out << "))";}
                for (int i = arity - auxiliaryArity + 3; i < (int)arity; i++) {
                    out << " || (";
                    for (int j = arity - auxiliaryArity + 2; j < i; j++) {
                        out << "(*existenceCheck.begin())[" << j << "] == ";
                        visit(*(provExists.getValues()[j]), out);
                        out << " && ";
                    }
                    out << "(*existenceCheck.begin())[" << i << "] > ";
                    visit(*(provExists.getValues()[i]), out);
                    out << ")";
                }

                out << "))";
            }
            out << ";}()\n";
            PRINT_END_COMMENT(out);
        }

        // -- values --
        void visitUnsignedConstant(const RamUnsignedConstant& constant, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "RamUnsigned(" << constant.getValue() << ")";
            PRINT_END_COMMENT(out);
        }

        void visitFloatConstant(const RamFloatConstant& constant, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "RamFloat(" << constant.getValue() << ")";
            PRINT_END_COMMENT(out);
        }

        void visitSignedConstant(const RamSignedConstant& constant, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "RamSigned(" << constant.getConstant() << ")";
            PRINT_END_COMMENT(out);
        }

        void visitTupleElement(const RamTupleElement& access, std::ostream& out) override {
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

// clang-format off
#define UNARY_OP(opcode, ty, op)                \
    case FunctorOp::opcode: {                   \
        out << "(" #op "(ramBitCast<" #ty ">("; \
        visit(args[0], out);                    \
        out << ")))";                           \
        break;                                  \
    }
#define UNARY_OP_I(opcode, op) UNARY_OP(   opcode, RamSigned  , op)
#define UNARY_OP_U(opcode, op) UNARY_OP(U##opcode, RamUnsigned, op)
#define UNARY_OP_F(opcode, op) UNARY_OP(F##opcode, RamFloat   , op)
#define UNARY_OP_INTEGRAL(opcode, op) \
    UNARY_OP_I(opcode, op)            \
    UNARY_OP_U(opcode, op)


#define BINARY_OP_EXPR_EX(ty, op, rhs_post)      \
    {                                            \
        out << "(ramBitCast<" #ty ">(";          \
        visit(args[0], out);                     \
        out << ") " #op " ramBitCast<" #ty ">("; \
        visit(args[1], out);                     \
        out << rhs_post "))";                    \
        break;                                   \
    }
#define BINARY_OP_EXPR(ty, op) BINARY_OP_EXPR_EX(ty, op, "")
#define BINARY_OP_EXPR_SHIFT(ty, op) BINARY_OP_EXPR_EX(ty, op, " & RAM_BIT_SHIFT_MASK")
#define BINARY_OP_EXPR_LOGICAL(ty, op) out << "RamDomain"; BINARY_OP_EXPR(ty, op)

#define BINARY_OP_INTEGRAL(opcode, op)                         \
    case FunctorOp::   opcode: BINARY_OP_EXPR(RamSigned  , op) \
    case FunctorOp::U##opcode: BINARY_OP_EXPR(RamUnsigned, op)
#define BINARY_OP_LOGICAL(opcode, op)                                  \
    case FunctorOp::   opcode: BINARY_OP_EXPR_LOGICAL(RamSigned  , op) \
    case FunctorOp::U##opcode: BINARY_OP_EXPR_LOGICAL(RamUnsigned, op)
#define BINARY_OP_NUMERIC(opcode, op)                          \
    BINARY_OP_INTEGRAL(opcode, op)                             \
    case FunctorOp::F##opcode: BINARY_OP_EXPR(RamFloat   , op)
#define BINARY_OP_BITWISE(opcode, op)                        \
    case FunctorOp::   opcode: /* fall through */            \
    case FunctorOp::U##opcode: BINARY_OP_EXPR(RamDomain, op)
#define BINARY_OP_INTEGRAL_SHIFT(opcode, op, tySigned, tyUnsigned)  \
    case FunctorOp::   opcode: BINARY_OP_EXPR_SHIFT(tySigned  , op) \
    case FunctorOp::U##opcode: BINARY_OP_EXPR_SHIFT(tyUnsigned, op)

#define BINARY_OP_EXP(opcode, ty, tyTemp)                                                     \
    case FunctorOp::opcode: {                                                                 \
        out << "static_cast<" #ty ">(static_cast<" #tyTemp ">(std::pow(ramBitCast<" #ty ">("; \
        visit(args[0], out);                                                                  \
        out << "), ramBitCast<" #ty ">(";                                                     \
        visit(args[1], out);                                                                  \
        out << "))))";                                                                        \
        break;                                                                                \
    }

#define NARY_OP(opcode, ty, op)            \
    case FunctorOp::opcode: {              \
        out << #op "({";                   \
        for (auto& cur : args) {           \
            out << "ramBitCast<" #ty ">("; \
            visit(cur, out);               \
            out << "), ";                  \
        }                                  \
        out << "})";                       \
        break;                             \
    }
#define NARY_OP_ORDERED(opcode, op)     \
    NARY_OP(   opcode, RamSigned  , op) \
    NARY_OP(U##opcode, RamUnsigned, op) \
    NARY_OP(F##opcode, RamFloat   , op)
            // clang-format on

            auto args = op.getArguments();
            switch (op.getOperator()) {
                /** Unary Functor Operators */
                case FunctorOp::ORD: {
                    visit(args[0], out);
                    break;
                }
                // TODO: change the signature of `STRLEN` to return an unsigned?
                case FunctorOp::STRLEN: {
                    out << "static_cast<RamSigned>(symTable.resolve(";
                    visit(args[0], out);
                    out << ").size())";
                    break;
                }
                case FunctorOp::TOSTRING: {
                    out << "symTable.lookup(std::to_string(";
                    visit(args[0], out);
                    out << "))";
                    break;
                }
                case FunctorOp::TONUMBER: {
                    out << "(wrapper_tonumber(symTable.resolve((size_t)";
                    visit(args[0], out);
                    out << ")))";
                    break;
                }

                    // clang-format off
                UNARY_OP_I(NEG, -)
                UNARY_OP_F(NEG, -)

                UNARY_OP_INTEGRAL(BNOT, ~)
                UNARY_OP_INTEGRAL(LNOT, (RamDomain)!)

                // Numeric coercions.
                // Behaviour is similar to C++ except we saturate instead of overflowing.
                UNARY_OP(FTOI, RamFloat   , static_cast<RamSigned>)
                UNARY_OP(UTOI, RamUnsigned, static_cast<RamSigned>)
                UNARY_OP(FTOU, RamFloat   , static_cast<RamUnsigned>)
                UNARY_OP(ITOU, RamSigned  , static_cast<RamUnsigned>)
                UNARY_OP(ITOF, RamSigned  , static_cast<RamFloat>)
                UNARY_OP(UTOF, RamUnsigned, static_cast<RamFloat>)

                /** Binary Functor Operators */
                // arithmetic

                BINARY_OP_NUMERIC(ADD, +)
                BINARY_OP_NUMERIC(SUB, -)
                BINARY_OP_NUMERIC(MUL, *)
                BINARY_OP_NUMERIC(DIV, /)
                BINARY_OP_INTEGRAL(MOD, %)

                BINARY_OP_EXP(FEXP, RamFloat   , RamFloat)
#if RAM_DOMAIN_SIZE == 32
                BINARY_OP_EXP(UEXP, RamUnsigned, int64_t)
                BINARY_OP_EXP( EXP, RamSigned  , int64_t)
#elif RAM_DOMAIN_SIZE == 64
                BINARY_OP_EXP(UEXP, RamUnsigned, RamUnsigned)
                BINARY_OP_EXP( EXP, RamSigned  , RamSigned)
#else
#error "unhandled domain size"
#endif

                BINARY_OP_LOGICAL(LAND, &&)
                BINARY_OP_LOGICAL(LOR , ||)

                BINARY_OP_BITWISE(BAND, &)
                BINARY_OP_BITWISE(BOR , |)
                BINARY_OP_BITWISE(BXOR, ^)
                // Handle left-shift as unsigned to match Java semantics of `<<`, namely:
                //  "... `n << s` is `n` left-shifted `s` bit positions; ..."
                // Using `RamSigned` would imply UB due to signed overflow when shifting negatives.
                BINARY_OP_INTEGRAL_SHIFT(BSHIFT_L         , <<, RamUnsigned, RamUnsigned)
                // For right-shift, we do need sign extension.
                BINARY_OP_INTEGRAL_SHIFT(BSHIFT_R         , >>, RamSigned  , RamUnsigned)
                BINARY_OP_INTEGRAL_SHIFT(BSHIFT_R_UNSIGNED, >>, RamUnsigned, RamUnsigned)

                NARY_OP_ORDERED(MAX, std::max)
                NARY_OP_ORDERED(MIN, std::min)
                    // clang-format on

                // strings
                case FunctorOp::CAT: {
                    out << "symTable.lookup(";
                    size_t i = 0;
                    while (i < args.size() - 1) {
                        out << "symTable.resolve(";
                        visit(args[i], out);
                        out << ") + ";
                        i++;
                    }
                    out << "symTable.resolve(";
                    visit(args[i], out);
                    out << "))";
                    break;
                }

                /** Ternary Functor Operators */
                case FunctorOp::SUBSTR: {
                    out << "symTable.lookup(";
                    out << "substr_wrapper(symTable.resolve(";
                    visit(args[0], out);
                    out << "),(";
                    visit(args[1], out);
                    out << "),(";
                    visit(args[2], out);
                    out << ")))";
                    break;
                }
            }
            PRINT_END_COMMENT(out);
        }

        void visitUserDefinedOperator(const RamUserDefinedOperator& op, std::ostream& out) override {
            const std::string& name = op.getName();

            const std::vector<TypeAttribute>& argTypes = op.getArgsTypes();
            auto args = op.getArguments();

            if (op.getReturnType() == TypeAttribute::Symbol) {
                out << "symTable.lookup(";
            }
            out << name << "(";

            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) {
                    out << ",";
                }
                switch (argTypes[i]) {
                    case TypeAttribute::Signed:
                        out << "((RamSigned)";
                        visit(args[i], out);
                        out << ")";
                        break;
                    case TypeAttribute::Unsigned:
                        out << "((RamUnsigned)";
                        visit(args[i], out);
                        out << ")";
                        break;
                    case TypeAttribute::Float:
                        out << "((RamFloat)";
                        visit(args[i], out);
                        out << ")";
                        break;
                    case TypeAttribute::Symbol:
                        out << "symTable.resolve(";
                        visit(args[i], out);
                        out << ").c_str()";
                        break;
                    case TypeAttribute::Record:
                        assert(false);
                }
            }
            out << ")";
            if (op.getReturnType() == TypeAttribute::Symbol) {
                out << ")";
            }
        }

        // -- records --

        void visitPackRecord(const RamPackRecord& pack, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);

            out << "recordTable.pack("
                << "ram::Tuple<RamDomain," << pack.getArguments().size() << ">";
            if (pack.getArguments().size() == 0) {
                out << "{{}}";
            } else {
                out << "{{ramBitCast(" << join(pack.getArguments(), "),ramBitCast(", rec) << ")}}\n";
            }
            out << ")";

            PRINT_END_COMMENT(out);
        }

        // -- subroutine argument --

        void visitSubroutineArgument(const RamSubroutineArgument& arg, std::ostream& out) override {
            out << "(args)[" << arg.getArgument() << "]";
        }

        // -- subroutine return --

        void visitSubroutineReturnValue(const RamSubroutineReturnValue& ret, std::ostream& out) override {
            out << "std::lock_guard<std::mutex> guard(lock);\n";
            for (auto val : ret.getValues()) {
                if (isRamUndefValue(val)) {
                    out << "ret.push_back(0);\n";
                } else {
                    out << "ret.push_back(";
                    visit(val, out);
                    out << ");\n";
                }
            }
        }

        // -- safety net --

        void visitUndefValue(const RamUndefValue&, std::ostream& /*out*/) override {
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
    const RamProgram& prog = translationUnit.getProgram();
    auto* idxAnalysis = translationUnit.getAnalysis<RamIndexAnalysis>();

    // ---------------------------------------------------------------
    //                      Code Generation
    // ---------------------------------------------------------------

    withSharedLibrary = false;

    std::string classname = "Sf_" + id;

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
    std::map<std::string, std::pair<TypeAttribute, std::vector<TypeAttribute>>> functors;
    visitDepthFirst(prog, [&](const RamUserDefinedOperator& op) {
        if (functors.find(op.getName()) == functors.end()) {
            functors[op.getName()] = std::make_pair(op.getReturnType(), op.getArgsTypes());
        }
        withSharedLibrary = true;
    });
    os << "extern \"C\" {\n";
    for (const auto& f : functors) {
        //        size_t arity = f.second.length() - 1;
        const std::string& name = f.first;

        const auto& functorTypes = f.second;
        const auto& returnType = functorTypes.first;
        const auto& argsTypes = functorTypes.second;

        switch (returnType) {
            case TypeAttribute::Signed:
                os << "souffle::RamSigned ";
                break;
            case TypeAttribute::Unsigned:
                os << "souffle::RamUnsigned ";
                break;
            case TypeAttribute::Float:
                os << "souffle::RamFloat ";
                break;
            case TypeAttribute::Symbol:
                os << "const char * ";
                break;
            case TypeAttribute::Record:
                abort();
        }

        os << name << "(";
        std::vector<std::string> args;
        for (const TypeAttribute typeAttribute : argsTypes) {
            switch (typeAttribute) {
                case TypeAttribute::Signed:
                    args.push_back("souffle::RamSigned");
                    break;
                case TypeAttribute::Unsigned:
                    args.push_back("souffle::RamUnsigned");
                    break;
                case TypeAttribute::Float:
                    args.push_back("souffle::RamFloat");
                    break;
                case TypeAttribute::Symbol:
                    args.push_back("const char *");
                    break;
                case TypeAttribute::Record:
                    abort();
            }
        }
        os << join(args, ",");
        os << ");\n";
    }
    os << "}\n";
    os << "\n";
    os << "namespace souffle {\n";
    os << "using namespace ram;\n";
    os << "static const RamDomain RAM_BIT_SHIFT_MASK = RAM_DOMAIN_SIZE - 1;\n";

    // synthesise data-structures for relations
    for (auto rel : prog.getRelations()) {
        bool isProvInfo = rel->getRepresentation() == RelationRepresentation::INFO;
        auto relationType = SynthesiserRelation::getSynthesiserRelation(
                *rel, idxAnalysis->getIndexes(*rel), Global::config().has("provenance") && !isProvInfo);

        generateRelationTypeStruct(os, std::move(relationType));
    }
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
    os << "   try { result = RamSignedFromString(str); } catch(...) { \n";
    os << "     std::cerr << \"error: wrong string provided by to_number(\\\"\";\n";
    os << R"(     std::cerr << str << "\") )";
    os << "functor.\\n\";\n";
    os << "     raise(SIGFPE);\n";
    os << "   } return result;\n";
    os << "}\n";

    if (Global::config().has("profile")) {
        os << "std::string profiling_fname;\n";
    }

    os << "public:\n";

    // declare symbol table
    os << "// -- initialize symbol table --\n";

    os << "SymbolTable symTable\n";
    if (symTable.size() > 0) {
        os << "{\n";
        for (size_t i = 0; i < symTable.size(); i++) {
            os << "\tR\"_(" << symTable.resolve(i) << ")_\",\n";
        }
        os << "}";
    }
    os << ";";

    // declare record table
    os << "// -- initialize record table --\n";

    os << "RecordTable recordTable;"
       << "\n";

    if (Global::config().has("profile")) {
        os << "private:\n";
        size_t numFreq = 0;
        visitDepthFirst(prog.getMain(), [&](const RamStatement&) { numFreq++; });
        os << "  size_t freqs[" << numFreq << "]{};\n";
        size_t numRead = 0;
        for (auto rel : prog.getRelations()) {
            if (!rel->isTemp()) {
                numRead++;
            }
        }
        os << "  size_t reads[" << numRead << "]{};\n";
    }

    // print relation definitions
    std::string initCons;     // initialization of constructor
    std::string registerRel;  // registration of relations
    int relCtr = 0;
    std::set<std::string> storeRelations;
    std::set<std::string> loadRelations;
    std::set<const RamIO*> loadIOs;
    std::set<const RamIO*> storeIOs;

    // collect load/store operations/relations
    visitDepthFirst(prog.getMain(), [&](const RamIO& io) {
        auto op = io.get("operation");
        if (op == "input") {
            loadRelations.insert(io.getRelation().getName());
            loadIOs.insert(&io);
        } else if (op == "printsize" || op == "output") {
            storeRelations.insert(io.getRelation().getName());
            storeIOs.insert(&io);
        } else {
            assert("wrong I/O operation");
        }
    });

    for (auto rel : prog.getRelations()) {
        // get some table details
        int arity = rel->getArity();
        int auxiliaryArity = rel->getAuxiliaryArity();
        const std::string& datalogName = rel->getName();
        const std::string& cppName = getRelationName(*rel);

        bool isProvInfo = rel->getRepresentation() == RelationRepresentation::INFO;
        auto relationType = SynthesiserRelation::getSynthesiserRelation(
                *rel, idxAnalysis->getIndexes(*rel), Global::config().has("provenance") && !isProvInfo);
        const std::string& type = relationType->getTypeName();

        // defining table
        os << "// -- Table: " << datalogName << "\n";

        os << "std::unique_ptr<" << type << "> " << cppName << " = std::make_unique<" << type << ">();\n";
        if (!rel->isTemp()) {
            os << "souffle::RelationWrapper<";
            os << relCtr++ << ",";
            os << type << ",";
            os << "Tuple<RamDomain," << arity << ">,";
            os << arity << ",";
            os << auxiliaryArity;
            os << "> wrapper_" << cppName << ";\n";

            // construct types
            std::string tupleType = "std::array<const char *," + std::to_string(arity) + ">{{";
            std::string tupleName = "std::array<const char *," + std::to_string(arity) + ">{{";

            if (rel->getArity() != 0u) {
                const auto& attrib = rel->getAttributeNames();
                const auto& attribType = rel->getAttributeTypes();
                tupleType += "\"" + attribType[0] + "\"";

                for (int i = 1; i < arity; i++) {
                    tupleType += ",\"" + attribType[i] + "\"";
                }
                tupleName += "\"" + attrib[0] + "\"";
                for (int i = 1; i < arity; i++) {
                    tupleName += ",\"" + attrib[i] + "\"";
                }
            }
            tupleType += "}}";
            tupleName += "}}";

            if (!initCons.empty()) {
                initCons += ",\n";
            }
            initCons += "\nwrapper_" + cppName + "(" + "*" + cppName + ",symTable,\"" + datalogName + "\"," +
                        tupleType + "," + tupleName + ")";
            registerRel += "addRelation(\"" + datalogName + "\",&wrapper_" + cppName + ",";
            registerRel += (loadRelations.count(rel->getName()) > 0) ? "true" : "false";
            registerRel += ",";
            registerRel += (storeRelations.count(rel->getName()) > 0) ? "true" : "false";
            registerRel += ");\n";
        }
    }
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
          "std::string outputDirectory = \".\", bool performIO = false) "
          "{\n";

    os << "SignalHandler::instance()->set();\n";
    if (Global::config().has("verbose")) {
        os << "SignalHandler::instance()->enableLogging();\n";
    }
    bool hasIncrement = false;
    visitDepthFirst(prog.getMain(), [&](const RamAutoIncrement&) { hasIncrement = true; });
    // initialize counter
    if (hasIncrement) {
        os << "// -- initialize counter --\n";
        os << "std::atomic<RamDomain> ctr(0);\n\n";
    }
    os << "std::atomic<size_t> iter(0);\n\n";

    // set default threads (in embedded mode)
    // if this is not set, and omp is used, the default omp setting of number of cores is used.
    os << "#if defined(_OPENMP)\n";
    os << "if (getNumThreads() > 0) {omp_set_num_threads(getNumThreads());}\n";
    os << "#endif\n\n";

    // add actual program body
    os << "// -- query evaluation --\n";
    if (Global::config().has("profile")) {
        os << "ProfileEventSingleton::instance().startTimer();\n";
        os << R"_(ProfileEventSingleton::instance().makeTimeEvent("@time;starttime");)_" << '\n';
        os << "{\n"
           << R"_(Logger logger("@runtime;", 0);)_" << '\n';
        // Store count of relations
        size_t relationCount = 0;
        for (auto rel : prog.getRelations()) {
            if (rel->getName()[0] != '@') {
                ++relationCount;
            }
        }
        // Store configuration
        os << R"_(ProfileEventSingleton::instance().makeConfigRecord("relationCount", std::to_string()_"
           << relationCount << "));";
    }

    // emit code
    emitCode(os, prog.getMain());

    if (Global::config().has("profile")) {
        os << "}\n";
        os << "ProfileEventSingleton::instance().stopTimer();\n";
        os << "dumpFreqs();\n";
    }

    // add code printing hint statistics
    os << "\n// -- relation hint statistics --\n";
    os << "if(isHintsProfilingEnabled()) {\n";
    os << "std::cout << \" -- Operation Hint Statistics --\\n\";\n";
    for (auto rel : prog.getRelations()) {
        auto name = getRelationName(*rel);
        os << "std::cout << \"Relation " << name << ":\\n\";\n";
        os << name << "->printHintStatistics(std::cout,\"  \");\n";
        os << "std::cout << \"\\n\";\n";
    }
    os << "}\n";

    os << "SignalHandler::instance()->reset();\n";

    os << "}\n";  // end of runFunction() method

    // add methods to run with and without performing IO (mainly for the interface)
    os << "public:\nvoid run() override { runFunction(\".\", \".\", "
          "false); }\n";
    os << "public:\nvoid runAll(std::string inputDirectory = \".\", std::string outputDirectory = \".\") "
          "override { ";
    if (Global::config().has("live-profile")) {
        os << "std::thread profiler([]() { profile::Tui().runProf(); });\n";
    }
    os << "runFunction(inputDirectory, outputDirectory, true);\n";
    if (Global::config().has("live-profile")) {
        os << "if (profiler.joinable()) { profiler.join(); }\n";
    }
    os << "}\n";
    // issue printAll method
    os << "public:\n";
    os << "void printAll(std::string outputDirectory = \".\") override {\n";

    // print directives as C++ initializers
    auto printDirectives = [&](const std::map<std::string, std::string>& registry) {
        auto cur = registry.begin();
        if (cur == registry.end()) {
            return;
        }
        os << "{{\"" << cur->first << "\",\"" << escape(cur->second) << "\"}";
        ++cur;
        for (; cur != registry.end(); ++cur) {
            os << ",{\"" << cur->first << "\",\"" << escape(cur->second) << "\"}";
        }
        os << '}';
    };

    for (auto store : storeIOs) {
        auto const& directive = store->getDirectives();
        os << "try {";
        os << "std::map<std::string, std::string> directiveMap(";
        printDirectives(directive);
        os << ");\n";
        os << R"_(if (!outputDirectory.empty() && directiveMap["IO"] == "file" && )_";
        os << "directiveMap[\"filename\"].front() != '/') {";
        os << R"_(directiveMap["filename"] = outputDirectory + "/" + directiveMap["filename"];)_";
        os << "}\n";
        os << "RWOperation rwOperation(directiveMap);\n";
        os << "IOSystem::getInstance().getWriter(";
        os << "rwOperation, symTable, recordTable";
        os << ")->writeAll(*" << getRelationName(store->getRelation()) << ");\n";

        os << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
    }
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

    for (auto load : loadIOs) {
        os << "try {";
        os << "std::map<std::string, std::string> directiveMap(";
        printDirectives(load->getDirectives());
        os << ");\n";
        os << R"_(if (!inputDirectory.empty() && directiveMap["IO"] == "file" && )_";
        os << "directiveMap[\"filename\"].front() != '/') {";
        os << R"_(directiveMap["filename"] = inputDirectory + "/" + directiveMap["filename"];)_";
        os << "}\n";
        os << "RWOperation rwOperation(directiveMap);\n";
        os << "IOSystem::getInstance().getReader(";
        os << "rwOperation, symTable, recordTable";
        os << ")->readAll(*" << getRelationName(load->getRelation());
        os << ");\n";
        os << "} catch (std::exception& e) {std::cerr << \"Error loading data: \" << e.what() << "
              "'\\n';}\n";
    }

    os << "}\n";  // end of loadAll() method
    // issue dump methods
    auto dumpRelation = [&](const RamRelation& ramRelation) {
        const auto& relName = getRelationName(ramRelation);
        const auto& name = ramRelation.getName();
        const auto& attributesTypes = ramRelation.getAttributeTypes();

        Json relJson = Json::object{{"arity", static_cast<long long>(attributesTypes.size())},
                {"auxArity", static_cast<long long>(0)},
                {"types", Json::array(attributesTypes.begin(), attributesTypes.end())}};

        Json types = Json::object{{name, relJson}};

        os << "try {";
        os << "RWOperation rwOperation;\n";
        os << "rwOperation.set(\"IO\", \"stdout\");\n";
        os << R"(rwOperation.set("name", ")" << name << "\");\n";
        os << "rwOperation.set(\"types\",";
        os << "\"" << escapeJSONstring(types.dump()) << "\"";
        os << ");\n";
        os << "IOSystem::getInstance().getWriter(";
        os << "rwOperation, symTable, recordTable";
        os << ")->writeAll(*" << relName << ");\n";
        os << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
    };

    // dump inputs
    os << "public:\n";
    os << "void dumpInputs(std::ostream& out = std::cout) override {\n";
    for (auto load : loadIOs) {
        dumpRelation(load->getRelation());
    }
    os << "}\n";  // end of dumpInputs() method

    // dump outputs
    os << "public:\n";
    os << "void dumpOutputs(std::ostream& out = std::cout) override {\n";
    for (auto store : storeIOs) {
        dumpRelation(store->getRelation());
    }
    os << "}\n";  // end of dumpOutputs() method

    os << "public:\n";
    os << "SymbolTable& getSymbolTable() override {\n";
    os << "return symTable;\n";
    os << "}\n";  // end of getSymbolTable() method

    // TODO: generate code for subroutines
    if (Global::config().has("provenance")) {
        if (Global::config().get("provenance") == "subtreeHeights") {
            // method that populates provenance indices
            os << "void copyIndex() {\n";
            for (auto rel : prog.getRelations()) {
                // get some table details
                const std::string& cppName = getRelationName(*rel);

                bool isProvInfo = rel->getRepresentation() == RelationRepresentation::INFO;
                auto relationType = SynthesiserRelation::getSynthesiserRelation(*rel,
                        idxAnalysis->getIndexes(*rel), Global::config().has("provenance") && !isProvInfo);

                if (!relationType->getProvenenceIndexNumbers().empty()) {
                    os << cppName << "->copyIndex();\n";
                }
            }
            os << "}\n";
        }

        // generate subroutine adapter
        os << "void executeSubroutine(std::string name, const std::vector<RamDomain>& args, "
              "std::vector<RamDomain>& ret) override {\n";

        // subroutine number
        size_t subroutineNum = 0;
        for (auto& sub : prog.getSubroutines()) {
            os << "if (name == \"" << sub.first << "\") {\n"
               << "subproof_" << subroutineNum
               << "(args, ret);\n"  // subproof_i to deal with special characters in relation names
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
                  "std::vector<RamDomain>& ret) {\n";

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
    os << std::stoi(Global::config().get("jobs"));
    os << ");\n";

    os << "if (!opt.parse(argc,argv)) return 1;\n";

    os << "souffle::";
    if (Global::config().has("profile")) {
        os << classname + " obj(opt.getProfileName());\n";
    } else {
        os << classname + " obj;\n";
    }

    os << "#if defined(_OPENMP) \n";
    os << "obj.setNumThreads(opt.getNumJobs());\n";
    os << "\n#endif\n";

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
    os << "obj.runAll(opt.getInputFileDir(), opt.getOutputFileDir());\n";

    if (Global::config().get("provenance") == "explain") {
        os << "explain(obj, false, false);\n";
    } else if (Global::config().get("provenance") == "subtreeHeights") {
        os << "obj.copyIndex();\n";
        os << "explain(obj, false, true);\n";
    } else if (Global::config().get("provenance") == "explore") {
        os << "explain(obj, true, false);\n";
    }
    os << "return 0;\n";
    os << "} catch(std::exception &e) { souffle::SignalHandler::instance()->error(e.what());}\n";
    os << "}\n";
    os << "\n#endif\n";
}

}  // end of namespace souffle
