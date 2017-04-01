/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstTypes.h
 *
 * Defines a numeric type for the AST, mimics "RamTypes.h"
 *
 ***********************************************************************/

#pragma once

#include <stdint.h>

namespace souffle {

/** ast domain to mimic ram domain */
#ifdef AST_DOMAIN_TYPE
typedef AST_DOMAIN_TYPE AstDomain;
#else
typedef int64_t AstDomain;
#endif

/** lower and upper boundaries for the ast domain **/
#define MIN_AST_DOMAIN (std::numeric_limits<AstDomain>::min())
#define MAX_Ast_DOMAIN (std::numeric_limits<AstDomain>::max())
}  // namespace souffle
