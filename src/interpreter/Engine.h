/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Engine.h
 *
 * Declares the Interpreter Engine class. The engine takes in an Node
 * representation and execute them.
 ***********************************************************************/

#pragma once

#include "Global.h"
#include "interpreter/Context.h"
#include "interpreter/Generator.h"
#include "interpreter/Index.h"
#include "interpreter/Node.h"
#include "interpreter/Relation.h"
#include "ram/TranslationUnit.h"
#include "ram/analysis/Index.h"
#include "souffle/RamTypes.h"
#include "souffle/RecordTable.h"
#include "souffle/SymbolTable.h"
#include "souffle/datastructure/RecordTableImpl.h"
#include "souffle/datastructure/SymbolTableImpl.h"
#include "souffle/utility/ContainerUtil.h"
#include <atomic>
#include <cstddef>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>
#ifdef _OPENMP
#include <omp.h>
#endif

namespace souffle::interpreter {

class ProgInterface;

/**
 * @class Engine
 * @brief This class translate the RAM Program into executable format and interpreter it.
 */
class Engine {
    using RelationHandle = Own<RelationWrapper>;
    friend ProgInterface;
    friend NodeGenerator;

public:
    Engine(ram::TranslationUnit& tUnit);

    /** @brief Execute the main program */
    void executeMain();
    /** @brief Execute the subroutine program */
    void executeSubroutine(
            const std::string& name, const std::vector<RamDomain>& args, std::vector<RamDomain>& ret);

private:
    /** @brief Generate intermediate representation from RAM */
    void generateIR();
    /** @brief Remove a relation from the environment */
    void dropRelation(const std::size_t relId);
    /** @brief Swap the content of two relations */
    void swapRelation(const std::size_t ramRel1, const std::size_t ramRel2);
    /** @brief Return a reference to the relation on the given index */
    RelationHandle& getRelationHandle(const std::size_t idx);
    /** @brief Return the string symbol table */
    SymbolTable& getSymbolTable() {
        return symbolTable;
    }
    /** @brief Return the record table */
    RecordTable& getRecordTable();
    /** @brief Return the ram::TranslationUnit */
    ram::TranslationUnit& getTranslationUnit();
    /** @brief Execute the program */
    RamDomain execute(const Node*, Context&);
    /** @brief Return method handler */
    void* getMethodHandle(const std::string& method);
    /** @brief Load DLL */
    const std::vector<void*>& loadDLL();
    /** @brief Return current iteration number for loop operation */
    std::size_t getIterationNumber() const;
    /** @brief Increase iteration number by one */
    void incIterationNumber();
    /** @brief Reset iteration number */
    void resetIterationNumber();
    /** @brief Increment the counter */
    RamDomain incCounter();
    /** @brief Return the relation map. */
    VecOwn<RelationHandle>& getRelationMap();
    /** @brief Create and add relation into the runtime environment.  */
    void createRelation(const ram::Relation& id, const std::size_t idx);

    // -- Defines template for specialized interpreter operation -- */
    template <typename Rel>
    RamDomain evalExistenceCheck(const ExistenceCheck& shadow, Context& ctxt);

    template <typename Rel>
    RamDomain evalProvenanceExistenceCheck(const ProvenanceExistenceCheck& shadow, Context& ctxt);

    template <typename Rel>
    RamDomain evalScan(const Rel& rel, const ram::Scan& cur, const Scan& shadow, Context& ctxt);

    template <typename Rel>
    RamDomain evalParallelScan(
            const Rel& rel, const ram::ParallelScan& cur, const ParallelScan& shadow, Context& ctxt);

    template <typename Rel>
    RamDomain evalCountUniqueKeys(
            const Rel& rel, const ram::CountUniqueKeys& cur, const CountUniqueKeys& shadow, Context& ctxt);

    template <typename Rel>
    RamDomain evalIndexScan(const ram::IndexScan& cur, const IndexScan& shadow, Context& ctxt);

    template <typename Rel>
    RamDomain evalParallelIndexScan(const Rel& rel, const ram::ParallelIndexScan& cur,
            const ParallelIndexScan& shadow, Context& ctxt);

    template <typename Rel>
    RamDomain evalIfExists(const Rel& rel, const ram::IfExists& cur, const IfExists& shadow, Context& ctxt);

    template <typename Rel>
    RamDomain evalParallelIfExists(
            const Rel& rel, const ram::ParallelIfExists& cur, const ParallelIfExists& shadow, Context& ctxt);

    template <typename Rel>
    RamDomain evalIndexIfExists(const ram::IndexIfExists& cur, const IndexIfExists& shadow, Context& ctxt);

    template <typename Rel>
    RamDomain evalParallelIndexIfExists(const Rel& rel, const ram::ParallelIndexIfExists& cur,
            const ParallelIndexIfExists& shadow, Context& ctxt);

    template <typename Aggregate, typename Iter>
    RamDomain evalAggregate(const Aggregate& aggregate, const Node& filter, const Node* expression,
            const Node& nestedOperation, const Iter& ranges, Context& ctxt);

    template <typename Rel>
    RamDomain evalParallelAggregate(const Rel& rel, const ram::ParallelAggregate& cur,
            const ParallelAggregate& shadow, Context& ctxt);

    template <typename Rel>
    RamDomain evalParallelIndexAggregate(
            const ram::ParallelIndexAggregate& cur, const ParallelIndexAggregate& shadow, Context& ctxt);

    template <typename Rel>
    RamDomain evalIndexAggregate(const ram::IndexAggregate& cur, const IndexAggregate& shadow, Context& ctxt);

    template <typename Rel>
    RamDomain evalGuardedInsert(Rel& rel, const GuardedInsert& shadow, Context& ctxt);

    template <typename Rel>
    RamDomain evalInsert(Rel& rel, const Insert& shadow, Context& ctxt);

    template <typename Rel>
    RamDomain evalErase(Rel& rel, const Erase& shadow, Context& ctxt);

    /** If profile is enable in this program */
    const bool profileEnabled;
    const bool frequencyCounterEnabled;
    /** subroutines */
    VecOwn<Node> subroutine;
    /** main program */
    Own<Node> main;
    /** Number of threads enabled for this program */
    std::size_t numOfThreads;
    /** Profile counter */
    std::atomic<RamDomain> counter{0};
    /** Loop iteration counter */
    std::size_t iteration = 0;
    /** Profile for rule frequencies */
    std::map<std::string, std::deque<std::atomic<std::size_t>>> frequencies;
    /** Profile for relation reads */
    std::map<std::string, std::atomic<std::size_t>> reads;
    /** DLL */
    std::vector<void*> dll;
    /** Program */
    ram::TranslationUnit& tUnit;
    /** IndexAnalysis */
    ram::analysis::IndexAnalysis& isa;
    /** Record Table Implementation*/
    SpecializedRecordTable<0, 1, 2, 3, 4, 5, 6, 7, 8, 9> recordTable;
    /** Symbol table for relations */
    VecOwn<RelationHandle> relations;
    /** Symbol table */
    SymbolTableImpl symbolTable;
};

}  // namespace souffle::interpreter
