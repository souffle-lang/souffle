/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file GraphUtils.h
 *
 * A simple utility graph for conducting simple, graph-based operations.
 *
 ***********************************************************************/
#pragma once

#include <ostream>
#include <string>
#include <vector>

namespace souffle {
std::string toBase64(const std::string& data);
std::string convertDotToSVG(const std::string& dotSpec);
void printHTMLGraph(std::ostream& out, const std::string& dotSpec, const std::string& id);
}  // end of namespace souffle
