/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstRelationIdentifier.h
 *
 * Defines the token utilized to address relations.
 *
 ***********************************************************************/

#pragma once

#include "Util.h"

#include <algorithm>
#include <string>
#include <vector>

namespace souffle {

/**
 * The type of identifier utilized for referencing relations. Relation
 * name identifiers are hierarchically qualified names, e.g.
 *
 *          problem.graph.edge
 *
 */
class AstRelationIdentifier {
    /**
     * The list of names forming this identifier.
     */
    std::vector<std::string> names;

public:
    // -- constructors --

    AstRelationIdentifier(const std::string& name = "") : names(toVector(name)) {}

    AstRelationIdentifier(const char* name) : AstRelationIdentifier(std::string(name)) {}

    AstRelationIdentifier(const AstRelationIdentifier&) = default;
    AstRelationIdentifier(AstRelationIdentifier&&) = default;

    // -- assignment operators --

    AstRelationIdentifier& operator=(const AstRelationIdentifier&) = default;
    AstRelationIdentifier& operator=(AstRelationIdentifier&&) = default;

    // -- mutators --

    void append(const std::string& name) {
        names.push_back(name);
    }

    void prepend(const std::string& name) {
        names.insert(names.begin(), name);
    }

    // -- getters and setters --

    const std::vector<std::string>& getNames() const {
        return names;
    }

    // -- comparison operators --

    bool operator==(const AstRelationIdentifier& other) const {
        return names == other.names;
    }

    bool operator!=(const AstRelationIdentifier& other) const {
        return !(*this == other);
    }

    bool operator<(const AstRelationIdentifier& other) const {
        return std::lexicographical_compare(
                names.begin(), names.end(), other.names.begin(), other.names.end());
    }

    void print(std::ostream& out) const {
        out << join(names, ".");
    }

    friend std::ostream& operator<<(std::ostream& out, const AstRelationIdentifier& id) {
        id.print(out);
        return out;
    }
};

/**
 * A overloaded operator to add a new prefix to a given relation identifier.
 */
inline AstRelationIdentifier operator+(const std::string& name, const AstRelationIdentifier& id) {
    AstRelationIdentifier res = id;
    res.prepend(name);
    return res;
}

}  // end of namespace souffle
