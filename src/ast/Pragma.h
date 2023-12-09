/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Pragma.h
 *
 * Defines the pragma class
 *
 ***********************************************************************/

#pragma once

#include "ast/Node.h"
#include "parser/SrcLocation.h"
#include <iosfwd>
#include <string>
#include <utility>

namespace souffle::ast {

/**
 * @class Pragma
 * @brief Representation of a global option
 */
class Pragma : public Node {
public:
    Pragma(std::string key, std::string value, SrcLocation loc = {});

    /* Get kvp */
    std::pair<std::string, std::string> getkvp() const {
        return std::make_pair(key, value);
    }

    static bool classof(const Node*);

protected:
    void print(std::ostream& os) const override;

private:
    bool equal(const Node& node) const override;

    Pragma* cloning() const override;

private:
    /** Name of the key */
    std::string key;

    /** Value */
    std::string value;
};

}  // namespace souffle::ast
