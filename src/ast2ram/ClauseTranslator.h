/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ClauseTranslator.h
 *
 * Abstract class providing an interface for translating an
 * ast::Clause into an equivalent ram::Statement.
 *
 ***********************************************************************/

#pragma once

#include "souffle/utility/ContainerUtil.h"

namespace souffle::ast {
class Clause;
class Relation;
}  // namespace souffle::ast

namespace souffle::ram {
class Statement;
}

namespace souffle::ast2ram {

class TranslatorContext;

/** Translation modes */
enum TranslationMode {
    DEFAULT,

    // Subsumptive clauses
    //
    //   R(x0) <= R(x1) :- body.
    //
    // are translated to clauses of the format
    //
    //   R(x0) :- R(x0), R(x1), body.
    //
    // using different auxiliary tables (delta/new/reject/delete).
    //
    // There are different modes for translating a subsumptive clause
    // (i.e. inside / outside of the fix-point).

    // delete reject-R(x0) :- new-R(x0), R(x1), body. (inside fix-point)
    SubsumeRejectNewCurrent,

    // delete reject-R(x0) :- new-R(x0), new-R(x1), x0!=x1, body. (inside fix-point)
    SubsumeRejectNewNew,

    // delete delete-R(x0) :- R(x0), delta-R(x1), body. (inside fix-point)
    SubsumeDeleteCurrentDelta,

    // delete delete-R(x0) :- R(x0), R(x1), x0!=x1, body. (outside fix-point)
    SubsumeDeleteCurrentCurrent,

    // Incremental update clauses
    //
    //   R(x0) :- R1(x1), R2(x2).
    // 
    // are translated into various versions, each involving various combinations
    // of diff_plus and diff_minus.
    //
    // Note these following modes only apply in ast2ram/incremental/update

    // diff_plus versions for insertion
    IncrementalDiffPlus,

    // diff_minus versions for deletion
    IncrementalDiffMinus
};

/* Abstract Clause Translator */
class ClauseTranslator {
public:
    ClauseTranslator(const TranslatorContext& context, TranslationMode mode) : context(context), mode(mode) {}
    virtual ~ClauseTranslator() = default;

    /** Translate a non-recursive clause */
    virtual Own<ram::Statement> translateNonRecursiveClause(const ast::Clause& clause) = 0;

    /** Translate a recursive clause */
    virtual Own<ram::Statement> translateRecursiveClause(
            const ast::Clause& clause, const std::set<const ast::Relation*>& scc, std::size_t version) = 0;

protected:
    /** Translation context */
    const TranslatorContext& context;

    /** Translation mode */
    enum TranslationMode mode;
};

}  // namespace souffle::ast2ram
