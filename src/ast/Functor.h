/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
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
#include "parser/TypeAttribute.h"
#include <cstddef>

namespace souffle {

/**
 * @class AstFunctor
 * @brief Abstract functor class
 */

class AstFunctor : public AstTerm {
public:
    /** Return return type of functor */
    virtual TypeAttribute getReturnType() const = 0;

    /** Return argument type of functor */
    virtual TypeAttribute getArgType(const size_t arg) const = 0;

    AstFunctor* clone() const override = 0;

protected:
    /* TODO(b-scholz): check whehter still in use */
    using AstTerm::AstTerm;
};

}  // end of namespace souffle
