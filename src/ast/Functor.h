/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Functor.h
 *
 * Defines the abstract class for functors
 *
 ***********************************************************************/

#pragma once

#include "ast/Term.h"

namespace souffle::ast {

/**
 * @class Functor
 * @brief Abstract functor class
 */

class Functor : public Term {
protected:
    using Term::Term;

    Functor(NodeKind kind, SrcLocation loc = {}) : Term(kind, loc) {
        assert(kind > NK_FirstFunctor && kind < NK_LastFunctor);
    }

    static bool classof(const Node*);
};

}  // namespace souffle::ast
