/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file NodeMapper.h
 *
 * Defines the node mapper class
 *
 ***********************************************************************/

#pragma once

#include "ast/Node.h"
#include "souffle/utility/NodeMapper.h"

namespace souffle::ast {

using NodeMapper = souffle::detail::NodeMapper<Node>;

}  // namespace souffle::ast
