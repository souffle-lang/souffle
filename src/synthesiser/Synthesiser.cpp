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

#include "synthesiser/Synthesiser.h"
#include "AggregateOp.h"
#include "FunctorOps.h"
#include "Global.h"
#include "RelationTag.h"
#include "ram/AbstractParallel.h"
#include "ram/Aggregate.h"
#include "ram/AutoIncrement.h"
#include "ram/Break.h"
#include "ram/Call.h"
#include "ram/Clear.h"
#include "ram/Condition.h"
#include "ram/Conjunction.h"
#include "ram/Constraint.h"
#include "ram/DebugInfo.h"
#include "ram/EmptinessCheck.h"
#include "ram/ExistenceCheck.h"
#include "ram/Exit.h"
#include "ram/Expression.h"
#include "ram/Extend.h"
#include "ram/False.h"
#include "ram/Filter.h"
#include "ram/FloatConstant.h"
#include "ram/IO.h"
#include "ram/IfExists.h"
#include "ram/IndexAggregate.h"
#include "ram/IndexIfExists.h"
#include "ram/IndexScan.h"
#include "ram/Insert.h"
#include "ram/IntrinsicOperator.h"
#include "ram/LogRelationTimer.h"
#include "ram/LogSize.h"
#include "ram/LogTimer.h"
#include "ram/Loop.h"
#include "ram/Negation.h"
#include "ram/NestedIntrinsicOperator.h"
#include "ram/NestedOperation.h"
#include "ram/Node.h"
#include "ram/Operation.h"
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
#include "ram/RelationOperation.h"
#include "ram/RelationSize.h"
#include "ram/Scan.h"
#include "ram/Sequence.h"
#include "ram/SignedConstant.h"
#include "ram/Statement.h"
#include "ram/SubroutineArgument.h"
#include "ram/SubroutineReturn.h"
#include "ram/Swap.h"
#include "ram/TranslationUnit.h"
#include "ram/True.h"
#include "ram/TupleElement.h"
#include "ram/TupleOperation.h"
#include "ram/UndefValue.h"
#include "ram/UnpackRecord.h"
#include "ram/UnsignedConstant.h"
#include "ram/UserDefinedOperator.h"
#include "ram/analysis/Index.h"
#include "ram/utility/Utils.h"
#include "ram/utility/Visitor.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/RamTypes.h"
#include "souffle/TypeAttribute.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/FileUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/StringUtil.h"
#include "souffle/utility/json11.h"
#include "souffle/utility/tinyformat.h"
#include "synthesiser/Relation.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <functional>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <sstream>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace souffle::synthesiser {

using json11::Json;
using ram::analysis::IndexAnalysis;
using namespace ram;
using namespace stream_write_qualified_char_as_number;

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
std::size_t Synthesiser::lookupReadIdx(const std::string& txt) {
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
const std::string Synthesiser::getRelationName(const ram::Relation& rel) {
    return "rel_" + convertRamIdent(rel.getName());
}

const std::string Synthesiser::getRelationName(const ram::Relation* rel) {
    return "rel_" + convertRamIdent(rel->getName());
}

/** Get context name */
const std::string Synthesiser::getOpContextName(const ram::Relation& rel) {
    return getRelationName(rel) + "_op_ctxt";
}

/** Get relation type struct */
void Synthesiser::generateRelationTypeStruct(std::ostream& out, Own<Relation> relationType) {
    // If this type has been generated already, use the cached version
    if (typeCache.find(relationType->getTypeName()) != typeCache.end()) {
        return;
    }
    typeCache.insert(relationType->getTypeName());

    // Generate the type struct for the relation
    relationType->generateTypeStruct(out);
}

/** Get referenced relations */
std::set<const ram::Relation*> Synthesiser::getReferencedRelations(const Operation& op) {
    std::set<const ram::Relation*> res;
    visit(op, [&](const Node& node) {
        if (auto scan = as<RelationOperation>(node)) {
            res.insert(lookup(scan->getRelation()));
        } else if (auto agg = as<Aggregate>(node)) {
            res.insert(lookup(agg->getRelation()));
        } else if (auto exists = as<ExistenceCheck>(node)) {
            res.insert(lookup(exists->getRelation()));
        } else if (auto provExists = as<ProvenanceExistenceCheck>(node)) {
            res.insert(lookup(provExists->getRelation()));
        } else if (auto insert = as<Insert>(node)) {
            res.insert(lookup(insert->getRelation()));
        }
    });
    return res;
}

void Synthesiser::emitCode(std::ostream& out, const Statement& stmt) {
    class CodeEmitter : public ram::Visitor<void, Node const, std::ostream&> {
        using ram::Visitor<void, Node const, std::ostream&>::visit_;

    private:
        Synthesiser& synthesiser;
        IndexAnalysis* const isa = &synthesiser.getTranslationUnit().getAnalysis<IndexAnalysis>();

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
        std::function<void(std::ostream&, const Expression*)> rec;
        std::function<void(std::ostream&, const Expression*)> recWithDefault;

        std::ostringstream preamble;
        bool preambleIssued = false;

    public:
        CodeEmitter(Synthesiser& syn) : synthesiser(syn) {
            rec = [&](auto& out, const auto* value) {
                out << "ramBitCast(";
                dispatch(*value, out);
                out << ")";
            };
            recWithDefault = [&](auto& out, const auto* value) {
                if (!isUndefValue(&*value)) {
                    rec(out, value);
                } else {
                    out << "0";
                }
            };
        }

        std::pair<std::stringstream, std::stringstream> getPaddedRangeBounds(const ram::Relation& rel,
                const std::vector<Expression*>& rangePatternLower,
                const std::vector<Expression*>& rangePatternUpper) {
            std::stringstream low;
            std::stringstream high;

            // making this distinction for provenance
            std::size_t realArity = rel.getArity();
            std::size_t arity = rangePatternLower.size();

            low << "Tuple<RamDomain," << realArity << ">{{";
            high << "Tuple<RamDomain," << realArity << ">{{";

            for (std::size_t column = 0; column < arity; column++) {
                std::string supremum;
                std::string infimum;

                switch (rel.getAttributeTypes()[column][0]) {
                    case 'f':
                        supremum = "ramBitCast<RamDomain>(MIN_RAM_FLOAT)";
                        infimum = "ramBitCast<RamDomain>(MAX_RAM_FLOAT)";
                        break;
                    case 'u':
                        supremum = "ramBitCast<RamDomain>(MIN_RAM_UNSIGNED)";
                        infimum = "ramBitCast<RamDomain>(MAX_RAM_UNSIGNED)";
                        break;
                    default:
                        supremum = "ramBitCast<RamDomain>(MIN_RAM_SIGNED)";
                        infimum = "ramBitCast<RamDomain>(MAX_RAM_SIGNED)";
                }

                // if we have an inequality where either side is not set
                if (column != 0) {
                    low << ", ";
                    high << ", ";
                }

                if (isUndefValue(rangePatternLower[column])) {
                    low << supremum;
                } else {
                    low << "ramBitCast(";
                    dispatch(*rangePatternLower[column], low);
                    low << ")";
                }

                if (isUndefValue(rangePatternUpper[column])) {
                    high << infimum;
                } else {
                    high << "ramBitCast(";
                    dispatch(*rangePatternUpper[column], high);
                    high << ")";
                }
            }

            low << "}}";
            high << "}}";
            return std::make_pair(std::move(low), std::move(high));
        }

        // -- relation statements --

        void visit_(type_identity<IO>, const IO& io, std::ostream& out) override {
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
                out << R"_(if (!inputDirectory.empty()) {)_";
                out << R"_(directiveMap["fact-dir"] = inputDirectory;)_";
                out << "}\n";
                out << "IOSystem::getInstance().getReader(";
                out << "directiveMap, symTable, recordTable";
                out << ")->readAll(*" << synthesiser.getRelationName(synthesiser.lookup(io.getRelation()));
                out << ");\n";
                out << "} catch (std::exception& e) {std::cerr << \"Error loading data: \" << e.what() "
                       "<< "
                       "'\\n';}\n";
            } else if (op == "output" || op == "printsize") {
                out << "try {";
                out << "std::map<std::string, std::string> directiveMap(";
                printDirectives(directives);
                out << ");\n";
                out << R"_(if (!outputDirectory.empty()) {)_";
                out << R"_(directiveMap["output-dir"] = outputDirectory;)_";
                out << "}\n";
                out << "IOSystem::getInstance().getWriter(";
                out << "directiveMap, symTable, recordTable";
                out << ")->writeAll(*" << synthesiser.getRelationName(synthesiser.lookup(io.getRelation()))
                    << ");\n";
                out << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
            } else {
                assert("Wrong i/o operation");
            }
            out << "}\n";
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<Query>, const Query& query, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);

            // split terms of conditions of outer filter operation
            // into terms that require a context and terms that
            // do not require a context
            const Operation* next = &query.getOperation();
            VecOwn<Condition> requireCtx;
            VecOwn<Condition> freeOfCtx;
            if (const auto* filter = as<Filter>(query.getOperation())) {
                next = &filter->getOperation();
                // Check terms of outer filter operation whether they can be pushed before
                // the context-generation for speed imrovements
                auto conditions = toConjunctionList(&filter->getCondition());
                for (auto const& cur : conditions) {
                    bool needContext = false;
                    visit(*cur, [&](const ExistenceCheck&) { needContext = true; });
                    visit(*cur, [&](const ProvenanceExistenceCheck&) { needContext = true; });
                    if (needContext) {
                        requireCtx.push_back(clone(cur));
                    } else {
                        freeOfCtx.push_back(clone(cur));
                    }
                }
                // discharge conditions that do not require a context
                if (freeOfCtx.size() > 0) {
                    out << "if(";
                    dispatch(*toCondition(freeOfCtx), out);
                    out << ") {\n";
                }
            }

            // outline each search operation to improve compilation time
            out << "[&]()";
            // enclose operation in its own scope
            out << "{\n";

            // check whether loop nest can be parallelized
            bool isParallel = visitExists(
                    *next, [&](const Node& n) { return as<AbstractParallel, AllowCrossCast>(n); });

            // reset preamble
            preamble.str("");
            preamble.clear();
            preambleIssued = false;

            // create operation contexts for this operation
            for (const ram::Relation* rel : synthesiser.getReferencedRelations(query.getOperation())) {
                preamble << "CREATE_OP_CONTEXT(" << synthesiser.getOpContextName(*rel);
                preamble << "," << synthesiser.getRelationName(*rel);
                preamble << "->createContext());\n";
            }

            // discharge conditions that require a context
            if (isParallel) {
                if (requireCtx.size() > 0) {
                    preamble << "if(";
                    dispatch(*toCondition(requireCtx), preamble);
                    preamble << ") {\n";
                    dispatch(*next, out);
                    out << "}\n";
                } else {
                    dispatch(*next, out);
                }
            } else {
                out << preamble.str();
                if (requireCtx.size() > 0) {
                    out << "if(";
                    dispatch(*toCondition(requireCtx), out);
                    out << ") {\n";
                    dispatch(*next, out);
                    out << "}\n";
                } else {
                    dispatch(*next, out);
                }
            }

            if (isParallel) {
                out << "PARALLEL_END\n";  // end parallel
            }

            out << "}\n";
            out << "();";  // call lambda

            if (freeOfCtx.size() > 0) {
                out << "}\n";
            }

            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<Clear>, const Clear& clear, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);

            if (!synthesiser.lookup(clear.getRelation())->isTemp()) {
                out << "if (pruneImdtRels) ";
            }
            out << synthesiser.getRelationName(synthesiser.lookup(clear.getRelation())) << "->"
                << "purge();\n";

            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<LogSize>, const LogSize& size, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "ProfileEventSingleton::instance().makeQuantityEvent( R\"(";
            out << size.getMessage() << ")\",";
            out << synthesiser.getRelationName(synthesiser.lookup(size.getRelation())) << "->size(),iter);";
            PRINT_END_COMMENT(out);
        }

        // -- control flow statements --

        void visit_(type_identity<Sequence>, const Sequence& seq, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            for (const auto& cur : seq.getStatements()) {
                dispatch(*cur, out);
            }
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<Parallel>, const Parallel& parallel, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            auto stmts = parallel.getStatements();

            // special handling cases
            if (stmts.empty()) {
                PRINT_END_COMMENT(out);
                return;
            }

            // a single statement => save the overhead
            if (stmts.size() == 1) {
                dispatch(*stmts[0], out);
                PRINT_END_COMMENT(out);
                return;
            }

            // more than one => parallel sections

            // start parallel section
            out << "SECTIONS_START;\n";

            // put each thread in another section
            for (const auto& cur : stmts) {
                out << "SECTION_START;\n";
                dispatch(*cur, out);
                out << "SECTION_END\n";
            }

            // done
            out << "SECTIONS_END;\n";
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<Loop>, const Loop& loop, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "iter = 0;\n";
            out << "for(;;) {\n";
            dispatch(loop.getBody(), out);
            out << "iter++;\n";
            out << "}\n";
            out << "iter = 0;\n";
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<Swap>, const Swap& swap, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            const std::string& deltaKnowledge =
                    synthesiser.getRelationName(synthesiser.lookup(swap.getFirstRelation()));
            const std::string& newKnowledge =
                    synthesiser.getRelationName(synthesiser.lookup(swap.getSecondRelation()));

            out << "std::swap(" << deltaKnowledge << ", " << newKnowledge << ");\n";
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<Extend>, const Extend& extend, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << synthesiser.getRelationName(synthesiser.lookup(extend.getSourceRelation())) << "->"
                << "extend("
                << "*" << synthesiser.getRelationName(synthesiser.lookup(extend.getTargetRelation()))
                << ");\n";
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<Exit>, const Exit& exit, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "if(";
            dispatch(exit.getCondition(), out);
            out << ") break;\n";
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<Call>, const Call& call, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            const Program& prog = synthesiser.getTranslationUnit().getProgram();
            const auto& subs = prog.getSubroutines();
            out << "{\n";
            out << " std::vector<RamDomain> args, ret;\n";
            out << "subroutine_" << distance(subs.begin(), subs.find(call.getName())) << "(args, ret);\n";
            out << "}\n";
            PRINT_END_COMMENT(out);
        }

        void visit_(
                type_identity<LogRelationTimer>, const LogRelationTimer& timer, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            // create local scope for name resolution
            out << "{\n";

            const std::string ext = fileExtension(Global::config().get("profile"));

            const auto* rel = synthesiser.lookup(timer.getRelation());
            auto relName = synthesiser.getRelationName(rel);

            out << "\tLogger logger(R\"_(" << timer.getMessage() << ")_\",iter, [&](){return " << relName
                << "->size();});\n";
            // insert statement to be measured
            dispatch(timer.getStatement(), out);

            // done
            out << "}\n";
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<LogTimer>, const LogTimer& timer, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            // create local scope for name resolution
            out << "{\n";

            const std::string ext = fileExtension(Global::config().get("profile"));

            // create local timer
            out << "\tLogger logger(R\"_(" << timer.getMessage() << ")_\",iter);\n";
            // insert statement to be measured
            dispatch(timer.getStatement(), out);

            // done
            out << "}\n";
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<DebugInfo>, const DebugInfo& dbg, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "signalHandler->setMsg(R\"_(";
            out << dbg.getMessage();
            out << ")_\");\n";

            // insert statements of the rule
            dispatch(dbg.getStatement(), out);
            PRINT_END_COMMENT(out);
        }

        // -- operations --

        void visit_(
                type_identity<NestedOperation>, const NestedOperation& nested, std::ostream& out) override {
            dispatch(nested.getOperation(), out);
            if (Global::config().has("profile") && Global::config().has("profile-frequency") &&
                    !nested.getProfileText().empty()) {
                out << "freqs[" << synthesiser.lookupFreqIdx(nested.getProfileText()) << "]++;\n";
            }
        }

        void visit_(type_identity<TupleOperation>, const TupleOperation& search, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            visit_(type_identity<NestedOperation>(), search, out);
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<ParallelScan>, const ParallelScan& pscan, std::ostream& out) override {
            const auto* rel = synthesiser.lookup(pscan.getRelation());
            const auto& relName = synthesiser.getRelationName(rel);

            assert(pscan.getTupleId() == 0 && "not outer-most loop");

            assert(rel->getArity() > 0 && "AstToRamTranslator failed/no parallel scans for nullaries");

            assert(!preambleIssued && "only first loop can be made parallel");
            preambleIssued = true;

            PRINT_BEGIN_COMMENT(out);

            out << "auto part = " << relName << "->partition();\n";
            out << "PARALLEL_START\n";
            out << preamble.str();
            out << "pfor(auto it = part.begin(); it<part.end();++it){\n";
            out << "try{\n";
            out << "for(const auto& env0 : *it) {\n";

            visit_(type_identity<TupleOperation>(), pscan, out);

            out << "}\n";
            out << "} catch(std::exception &e) { signalHandler->error(e.what());}\n";
            out << "}\n";

            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<Scan>, const Scan& scan, std::ostream& out) override {
            const auto* rel = synthesiser.lookup(scan.getRelation());
            auto relName = synthesiser.getRelationName(rel);
            auto id = scan.getTupleId();

            PRINT_BEGIN_COMMENT(out);

            assert(rel->getArity() > 0 && "AstToRamTranslator failed/no scans for nullaries");

            out << "for(const auto& env" << id << " : "
                << "*" << relName << ") {\n";

            visit_(type_identity<TupleOperation>(), scan, out);

            out << "}\n";

            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<IfExists>, const IfExists& ifexists, std::ostream& out) override {
            const auto* rel = synthesiser.lookup(ifexists.getRelation());
            auto relName = synthesiser.getRelationName(rel);
            auto identifier = ifexists.getTupleId();

            assert(rel->getArity() > 0 && "AstToRamTranslator failed/no ifexists for nullaries");

            PRINT_BEGIN_COMMENT(out);

            out << "for(const auto& env" << identifier << " : "
                << "*" << relName << ") {\n";
            out << "if( ";

            dispatch(ifexists.getCondition(), out);

            out << ") {\n";

            visit_(type_identity<TupleOperation>(), ifexists, out);

            out << "break;\n";
            out << "}\n";
            out << "}\n";

            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<ParallelIfExists>, const ParallelIfExists& pifexists,
                std::ostream& out) override {
            const auto* rel = synthesiser.lookup(pifexists.getRelation());
            auto relName = synthesiser.getRelationName(rel);

            assert(pifexists.getTupleId() == 0 && "not outer-most loop");

            assert(rel->getArity() > 0 && "AstToRamTranslator failed/no parallel ifexists for nullaries");

            assert(!preambleIssued && "only first loop can be made parallel");
            preambleIssued = true;

            PRINT_BEGIN_COMMENT(out);

            out << "auto part = " << relName << "->partition();\n";
            out << "PARALLEL_START\n";
            out << preamble.str();
            out << "pfor(auto it = part.begin(); it<part.end();++it){\n";
            out << "try{\n";
            out << "for(const auto& env0 : *it) {\n";
            out << "if( ";

            dispatch(pifexists.getCondition(), out);

            out << ") {\n";

            visit_(type_identity<TupleOperation>(), pifexists, out);

            out << "break;\n";
            out << "}\n";
            out << "}\n";
            out << "} catch(std::exception &e) { signalHandler->error(e.what());}\n";
            out << "}\n";

            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<IndexScan>, const IndexScan& iscan, std::ostream& out) override {
            const auto* rel = synthesiser.lookup(iscan.getRelation());
            auto relName = synthesiser.getRelationName(rel);
            auto identifier = iscan.getTupleId();
            auto keys = isa->getSearchSignature(&iscan);

            const auto& rangePatternLower = iscan.getRangePattern().first;
            const auto& rangePatternUpper = iscan.getRangePattern().second;

            assert(0 < rel->getArity() && "AstToRamTranslator failed/no index scans for nullaries");

            PRINT_BEGIN_COMMENT(out);
            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(*rel) + ")";
            auto rangeBounds = getPaddedRangeBounds(*rel, rangePatternLower, rangePatternUpper);

            out << "auto range = " << relName << "->"
                << "lowerUpperRange_" << keys << "(" << rangeBounds.first.str() << ","
                << rangeBounds.second.str() << "," << ctxName << ");\n";
            out << "for(const auto& env" << identifier << " : range) {\n";

            visit_(type_identity<TupleOperation>(), iscan, out);

            out << "}\n";
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<ParallelIndexScan>, const ParallelIndexScan& piscan,
                std::ostream& out) override {
            const auto* rel = synthesiser.lookup(piscan.getRelation());
            auto relName = synthesiser.getRelationName(rel);
            auto keys = isa->getSearchSignature(&piscan);

            const auto& rangePatternLower = piscan.getRangePattern().first;
            const auto& rangePatternUpper = piscan.getRangePattern().second;

            assert(piscan.getTupleId() == 0 && "not outer-most loop");

            assert(0 < rel->getArity() && "AstToRamTranslator failed/no parallel index scan for nullaries");

            assert(!preambleIssued && "only first loop can be made parallel");
            preambleIssued = true;

            PRINT_BEGIN_COMMENT(out);
            auto rangeBounds = getPaddedRangeBounds(*rel, rangePatternLower, rangePatternUpper);
            out << "auto range = " << relName
                << "->"
                // TODO (b-scholz): context may be missing here?
                << "lowerUpperRange_" << keys << "(" << rangeBounds.first.str() << ","
                << rangeBounds.second.str() << ");\n";
            out << "auto part = range.partition();\n";
            out << "PARALLEL_START\n";
            out << preamble.str();
            out << "pfor(auto it = part.begin(); it<part.end(); ++it) { \n";
            out << "try{\n";
            out << "for(const auto& env0 : *it) {\n";

            visit_(type_identity<TupleOperation>(), piscan, out);

            out << "}\n";
            out << "} catch(std::exception &e) { signalHandler->error(e.what());}\n";
            out << "}\n";

            PRINT_END_COMMENT(out);
        }

        void visit_(
                type_identity<IndexIfExists>, const IndexIfExists& iifexists, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            const auto* rel = synthesiser.lookup(iifexists.getRelation());
            auto relName = synthesiser.getRelationName(rel);
            auto identifier = iifexists.getTupleId();
            const auto& rangePatternLower = iifexists.getRangePattern().first;
            const auto& rangePatternUpper = iifexists.getRangePattern().second;
            auto keys = isa->getSearchSignature(&iifexists);

            // check list of keys
            assert(0 < rel->getArity() && "AstToRamTranslator failed");
            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(*rel) + ")";
            auto rangeBounds = getPaddedRangeBounds(*rel, rangePatternLower, rangePatternUpper);

            out << "auto range = " << relName << "->"
                << "lowerUpperRange_" << keys << "(" << rangeBounds.first.str() << ","
                << rangeBounds.second.str() << "," << ctxName << ");\n";
            out << "for(const auto& env" << identifier << " : range) {\n";
            out << "if( ";

            dispatch(iifexists.getCondition(), out);

            out << ") {\n";

            visit_(type_identity<TupleOperation>(), iifexists, out);

            out << "break;\n";
            out << "}\n";
            out << "}\n";

            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<ParallelIndexIfExists>, const ParallelIndexIfExists& piifexists,
                std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            const auto* rel = synthesiser.lookup(piifexists.getRelation());
            auto relName = synthesiser.getRelationName(rel);
            const auto& rangePatternLower = piifexists.getRangePattern().first;
            const auto& rangePatternUpper = piifexists.getRangePattern().second;
            auto keys = isa->getSearchSignature(&piifexists);

            assert(piifexists.getTupleId() == 0 && "not outer-most loop");
            assert(0 < rel->getArity() && "AstToRamTranslator failed");
            assert(!preambleIssued && "only first loop can be made parallel");
            preambleIssued = true;

            PRINT_BEGIN_COMMENT(out);
            auto rangeBounds = getPaddedRangeBounds(*rel, rangePatternLower, rangePatternUpper);
            out << "auto range = " << relName
                << "->"
                // TODO (b-scholz): context may be missing here?
                << "lowerUpperRange_" << keys << "(" << rangeBounds.first.str() << ","
                << rangeBounds.second.str() << ");\n";
            out << "auto part = range.partition();\n";
            out << "PARALLEL_START\n";
            out << preamble.str();
            out << "pfor(auto it = part.begin(); it<part.end(); ++it) { \n";
            out << "try{";
            out << "for(const auto& env0 : *it) {\n";
            out << "if( ";

            dispatch(piifexists.getCondition(), out);

            out << ") {\n";

            visit_(type_identity<TupleOperation>(), piifexists, out);

            out << "break;\n";
            out << "}\n";
            out << "}\n";
            out << "} catch(std::exception &e) { signalHandler->error(e.what());}\n";
            out << "}\n";

            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<UnpackRecord>, const UnpackRecord& unpack, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            auto arity = unpack.getArity();

            synthesiser.arities.emplace(arity);

            // look up reference
            out << "RamDomain const ref = ";
            dispatch(unpack.getExpression(), out);
            out << ";\n";

            // Handle nil case.
            out << "if (ref == 0) continue;\n";

            // Unpack tuple
            out << "const RamDomain *"
                << "env" << unpack.getTupleId() << " = "
                << "recordTable.unpack(ref," << arity << ");"
                << "\n";

            out << "{\n";

            // continue with condition checks and nested body
            visit_(type_identity<TupleOperation>(), unpack, out);

            out << "}\n";
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<ParallelIndexAggregate>, const ParallelIndexAggregate& aggregate,
                std::ostream& out) override {
            assert(aggregate.getTupleId() == 0 && "not outer-most loop");
            assert(!preambleIssued && "only first loop can be made parallel");
            preambleIssued = true;
            PRINT_BEGIN_COMMENT(out);
            // get some properties
            const auto* rel = synthesiser.lookup(aggregate.getRelation());
            auto arity = rel->getArity();
            auto relName = synthesiser.getRelationName(rel);
            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(*rel) + ")";
            auto identifier = aggregate.getTupleId();

            // aggregate tuple storing the result of aggregate
            std::string tuple_type = "Tuple<RamDomain," + toString(arity) + ">";

            // declare environment variable
            out << "Tuple<RamDomain,1> env" << identifier << ";\n";

            // get range to aggregate
            auto keys = isa->getSearchSignature(&aggregate);

            // special case: counting number elements over an unrestricted predicate
            if (aggregate.getFunction() == AggregateOp::COUNT && keys.empty() &&
                    isTrue(&aggregate.getCondition())) {
                // shortcut: use relation size
                out << "env" << identifier << "[0] = " << relName << "->"
                    << "size();\n";
                out << "{\n";  // to match PARALLEL_END closing bracket
                out << preamble.str();
                visit_(type_identity<TupleOperation>(), aggregate, out);
                PRINT_END_COMMENT(out);
                return;
            }

            out << "bool shouldRunNested = false;\n";

            // init result and reduction operation
            std::string init;
            switch (aggregate.getFunction()) {
                case AggregateOp::MIN: init = "MAX_RAM_SIGNED"; break;
                case AggregateOp::FMIN: init = "MAX_RAM_FLOAT"; break;
                case AggregateOp::UMIN: init = "MAX_RAM_UNSIGNED"; break;
                case AggregateOp::MAX: init = "MIN_RAM_SIGNED"; break;
                case AggregateOp::FMAX: init = "MIN_RAM_FLOAT"; break;
                case AggregateOp::UMAX: init = "MIN_RAM_UNSIGNED"; break;
                case AggregateOp::COUNT:
                    init = "0";
                    out << "shouldRunNested = true;\n";
                    break;
                case AggregateOp::MEAN: init = "0"; break;
                case AggregateOp::FSUM:
                case AggregateOp::USUM:
                case AggregateOp::SUM:
                    init = "0";
                    out << "shouldRunNested = true;\n";
                    break;
            }

            // Set reduction operation
            std::string op;
            switch (aggregate.getFunction()) {
                case AggregateOp::MIN:
                case AggregateOp::FMIN:
                case AggregateOp::UMIN: {
                    op = "min";
                    break;
                }

                case AggregateOp::MAX:
                case AggregateOp::FMAX:
                case AggregateOp::UMAX: {
                    op = "max";
                    break;
                }

                case AggregateOp::MEAN:
                case AggregateOp::FSUM:
                case AggregateOp::USUM:
                case AggregateOp::COUNT:
                case AggregateOp::SUM: {
                    op = "+";
                    break;
                }
                default: fatal("Unhandled aggregate operation");
            }
            // res0 stores the aggregate result
            std::string sharedVariable = "res0";

            std::string type;
            switch (getTypeAttributeAggregate(aggregate.getFunction())) {
                case TypeAttribute::Signed: type = "RamSigned"; break;
                case TypeAttribute::Unsigned: type = "RamUnsigned"; break;
                case TypeAttribute::Float: type = "RamFloat"; break;

                case TypeAttribute::Symbol:
                case TypeAttribute::ADT:
                case TypeAttribute::Record: type = "RamDomain"; break;
            }
            out << type << " res0 = " << init << ";\n";
            if (aggregate.getFunction() == AggregateOp::MEAN) {
                out << "RamUnsigned res1 = 0;\n";
                sharedVariable += ", res1";
            }

            out << preamble.str();
            out << "PARALLEL_START\n";
            // check whether there is an index to use
            if (keys.empty()) {
                out << "#pragma omp for reduction(" << op << ":" << sharedVariable << ")\n";
                out << "for(const auto& env" << identifier << " : "
                    << "*" << relName << ") {\n";
            } else {
                const auto& rangePatternLower = aggregate.getRangePattern().first;
                const auto& rangePatternUpper = aggregate.getRangePattern().second;

                auto rangeBounds = getPaddedRangeBounds(*rel, rangePatternLower, rangePatternUpper);
                out << "auto range = " << relName << "->"
                    << "lowerUpperRange_" << keys << "(" << rangeBounds.first.str() << ","
                    << rangeBounds.second.str() << "," << ctxName << ");\n";

                out << "auto part = range.partition();\n";
                out << "#pragma omp for reduction(" << op << ":" << sharedVariable << ")\n";
                // iterate over each part
                out << "for (auto it = part.begin(); it < part.end(); ++it) {\n";
                // iterate over tuples in each part
                out << "for (const auto& env" << identifier << ": *it) {\n";
            }

            // produce condition inside the loop if necessary
            out << "if( ";
            dispatch(aggregate.getCondition(), out);
            out << ") {\n";

            out << "shouldRunNested = true;\n";

            // pick function
            switch (aggregate.getFunction()) {
                case AggregateOp::FMIN:
                case AggregateOp::UMIN:
                case AggregateOp::MIN:
                    out << "res0 = std::min(res0,ramBitCast<" << type << ">(";
                    dispatch(aggregate.getExpression(), out);
                    out << "));\n";
                    break;
                case AggregateOp::FMAX:
                case AggregateOp::UMAX:
                case AggregateOp::MAX:
                    out << "res0 = std::max(res0,ramBitCast<" << type << ">(";
                    dispatch(aggregate.getExpression(), out);
                    out << "));\n";
                    break;
                case AggregateOp::COUNT: out << "++res0\n;"; break;
                case AggregateOp::FSUM:
                case AggregateOp::USUM:
                case AggregateOp::SUM:
                    out << "res0 += "
                        << "ramBitCast<" << type << ">(";
                    dispatch(aggregate.getExpression(), out);
                    out << ");\n";
                    break;

                case AggregateOp::MEAN:
                    out << "res0 += "
                        << "ramBitCast<RamFloat>(";
                    dispatch(aggregate.getExpression(), out);
                    out << ");\n";
                    out << "++res1;\n";
                    break;
            }

            // end if statement
            out << "}\n";

            // end aggregator loop
            out << "}\n";

            // if keys weren't empty then there'll be another loop to close off
            if (!keys.empty()) {
                out << "}\n";
            }

            // start single-threaded section
            out << "#pragma omp single\n{\n";

            if (aggregate.getFunction() == AggregateOp::MEAN) {
                out << "if (res1 != 0) {\n";
                out << "res0 = res0 / res1;\n";
                out << "}\n";
            }

            // write result into environment tuple
            out << "env" << identifier << "[0] = ramBitCast(res0);\n";

            // check whether there exists a min/max first before next loop
            out << "if (shouldRunNested) {\n";
            visit_(type_identity<TupleOperation>(), aggregate, out);
            out << "}\n";
            // end single-threaded section
            out << "}\n";
            PRINT_END_COMMENT(out);
        }

        bool isGuaranteedToBeMinimum(const IndexAggregate& aggregate) {
            auto identifier = aggregate.getTupleId();
            auto keys = isa->getSearchSignature(&aggregate);
            RelationRepresentation repr = synthesiser.lookup(aggregate.getRelation())->getRepresentation();

            const auto* tupleElem = as<TupleElement>(aggregate.getExpression());
            return tupleElem && tupleElem->getTupleId() == identifier &&
                   keys[tupleElem->getElement()] != ram::analysis::AttributeConstraint::None &&
                   (repr == RelationRepresentation::BTREE || repr == RelationRepresentation::DEFAULT);
        }

        void visit_(
                type_identity<IndexAggregate>, const IndexAggregate& aggregate, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            // get some properties
            const auto* rel = synthesiser.lookup(aggregate.getRelation());
            auto arity = rel->getArity();
            auto relName = synthesiser.getRelationName(rel);
            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(*rel) + ")";
            auto identifier = aggregate.getTupleId();

            // aggregate tuple storing the result of aggregate
            std::string tuple_type = "Tuple<RamDomain," + toString(arity) + ">";

            // declare environment variable
            out << "Tuple<RamDomain,1> env" << identifier << ";\n";

            // get range to aggregate
            auto keys = isa->getSearchSignature(&aggregate);

            // special case: counting number elements over an unrestricted predicate
            if (aggregate.getFunction() == AggregateOp::COUNT && keys.empty() &&
                    isTrue(&aggregate.getCondition())) {
                // shortcut: use relation size
                out << "env" << identifier << "[0] = " << relName << "->"
                    << "size();\n";
                visit_(type_identity<TupleOperation>(), aggregate, out);
                PRINT_END_COMMENT(out);
                return;
            }

            out << "bool shouldRunNested = false;\n";

            // init result
            std::string init;
            switch (aggregate.getFunction()) {
                case AggregateOp::MIN: init = "MAX_RAM_SIGNED"; break;
                case AggregateOp::FMIN: init = "MAX_RAM_FLOAT"; break;
                case AggregateOp::UMIN: init = "MAX_RAM_UNSIGNED"; break;
                case AggregateOp::MAX: init = "MIN_RAM_SIGNED"; break;
                case AggregateOp::FMAX: init = "MIN_RAM_FLOAT"; break;
                case AggregateOp::UMAX: init = "MIN_RAM_UNSIGNED"; break;
                case AggregateOp::COUNT:
                    init = "0";
                    out << "shouldRunNested = true;\n";
                    break;
                case AggregateOp::MEAN: init = "0"; break;
                case AggregateOp::FSUM:
                case AggregateOp::USUM:
                case AggregateOp::SUM:
                    init = "0";
                    out << "shouldRunNested = true;\n";
                    break;
            }

            std::string type;
            switch (getTypeAttributeAggregate(aggregate.getFunction())) {
                case TypeAttribute::Signed: type = "RamSigned"; break;
                case TypeAttribute::Unsigned: type = "RamUnsigned"; break;
                case TypeAttribute::Float: type = "RamFloat"; break;

                case TypeAttribute::Symbol:
                case TypeAttribute::ADT:
                case TypeAttribute::Record: type = "RamDomain"; break;
            }
            out << type << " res0 = " << init << ";\n";

            if (aggregate.getFunction() == AggregateOp::MEAN) {
                out << "RamUnsigned res1 = 0;\n";
            }

            // check whether there is an index to use
            if (keys.empty()) {
                out << "for(const auto& env" << identifier << " : "
                    << "*" << relName << ") {\n";
            } else {
                const auto& rangePatternLower = aggregate.getRangePattern().first;
                const auto& rangePatternUpper = aggregate.getRangePattern().second;

                auto rangeBounds = getPaddedRangeBounds(*rel, rangePatternLower, rangePatternUpper);

                out << "auto range = " << relName << "->"
                    << "lowerUpperRange_" << keys << "(" << rangeBounds.first.str() << ","
                    << rangeBounds.second.str() << "," << ctxName << ");\n";

                // aggregate result
                out << "for(const auto& env" << identifier << " : range) {\n";
            }

            // produce condition inside the loop
            out << "if( ";
            dispatch(aggregate.getCondition(), out);
            out << ") {\n";

            out << "shouldRunNested = true;\n";

            // pick function
            switch (aggregate.getFunction()) {
                case AggregateOp::FMIN:
                case AggregateOp::UMIN:
                case AggregateOp::MIN:
                    out << "res0 = std::min(res0,ramBitCast<" << type << ">(";
                    dispatch(aggregate.getExpression(), out);
                    out << "));\n";
                    if (isGuaranteedToBeMinimum(aggregate)) {
                        out << "break;\n";
                    }
                    break;
                case AggregateOp::FMAX:
                case AggregateOp::UMAX:
                case AggregateOp::MAX:
                    out << "res0 = std::max(res0,ramBitCast<" << type << ">(";
                    dispatch(aggregate.getExpression(), out);
                    out << "));\n";
                    break;
                case AggregateOp::COUNT: out << "++res0\n;"; break;
                case AggregateOp::FSUM:
                case AggregateOp::USUM:
                case AggregateOp::SUM:
                    out << "res0 += "
                        << "ramBitCast<" << type << ">(";
                    dispatch(aggregate.getExpression(), out);
                    out << ");\n";
                    break;

                case AggregateOp::MEAN:
                    out << "res0 += "
                        << "ramBitCast<RamFloat>(";
                    dispatch(aggregate.getExpression(), out);
                    out << ");\n";
                    out << "++res1;\n";
                    break;
            }

            out << "}\n";

            // end aggregator loop
            out << "}\n";

            if (aggregate.getFunction() == AggregateOp::MEAN) {
                out << "if (res1 != 0) {\n";
                out << "res0 = res0 / res1;\n";
                out << "}\n";
            }

            // write result into environment tuple
            out << "env" << identifier << "[0] = ramBitCast(res0);\n";

            // check whether there exists a min/max first before next loop
            out << "if (shouldRunNested) {\n";
            visit_(type_identity<TupleOperation>(), aggregate, out);
            out << "}\n";

            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<ParallelAggregate>, const ParallelAggregate& aggregate,
                std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            // get some properties
            const auto* rel = synthesiser.lookup(aggregate.getRelation());
            auto relName = synthesiser.getRelationName(rel);
            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(*rel) + ")";
            auto identifier = aggregate.getTupleId();

            assert(aggregate.getTupleId() == 0 && "not outer-most loop");
            assert(!preambleIssued && "only first loop can be made parallel");
            preambleIssued = true;

            // declare environment variable
            out << "Tuple<RamDomain,1> env" << identifier << ";\n";

            // special case: counting number elements over an unrestricted predicate
            if (aggregate.getFunction() == AggregateOp::COUNT && isTrue(&aggregate.getCondition())) {
                // shortcut: use relation size
                out << "env" << identifier << "[0] = " << relName << "->"
                    << "size();\n";
                out << "PARALLEL_START\n";
                out << preamble.str();
                visit_(type_identity<TupleOperation>(), aggregate, out);
                PRINT_END_COMMENT(out);
                return;
            }

            out << "bool shouldRunNested = false;\n";

            // init result
            std::string init;
            switch (aggregate.getFunction()) {
                case AggregateOp::MIN: init = "MAX_RAM_SIGNED"; break;
                case AggregateOp::FMIN: init = "MAX_RAM_FLOAT"; break;
                case AggregateOp::UMIN: init = "MAX_RAM_UNSIGNED"; break;
                case AggregateOp::MAX: init = "MIN_RAM_SIGNED"; break;
                case AggregateOp::FMAX: init = "MIN_RAM_FLOAT"; break;
                case AggregateOp::UMAX: init = "MIN_RAM_UNSIGNED"; break;
                case AggregateOp::COUNT:
                    init = "0";
                    out << "shouldRunNested = true;\n";
                    break;

                case AggregateOp::MEAN: init = "0"; break;

                case AggregateOp::FSUM:
                case AggregateOp::USUM:
                case AggregateOp::SUM:
                    init = "0";
                    out << "shouldRunNested = true;\n";
                    break;
            }

            // Set reduction operation
            std::string op;
            switch (aggregate.getFunction()) {
                case AggregateOp::MIN:
                case AggregateOp::FMIN:
                case AggregateOp::UMIN: {
                    op = "min";
                    break;
                }

                case AggregateOp::MAX:
                case AggregateOp::FMAX:
                case AggregateOp::UMAX: {
                    op = "max";
                    break;
                }

                case AggregateOp::MEAN:
                case AggregateOp::FSUM:
                case AggregateOp::USUM:
                case AggregateOp::COUNT:
                case AggregateOp::SUM: {
                    op = "+";
                    break;
                }

                default: fatal("Unhandled aggregate operation");
            }


            char const* type = NULL;
            switch (getTypeAttributeAggregate(aggregate.getFunction())) {
                case TypeAttribute::Signed: type = "RamSigned"; break;
                case TypeAttribute::Unsigned: type = "RamUnsigned"; break;
                case TypeAttribute::Float: type = "RamFloat"; break;

                case TypeAttribute::Symbol:
                case TypeAttribute::ADT:
                case TypeAttribute::Record: type = "RamDomain"; break;
            }
            out << type << " res0 = " << init << ";\n";

            std::string sharedVariable = "res0";
            if (aggregate.getFunction() == AggregateOp::MEAN) {
                out << "RamUnsigned res1 = " << init << ";\n";
                sharedVariable += ", res1";
            }

            // create a partitioning of the relation to iterate over simeltaneously
            out << "auto part = " << relName << "->partition();\n";
            out << "PARALLEL_START\n";
            out << preamble.str();
            // pragma statement
            out << "#pragma omp for reduction(" << op << ":" << sharedVariable << ")\n";
            // iterate over each part
            out << "for (auto it = part.begin(); it < part.end(); ++it) {\n";
            // iterate over tuples in each part
            out << "for (const auto& env" << identifier << ": *it) {\n";

            // produce condition inside the loop
            out << "if( ";
            dispatch(aggregate.getCondition(), out);
            out << ") {\n";

            out << "shouldRunNested = true;\n";
            // pick function
            switch (aggregate.getFunction()) {
                case AggregateOp::FMIN:
                case AggregateOp::UMIN:
                case AggregateOp::MIN:
                    out << "res0 = std::min(res0, ramBitCast<" << type << ">(";
                    dispatch(aggregate.getExpression(), out);
                    out << "));\n";
                    break;
                case AggregateOp::FMAX:
                case AggregateOp::UMAX:
                case AggregateOp::MAX:
                    out << "res0 = std::max(res0, ramBitCast<" << type << ">(";
                    dispatch(aggregate.getExpression(), out);
                    out << "));\n";
                    break;
                case AggregateOp::COUNT: out << "++res0\n;"; break;
                case AggregateOp::FSUM:
                case AggregateOp::USUM:
                case AggregateOp::SUM:
                    out << "res0 += ramBitCast<" << type << ">(";
                    dispatch(aggregate.getExpression(), out);
                    out << ");\n";
                    break;

                case AggregateOp::MEAN:
                    out << "res0 += ramBitCast<RamFloat>(";
                    dispatch(aggregate.getExpression(), out);
                    out << ");\n";
                    out << "++res1;\n";
                    break;
            }

            out << "}\n";

            // end aggregator loop
            out << "}\n";
            // end partition loop
            out << "}\n";

            // the rest shouldn't be run in parallel
            out << "#pragma omp single\n{\n";

            if (aggregate.getFunction() == AggregateOp::MEAN) {
                out << "if (res1 != 0) {\n";
                out << "res0 = res0 / res1;\n";
                out << "}\n";
            }

            // write result into environment tuple
            out << "env" << identifier << "[0] = ramBitCast(res0);\n";

            // check whether there exists a min/max first before next loop
            out << "if (shouldRunNested) {\n";
            visit_(type_identity<TupleOperation>(), aggregate, out);
            out << "}\n";
            out << "}\n";  // to close off pragma omp single section
            PRINT_END_COMMENT(out);
        }
        void visit_(type_identity<Aggregate>, const Aggregate& aggregate, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            // get some properties
            const auto* rel = synthesiser.lookup(aggregate.getRelation());
            auto relName = synthesiser.getRelationName(rel);
            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(*rel) + ")";
            auto identifier = aggregate.getTupleId();

            // declare environment variable
            out << "Tuple<RamDomain,1> env" << identifier << ";\n";

            // special case: counting number elements over an unrestricted predicate
            if (aggregate.getFunction() == AggregateOp::COUNT && isTrue(&aggregate.getCondition())) {
                // shortcut: use relation size
                out << "env" << identifier << "[0] = " << relName << "->"
                    << "size();\n";
                visit_(type_identity<TupleOperation>(), aggregate, out);
                PRINT_END_COMMENT(out);
                return;
            }

            out << "bool shouldRunNested = false;\n";

            // init result
            std::string init;
            switch (aggregate.getFunction()) {
                case AggregateOp::MIN: init = "MAX_RAM_SIGNED"; break;
                case AggregateOp::FMIN: init = "MAX_RAM_FLOAT"; break;
                case AggregateOp::UMIN: init = "MAX_RAM_UNSIGNED"; break;
                case AggregateOp::MAX: init = "MIN_RAM_SIGNED"; break;
                case AggregateOp::FMAX: init = "MIN_RAM_FLOAT"; break;
                case AggregateOp::UMAX: init = "MIN_RAM_UNSIGNED"; break;
                case AggregateOp::COUNT:
                    init = "0";
                    out << "shouldRunNested = true;\n";
                    break;

                case AggregateOp::MEAN: init = "0"; break;

                case AggregateOp::FSUM:
                case AggregateOp::USUM:
                case AggregateOp::SUM:
                    init = "0";
                    out << "shouldRunNested = true;\n";
                    break;
            }

            std::string type;
            switch (getTypeAttributeAggregate(aggregate.getFunction())) {
                case TypeAttribute::Signed: type = "RamSigned"; break;
                case TypeAttribute::Unsigned: type = "RamUnsigned"; break;
                case TypeAttribute::Float: type = "RamFloat"; break;

                case TypeAttribute::Symbol:
                case TypeAttribute::ADT:
                case TypeAttribute::Record:
                default: type = "RamDomain"; break;
            }
            out << type << " res0 = " << init << ";\n";

            if (aggregate.getFunction() == AggregateOp::MEAN) {
                out << "RamUnsigned res1 = 0;\n";
            }

            // check whether there is an index to use
            out << "for(const auto& env" << identifier << " : "
                << "*" << relName << ") {\n";

            // produce condition inside the loop
            out << "if( ";
            dispatch(aggregate.getCondition(), out);
            out << ") {\n";

            out << "shouldRunNested = true;\n";
            // pick function
            switch (aggregate.getFunction()) {
                case AggregateOp::FMIN:
                case AggregateOp::UMIN:
                case AggregateOp::MIN:
                    out << "res0 = std::min(res0, ramBitCast<" << type << ">(";
                    dispatch(aggregate.getExpression(), out);
                    out << "));\n";
                    break;
                case AggregateOp::FMAX:
                case AggregateOp::UMAX:
                case AggregateOp::MAX:
                    out << "res0 = std::max(res0,ramBitCast<" << type << ">(";
                    dispatch(aggregate.getExpression(), out);
                    out << "));\n";
                    break;
                case AggregateOp::COUNT: out << "++res0\n;"; break;
                case AggregateOp::FSUM:
                case AggregateOp::USUM:
                case AggregateOp::SUM:
                    out << "res0 += "
                        << "ramBitCast<" << type << ">(";
                    ;
                    dispatch(aggregate.getExpression(), out);
                    out << ");\n";
                    break;

                case AggregateOp::MEAN:
                    out << "res0 += "
                        << "ramBitCast<RamFloat>(";
                    dispatch(aggregate.getExpression(), out);
                    out << ");\n";
                    out << "++res1;\n";
                    break;
            }

            out << "}\n";

            // end aggregator loop
            out << "}\n";

            if (aggregate.getFunction() == AggregateOp::MEAN) {
                out << "res0 = res0 / res1;\n";
            }

            // write result into environment tuple
            out << "env" << identifier << "[0] = ramBitCast(res0);\n";

            // check whether there exists a min/max first before next loop
            out << "if (shouldRunNested) {\n";
            visit_(type_identity<TupleOperation>(), aggregate, out);
            out << "}\n";

            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<Filter>, const Filter& filter, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "if( ";
            dispatch(filter.getCondition(), out);
            out << ") {\n";
            visit_(type_identity<NestedOperation>(), filter, out);
            out << "}\n";
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<Break>, const Break& breakOp, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "if( ";
            dispatch(breakOp.getCondition(), out);
            out << ") break;\n";
            visit_(type_identity<NestedOperation>(), breakOp, out);
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<GuardedInsert>, const GuardedInsert& guardedInsert,
                std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            const auto* rel = synthesiser.lookup(guardedInsert.getRelation());
            auto arity = rel->getArity();
            auto relName = synthesiser.getRelationName(rel);
            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(*rel) + ")";

            auto condition = guardedInsert.getCondition();
            // guarded conditions
            out << "if( ";
            dispatch(*condition, out);
            out << ") {\n";

            // create inserted tuple
            out << "Tuple<RamDomain," << arity << "> tuple{{" << join(guardedInsert.getValues(), ",", rec)
                << "}};\n";

            // insert tuple
            out << relName << "->"
                << "insert(tuple," << ctxName << ");\n";

            // end of conseq body.
            out << "}\n";

            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<Insert>, const Insert& insert, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            const auto* rel = synthesiser.lookup(insert.getRelation());
            auto arity = rel->getArity();
            auto relName = synthesiser.getRelationName(rel);
            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(*rel) + ")";

            // create inserted tuple
            out << "Tuple<RamDomain," << arity << "> tuple{{" << join(insert.getValues(), ",", rec)
                << "}};\n";

            // insert tuple
            out << relName << "->"
                << "insert(tuple," << ctxName << ");\n";

            PRINT_END_COMMENT(out);
        }

        // -- conditions --

        void visit_(type_identity<True>, const True&, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "true";
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<False>, const False&, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "false";
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<Conjunction>, const Conjunction& conj, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            dispatch(conj.getLHS(), out);
            out << " && ";
            dispatch(conj.getRHS(), out);
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<Negation>, const Negation& neg, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "!(";
            dispatch(neg.getOperand(), out);
            out << ")";
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<Constraint>, const Constraint& rel, std::ostream& out) override {
            // clang-format off
#define EVAL_CHILD(ty, idx)        \
    out << "ramBitCast<" #ty ">("; \
    dispatch(rel.idx(), out);      \
    out << ")"
#define COMPARE_NUMERIC(ty, op) \
    out << "(";                 \
    EVAL_CHILD(ty, getLHS);     \
    out << " " #op " ";         \
    EVAL_CHILD(ty, getRHS);     \
    out << ")";                 \
    break
#define COMPARE_STRING(op)                \
    out << "(symTable.decode(";           \
    EVAL_CHILD(RamDomain, getLHS);        \
    out << ") " #op " symTable.decode(";  \
    EVAL_CHILD(RamDomain, getRHS);        \
    out << "))";                          \
    break
#define COMPARE_EQ_NE(opCode, op)                                         \
    case BinaryConstraintOp::   opCode: COMPARE_NUMERIC(RamDomain  , op); \
    case BinaryConstraintOp::F##opCode: COMPARE_NUMERIC(RamFloat   , op);
#define COMPARE(opCode, op)                                               \
    case BinaryConstraintOp::   opCode: COMPARE_NUMERIC(RamSigned  , op); \
    case BinaryConstraintOp::U##opCode: COMPARE_NUMERIC(RamUnsigned, op); \
    case BinaryConstraintOp::F##opCode: COMPARE_NUMERIC(RamFloat   , op); \
    case BinaryConstraintOp::S##opCode: COMPARE_STRING(op);
            // clang-format on

            PRINT_BEGIN_COMMENT(out);
            switch (rel.getOperator()) {
                // comparison operators
                COMPARE_EQ_NE(EQ, ==)
                COMPARE_EQ_NE(NE, !=)

                COMPARE(LT, <)
                COMPARE(LE, <=)
                COMPARE(GT, >)
                COMPARE(GE, >=)

                // strings
                case BinaryConstraintOp::MATCH: {
                    synthesiser.UsingStdRegex = true;
                    out << "regex_wrapper(symTable.decode(";
                    dispatch(rel.getLHS(), out);
                    out << "),symTable.decode(";
                    dispatch(rel.getRHS(), out);
                    out << "))";
                    break;
                }
                case BinaryConstraintOp::NOT_MATCH: {
                    synthesiser.UsingStdRegex = true;
                    out << "!regex_wrapper(symTable.decode(";
                    dispatch(rel.getLHS(), out);
                    out << "),symTable.decode(";
                    dispatch(rel.getRHS(), out);
                    out << "))";
                    break;
                }
                case BinaryConstraintOp::CONTAINS: {
                    out << "(symTable.decode(";
                    dispatch(rel.getRHS(), out);
                    out << ").find(symTable.decode(";
                    dispatch(rel.getLHS(), out);
                    out << ")) != std::string::npos)";
                    break;
                }
                case BinaryConstraintOp::NOT_CONTAINS: {
                    out << "(symTable.decode(";
                    dispatch(rel.getRHS(), out);
                    out << ").find(symTable.decode(";
                    dispatch(rel.getLHS(), out);
                    out << ")) == std::string::npos)";
                    break;
                }
            }

            PRINT_END_COMMENT(out);

#undef EVAL_CHILD
#undef COMPARE_NUMERIC
#undef COMPARE_STRING
#undef COMPARE
#undef COMPARE_EQ_NE
        }

        void visit_(
                type_identity<EmptinessCheck>, const EmptinessCheck& emptiness, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << synthesiser.getRelationName(synthesiser.lookup(emptiness.getRelation())) << "->"
                << "empty()";
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<RelationSize>, const RelationSize& size, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "(RamDomain)" << synthesiser.getRelationName(synthesiser.lookup(size.getRelation()))
                << "->"
                << "size()";
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<ExistenceCheck>, const ExistenceCheck& exists, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            // get some details
            const auto* rel = synthesiser.lookup(exists.getRelation());
            auto relName = synthesiser.getRelationName(rel);
            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(*rel) + ")";
            auto arity = rel->getArity();
            assert(arity > 0 && "AstToRamTranslator failed");
            std::string after;
            if (Global::config().has("profile") && Global::config().has("profile-frequency") &&
                    !synthesiser.lookup(exists.getRelation())->isTemp()) {
                out << R"_((reads[)_" << synthesiser.lookupReadIdx(rel->getName()) << R"_(]++,)_";
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

            auto rangePatternLower = exists.getValues();
            auto rangePatternUpper = exists.getValues();

            auto rangeBounds = getPaddedRangeBounds(*rel, rangePatternLower, rangePatternUpper);
            // else we conduct a range query
            out << "!" << relName << "->"
                << "lowerUpperRange";
            out << "_" << isa->getSearchSignature(&exists);
            out << "(" << rangeBounds.first.str() << "," << rangeBounds.second.str() << "," << ctxName
                << ").empty()" << after;
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<ProvenanceExistenceCheck>, const ProvenanceExistenceCheck& provExists,
                std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            // get some details
            const auto* rel = synthesiser.lookup(provExists.getRelation());
            auto relName = synthesiser.getRelationName(rel);
            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(*rel) + ")";
            auto arity = rel->getArity();
            auto auxiliaryArity = rel->getAuxiliaryArity();

            // provenance not exists is never total, conduct a range query
            out << "[&]() -> bool {\n";
            out << "auto existenceCheck = " << relName << "->"
                << "lowerUpperRange";
            out << "_" << isa->getSearchSignature(&provExists);

            // parts refers to payload + rule number
            std::size_t parts = arity - auxiliaryArity + 1;

            // make a copy of provExists.getValues() so we can be sure that vals is always the same vector
            // since provExists.getValues() creates a new vector on the stack each time
            auto vals = provExists.getValues();

            // sanity check to ensure that all payload values are specified
            for (std::size_t i = 0; i < arity - auxiliaryArity; i++) {
                assert(!isUndefValue(vals[i]) &&
                        "ProvenanceExistenceCheck should always be specified for payload");
            }

            auto valsCopy = std::vector<Expression*>(vals.begin(), vals.begin() + parts);
            auto rangeBounds = getPaddedRangeBounds(*rel, valsCopy, valsCopy);

            // remove the ending }} from both strings
            rangeBounds.first.seekp(-2, std::ios_base::end);
            rangeBounds.second.seekp(-2, std::ios_base::end);

            // extra bounds for provenance height annotations
            for (std::size_t i = 0; i < auxiliaryArity - 2; i++) {
                rangeBounds.first << ",ramBitCast<RamDomain, RamSigned>(MIN_RAM_SIGNED)";
                rangeBounds.second << ",ramBitCast<RamDomain, RamSigned>(MAX_RAM_SIGNED)";
            }
            rangeBounds.first << ",ramBitCast<RamDomain, RamSigned>(MIN_RAM_SIGNED)}}";
            rangeBounds.second << ",ramBitCast<RamDomain, RamSigned>(MAX_RAM_SIGNED)}}";

            out << "(" << rangeBounds.first.str() << "," << rangeBounds.second.str() << "," << ctxName
                << ");\n";
            out << "if (existenceCheck.empty()) return false; else return ((*existenceCheck.begin())["
                << arity - auxiliaryArity + 1 << "] <= ";

            dispatch(*(provExists.getValues()[arity - auxiliaryArity + 1]), out);
            out << ")";
            out << ";}()\n";
            PRINT_END_COMMENT(out);
        }

        // -- values --
        void visit_(type_identity<UnsignedConstant>, const UnsignedConstant& constant,
                std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "RamUnsigned(" << constant.getValue() << ")";
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<FloatConstant>, const FloatConstant& constant, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "RamFloat(" << constant.getValue() << ")";
            PRINT_END_COMMENT(out);
        }

        void visit_(
                type_identity<SignedConstant>, const SignedConstant& constant, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "RamSigned(" << constant.getConstant() << ")";
            PRINT_END_COMMENT(out);
        }

        void visit_(
                type_identity<StringConstant>, const StringConstant& constant, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "RamSigned(" << synthesiser.convertSymbol2Idx(constant.getConstant()) << ")";
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<TupleElement>, const TupleElement& access, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "env" << access.getTupleId() << "[" << access.getElement() << "]";
            PRINT_END_COMMENT(out);
        }

        void visit_(type_identity<AutoIncrement>, const AutoIncrement& /*inc*/, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << "(ctr++)";
            PRINT_END_COMMENT(out);
        }

        void visit_(
                type_identity<IntrinsicOperator>, const IntrinsicOperator& op, std::ostream& out) override {
#define MINMAX_SYMBOL(op)                   \
    {                                       \
        out << "symTable.encode(" #op "({"; \
        for (auto& cur : args) {            \
            out << "symTable.decode(";      \
            dispatch(*cur, out);            \
            out << "), ";                   \
        }                                   \
        out << "}))";                       \
        break;                              \
    }

            PRINT_BEGIN_COMMENT(out);

// clang-format off
#define UNARY_OP(opcode, ty, op)                \
    case FunctorOp::opcode: {                   \
        out << "(" #op "(ramBitCast<" #ty ">("; \
        dispatch(*args[0], out);                \
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
        dispatch(*args[0], out);                 \
        out << ") " #op " ramBitCast<" #ty ">("; \
        dispatch(*args[1], out);                 \
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
        dispatch(*args[0], out);                                                              \
        out << "), ramBitCast<" #ty ">(";                                                     \
        dispatch(*args[1], out);                                                              \
        out << "))))";                                                                        \
        break;                                                                                \
    }

#define NARY_OP(opcode, ty, op)            \
    case FunctorOp::opcode: {              \
        out << #op "({";                   \
        for (auto& cur : args) {           \
            out << "ramBitCast<" #ty ">("; \
            dispatch(*cur, out);           \
            out << "), ";                  \
        }                                  \
        out << "})";                       \
        break;                             \
    }
#define NARY_OP_ORDERED(opcode, op)     \
    NARY_OP(   opcode, RamSigned  , op) \
    NARY_OP(U##opcode, RamUnsigned, op) \
    NARY_OP(F##opcode, RamFloat   , op)


#define CONV_TO_STRING(opcode, ty)                \
    case FunctorOp::opcode: {                     \
        out << "symTable.encode(std::to_string("; \
        dispatch(*args[0], out);                  \
        out << "))";                              \
    } break;
#define CONV_FROM_STRING(opcode, ty)                                            \
    case FunctorOp::opcode: {                                                   \
        out << "souffle::evaluator::symbol2numeric<" #ty ">(symTable.decode(";  \
        dispatch(*args[0], out);                                                \
        out << "))";                                                            \
    } break;
            // clang-format on

            auto args = op.getArguments();
            switch (op.getOperator()) {
                /** Unary Functor Operators */
                case FunctorOp::ORD: {
                    dispatch(*args[0], out);
                    break;
                }
                // TODO: change the signature of `STRLEN` to return an unsigned?
                case FunctorOp::STRLEN: {
                    out << "static_cast<RamSigned>(symTable.decode(";
                    dispatch(*args[0], out);
                    out << ").size())";
                    break;
                }

                    // clang-format off
                UNARY_OP_I(NEG, -)
                UNARY_OP_F(NEG, -)

                UNARY_OP_INTEGRAL(BNOT, ~)
                UNARY_OP_INTEGRAL(LNOT, (RamDomain)!)

                /** numeric coersions follow C++ semantics. */
                // identities
                case FunctorOp::F2F:
                case FunctorOp::I2I:
                case FunctorOp::U2U:
                case FunctorOp::S2S: {
                    dispatch(*args[0], out);
                    break;
                }

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
                BINARY_OP_LOGICAL(LXOR, + souffle::evaluator::lxor_infix() +)

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

                case FunctorOp::SMAX: MINMAX_SYMBOL(std::max)

                case FunctorOp::SMIN: MINMAX_SYMBOL(std::min)

                // strings
                case FunctorOp::CAT: {
                    out << "symTable.encode(";
                    std::size_t i = 0;
                    while (i < args.size() - 1) {
                        out << "symTable.decode(";
                        dispatch(*args[i], out);
                        out << ") + ";
                        i++;
                    }
                    out << "symTable.decode(";
                    dispatch(*args[i], out);
                    out << "))";
                    break;
                }

                /** Ternary Functor Operators */
                case FunctorOp::SUBSTR: {
                    out << "symTable.encode(";
                    out << "substr_wrapper(symTable.decode(";
                    dispatch(*args[0], out);
                    out << "),(";
                    dispatch(*args[1], out);
                    out << "),(";
                    dispatch(*args[2], out);
                    out << ")))";
                    break;
                }

                case FunctorOp::RANGE:
                case FunctorOp::URANGE:
                case FunctorOp::FRANGE:
                    fatal("ICE: functor `%s` must map onto `NestedIntrinsicOperator`", op.getOperator());
            }
            PRINT_END_COMMENT(out);

#undef MINMAX_SYMBOL
        }

        void visit_(type_identity<NestedIntrinsicOperator>, const NestedIntrinsicOperator& op,
                std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);

            auto emitHelper = [&](auto&& func) {
                tfm::format(out, "%s(%s, [&](auto&& env%d) {\n", func,
                        join(op.getArguments(), ",", [&](auto& os, auto* arg) { return dispatch(*arg, os); }),
                        op.getTupleId());
                visit_(type_identity<TupleOperation>(), op, out);
                out << "});\n";

                PRINT_END_COMMENT(out);
            };

            auto emitRange = [&](char const* ty) {
                return emitHelper(tfm::format("souffle::evaluator::runRange<%s>", ty));
            };

            switch (op.getFunction()) {
                case NestedIntrinsicOp::RANGE: return emitRange("RamSigned");
                case NestedIntrinsicOp::URANGE: return emitRange("RamUnsigned");
                case NestedIntrinsicOp::FRANGE: return emitRange("RamFloat");
            }

            UNREACHABLE_BAD_CASE_ANALYSIS
        }

        void visit_(type_identity<UserDefinedOperator>, const UserDefinedOperator& op,
                std::ostream& out) override {
            const std::string& name = op.getName();

            auto args = op.getArguments();
            if (op.isStateful()) {
                out << name << "(&symTable, &recordTable";
                for (auto& arg : args) {
                    out << ",";
                    dispatch(*arg, out);
                }
                out << ")";
            } else {
                const std::vector<TypeAttribute>& argTypes = op.getArgsTypes();

                if (op.getReturnType() == TypeAttribute::Symbol) {
                    out << "symTable.encode(";
                }
                out << name << "(";

                for (std::size_t i = 0; i < args.size(); i++) {
                    if (i > 0) {
                        out << ",";
                    }
                    switch (argTypes[i]) {
                        case TypeAttribute::Signed:
                            out << "((RamSigned)";
                            dispatch(*args[i], out);
                            out << ")";
                            break;
                        case TypeAttribute::Unsigned:
                            out << "((RamUnsigned)";
                            dispatch(*args[i], out);
                            out << ")";
                            break;
                        case TypeAttribute::Float:
                            out << "((RamFloat)";
                            dispatch(*args[i], out);
                            out << ")";
                            break;
                        case TypeAttribute::Symbol:
                            out << "symTable.decode(";
                            dispatch(*args[i], out);
                            out << ").c_str()";
                            break;
                        case TypeAttribute::ADT:
                        case TypeAttribute::Record: fatal("unhandled type");
                    }
                }
                out << ")";
                if (op.getReturnType() == TypeAttribute::Symbol) {
                    out << ")";
                }
            }
        }

        // -- records --

        void visit_(type_identity<PackRecord>, const PackRecord& pack, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);

            const auto arity = pack.getArguments().size();

            synthesiser.arities.emplace(arity);

            out << "pack(recordTable,"
                << "Tuple<RamDomain," << arity << ">";
            if (pack.getArguments().size() == 0) {
                out << "{{}}";
            } else {
                out << "{{ramBitCast(" << join(pack.getArguments(), "),ramBitCast(", rec) << ")}}\n";
            }
            out << ")";

            PRINT_END_COMMENT(out);
        }

        // -- subroutine argument --

        void visit_(type_identity<SubroutineArgument>, const SubroutineArgument& arg,
                std::ostream& out) override {
            out << "(args)[" << arg.getArgument() << "]";
        }

        // -- subroutine return --

        void visit_(
                type_identity<SubroutineReturn>, const SubroutineReturn& ret, std::ostream& out) override {
            out << "std::lock_guard<std::mutex> guard(lock);\n";
            for (auto val : ret.getValues()) {
                if (isUndefValue(val)) {
                    out << "ret.push_back(0);\n";
                } else {
                    out << "ret.push_back(";
                    dispatch(*val, out);
                    out << ");\n";
                }
            }
        }

        // -- safety net --

        void visit_(type_identity<UndefValue>, const UndefValue&, std::ostream& /*out*/) override {
            fatal("Compilation error");
        }

        void visit_(type_identity<Node>, const Node& node, std::ostream& /*out*/) override {
            fatal("Unsupported node type: %s", typeid(node).name());
        }
    };

    out << std::setprecision(std::numeric_limits<RamFloat>::max_digits10);
    // emit code
    CodeEmitter(*this).dispatch(stmt, out);
}

/** An output stream where some pieces may be filled later or conditionnaly. */
class DelayableOutputStream : public std::streambuf, public std::ostream {
public:
    DelayableOutputStream() : std::ostream(this) {}

    ~DelayableOutputStream() {}

    std::streambuf::int_type overflow(std::streambuf::int_type ch) override {
        if (!current_stream) {
            pieces.emplace_back(std::nullopt, std::make_shared<std::stringstream>());
            current_stream = pieces.back().second;
        }
        current_stream->put(ch);
        return ch;
    }

    /** Return a piece of stream that will be included in the output only if the given condition is true when
     * this stream is flushed. */
    std::shared_ptr<std::ostream> delayed_if(const bool& cond) {
        current_stream.reset();
        pieces.emplace_back(&cond, std::make_shared<std::stringstream>());
        return pieces.back().second;
    }

    /** Return a piece of stream that will be included in the output when this stream is flushed. */
    std::shared_ptr<std::ostream> delayed() {
        current_stream.reset();
        pieces.emplace_back(std::nullopt, std::make_shared<std::stringstream>());
        return pieces.back().second;
    }

    /** */
    void flushAll(std::ostream& os) {
        current_stream.reset();
        while (!pieces.empty()) {
            auto& piece = pieces.front();
            if ((!piece.first) || **piece.first) {
                os << piece.second->str();
            }
            pieces.pop_front();
        }
    }

private:
    /* the sequence of pieces that compose the output stream. */
    std::list<std::pair<std::optional<const bool*>, std::shared_ptr<std::stringstream>>> pieces;

    /* points to the current piece's stream. */
    std::shared_ptr<std::ostream> current_stream;
};

void Synthesiser::generateCode(std::ostream& sos, const std::string& id, bool& withSharedLibrary) {
    // ---------------------------------------------------------------
    //                      Auto-Index Generation
    // ---------------------------------------------------------------
    const Program& prog = translationUnit.getProgram();
    auto& idxAnalysis = translationUnit.getAnalysis<IndexAnalysis>();
    // ---------------------------------------------------------------
    //                      Code Generation
    // ---------------------------------------------------------------

    DelayableOutputStream os;

    withSharedLibrary = false;

    std::string classname = "Sf_" + id;

    // generate C++ program

    if (Global::config().has("verbose")) {
        os << "#define _SOUFFLE_STATS\n";
        os << "#include \"souffle/profile/ProfileEvent.h\"";
    }
    os << "\n#include \"souffle/CompiledSouffle.h\"\n";
    if (Global::config().has("provenance")) {
        os << "#include <mutex>\n";
        os << "#include \"souffle/provenance/Explain.h\"\n";
    }

    if (Global::config().has("live-profile")) {
        os << "#include <thread>\n";
        os << "#include \"souffle/profile/Tui.h\"\n";
    }

    {
        auto _os = os.delayed_if(UsingStdRegex);
        *_os << "#include <regex>\n";
    }

    if (Global::config().has("profile") || Global::config().has("live-profile")) {
        os << "#include \"souffle/profile/ProfileEvent.h\"";
    }

    os << "\n";
    // produce external definitions for user-defined functors
    std::map<std::string, std::tuple<TypeAttribute, std::vector<TypeAttribute>, bool>> functors;
    visit(prog, [&](const UserDefinedOperator& op) {
        if (functors.find(op.getName()) == functors.end()) {
            functors[op.getName()] = std::make_tuple(op.getReturnType(), op.getArgsTypes(), op.isStateful());
        }
        withSharedLibrary = true;
    });
    os << "extern \"C\" {\n";
    for (const auto& f : functors) {
        //        std::size_t arity = f.second.length() - 1;
        const std::string& name = f.first;

        const auto& functorTypes = f.second;
        const auto& returnType = std::get<0>(functorTypes);
        const auto& argsTypes = std::get<1>(functorTypes);
        const auto& stateful = std::get<2>(functorTypes);

        auto cppTypeDecl = [](TypeAttribute ty) -> char const* {
            switch (ty) {
                case TypeAttribute::Signed: return "souffle::RamSigned";
                case TypeAttribute::Unsigned: return "souffle::RamUnsigned";
                case TypeAttribute::Float: return "souffle::RamFloat";
                case TypeAttribute::Symbol: return "const char *";
                case TypeAttribute::ADT: fatal("adts cannot be used by user-defined functors");
                case TypeAttribute::Record: fatal("records cannot be used by user-defined functors");
            }

            UNREACHABLE_BAD_CASE_ANALYSIS
        };

        if (stateful) {
            os << "souffle::RamDomain " << name << "(souffle::SymbolTable *, souffle::RecordTable *";
            for (std::size_t i = 0; i < argsTypes.size(); i++) {
                os << ",souffle::RamDomain";
            }
            os << ");\n";
        } else {
            tfm::format(os, "%s %s(%s);\n", cppTypeDecl(returnType), name,
                    join(map(argsTypes, cppTypeDecl), ","));
        }
    }
    os << "}\n";
    os << "\n";
    os << "namespace souffle {\n";
    os << "static const RamDomain RAM_BIT_SHIFT_MASK = RAM_DOMAIN_SIZE - 1;\n";

    // synthesise data-structures for relations
    for (auto rel : prog.getRelations()) {
        bool isProvInfo = rel->getRepresentation() == RelationRepresentation::INFO;
        auto relationType =
                Relation::getSynthesiserRelation(*rel, idxAnalysis.getIndexSelection(rel->getName()),
                        Global::config().has("provenance") && !isProvInfo);

        generateRelationTypeStruct(os, std::move(relationType));
    }
    os << '\n';

    os << "class " << classname << " : public SouffleProgram {\n";

    {
        // regex wrapper
        auto osp = os.delayed_if(UsingStdRegex);
        auto& _os = *osp;
        _os << "private:\n";
        _os << "static inline bool regex_wrapper(const std::string& pattern, const std::string& text) {\n";
        _os << "   bool result = false; \n";
        _os << "   try { result = std::regex_match(text, std::regex(pattern)); } catch(...) { \n";
        _os << "     std::cerr << \"warning: wrong pattern provided for match(\\\"\" << pattern << "
               "\"\\\",\\\"\" "
               "<< text << \"\\\").\\n\";\n}\n";
        _os << "   return result;\n";
        _os << "}\n";
    }

    // substring wrapper
    os << "private:\n";
    os << "static inline std::string substr_wrapper(const std::string& str, std::size_t idx, "
          "std::size_t "
          "len) {\n";
    os << "   std::string result; \n";
    os << "   try { result = str.substr(idx,len); } catch(...) { \n";
    os << "     std::cerr << \"warning: wrong index position provided by substr(\\\"\";\n";
    os << "     std::cerr << str << \"\\\",\" << (int32_t)idx << \",\" << (int32_t)len << \") "
          "functor.\\n\";\n";
    os << "   } return result;\n";
    os << "}\n";

    if (Global::config().has("profile")) {
        os << "std::string profiling_fname;\n";
    }

    os << "public:\n";

    // declare symbol table
    os << "// -- initialize symbol table --\n";

    // issue symbol table with string constants
    visit(prog, [&](const StringConstant& sc) { convertSymbol2Idx(sc.getConstant()); });
    os << "SymbolTable symTable";
    if (!symbolMap.empty()) {
        os << "{\n";
        for (const auto& x : symbolIndex) {
            os << "\tR\"_(" << x << ")_\",\n";
        }
        os << "}";
    }
    os << ";";

    // declare record table
    os << "// -- initialize record table --\n";

    auto recordTable_os = os.delayed();

    if (Global::config().has("profile")) {
        os << "private:\n";
        std::size_t numFreq = 0;
        visit(prog, [&](const Statement&) { numFreq++; });
        os << "  std::size_t freqs[" << numFreq << "]{};\n";
        std::size_t numRead = 0;
        for (auto rel : prog.getRelations()) {
            if (!rel->isTemp()) {
                numRead++;
            }
        }
        os << "  std::size_t reads[" << numRead << "]{};\n";
    }

    // print relation definitions
    std::stringstream initCons;     // initialization of constructor
    std::stringstream registerRel;  // registration of relations
    auto initConsSep = [&, empty = true]() mutable -> std::stringstream& {
        initCons << (empty ? "\n: " : "\n, ");
        empty = false;
        return initCons;
    };

    // `pf` must be a ctor param (see below)
    if (Global::config().has("profile")) {
        initConsSep() << "profiling_fname(std::move(pf))";
    }

    int relCtr = 0;
    std::set<std::string> storeRelations;
    std::set<std::string> loadRelations;
    std::set<const IO*> loadIOs;
    std::set<const IO*> storeIOs;

    // collect load/store operations/relations
    visit(prog, [&](const IO& io) {
        auto op = io.get("operation");
        if (op == "input") {
            loadRelations.insert(io.getRelation());
            loadIOs.insert(&io);
        } else if (op == "printsize" || op == "output") {
            storeRelations.insert(io.getRelation());
            storeIOs.insert(&io);
        } else {
            assert("wrong I/O operation");
        }
    });

    for (auto rel : prog.getRelations()) {
        // get some table details
        const std::string& datalogName = rel->getName();
        const std::string& cppName = getRelationName(*rel);

        bool isProvInfo = rel->getRepresentation() == RelationRepresentation::INFO;
        auto relationType = Relation::getSynthesiserRelation(*rel, idxAnalysis.getIndexSelection(datalogName),
                Global::config().has("provenance") && !isProvInfo);
        const std::string& type = relationType->getTypeName();

        // defining table
        os << "// -- Table: " << datalogName << "\n";

        os << "Own<" << type << "> " << cppName << " = mk<" << type << ">();\n";
        if (!rel->isTemp()) {
            tfm::format(os, "souffle::RelationWrapper<%s> wrapper_%s;\n", type, cppName);

            auto strLitAry = [](auto&& xs) {
                std::stringstream ss;
                ss << "std::array<const char *," << xs.size() << ">{{"
                   << join(xs, ",", [](auto&& os, auto&& x) { os << '"' << x << '"'; }) << "}}";
                return ss.str();
            };

            auto foundIn = [&](auto&& set) { return contains(set, rel->getName()) ? "true" : "false"; };

            tfm::format(initConsSep(), "wrapper_%s(%s, *%s, *this, \"%s\", %s, %s, %s)", cppName, relCtr++,
                    cppName, datalogName, strLitAry(rel->getAttributeTypes()),
                    strLitAry(rel->getAttributeNames()), rel->getAuxiliaryArity());
            tfm::format(registerRel, "addRelation(\"%s\", wrapper_%s, %s, %s);\n", datalogName, cppName,
                    foundIn(loadRelations), foundIn(storeRelations));
        }
    }
    os << "public:\n";

    // -- constructor --

    os << classname;
    os << (Global::config().has("profile") ? "(std::string pf=\"profile.log\")" : "()");
    os << initCons.str() << '\n';
    os << "{\n";
    if (Global::config().has("profile")) {
        os << "ProfileEventSingleton::instance().setOutputFile(profiling_fname);\n";
    }
    os << registerRel.str();
    os << "}\n";
    // -- destructor --

    os << "~" << classname << "() {\n";
    os << "}\n";

    // issue state variables for the evaluation
    //
    // Improve compile time by storing the signal handler in one loc instead of
    // emitting thousands of `SignalHandler::instance()`. The volume of calls
    // makes GVN and register alloc very expensive, even if the call is inlined.
    os << R"_(
private:
std::string             inputDirectory;
std::string             outputDirectory;
SignalHandler*          signalHandler {SignalHandler::instance()};
std::atomic<RamDomain>  ctr {};
std::atomic<std::size_t>     iter {};

void runFunction(std::string  inputDirectoryArg,
                 std::string  outputDirectoryArg,
                 bool         performIOArg,
                 bool         pruneImdtRelsArg) {
    this->inputDirectory  = std::move(inputDirectoryArg);
    this->outputDirectory = std::move(outputDirectoryArg);
    this->performIO       = performIOArg;
    this->pruneImdtRels   = pruneImdtRelsArg;

    // set default threads (in embedded mode)
    // if this is not set, and omp is used, the default omp setting of number of cores is used.
#if defined(_OPENMP)
    if (0 < getNumThreads()) { omp_set_num_threads(getNumThreads()); }
#endif

    signalHandler->set();
)_";
    if (Global::config().has("verbose")) {
        os << "signalHandler->enableLogging();\n";
    }

    // add actual program body
    os << "// -- query evaluation --\n";
    if (Global::config().has("profile")) {
        os << "ProfileEventSingleton::instance().startTimer();\n";
        os << R"_(ProfileEventSingleton::instance().makeTimeEvent("@time;starttime");)_" << '\n';
        os << "{\n"
           << R"_(Logger logger("@runtime;", 0);)_" << '\n';
        // Store count of relations
        std::size_t relationCount = 0;
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

    if (Global::config().has("verbose")) {
        for (auto rel : prog.getRelations()) {
            auto name = getRelationName(*rel);
            os << "std::cout << \"Statistics for Relation " << name << ":\\n\";\n";
            os << name << "->printStatistics(std::cout);\n";
            os << "std::cout << \"\\n\";\n";
        }
    }

    os << "signalHandler->reset();\n";

    os << "}\n";  // end of runFunction() method

    // add methods to run with and without performing IO (mainly for the interface)
    os << "public:\nvoid run() override { runFunction(\"\", \"\", "
          "false, false); }\n";
    os << "public:\nvoid runAll(std::string inputDirectoryArg = \"\", std::string outputDirectoryArg = \"\", "
          "bool performIOArg=true, bool pruneImdtRelsArg=true) override { ";
    if (Global::config().has("live-profile")) {
        os << "std::thread profiler([]() { profile::Tui().runProf(); });\n";
    }
    os << "runFunction(inputDirectoryArg, outputDirectoryArg, performIOArg, pruneImdtRelsArg);\n";
    if (Global::config().has("live-profile")) {
        os << "if (profiler.joinable()) { profiler.join(); }\n";
    }
    os << "}\n";
    // issue printAll method
    os << "public:\n";
    os << "void printAll(std::string outputDirectoryArg = \"\") override {\n";

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
        os << R"_(if (!outputDirectoryArg.empty()) {)_";
        os << R"_(directiveMap["output-dir"] = outputDirectoryArg;)_";
        os << "}\n";
        os << "IOSystem::getInstance().getWriter(";
        os << "directiveMap, symTable, recordTable";
        os << ")->writeAll(*" << getRelationName(lookup(store->getRelation())) << ");\n";

        os << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
    }
    os << "}\n";  // end of printAll() method

    // issue loadAll method
    os << "public:\n";
    os << "void loadAll(std::string inputDirectoryArg = \"\") override {\n";

    for (auto load : loadIOs) {
        os << "try {";
        os << "std::map<std::string, std::string> directiveMap(";
        printDirectives(load->getDirectives());
        os << ");\n";
        os << R"_(if (!inputDirectoryArg.empty()) {)_";
        os << R"_(directiveMap["fact-dir"] = inputDirectoryArg;)_";
        os << "}\n";
        os << "IOSystem::getInstance().getReader(";
        os << "directiveMap, symTable, recordTable";
        os << ")->readAll(*" << getRelationName(lookup(load->getRelation()));
        os << ");\n";
        os << "} catch (std::exception& e) {std::cerr << \"Error loading data: \" << e.what() << "
              "'\\n';}\n";
    }

    os << "}\n";  // end of loadAll() method
    // issue dump methods
    auto dumpRelation = [&](const ram::Relation& ramRelation) {
        const auto& relName = getRelationName(ramRelation);
        const auto& name = ramRelation.getName();
        const auto& attributesTypes = ramRelation.getAttributeTypes();

        Json relJson = Json::object{{"arity", static_cast<long long>(attributesTypes.size())},
                {"auxArity", static_cast<long long>(0)},
                {"types", Json::array(attributesTypes.begin(), attributesTypes.end())}};

        Json types = Json::object{{"relation", relJson}};

        os << "try {";
        os << "std::map<std::string, std::string> rwOperation;\n";
        os << "rwOperation[\"IO\"] = \"stdout\";\n";
        os << R"(rwOperation["name"] = ")" << name << "\";\n";
        os << "rwOperation[\"types\"] = ";
        os << "\"" << escapeJSONstring(types.dump()) << "\"";
        os << ";\n";
        os << "IOSystem::getInstance().getWriter(";
        os << "rwOperation, symTable, recordTable";
        os << ")->writeAll(*" << relName << ");\n";
        os << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
    };

    // dump inputs
    os << "public:\n";
    os << "void dumpInputs() override {\n";
    for (auto load : loadIOs) {
        dumpRelation(*lookup(load->getRelation()));
    }
    os << "}\n";  // end of dumpInputs() method

    // dump outputs
    os << "public:\n";
    os << "void dumpOutputs() override {\n";
    for (auto store : storeIOs) {
        dumpRelation(*lookup(store->getRelation()));
    }
    os << "}\n";  // end of dumpOutputs() method

    os << "public:\n";
    os << "SymbolTable& getSymbolTable() override {\n";
    os << "return symTable;\n";
    os << "}\n";  // end of getSymbolTable() method

    os << "RecordTable& getRecordTable() override {\n";
    os << "return recordTable;\n";
    os << "}\n";  // end of getRecordTable() method

    os << "void setNumThreads(std::size_t numThreadsValue) override {\n";
    os << "SouffleProgram::setNumThreads(numThreadsValue);\n";
    os << "symTable.setNumLanes(getNumThreads());\n";
    os << "recordTable.setNumLanes(getNumThreads());\n";
    os << "}\n";  // end of setNumThreads

    if (!prog.getSubroutines().empty()) {
        // generate subroutine adapter
        os << "void executeSubroutine(std::string name, const std::vector<RamDomain>& args, "
              "std::vector<RamDomain>& ret) override {\n";
        // subroutine number
        std::size_t subroutineNum = 0;
        for (auto& sub : prog.getSubroutines()) {
            os << "if (name == \"" << sub.first << "\") {\n"
               << "subroutine_" << subroutineNum
               << "(args, ret);\n"  // subroutine_<i> to deal with special characters in relation names
               << "return;"
               << "}\n";
            subroutineNum++;
        }
        os << "fatal(\"unknown subroutine\");\n";
        os << "}\n";  // end of executeSubroutine

        // generate method for each subroutine
        subroutineNum = 0;
        for (auto& sub : prog.getSubroutines()) {
            // silence unused argument warnings on MSVC
            os << "#ifdef _MSC_VER\n";
            os << "#pragma warning(disable: 4100)\n";
            os << "#endif // _MSC_VER\n";

            // issue method header
            os << "void "
               << "subroutine_" << subroutineNum
               << "(const std::vector<RamDomain>& args, "
                  "std::vector<RamDomain>& ret) {\n";

            // issue lock variable for return statements
            bool needLock = false;
            visit(*sub.second, [&](const SubroutineReturn&) { needLock = true; });
            if (needLock) {
                os << "std::mutex lock;\n";
            }

            // emit code for subroutine
            emitCode(os, *sub.second);

            // issue end of subroutine
            os << "}\n";

            // restore unused argument warning
            os << "#ifdef _MSC_VER\n";
            os << "#pragma warning(default: 4100)\n";
            os << "#endif // _MSC_VER\n";
            subroutineNum++;
        }
    }
    // dumpFreqs method
    //  Frequency counts must be emitted after subroutines otherwise lookup tables
    //  are not populated.
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
    os << "};\n";  // end of class declaration

    // hidden hooks
    os << "SouffleProgram *newInstance_" << id << "(){return new " << classname << ";}\n";
    os << "SymbolTable *getST_" << id << "(SouffleProgram *p){return &reinterpret_cast<" << classname
       << "*>(p)->getSymbolTable();}\n";

    os << "\n#ifdef __EMBEDDED_SOUFFLE__\n";
    os << "class factory_" << classname << ": public souffle::ProgramFactory {\n";
    os << "SouffleProgram *newInstance() {\n";
    os << "return new " << classname << "();\n";
    os << "};\n";
    os << "public:\n";
    os << "factory_" << classname << "() : ProgramFactory(\"" << id << "\"){}\n";
    os << "};\n";
    os << "extern \"C\" {\n";
    os << "factory_" << classname << " __factory_" << classname << "_instance;\n";
    os << "}\n";
    os << "}\n";
    os << "#else\n";
    os << "}\n";
    os << "int main(int argc, char** argv)\n{\n";
    os << "try{\n";

    // parse arguments
    os << "souffle::CmdOptions opt(";
    os << "R\"(" << Global::config().get("") << ")\",\n";
    os << "R\"()\",\n";
    os << "R\"()\",\n";
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
        os << "explain(obj, false);\n";
    } else if (Global::config().get("provenance") == "explore") {
        os << "explain(obj, true);\n";
    }
    os << "return 0;\n";
    os << "} catch(std::exception &e) { souffle::SignalHandler::instance()->error(e.what());}\n";
    os << "}\n";
    os << "\n#endif\n";

    *recordTable_os << "SpecializedRecordTable<0";
    for (std::size_t arity : arities) {
        if (arity > 0) {
            *recordTable_os << "," << arity;
        }
    }
    *recordTable_os << "> recordTable{};\n";

    os.flushAll(sos);
}

}  // namespace souffle::synthesiser
