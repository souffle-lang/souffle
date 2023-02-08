/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Synthesiser.h
 *
 * Declares synthesiser classes to synthesise C++ code from a RAM program.
 *
 ***********************************************************************/

#pragma once

#include "ram/Operation.h"
#include "ram/Relation.h"
#include "ram/Statement.h"
#include "ram/TranslationUnit.h"
#include "ram/utility/Visitor.h"
#include "souffle/RecordTable.h"
#include "souffle/utility/ContainerUtil.h"
#include "synthesiser/GenDb.h"
#include "synthesiser/Relation.h"
#include <cstddef>
#include <map>
#include <memory>
#include <ostream>
#include <regex>
#include <set>
#include <string>

namespace souffle::synthesiser {

/**
 * A RAM synthesiser: synthesises a C++ program from a RAM program.
 */
class Synthesiser {
private:
    /** Record Table */

    /** RAM translation unit */
    ram::TranslationUnit& translationUnit;

    /** Global */
    Global& glb;

    /** RAM identifier to C++ identifier map */
    std::map<const std::string, const std::string> identifiers;

    /** Frequency profiling of searches */
    std::map<std::string, unsigned> idxMap;

    /** Frequency profiling of non-existence checks */
    std::map<std::string, std::size_t> neIdxMap;

    /** Cache for generated types for relations */
    std::set<std::string> typeCache;

    /** Relation map */
    std::map<std::string, const ram::Relation*> relationMap;

    /** Symbol map */
    mutable std::map<std::string, RamUnsigned> symbolMap;

    /** Symbol map */
    mutable std::vector<std::string> symbolIndex;

    /** Is set to true if there is a need to include std::regex */
    bool UsingStdRegex = false;

    /** Is set to true if the current subroutine uses std::regex */
    bool SubroutineUsingStdRegex = false;
    bool SubroutineUsingSubstr = false;

    /** A mapping of valid regex patterns to to a unique index.
     * The index to which the pattern is mapped is in
     *  the range from 0 regexes.size()-1.
     */
    std::map<std::string, std::size_t> regexes;

    /** Pointer to the subroutine class currently being built */
    GenClass* currentClass = nullptr;

    /** Set of packed and unpacked records arities */
    std::set<std::size_t> arities;

    /** signatures of the user-defined functors */
    std::map<std::string, std::pair<std::vector<std::string>, std::string>> functor_signatures;

    /** Output relations */
    std::set<std::string> storeRelations;

protected:
    /** Convert RAM identifier */
    const std::string convertRamIdent(const std::string& name);

    /** Convert stratum name to a C++-compliant identifier */
    const std::string convertStratumIdent(const std::string& name);

    /** Get relation name */
    const std::string getRelationName(const ram::Relation& rel);
    const std::string getRelationName(const ram::Relation* rel);

    /** Get context name */
    const std::string getOpContextName(const ram::Relation& rel);

    /** Get relation struct definition */
    void generateRelationTypeStruct(GenDb& db, Own<Relation> relationType);

    /** Get referenced relations */
    ram::RelationSet getReferencedRelations(const ram::Operation& op);

    /** Compile a regular expression and return a unique name for it */
    std::optional<std::size_t> compileRegex(const std::string& pattern);

    /** Generate code */
    void emitCode(std::ostream& out, const ram::Statement& stmt);

    /** Lookup frequency counter */
    unsigned lookupFreqIdx(const std::string& txt);

    /** Lookup read counter */
    std::size_t lookupReadIdx(const std::string& txt);

    /** Lookup relation by relation name */
    const ram::Relation* lookup(const std::string& relName) {
        auto it = relationMap.find(relName);
        assert(it != relationMap.end() && "relation not found");
        return it->second;
    }

    /** Lookup symbol index */
    RamUnsigned convertSymbol2Idx(const std::string& symbol) const {
        auto it = symbolMap.find(symbol);
        if (it != symbolMap.end()) {
            return it->second;
        } else {
            symbolIndex.push_back(symbol);
            std::size_t idx = symbolMap.size();
            symbolMap[symbol] = RamUnsigned(idx);
            return RamUnsigned(idx);
        }
    }

    std::string convertSymbolToIdentifier(const std::string& symbol) const;

    /** return the set of relation names accessed/used in the statement */
    std::set<std::string> accessedRelations(ram::Statement& stmt);

    /** return the set of User-defined functor names used in the statement */
    std::set<std::string> accessedUserDefinedFunctors(ram::Statement& stmt);

public:
    explicit Synthesiser(ram::TranslationUnit& tUnit) : translationUnit(tUnit), glb(tUnit.global()) {
        visit(tUnit.getProgram(),
                [&](const ram::Relation& relation) { relationMap[relation.getName()] = &relation; });
    }

    virtual ~Synthesiser() = default;

    /** Get translation unit */
    ram::TranslationUnit& getTranslationUnit() {
        return translationUnit;
    }

    /** Generate code */
    void generateCode(GenDb& db, const std::string& id, bool& withSharedLibrary);
};
}  // namespace souffle::synthesiser
