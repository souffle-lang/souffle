/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstQualifiedName.h
 *
 * Defines qualified names so that objects in components (types,
 * relations, etc.) can be accessed via either a fully/partially
 * qualified name.
 *
 ***********************************************************************/

#pragma once

#include "utility/StreamUtil.h"
#include <algorithm>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 * Qualified Name class for fully/partially qualified names
 * to identify objects in components.
 */
class AstQualifiedName {
public:
    AstQualifiedName() : qualifiers() {}
    AstQualifiedName(const std::string& name) : qualifiers({name}) {}
    AstQualifiedName(const char* name) : AstQualifiedName(std::string(name)) {}
    AstQualifiedName(const std::vector<std::string> qualifiers) : qualifiers(qualifiers) {}
    AstQualifiedName(const AstQualifiedName&) = default;
    AstQualifiedName(AstQualifiedName&&) = default;
    AstQualifiedName& operator=(const AstQualifiedName&) = default;
    AstQualifiedName& operator=(AstQualifiedName&&) = default;

    /** append qualifiers */
    void append(std::string name) {
        qualifiers.push_back(std::move(name));
    }

    /** prepend qualifiers */
    void prepend(std::string name) {
        qualifiers.insert(qualifiers.begin(), std::move(name));
    }

    /** check for emptiness */
    bool empty() const {
        return qualifiers.empty();
    }

    /** get qualifiers */
    const std::vector<std::string>& getQualifiers() const {
        return qualifiers;
    }

    /** convert to a string separated by fullstop */
    std::string toString() const {
        std::stringstream ss;
        ss << join(qualifiers, ".");
        return ss.str();
    }

    bool operator==(const AstQualifiedName& other) const {
        return qualifiers == other.qualifiers;
    }

    bool operator!=(const AstQualifiedName& other) const {
        return !(*this == other);
    }

    bool operator<(const AstQualifiedName& other) const {
        return std::lexicographical_compare(
                qualifiers.begin(), qualifiers.end(), other.qualifiers.begin(), other.qualifiers.end());
    }

    /** print qualified name */
    void print(std::ostream& out) const {
        out << join(qualifiers, ".");
    }

    friend std::ostream& operator<<(std::ostream& out, const AstQualifiedName& id) {
        id.print(out);
        return out;
    }

private:
    /* array of name qualifiers */
    std::vector<std::string> qualifiers;
};

inline AstQualifiedName operator+(const std::string& name, const AstQualifiedName& id) {
    AstQualifiedName res = id;
    res.prepend(name);
    return res;
}

}  // namespace souffle
