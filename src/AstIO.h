/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstIO.h
 *
 * Define the classes representing IO operations.
 *
 ***********************************************************************/

#pragma once

#include "AstNode.h"
#include "AstQualifiedName.h"

#include <map>
#include <string>

namespace souffle {

/**
 * @class AstIO
 * @brief I/O operation has a type (input/output/printsize), qualified relation name, and I/O directives.
 */
class AstIO : public AstNode {
public:
    enum AstIOType { UndefinedIO, InputIO, OutputIO, PrintsizeIO };

    AstIO(const AstIO& io) : type(io.type), name(io.name), directives(io.directives) {
        this->setSrcLoc(io.getSrcLoc());
    }
    AstIO() = default;

    /** get I/O type */
    AstIOType getType() const {
        return type;
    }

    /** set I/O type */
    void setType(AstIOType type) {
        this->type = type;
    }

    /** get relation name */
    const AstQualifiedName& getQualifiedName() const {
        return name;
    }

    /** set relation name */
    void setQualifiedName(const AstQualifiedName& name) {
        this->name = name;
    }

    /** get value of I/O directive */
    const std::string& getDirective(const std::string& key) const {
        return directives.at(key);
    }

    /** add new I/O directive */
    void addDirective(const std::string& key, const std::string& value) {
        directives[key] = value;
    }

    /** check for I/O directive */
    bool hasDirective(const std::string& key) const {
        return directives.find(key) != directives.end();
    }

    /** get I/O-directive map */
    const std::map<std::string, std::string>& getDirectives() const {
        return directives;
    }

    AstIO* clone() const override {
        auto res = new AstIO();
        res->type = type;
        res->name = name;
        res->directives = directives;
        res->setSrcLoc(getSrcLoc());
        return res;
    }

protected:
    void print(std::ostream& os) const override {
        switch (type) {
            case UndefinedIO:
                os << ".undefined ";
                break;
            case InputIO:
                os << ".input ";
                break;
            case OutputIO:
                os << ".output ";
                break;
            case PrintsizeIO:
                os << ".printsize ";
                break;
            default:
                assert("Unknown I/O operation type");
        }
        os << name;
        if (!directives.empty()) {
            os << "(" << join(directives, ",", [](std::ostream& out, const auto& arg) {
                out << arg.first << "=\"" << arg.second << "\"";
            }) << ")";
        }
    }

    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstIO*>(&node));
        const auto& other = static_cast<const AstIO&>(node);
        return other.type == type && other.name == name && other.directives == directives;
    }

    /** type of I/O operation */
    AstIOType type = UndefinedIO;

    /** relation name of I/O operation */
    AstQualifiedName name;

    /** I/O directives */
    std::map<std::string, std::string> directives;
};

}  // end of namespace souffle
