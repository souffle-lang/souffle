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

#include "AstNode.h"
#include <cassert>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 * @class AstPragma
 * @brief Representation of a global option
 */
class AstPragma : public AstNode {
public:
    AstPragma() = default;

    AstPragma(std::string k, std::string v) : key(std::move(k)), value(std::move(v)) {}

    void print(std::ostream& os) const override {
        os << ".pragma " << key << " " << value << "\n";
    }

    AstPragma* clone() const override {
        auto res = new AstPragma();
        res->key = key;
        res->value = value;
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /* get key-value pair */
    std::pair<std::string, std::string> getkvp() const {
        return std::pair<std::string, std::string>(key, value);
    }

protected:
    /** An internal function to determine equality to another node */
    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstPragma*>(&node));
        const auto& other = static_cast<const AstPragma&>(node);
        return other.key == key && other.value == value;
    }

    /** Name of the key */
    std::string key;

    /** Value */
    std::string value;
};

}  // end of namespace souffle
