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
#include "GenDb.h"
#include "Global.h"
#include "RelationTag.h"
#include "config.h"
#include "ram/AbstractParallel.h"
#include "ram/Aggregate.h"
#include "ram/Aggregator.h"
#include "ram/AutoIncrement.h"
#include "ram/Break.h"
#include "ram/Call.h"
#include "ram/Clear.h"
#include "ram/Condition.h"
#include "ram/Conjunction.h"
#include "ram/Constraint.h"
#include "ram/DebugInfo.h"
#include "ram/EmptinessCheck.h"
#include "ram/Erase.h"
#include "ram/ExistenceCheck.h"
#include "ram/Exit.h"
#include "ram/Expression.h"
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
#include "ram/MergeExtend.h"
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
#include "ram/UserDefinedAggregator.h"
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
#include "synthesiser/GenDb.h"
#include "synthesiser/Relation.h"
#include "synthesiser/Utils.h"
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
    std::string id = uniqueCppIdent(name);
    identifiers.insert(std::make_pair(name, id));
    return id;
}

const std::string Synthesiser::convertStratumIdent(const std::string& name) {
    return convertRamIdent(name);
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
void Synthesiser::generateRelationTypeStruct(GenDb& db, Own<Relation> relationType) {
    std::string name = relationType->getTypeName();
    // If this type has been generated already, use the cached version
    if (typeCache.find(name) != typeCache.end()) {
        return;
    }
    typeCache.insert(name);

    // Generate the type struct for the relation
    relationType->generateTypeStruct(db);
}

/** Get referenced relations */
ram::RelationSet Synthesiser::getReferencedRelations(const Operation& op) {
    ram::RelationSet res;
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

std::optional<std::size_t> Synthesiser::compileRegex(const std::string& pattern) {
    auto i = regexes.find(pattern);
    if (i != regexes.end()) {
        return i->second;
    }
    try {
        const std::regex regex(pattern);
        std::size_t index = regexes.size();
        return regexes.emplace(pattern, index).first->second;
    } catch (const std::exception&) {
        std::cerr << "warning: wrong pattern provided \"" << pattern << "\"\n";
        return std::nullopt;
    }
}

void Synthesiser::emitCode(std::ostream& out, const Statement& stmt) {
    class CodeEmitter : public ram::Visitor<void, Node const, std::ostream&> {
        using ram::Visitor<void, Node const, std::ostream&>::visit_;

    private:
        Synthesiser& synthesiser;
        Global& glb;
        IndexAnalysis* const isa = &synthesiser.getTranslationUnit().getAnalysis<IndexAnalysis>();

// macros to add comments to generated code for debugging
#ifndef PRINT_BEGIN_COMMENT
#define PRINT_BEGIN_COMMENT(os)                                          \
    if (glb.config().has("debug-report") || glb.config().has("verbose")) \
    os << "/* BEGIN " << __FUNCTION__ << " @" << __FILE__ << ":" << __LINE__ << " */\n"
#endif

#ifndef PRINT_END_COMMENT
#define PRINT_END_COMMENT(os)                                            \
    if (glb.config().has("debug-report") || glb.config().has("verbose")) \
    os << "/* END " << __FUNCTION__ << " @" << __FILE__ << ":" << __LINE__ << " */\n"
#endif

        // used to populate tuple literal init expressions
        std::function<void(std::ostream&, const Expression*)> rec;
        std::function<void(std::ostream&, const Expression*)> recWithDefault;

        std::ostringstream preamble;
        bool preambleIssued = false;

    public:
        CodeEmitter(Synthesiser& syn) : synthesiser(syn), glb(synthesiser.glb) {
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

            synthesiser.currentClass->addInclude("\"souffle/io/IOSystem.h\"", true);

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
                out << "} catch (std::exception& e) {std::cerr << \"Error loading " << io.getRelation()
                    << " data: \" << e.what() "
                       "<< "
                       "'\\n';\nexit(1);\n}\n";
            } else if (op == "output" || op == "printsize") {
                out << "try {";
                out << "std::map<std::string, std::string> directiveMap(";
                printDirectives(directives);
                out << ");\n";
                out << R"_(if (outputDirectory == "-"){)_";
                out << R"_(directiveMap["IO"] = "stdout"; directiveMap["headers"] = "true";)_";
                out << "}\n";
                out << R"_(else if (!outputDirectory.empty()) {)_";
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

            auto Relation = synthesiser.lookup(clear.getRelation());
            bool isIntermediate =
                    !contains(synthesiser.storeRelations, Relation->getName()) && !Relation->isTemp();

            if (isIntermediate) {
                out << "if (pruneImdtRels) ";
            }
            if (Relation->isTemp() || isIntermediate) {
                out << synthesiser.getRelationName(Relation) << "->purge();\n";
            }

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

        void visit_(type_identity<MergeExtend>, const MergeExtend& extend, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            out << synthesiser.getRelationName(synthesiser.lookup(extend.getSourceRelation())) << "->"
                << "extendAndInsert("
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
            out << "{\n";
            out << " std::vector<RamDomain> args, ret;\n";
            out << synthesiser.convertStratumIdent(call.getName()) << ".run(args, ret);\n";
            out << "}\n";
            PRINT_END_COMMENT(out);
        }

        void visit_(
                type_identity<LogRelationTimer>, const LogRelationTimer& timer, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            // create local scope for name resolution
            out << "{\n";

            const std::string ext = fileExtension(glb.config().get("profile"));

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

            const std::string ext = fileExtension(glb.config().get("profile"));

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
            if (glb.config().has("profile") && glb.config().has("profile-frequency") &&
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
            out << R"cpp(
                   #if defined _OPENMP && _OPENMP < 200805
                           auto count = std::distance(part.begin(), part.end());
                           auto base = part.begin();
                           pfor(int index  = 0; index < count; index++) {
                               auto it = base + index;
                   #else
                           pfor(auto it = part.begin(); it < part.end(); it++) {
                   #endif
                   )cpp";
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
            out << R"cpp(
                   #if defined _OPENMP && _OPENMP < 200805
                           auto count = std::distance(part.begin(), part.end());
                           auto base = part.begin();
                           pfor(int index  = 0; index < count; index++) {
                               auto it = base + index;
                   #else
                           pfor(auto it = part.begin(); it < part.end(); it++) {
                   #endif
                   )cpp";
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

        void visit_(type_identity<EstimateJoinSize>, const EstimateJoinSize& estimateJoinSize,
                std::ostream& out) override {
            const auto* rel = synthesiser.lookup(estimateJoinSize.getRelation());
            auto relName = synthesiser.getRelationName(rel);
            auto keys = isa->getSearchSignature(&estimateJoinSize);

            std::size_t indexNumber = 0;
            if (!keys.empty()) {
                indexNumber = isa->getIndexSelection(estimateJoinSize.getRelation()).getLexOrderNum(keys);
            }
            auto indexName = relName + "->ind_" + std::to_string(indexNumber);

            bool onlyConstants = true;
            for (auto col : estimateJoinSize.getKeyColumns()) {
                if (estimateJoinSize.getConstantsMap().count(col) == 0) {
                    onlyConstants = false;
                    break;
                }
            }

            // create a copy of the map to the real numeric constants
            std::map<std::size_t, RamDomain> keyConstants;
            for (auto [k, constant] : estimateJoinSize.getConstantsMap()) {
                RamDomain value;
                if (const auto* signedConstant = as<ram::SignedConstant>(constant)) {
                    value = ramBitCast<RamDomain>(signedConstant->getValue());
                } else if (const auto* stringConstant = as<ram::StringConstant>(constant)) {
                    value = ramBitCast<RamDomain>(
                            synthesiser.convertSymbol2Idx(stringConstant->getConstant()));
                } else if (const auto* unsignedConstant = as<ram::UnsignedConstant>(constant)) {
                    value = ramBitCast<RamDomain>(unsignedConstant->getValue());
                } else if (const auto* floatConstant = as<ram::FloatConstant>(constant)) {
                    value = ramBitCast<RamDomain>(floatConstant->getValue());
                } else {
                    fatal("Something went wrong. Should have gotten a constant!");
                }

                keyConstants[k] = value;
            }
            std::stringstream columnsStream;
            columnsStream << estimateJoinSize.getKeyColumns();
            std::string columns = columnsStream.str();

            std::stringstream constantsStream;
            constantsStream << "{";
            bool first = true;
            for (auto& [k, constant] : estimateJoinSize.getConstantsMap()) {
                if (first) {
                    first = false;
                } else {
                    constantsStream << ",";
                }
                constantsStream << k << "->" << *constant;
            }
            constantsStream << "}";
            std::string constants = stringify(constantsStream.str());

            std::string profilerText =
                    (estimateJoinSize.isRecursiveRelation() ? stringify("@recursive-estimate-join-size;" +
                                                                        estimateJoinSize.getRelation() + ";" +
                                                                        columns + ";" + constants)
                                                            : stringify("@non-recursive-estimate-join-size;" +
                                                                        estimateJoinSize.getRelation() + ";" +
                                                                        columns + ";" + constants));

            PRINT_BEGIN_COMMENT(out);
            auto ctxName = "READ_OP_CONTEXT(" + synthesiser.getOpContextName(*rel) + ")";
            out << "{\n";
            out << "double total = 0;\n";
            out << "double duplicates = 0;\n";

            out << "if (!" << indexName << ".empty()) {\n";
            out << "bool first = true;\n";
            out << "auto prev = *" << indexName << ".begin();\n";
            out << "for(const auto& tup : " << indexName << ") {\n";
            out << "    bool matchesConstants = true;\n";
            for (auto& [k, constant] : keyConstants) {
                if (rel->getArity() > 6) {
                    out << "matchesConstants &= (tup[0][" << k << "] == " << constant << ");\n";
                } else {
                    out << "matchesConstants &= (tup[" << k << "] == " << constant << ");\n";
                }
            }
            out << "if (!matchesConstants) {\n";
            out << "    continue;\n";
            out << "}\n";
            out << "if (first) { first = false; }\n";
            out << "else {\n";
            out << "    bool matchesPrev = true;\n";
            for (auto k : estimateJoinSize.getKeyColumns()) {
                if (rel->getArity() > 6) {
                    out << "matchesPrev &= (tup[0][" << k << "] == prev[0][" << k << "]);\n";
                } else {
                    out << "matchesPrev &= (tup[" << k << "] == prev[" << k << "]);\n";
                }
            }
            out << "if (matchesPrev) { ++duplicates; }\n";
            out << "}\n";
            out << "prev = tup; ++total;\n";
            out << "\n";
            out << "}\n";
            out << "}\n";
            out << "double joinSize = ("
                << (onlyConstants ? "total" : "total / std::max(1.0, (total - duplicates))") << ");\n";
            if (estimateJoinSize.isRecursiveRelation()) {
                out << "ProfileEventSingleton::instance().makeRecursiveCountEvent(\"" << profilerText
                    << "\", joinSize, iter);\n";
            } else {
                out << "ProfileEventSingleton::instance().makeNonRecursiveCountEvent(\"" << profilerText
                    << "\", joinSize);\n";
            }
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
            out << R"cpp(
                   #if defined _OPENMP && _OPENMP < 200805
                           auto count = std::distance(part.begin(), part.end());
                           auto base = part.begin();
                           pfor(int index  = 0; index < count; index++) {
                               auto it = base + index;
                   #else
                           pfor(auto it = part.begin(); it < part.end(); it++) {
                   #endif
                   )cpp";
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
            out << R"cpp(
                   #if defined _OPENMP && _OPENMP < 200805
                           auto count = std::distance(part.begin(), part.end());
                           auto base = part.begin();
                           pfor(int index  = 0; index < count; index++) {
                               auto it = base + index;
                   #else
                           pfor(auto it = part.begin(); it < part.end(); it++) {
                   #endif
                   )cpp";
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

        std::string initValue(const Aggregator& aggregator) {
            if (const auto* ia = as<ram::IntrinsicAggregator>(aggregator)) {
                switch (ia->getFunction()) {
                    case AggregateOp::MIN: return "MAX_RAM_SIGNED";
                    case AggregateOp::FMIN: return "MAX_RAM_FLOAT";
                    case AggregateOp::UMIN: return "MAX_RAM_UNSIGNED";
                    case AggregateOp::MAX: return "MIN_RAM_SIGNED";
                    case AggregateOp::FMAX: return "MIN_RAM_FLOAT";
                    case AggregateOp::UMAX: return "MIN_RAM_UNSIGNED";
                    case AggregateOp::COUNT:
                    case AggregateOp::MEAN:
                    case AggregateOp::FSUM:
                    case AggregateOp::USUM:
                    case AggregateOp::SUM: return "0";
                }
            } else if (const auto* uda = as<ram::UserDefinedAggregator>(aggregator)) {
                assert(uda);
                std::stringstream ss;
                dispatch(*uda->getInitValue(), ss);
                return ss.str();
            }
            fatal("Unhandled aggregate operation");
        }

        void updateRes(std::ostream& out, const AbstractAggregate& aggregate) {
            const auto& aggregator = aggregate.getAggregator();
            if (const auto* ia = as<ram::IntrinsicAggregator>(aggregator)) {
                AggregateOp aggregateFun = ia->getFunction();
                std::string type = getType(aggregator);
                switch (aggregateFun) {
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
            } else if (const auto* uda = as<ram::UserDefinedAggregator>(aggregator)) {
                out << "res0 = " << uda->getName() << "(";
                if (uda->isStateful()) {
                    out << "&symTable, &recordTable, ";
                }
                out << "res0, ";
                dispatch(aggregate.getExpression(), out);
                out << ");\n";
            }
        }

        bool shouldRunNested(const Aggregator& aggregator) {
            if (const auto* ia = as<ram::IntrinsicAggregator>(aggregator)) {
                switch (ia->getFunction()) {
                    case AggregateOp::COUNT:
                    case AggregateOp::FSUM:
                    case AggregateOp::USUM:
                    case AggregateOp::SUM: return true;
                    default: return false;
                }
            } else if (isA<ram::UserDefinedAggregator>(aggregator)) {
                return false;
            }
            fatal("Unhandled aggregate operation");
        }

        std::string getType(const Aggregator& aggregator) {
            auto str = [&](souffle::TypeAttribute ta) {
                switch (ta) {
                    case TypeAttribute::Signed: return "RamSigned";
                    case TypeAttribute::Unsigned: return "RamUnsigned";
                    case TypeAttribute::Float: return "RamFloat";
                    case TypeAttribute::Symbol:
                    case TypeAttribute::ADT:
                    case TypeAttribute::Record: return "RamDomain";
                    default: return "RamDomain";
                }
            };
            if (const auto* ia = as<ram::IntrinsicAggregator>(aggregator)) {
                return str(getTypeAttributeAggregate(ia->getFunction()));
            } else if (const auto* uda = as<ram::UserDefinedAggregator>(aggregator)) {
                return str(uda->getReturnType());
            }
            fatal("Unhandled aggregator");
        }

        std::tuple<std::string, std::string, int> reductionOperation(const Aggregator& aggregator) {
            if (const auto* ia = as<ram::IntrinsicAggregator>(aggregator)) {
                switch (ia->getFunction()) {
                    case AggregateOp::MIN:
                    case AggregateOp::FMIN:
                    case AggregateOp::UMIN: return std::make_tuple("min", "", 200805);
                    case AggregateOp::MAX:
                    case AggregateOp::FMAX:
                    case AggregateOp::UMAX: return std::make_tuple("max", "", 200805);
                    case AggregateOp::MEAN:
                    case AggregateOp::FSUM:
                    case AggregateOp::USUM:
                    case AggregateOp::COUNT:
                    case AggregateOp::SUM: return std::make_tuple("+", "", 0);
                    default: fatal("Unhandled aggregate operation");
                }
            } else if (const auto* uda = as<ram::UserDefinedAggregator>(aggregator)) {
                std::stringstream def;
                std::string name = uda->getName();
                def << "#pragma omp declare reduction("
                    << "reduction_" << name << " : " << getType(aggregator) << " : \\\n";
                // TODO: does not work for stateful functors
                def << "omp_out = " << name << "(omp_out, omp_in) )\\\n";
                def << "initializer (omp_priv=(omp_orig))\n";
                return std::make_tuple("reduction_" + name, def.str(), 0);
            }
            fatal("Unhandled aggregator");
        }

        void ifIntrinsic(const ram::Aggregator& aggregator, AggregateOp op, std::function<void()> fn) {
            if (const auto* ia = as<ram::IntrinsicAggregator>(aggregator)) {
                if (ia->getFunction() == op) {
                    fn();
                };
            }
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

            const ram::Aggregator& aggregator = aggregate.getAggregator();

            bool isCount = false;
            ifIntrinsic(aggregator, AggregateOp::COUNT, [&]() { isCount = true; });

            // special case: counting number elements over an unrestricted predicate
            if (isCount && keys.empty() && isTrue(&aggregate.getCondition())) {
                // shortcut: use relation size
                out << "env" << identifier << "[0] = " << relName << "->"
                    << "size();\n";
                out << "{\n";  // to match PARALLEL_END closing bracket
                out << preamble.str();
                visit_(type_identity<TupleOperation>(), aggregate, out);
                PRINT_END_COMMENT(out);
                return;
            }

            // init result and reduction operation
            std::string init = initValue(aggregator);
            out << "bool shouldRunNested = " << (shouldRunNested(aggregator) ? "true" : "false") << ";\n";

            // Set reduction operation
            std::string op;
            std::string op_def;
            int omp_min_ver;
            std::tie(op, op_def, omp_min_ver) = reductionOperation(aggregator);

            // res0 stores the aggregate result
            std::string sharedVariable = "res0";

            std::string type = getType(aggregator);

            out << type << " res0 = " << init << ";\n";
            ifIntrinsic(aggregator, AggregateOp::MEAN, [&]() {
                out << "RamUnsigned res1 = 0;\n";
                sharedVariable += ", res1";
            });

            out << preamble.str();
            out << "PARALLEL_START\n";
            // check whether there is an index to use
            if (keys.empty()) {
                // OMP reduction is not available on all versions of OpenMP
                out << "#if defined _OPENMP && _OPENMP >= " << omp_min_ver << "\n";
                out << op_def << "\n";
                out << "#pragma omp for reduction(" << op << ":" << sharedVariable << ")\n";
                out << "#endif\n";

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

                // old OpenMP versions cannot loop on iterators
                out << R"cpp(
                   #if defined _OPENMP && _OPENMP < 200805
                           auto count = std::distance(part.begin(), part.end());
                           auto base = part.begin();
                   #endif
                   )cpp";

                // OMP reduction is not available on all versions of OpenMP
                out << "#if defined _OPENMP && _OPENMP >= " << omp_min_ver << "\n";
                out << op_def << "\n";
                out << "#pragma omp for reduction(" << op << ":" << sharedVariable << ")\n";
                out << "#endif\n";

                // iterate over each part
                out << R"cpp(
                   #if defined _OPENMP && _OPENMP < 200805
                           for(int index  = 0; index < count; index++) {
                               auto it = base + index;
                   #else
                           for(auto it = part.begin(); it < part.end(); ++it) {
                   #endif
                   )cpp";
                // iterate over tuples in each part
                out << "for (const auto& env" << identifier << ": *it) {\n";
            }

            // produce condition inside the loop if necessary
            out << "if( ";
            dispatch(aggregate.getCondition(), out);
            out << ") {\n";

            out << "shouldRunNested = true;\n";

            // pick function
            updateRes(out, aggregate);

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

            ifIntrinsic(aggregator, AggregateOp::MEAN, [&]() {
                out << "if (res1 != 0) {\n";
                out << "res0 = res0 / res1;\n";
                out << "}\n";
            });

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

            const ram::Aggregator& aggregator = aggregate.getAggregator();

            bool isCount = false;
            ifIntrinsic(aggregator, AggregateOp::COUNT, [&]() { isCount = true; });

            // special case: counting number elements over an unrestricted predicate
            if (isCount && keys.empty() && isTrue(&aggregate.getCondition())) {
                // shortcut: use relation size
                out << "env" << identifier << "[0] = " << relName << "->"
                    << "size();\n";
                visit_(type_identity<TupleOperation>(), aggregate, out);
                PRINT_END_COMMENT(out);
                return;
            }

            // init result
            std::string init = initValue(aggregator);
            out << "bool shouldRunNested = " << (shouldRunNested(aggregator) ? "true" : "false") << ";\n";

            std::string type = getType(aggregator);

            out << type << " res0 = " << init << ";\n";

            ifIntrinsic(aggregator, AggregateOp::MEAN, [&]() { out << "RamUnsigned res1 = 0;\n"; });

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
            updateRes(out, aggregate);
            auto printBreak = [&]() {
                if (isGuaranteedToBeMinimum(aggregate)) {
                    out << "break;\n";
                }
            };
            ifIntrinsic(aggregator, AggregateOp::FMIN, printBreak);
            ifIntrinsic(aggregator, AggregateOp::UMIN, printBreak);
            ifIntrinsic(aggregator, AggregateOp::MIN, printBreak);

            out << "}\n";

            // end aggregator loop
            out << "}\n";

            ifIntrinsic(aggregator, AggregateOp::MEAN, [&]() {
                out << "if (res1 != 0) {\n";
                out << "res0 = res0 / res1;\n";
                out << "}\n";
            });

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

            const ram::Aggregator& aggregator = aggregate.getAggregator();

            bool isCount = false;
            ifIntrinsic(aggregator, AggregateOp::COUNT, [&]() { isCount = true; });

            // special case: counting number elements over an unrestricted predicate
            if (isCount && isTrue(&aggregate.getCondition())) {
                // shortcut: use relation size
                out << "env" << identifier << "[0] = " << relName << "->"
                    << "size();\n";
                out << "PARALLEL_START\n";
                out << preamble.str();
                visit_(type_identity<TupleOperation>(), aggregate, out);
                PRINT_END_COMMENT(out);
                return;
            }

            // init result
            std::string init = initValue(aggregator);
            out << "bool shouldRunNested = " << (shouldRunNested(aggregator) ? "true" : "false") << ";\n";

            // Set reduction operation
            std::string op;
            std::string op_def;
            int omp_min_ver;
            std::tie(op, op_def, omp_min_ver) = reductionOperation(aggregator);

            std::string type = getType(aggregator);

            out << type << " res0 = " << init << ";\n";

            std::string sharedVariable = "res0";
            ifIntrinsic(aggregator, AggregateOp::MEAN, [&]() {
                out << "RamUnsigned res1 = " << init << ";\n";
                sharedVariable += ", res1";
            });

            // create a partitioning of the relation to iterate over simeltaneously
            out << "auto part = " << relName << "->partition();\n";
            out << "PARALLEL_START\n";
            out << preamble.str();

            // old OpenMP versions cannot loop on iterators
            out << R"cpp(
                   #if defined _OPENMP && _OPENMP < 200805
                           auto count = std::distance(part.begin(), part.end());
                           auto base = part.begin();
                   #endif
                   )cpp";

            // pragma statement
            out << "#if defined _OPENMP && _OPENMP >= " << omp_min_ver << "\n";
            out << op_def << "\n";
            out << "#pragma omp for reduction(" << op << ":" << sharedVariable << ")\n";
            out << "#endif\n";

            // iterate over each part
            out << R"cpp(
                   #if defined _OPENMP && _OPENMP < 200805
                           for(int index  = 0; index < count; index++) {
                               auto it = base + index;
                   #else
                           for(auto it = part.begin(); it < part.end(); ++it) {
                   #endif
                   )cpp";
            // iterate over tuples in each part
            out << "for (const auto& env" << identifier << ": *it) {\n";

            // produce condition inside the loop
            out << "if( ";
            dispatch(aggregate.getCondition(), out);
            out << ") {\n";

            out << "shouldRunNested = true;\n";
            // pick function
            updateRes(out, aggregate);

            out << "}\n";

            // end aggregator loop
            out << "}\n";
            // end partition loop
            out << "}\n";

            // the rest shouldn't be run in parallel
            out << "#pragma omp single\n{\n";

            ifIntrinsic(aggregator, AggregateOp::MEAN, [&]() {
                out << "if (res1 != 0) {\n";
                out << "res0 = res0 / res1;\n";
                out << "}\n";
            });

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

            const ram::Aggregator& aggregator = aggregate.getAggregator();

            bool isCount = false;
            ifIntrinsic(aggregator, AggregateOp::COUNT, [&]() { isCount = true; });

            // special case: counting number elements over an unrestricted predicate
            if (isCount && isTrue(&aggregate.getCondition())) {
                // shortcut: use relation size
                out << "env" << identifier << "[0] = " << relName << "->"
                    << "size();\n";
                visit_(type_identity<TupleOperation>(), aggregate, out);
                PRINT_END_COMMENT(out);
                return;
            }

            // init result
            std::string init = initValue(aggregator);
            out << "bool shouldRunNested = " << (shouldRunNested(aggregator) ? "true" : "false") << ";\n";

            std::string type = getType(aggregator);

            out << type << " res0 = " << init << ";\n";

            ifIntrinsic(aggregator, AggregateOp::MEAN, [&]() { out << "RamUnsigned res1 = 0;\n"; });

            // check whether there is an index to use
            out << "for(const auto& env" << identifier << " : "
                << "*" << relName << ") {\n";

            // produce condition inside the loop
            out << "if( ";
            dispatch(aggregate.getCondition(), out);
            out << ") {\n";

            out << "shouldRunNested = true;\n";
            // pick function
            updateRes(out, aggregate);

            out << "}\n";

            // end aggregator loop
            out << "}\n";

            ifIntrinsic(aggregator, AggregateOp::MEAN, [&]() { out << "res0 = res0 / res1;\n"; });

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

        void visit_(type_identity<Erase>, const Erase& erase, std::ostream& out) override {
            PRINT_BEGIN_COMMENT(out);
            const auto* rel = synthesiser.lookup(erase.getRelation());
            auto arity = rel->getArity();
            auto relName = synthesiser.getRelationName(rel);
            // create inserted tuple
            out << "Tuple<RamDomain," << arity << "> tuple{{" << join(erase.getValues(), ",", rec) << "}};\n";

            // insert tuple
            out << relName << "->erase(tuple);\n";
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
                    if (const StringConstant* str = dynamic_cast<const StringConstant*>(&rel.getLHS()); str) {
                        const auto& regex = synthesiser.compileRegex(str->getConstant());
                        if (regex) {
                            out << "std::regex_match(symTable.decode(";
                            dispatch(rel.getRHS(), out);
                            out << "), regexes.at(" << *regex << "))";
                        } else {
                            out << "false";
                        }
                    } else {
                        synthesiser.SubroutineUsingStdRegex = true;
                        out << "regex_wrapper(symTable.decode(";
                        dispatch(rel.getLHS(), out);
                        out << "),symTable.decode(";
                        dispatch(rel.getRHS(), out);
                        out << "))";
                    }
                    break;
                }
                case BinaryConstraintOp::NOT_MATCH: {
                    if (const StringConstant* str = dynamic_cast<const StringConstant*>(&rel.getLHS()); str) {
                        const auto& regex = synthesiser.compileRegex(str->getConstant());
                        if (regex) {
                            out << "!std::regex_match(symTable.decode(";
                            dispatch(rel.getRHS(), out);
                            out << "), regexes.at(" << *regex << "))";
                        } else {
                            out << "false";
                        }
                    } else {
                        synthesiser.SubroutineUsingStdRegex = true;
                        out << "!regex_wrapper(symTable.decode(";
                        dispatch(rel.getLHS(), out);
                        out << "),symTable.decode(";
                        dispatch(rel.getRHS(), out);
                        out << "))";
                    }
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
            if (glb.config().has("profile") && glb.config().has("profile-frequency") &&
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
#define CONV_FROM_STRING(opcode, ty)                                                       \
    case FunctorOp::opcode: {                                                              \
        synthesiser.currentClass->addInclude("\"souffle/utility/EvaluatorUtil.h\"", true); \
        out << "souffle::evaluator::symbol2numeric<" #ty ">(symTable.decode(";             \
        dispatch(*args[0], out);                                                           \
        out << "))";                                                                       \
    } break;
            // clang-format on
            if (op.getOperator() == FunctorOp::LXOR) {
                synthesiser.currentClass->addInclude("\"souffle/utility/EvaluatorUtil.h\"", true);
            }
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
                    synthesiser.SubroutineUsingSubstr = true;
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
                synthesiser.currentClass->addInclude("\"souffle/utility/EvaluatorUtil.h\"", true);
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

std::set<std::string> Synthesiser::accessedRelations(Statement& stmt) {
    std::set<std::string> accessed;
    visit(stmt, [&](const Insert& node) { accessed.insert(node.getRelation()); });
    visit(stmt, [&](const RelationOperation& node) { accessed.insert(node.getRelation()); });
    visit(stmt, [&](const RelationStatement& node) { accessed.insert(node.getRelation()); });
    visit(stmt, [&](const AbstractExistenceCheck& node) { accessed.insert(node.getRelation()); });
    visit(stmt, [&](const EmptinessCheck& node) { accessed.insert(node.getRelation()); });
    visit(stmt, [&](const RelationSize& node) { accessed.insert(node.getRelation()); });
    visit(stmt, [&](const BinRelationStatement& node) {
        accessed.insert(node.getFirstRelation());
        accessed.insert(node.getSecondRelation());
    });
    return accessed;
}

std::set<std::string> Synthesiser::accessedUserDefinedFunctors(Statement& stmt) {
    std::set<std::string> accessed;
    visit(stmt, [&](const UserDefinedOperator& node) {
        const std::string& name = node.getName();
        accessed.insert(name);
    });
    auto visitAggregate = [&](const AbstractAggregate& op) {
        const Aggregator& aggregator = op.getAggregator();
        if (const auto* uda = as<UserDefinedAggregator>(aggregator)) {
            accessed.insert(uda->getName());
        }
    };
    visit(stmt, [&](const Aggregate& op) { visitAggregate(op); });
    visit(stmt, [&](const IndexAggregate& op) { visitAggregate(op); });
    return accessed;
};

void Synthesiser::generateCode(GenDb& db, const std::string& id, bool& withSharedLibrary) {
    // ---------------------------------------------------------------
    //                      Auto-Index Generation
    // ---------------------------------------------------------------
    const Program& prog = translationUnit.getProgram();
    auto& idxAnalysis = translationUnit.getAnalysis<IndexAnalysis>();
    // ---------------------------------------------------------------
    //                      Code Generation
    // ---------------------------------------------------------------

    withSharedLibrary = false;

    std::string classname = "Sf_" + id;

    // generate C++ program
    std::string package_gen_version = "SOUFFLE_GENERATOR_VERSION \"";
    package_gen_version += PACKAGE_VERSION;
    package_gen_version += "\"";
    db.addGlobalDefine(package_gen_version);

    if (glb.config().has("verbose")) {
        db.addGlobalDefine("_SOUFFLE_STATS");
        db.addGlobalInclude("\"souffle/profile/ProfileEvent.h\"");
    }

    if (glb.config().has("provenance")) {
        db.addGlobalInclude("<mutex>");
        db.addGlobalInclude("\"souffle/provenance/Explain.h\"");
    }

    if (glb.config().has("live-profile")) {
        db.addGlobalInclude("<thread>");
        db.addGlobalInclude("\"souffle/profile/Tui.h\"");
    }

    if (glb.config().has("profile") || glb.config().has("live-profile")) {
        db.addGlobalInclude("\"souffle/profile/Logger.h\"");
        db.addGlobalInclude("\"souffle/profile/ProfileEvent.h\"");
    }

    if (glb.config().has("generate-namespace")) {
        db.setNS(glb.config().get("generate-namespace"));
    } else {
        db.setNS("souffle");
    }

    // produce external definitions for user-defined functors
    std::map<std::string, std::tuple<TypeAttribute, std::vector<TypeAttribute>, bool>> functors;
    visit(prog, [&](const UserDefinedOperator& op) {
        if (functors.find(op.getName()) == functors.end()) {
            functors[op.getName()] = std::make_tuple(op.getReturnType(), op.getArgsTypes(), op.isStateful());
        }
        withSharedLibrary = true;
    });
    auto visitAggregate = [&](const AbstractAggregate& op) {
        const Aggregator& aggregator = op.getAggregator();
        if (const auto* uda = as<UserDefinedAggregator>(aggregator)) {
            functors[uda->getName()] =
                    std::make_tuple(uda->getReturnType(), uda->getArgsTypes(), uda->isStateful());
            withSharedLibrary = true;
        }
    };
    visit(prog, [&](const Aggregate& op) { visitAggregate(op); });
    visit(prog, [&](const IndexAggregate& op) { visitAggregate(op); });

    for (const auto& f : functors) {
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

        std::vector<std::string> argsTy;
        std::string retTy;
        if (stateful) {
            retTy = "souffle::RamDomain";
            argsTy.push_back("souffle::SymbolTable*");
            argsTy.push_back("souffle::RecordTable*");
            for (std::size_t i = 0; i < argsTypes.size(); i++) {
                argsTy.push_back("souffle::RamDomain");
            }
        } else {
            retTy = cppTypeDecl(returnType);
            for (auto ty : argsTypes) {
                argsTy.push_back(cppTypeDecl(ty));
            }
        }
        functor_signatures[name] = std::make_pair(argsTy, retTy);
        auto extern_decl = [&](std::ostream& os) {
            os << retTy << " " << name << "("
               << join(argsTy, ", ", [&](auto& out, const std::string ty) { out << ty; }) << ");\n";
        };

        extern_decl(db.externC());
    }

    // main class
    GenClass& mainClass = db.getClass(classname, fs::path(classname));
    mainClass.inherits("public SouffleProgram");
    mainClass.addInclude("\"souffle/CompiledSouffle.h\"");
    mainClass.addInclude("<any>");
    mainClass.isMain = true;

    auto function_ty = [&](std::string name) -> std::string {
        auto [argsTy, retTy] = functor_signatures[name];
        std::stringstream os;
        os << "std::function<" << retTy << "("
           << join(argsTy, ", ", [&](auto& out, const std::string ty) { out << ty; }) << ")>";
        return os.str();
    };
    auto functors_initialize = [&](std::ostream& os, std::string name) {
        auto [argsTy, retTy] = functor_signatures[name];
        os << name << " = functors::" << name << ";\n";
    };

    std::map<std::string, std::string> relationTypes;

    // synthesise data-structures for relations
    for (auto rel : prog.getRelations()) {
        auto relationType =
                Relation::getSynthesiserRelation(*rel, idxAnalysis.getIndexSelection(rel->getName()));

        std::string typeName = relationType->getTypeName();
        generateRelationTypeStruct(db, std::move(relationType));

        relationTypes[getRelationName(*rel)] = typeName;

        db.usesDatastructure(mainClass, typeName);
    }

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

    // identify relations used by each subroutines
    std::multimap<std::string /* stratum_* */, std::string> subroutineUses;

    // generate class for each subroutine
    std::vector<std::pair<std::string, std::string>> subroutineInits;
    for (auto& sub : prog.getSubroutines()) {
        GenClass& gen = db.getClass(convertStratumIdent("Stratum_" + sub.first),
                fs::path(convertStratumIdent("Stratum_" + sub.first)));
        mainClass.addDependency(gen);

        auto accessedRels = accessedRelations(*sub.second);
        auto accessedFunctors = accessedUserDefinedFunctors(*sub.second);

        gen.addInclude("\"souffle/SouffleInterface.h\"");
        gen.addInclude("\"souffle/SignalHandler.h\"");

        GenFunction& constructor = gen.addConstructor(Visibility::Public);

        enum Mode { Reference, Relation };
        std::vector<std::tuple<Mode, std::string /*name*/, std::string /*type*/>> args;
        args.push_back(std::make_tuple(Reference, "symTable", "SymbolTable"));
        args.push_back(std::make_tuple(Reference, "recordTable", "RecordTable"));
        args.push_back(std::make_tuple(Reference, "regexCache", "ConcurrentCache<std::string,std::regex>"));
        args.push_back(std::make_tuple(Reference, "pruneImdtRels", "bool"));
        args.push_back(std::make_tuple(Reference, "performIO", "bool"));
        args.push_back(std::make_tuple(Reference, "signalHandler", "SignalHandler*"));
        args.push_back(std::make_tuple(Reference, "iter", "std::atomic<std::size_t>"));
        args.push_back(std::make_tuple(Reference, "ctr", "std::atomic<RamDomain>"));
        args.push_back(std::make_tuple(Reference, "inputDirectory", "std::string"));
        args.push_back(std::make_tuple(Reference, "outputDirectory", "std::string"));
        for (std::string rel : accessedRels) {
            std::string name = getRelationName(lookup(rel));
            std::string tyname = relationTypes[name];
            args.push_back(std::make_tuple(Relation, name, tyname));
            db.usesDatastructure(gen, tyname);
        }
        for (std::string fn : accessedFunctors) {
            args.push_back(std::make_tuple(Reference, fn, function_ty(fn)));
        }

        for (auto arg : args) {
            Mode kind;
            std::string name, ty;
            std::tie(kind, name, ty) = arg;
            constructor.setNextArg(ty + std::string("&"), name);

            constructor.setNextInitializer(
                    name, (kind == Relation ? std::string("&") : std::string("")) + name);

            gen.addField(ty + (kind == Relation ? "*" : "&"), name, Visibility::Private);
        }
        std::stringstream initStr;
        initStr << join(args, ",", [&](auto& out, const auto arg) {
            Mode kind;
            std::string name, ty;
            std::tie(kind, name, ty) = arg;
            out << (kind == Relation ? "*" : "") << name;
        });
        subroutineInits.push_back(std::make_pair(sub.first, initStr.str()));

        GenFunction& run = gen.addFunction("run", Visibility::Public);
        run.setRetType("void");
        run.setNextArg("[[maybe_unused]] const std::vector<RamDomain>&", "args");
        run.setNextArg("[[maybe_unused]] std::vector<RamDomain>&", "ret");

        bool needLock = false;
        visit(*sub.second, [&](const SubroutineReturn&) { needLock = true; });
        if (needLock) {
            run.body() << "std::mutex lock;\n";
        }
        SubroutineUsingStdRegex = false;
        SubroutineUsingSubstr = false;
        // emit code for subroutine
        currentClass = &gen;
        emitCode(run.body(), *sub.second);
        // issue end of subroutine
        UsingStdRegex |= SubroutineUsingStdRegex;

        if (SubroutineUsingStdRegex) {
            // regex wrapper
            GenFunction& wrapper = gen.addFunction("regex_wrapper", Visibility::Private);
            wrapper.setRetType("inline bool");
            wrapper.setNextArg("const std::string&", "pattern");
            wrapper.setNextArg("const std::string&", "text");
            wrapper.body()
                    << "   bool result = false; \n"
                    << "   try { result = std::regex_match(text, regexCache.getOrCreate(pattern)); } "
                       "catch(...) { "
                       "\n"
                    << "     std::cerr << \"warning: wrong pattern provided for match(\\\"\" << pattern << "
                       "\"\\\",\\\"\" "
                       "<< text << \"\\\").\\n\";\n}\n"
                    << "   return result;\n";
        }

        if (!regexes.empty()) {
            gen.addField("std::vector<std::regex>", "regexes", Visibility::Private);
            std::stringstream rst;
            // we need to collect the patterns first and place each
            // one into the correct slot
            std::vector<std::string> patterns;
            patterns.resize(regexes.size());
            for (const auto& pi : regexes) {
                patterns.at(pi.second) = pi.first;
            }
            rst << "{\n";
            for (const auto& p : patterns) {
                const std::string escaped = escape(p);
                rst << "\tstd::regex(\"" << escaped << "\"),\n";
            }
            rst << "}";

            constructor.setNextInitializer("regexes", rst.str());
            regexes.clear();
        }

        // substring wrapper
        if (SubroutineUsingSubstr) {
            GenFunction& wrapper = gen.addFunction("substr_wrapper", Visibility::Private);
            wrapper.setRetType("inline std::string");
            wrapper.setNextArg("const std::string&", "str");
            wrapper.setNextArg("std::size_t", "idx");
            wrapper.setNextArg("std::size_t", "len");
            wrapper.body() << "std::string result; \n"
                           << "try { result = str.substr(idx,len); } catch(...) { \n"
                           << "  std::cerr << \"warning: wrong index position provided by substr(\\\"\";\n"
                           << "  std::cerr << str << \"\\\",\" << (int32_t)idx << \",\" << (int32_t)len << "
                              "\") functor.\\n\";\n"
                           << "} return result;\n";
        }
    }

    GenFunction& constructor = mainClass.addConstructor(Visibility::Public);
    constructor.setIsConstructor();

    if (glb.config().has("profile")) {
        mainClass.addField("std::string", "profiling_fname", Visibility::Public);
        constructor.setNextArg("std::string", "pf", std::make_optional("\"profile.log\""));
        constructor.setNextInitializer("profiling_fname", "std::move(pf)");
    }

    // issue symbol table with string constants
    visit(prog, [&](const StringConstant& sc) { convertSymbol2Idx(sc.getConstant()); });
    std::stringstream st;
    if (!symbolMap.empty()) {
        st << "{\n";
        for (const auto& x : symbolIndex) {
            st << "\tR\"_(" << x << ")_\",\n";
        }
        st << "}";
    }
    mainClass.addField("SymbolTableImpl", "symTable", Visibility::Private);
    constructor.setNextInitializer("symTable", st.str());

    // declare record table
    std::stringstream rt;
    rt << "SpecializedRecordTable<0";
    for (std::size_t arity : arities) {
        if (arity > 0) {
            rt << "," << arity;
        }
    }
    rt << ">";
    mainClass.addField(rt.str(), "recordTable", Visibility::Private);
    constructor.setNextInitializer("recordTable", "");

    mainClass.addField("ConcurrentCache<std::string,std::regex>", "regexCache", Visibility::Private);
    constructor.setNextInitializer("regexCache", "");

    if (glb.config().has("profile")) {
        std::size_t numFreq = 0;
        visit(prog, [&](const Statement&) { numFreq++; });
        mainClass.addField("std::size_t", "freqs[" + std::to_string(numFreq) + "]", Visibility::Private);
        constructor.setNextInitializer("freqs", "");
        std::size_t numRead = 0;
        for (auto rel : prog.getRelations()) {
            if (!rel->isTemp()) {
                numRead++;
            }
        }
        mainClass.addField("std::size_t", "reads[" + std::to_string(numRead) + "]", Visibility::Private);
        constructor.setNextInitializer("reads", "");
    }

    for (const auto& f : functors) {
        const std::string& name = f.first;
        mainClass.addField(function_ty(name), name, Visibility::Private);
    }

    int relCtr = 0;
    for (auto rel : prog.getRelations()) {
        // get some table details
        const std::string& datalogName = rel->getName();
        const std::string& cppName = getRelationName(*rel);

        auto relationType =
                Relation::getSynthesiserRelation(*rel, idxAnalysis.getIndexSelection(datalogName));
        const std::string& type = relationType->getTypeName();

        // defining table
        mainClass.addField("Own<" + type + ">", cppName, Visibility::Private);
        constructor.setNextInitializer(cppName, "mk<" + type + ">()");
        if (!rel->isTemp()) {
            std::stringstream ty, init, wrapper_name;
            ty << "souffle::RelationWrapper<" << type << ">";
            wrapper_name << "wrapper_" << cppName;

            auto strLitAry = [](auto&& xs) {
                std::stringstream ss;
                ss << "std::array<const char *," << xs.size() << ">{{"
                   << join(xs, ",", [](auto&& os, auto&& x) { os << '"' << x << '"'; }) << "}}";
                return ss.str();
            };

            auto foundIn = [&](auto&& set) { return contains(set, rel->getName()) ? "true" : "false"; };

            init << relCtr++ << ", *" << cppName << ", *this, \"" << datalogName << "\", "
                 << strLitAry(rel->getAttributeTypes()) << ", " << strLitAry(rel->getAttributeNames()) << ", "
                 << rel->getAuxiliaryArity();
            constructor.body() << "addRelation(\"" << datalogName << "\", wrapper_" << cppName << ", "
                               << foundIn(loadRelations) << ", " << foundIn(storeRelations) << ");\n";

            mainClass.addField(ty.str(), wrapper_name.str(), Visibility::Private);
            constructor.setNextInitializer(wrapper_name.str(), init.str());
        }
    }

    for (auto [name, value] : subroutineInits) {
        std::string clName = convertStratumIdent("Stratum_" + name);
        std::string fName = convertStratumIdent("stratum_" + name);
        mainClass.addField(clName, fName, Visibility::Private);
        constructor.setNextInitializer(fName, value);
    }

    if (glb.config().has("profile")) {
        constructor.body() << "ProfileEventSingleton::instance().setOutputFile(profiling_fname);\n";
    }

    for (const auto& f : functors) {
        const std::string& name = f.first;
        functors_initialize(constructor.body(), name);
    }

    // -- destructor --
    GenFunction& destructor = mainClass.addFunction("~" + classname, Visibility::Public);
    destructor.setIsConstructor();

    // issue state variables for the evaluation
    //
    // Improve compile time by storing the signal handler in one loc instead of
    // emitting thousands of `SignalHandler::instance()`. The volume of calls
    // makes GVN and register alloc very expensive, even if the call is inlined.
    mainClass.addField("std::string", "inputDirectory", Visibility::Private);
    mainClass.addField("std::string", "outputDirectory", Visibility::Private);
    mainClass.addField("SignalHandler*", "signalHandler", Visibility::Private, "{SignalHandler::instance()}");
    mainClass.addField("std::atomic<RamDomain>", "ctr", Visibility::Private, "{}");
    mainClass.addField("std::atomic<std::size_t>", "iter", Visibility::Private, "{}");

    GenFunction& runFunction = mainClass.addFunction("runFunction", Visibility::Private);
    runFunction.setRetType("void");
    runFunction.setNextArg("std::string", "inputDirectoryArg");
    runFunction.setNextArg("std::string", "outputDirectoryArg");
    runFunction.setNextArg("bool", "performIOArg");
    runFunction.setNextArg("bool", "pruneImdtRelsArg");

    runFunction.body() << R"_(
    this->inputDirectory  = std::move(inputDirectoryArg);
    this->outputDirectory = std::move(outputDirectoryArg);
    this->performIO       = performIOArg;
    this->pruneImdtRels   = pruneImdtRelsArg;

    // set default threads (in embedded mode)
    // if this is not set, and omp is used, the default omp setting of number of cores is used.
#if defined(_OPENMP)
    if (0 < getNumThreads()) { omp_set_num_threads(static_cast<int>(getNumThreads())); }
#endif

    signalHandler->set();
)_";
    if (glb.config().has("verbose")) {
        runFunction.body() << "signalHandler->enableLogging();\n";
    }

    // add actual program body
    runFunction.body() << "// -- query evaluation --\n";
    if (glb.config().has("profile")) {
        runFunction.body() << "ProfileEventSingleton::instance().startTimer();\n"
                           << R"_(ProfileEventSingleton::instance().makeTimeEvent("@time;starttime");)_"
                           << '\n'
                           << "{\n"
                           << R"_(Logger logger("@runtime;", 0);)_" << '\n';
        // Store count of relations
        std::size_t relationCount = 0;
        for (auto rel : prog.getRelations()) {
            if (rel->getName()[0] != '@') {
                ++relationCount;
            }
        }
        // Store configuration
        runFunction.body()
                << R"_(ProfileEventSingleton::instance().makeConfigRecord("relationCount", std::to_string()_"
                << relationCount << "));";
    }

    // emit code
    currentClass = &mainClass;
    emitCode(runFunction.body(), prog.getMain());

    if (glb.config().has("profile")) {
        runFunction.body() << "}\n"
                           << "ProfileEventSingleton::instance().stopTimer();\n"
                           << "dumpFreqs();\n";
    }

    // add code printing hint statistics
    runFunction.body() << "\n// -- relation hint statistics --\n";

    if (glb.config().has("verbose")) {
        for (auto rel : prog.getRelations()) {
            auto name = getRelationName(*rel);
            runFunction.body() << "std::cout << \"Statistics for Relation " << name << ":\\n\";\n"
                               << name << "->printStatistics(std::cout);\n"
                               << "std::cout << \"\\n\";\n";
        }
    }

    runFunction.body() << "signalHandler->reset();\n";

    // add methods to run with and without performing IO (mainly for the interface)
    GenFunction& run = mainClass.addFunction("run", Visibility::Public);
    run.setOverride();
    run.setRetType("void");
    run.body() << "runFunction(\"\", \"\", false, false);\n";

    GenFunction& runAll = mainClass.addFunction("runAll", Visibility::Public);
    runAll.setOverride();
    runAll.setRetType("void");
    runAll.setNextArg("std::string", "inputDirectoryArg", std::make_optional("\"\""));
    runAll.setNextArg("std::string", "outputDirectoryArg", std::make_optional("\"\""));
    runAll.setNextArg("bool", "performIOArg", std::make_optional("true"));
    runAll.setNextArg("bool", "pruneImdtRelsArg", std::make_optional("true"));
    if (glb.config().has("live-profile")) {
        runAll.body() << "std::thread profiler([]() { profile::Tui().runProf(); });\n";
    }
    runAll.body() << "runFunction(inputDirectoryArg, outputDirectoryArg, performIOArg, pruneImdtRelsArg);\n";
    if (glb.config().has("live-profile")) {
        runAll.body() << "if (profiler.joinable()) { profiler.join(); }\n";
    }

    // issue printAll method
    GenFunction& printAll = mainClass.addFunction("printAll", Visibility::Public);
    printAll.setOverride();
    printAll.setRetType("void");
    printAll.setNextArg("[[maybe_unused]] std::string", "outputDirectoryArg", std::make_optional("\"\""));

    // print directives as C++ initializers
    auto printDirectives = [&](std::ostream& o, const std::map<std::string, std::string>& registry) {
        auto cur = registry.begin();
        if (cur == registry.end()) {
            return;
        }
        o << "{{\"" << cur->first << "\",\"" << escape(cur->second) << "\"}";
        ++cur;
        for (; cur != registry.end(); ++cur) {
            o << ",{\"" << cur->first << "\",\"" << escape(cur->second) << "\"}";
        }
        o << '}';
    };

    for (auto store : storeIOs) {
        auto const& directive = store->getDirectives();
        printAll.body() << "try {";
        printAll.body() << "std::map<std::string, std::string> directiveMap(";
        printDirectives(printAll.body(), directive);
        printAll.body() << ");\n";
        printAll.body() << R"_(if (!outputDirectoryArg.empty()) {)_";
        printAll.body() << R"_(directiveMap["output-dir"] = outputDirectoryArg;)_";
        printAll.body() << "}\n";
        printAll.body() << "IOSystem::getInstance().getWriter(";
        printAll.body() << "directiveMap, symTable, recordTable";
        printAll.body() << ")->writeAll(*" << getRelationName(lookup(store->getRelation())) << ");\n";

        printAll.body() << "} catch (std::exception& e) {std::cerr << e.what();exit(1);}\n";
    }

    // issue loadAll method
    GenFunction& loadAll = mainClass.addFunction("loadAll", Visibility::Public);
    loadAll.setOverride();
    loadAll.setRetType("void");
    loadAll.setNextArg("[[maybe_unused]] std::string", "inputDirectoryArg", std::make_optional("\"\""));

    for (auto load : loadIOs) {
        loadAll.body() << "try {";
        loadAll.body() << "std::map<std::string, std::string> directiveMap(";
        printDirectives(loadAll.body(), load->getDirectives());
        loadAll.body() << ");\n";
        loadAll.body() << R"_(if (!inputDirectoryArg.empty()) {)_";
        loadAll.body() << R"_(directiveMap["fact-dir"] = inputDirectoryArg;)_";
        loadAll.body() << "}\n";
        loadAll.body() << "IOSystem::getInstance().getReader(";
        loadAll.body() << "directiveMap, symTable, recordTable";
        loadAll.body() << ")->readAll(*" << getRelationName(lookup(load->getRelation()));
        loadAll.body() << ");\n";
        loadAll.body() << "} catch (std::exception& e) {std::cerr << \"Error loading " << load->getRelation()
                       << " data: \" << e.what() << "
                          "'\\n';\nexit(1);\n}\n";
    }

    // issue dump methods
    auto dumpRelation = [&](std::ostream& os, const ram::Relation& ramRelation) {
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
    GenFunction& dumpInputs = mainClass.addFunction("dumpInputs", Visibility::Public);
    dumpInputs.setOverride();
    dumpInputs.setRetType("void");
    for (auto load : loadIOs) {
        dumpRelation(dumpInputs.body(), *lookup(load->getRelation()));
    }

    // dump outputs
    GenFunction& dumpOutputs = mainClass.addFunction("dumpOutputs", Visibility::Public);
    dumpOutputs.setOverride();
    dumpOutputs.setRetType("void");
    for (auto store : storeIOs) {
        dumpRelation(dumpOutputs.body(), *lookup(store->getRelation()));
    }

    GenFunction& getSymbolTable = mainClass.addFunction("getSymbolTable", Visibility::Public);
    getSymbolTable.setOverride();
    getSymbolTable.setRetType("SymbolTable&");
    getSymbolTable.body() << "return symTable;\n";

    GenFunction& getRecordTable = mainClass.addFunction("getRecordTable", Visibility::Public);
    getRecordTable.setOverride();
    getRecordTable.setRetType("RecordTable&");
    getRecordTable.body() << "return recordTable;\n";

    GenFunction& setNumThreads = mainClass.addFunction("setNumThreads", Visibility::Public);
    setNumThreads.setRetType("void");
    setNumThreads.setNextArg("std::size_t", "numThreadsValue");

    setNumThreads.body() << "SouffleProgram::setNumThreads(numThreadsValue);\n";
    setNumThreads.body() << "symTable.setNumLanes(getNumThreads());\n";
    setNumThreads.body() << "recordTable.setNumLanes(getNumThreads());\n";
    setNumThreads.body() << "regexCache.setNumLanes(getNumThreads());\n";

    if (!prog.getSubroutines().empty()) {
        // generate subroutine adapter
        GenFunction& executeSubroutine = mainClass.addFunction("executeSubroutine", Visibility::Public);
        executeSubroutine.setRetType("void");
        executeSubroutine.setOverride();
        executeSubroutine.setNextArg("std::string", "name");
        executeSubroutine.setNextArg("const std::vector<RamDomain>&", "args");
        executeSubroutine.setNextArg("std::vector<RamDomain>&", "ret");

        for (auto& sub : prog.getSubroutines()) {
            executeSubroutine.body() << "if (name == \"" << sub.first << "\") {\n"
                                     << convertStratumIdent("stratum_" + sub.first) << ".run(args, ret);\n"
                                     << "return;"
                                     << "}\n";
        }
        executeSubroutine.body() << "fatal((\"unknown subroutine \" + name).c_str());\n";
    }

    // dumpFreqs method
    //  Frequency counts must be emitted after subroutines otherwise lookup tables
    //  are not populated.
    if (glb.config().has("profile")) {
        GenFunction& dumpFreqs = mainClass.addFunction("dumpFreqs", Visibility::Private);
        dumpFreqs.setRetType("void");

        for (auto const& cur : idxMap) {
            dumpFreqs.body() << "\tProfileEventSingleton::instance().makeQuantityEvent(R\"_(" << cur.first
                             << ")_\", freqs[" << cur.second << "],0);\n";
        }
        for (auto const& cur : neIdxMap) {
            dumpFreqs.body() << "\tProfileEventSingleton::instance().makeQuantityEvent(R\"_(@relation-reads;"
                             << cur.first << ")_\", reads[" << cur.second << "],0);\n";
        }
    }

    GenClass& factory = db.getClass("factory_" + classname, fs::path("factory_" + classname));
    factory.addInclude("\"souffle/SouffleInterface.h\"");
    factory.addDependency(mainClass, true);
    factory.inherits("souffle::ProgramFactory");
    GenFunction& newInstance = factory.addFunction("newInstance", Visibility::Public);
    newInstance.setRetType("souffle::SouffleProgram*");
    newInstance.body() << "return new " << db.getNS() << "::" << classname << "();\n";
    GenFunction& factoryConstructor = factory.addConstructor(Visibility::Public);
    factoryConstructor.setNextInitializer("souffle::ProgramFactory", "\"" + id + "\"");

    std::ostream& hook = mainClass.hooks();
    std::ostream& factory_hook = factory.hooks();

    // hidden hooks
    hook << "namespace souffle {\n";
    hook << "SouffleProgram *newInstance_" << id << "(){return new " << db.getNS() << "::" << classname
         << ";}\n";
    hook << "SymbolTable *getST_" << id << "(SouffleProgram *p){return &reinterpret_cast<" << db.getNS(false)
         << "::" << classname << "*>(p)->getSymbolTable();}\n";

    hook << "} // namespace souffle\n";

    factory_hook << "namespace souffle {\n";
    factory_hook << "\n#ifdef __EMBEDDED_SOUFFLE__\n";
    factory_hook << "extern \"C\" {\n";
    factory_hook << db.getNS(false) << "::factory_" << classname << " __factory_" << classname
                 << "_instance;\n";
    factory_hook << "}\n";
    factory_hook << "#endif\n";
    factory_hook << "} // namespace souffle\n";

    hook << "\n#ifndef __EMBEDDED_SOUFFLE__\n";
    hook << "#include \"souffle/CompiledOptions.h\"\n";

    hook << "int main(int argc, char** argv)\n{\n";
    hook << "try{\n";

    // parse arguments
    hook << "souffle::CmdOptions opt(";
    hook << "R\"(" << glb.config().get("") << ")\",\n";
    hook << "R\"()\",\n";
    hook << "R\"()\",\n";
    if (glb.config().has("profile")) {
        hook << "true,\n";
        hook << "R\"(" << glb.config().get("profile") << ")\",\n";
    } else {
        hook << "false,\n";
        hook << "R\"()\",\n";
    }
    hook << std::stoi(glb.config().get("jobs"));
    hook << ");\n";

    hook << "if (!opt.parse(argc,argv)) return 1;\n";

    if (!db.getNS(false).empty()) {
        hook << db.getNS(false) << "::";
    }
    if (glb.config().has("profile")) {
        hook << classname + " obj(opt.getProfileName());\n";
    } else {
        hook << classname + " obj;\n";
    }

    hook << "#if defined(_OPENMP) \n";
    hook << "obj.setNumThreads(opt.getNumJobs());\n";
    hook << "\n#endif\n";

    if (glb.config().has("profile")) {
        hook << R"_(souffle::ProfileEventSingleton::instance().makeConfigRecord("", opt.getSourceFileName());)_"
             << '\n';
        hook << R"_(souffle::ProfileEventSingleton::instance().makeConfigRecord("fact-dir", opt.getInputFileDir());)_"
             << '\n';
        hook << R"_(souffle::ProfileEventSingleton::instance().makeConfigRecord("jobs", std::to_string(opt.getNumJobs()));)_"
             << '\n';
        hook << R"_(souffle::ProfileEventSingleton::instance().makeConfigRecord("output-dir", opt.getOutputFileDir());)_"
             << '\n';
        hook << R"_(souffle::ProfileEventSingleton::instance().makeConfigRecord("version", ")_"
             << glb.config().get("version") << R"_(");)_" << '\n';
    }
    hook << "obj.runAll(opt.getInputFileDir(), opt.getOutputFileDir());\n";

    if (glb.config().get("provenance") == "explain") {
        hook << "explain(obj, false);\n";
    } else if (glb.config().get("provenance") == "explore") {
        hook << "explain(obj, true);\n";
    }
    hook << "return 0;\n";
    hook << "} catch(std::exception &e) { souffle::SignalHandler::instance()->error(e.what());}\n";
    hook << "}\n";
    hook << "#endif\n";
}

}  // namespace souffle::synthesiser
