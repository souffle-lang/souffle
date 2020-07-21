/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstPragma.h
 *
 * Define the class AstPragma to update global options based on parameter.
 *
 ***********************************************************************/

#pragma once

#include "SrcLocation.h"
#include "ast/AstNode.h"
#include <ostream>
#include <string>
#include <utility>

namespace souffle {

/**
 * @class AstPragma
 * @brief Representation of a global option
 */
class AstPragma : public AstNode {
public:
    AstPragma(std::string key, std::string value, SrcLocation loc = {})
            : AstNode(std::move(loc)), key(std::move(key)), value(std::move(value)) {}

    AstPragma* clone() const override {
        return new AstPragma(key, value, getSrcLoc());
    }

    /* Get kvp */
    std::pair<std::string, std::string> getkvp() const {
        return std::pair<std::string, std::string>(key, value);
    }

protected:
    void print(std::ostream& os) const override {
        os << ".pragma " << key << " " << value << "\n";
    }

    bool equal(const AstNode& node) const override {
        const auto& other = static_cast<const AstPragma&>(node);
        return other.key == key && other.value == value;
    }

    /** Name of the key */
    std::string key;

    /** Value */
    std::string value;
};

}  // end of namespace souffle
