/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstAttribute.h
 *
 * Defines an attribute for a relation
 *
 ***********************************************************************/

#pragma once

#include "AstNode.h"
#include "AstType.h"

#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <cctype>

namespace souffle {

/**
 *  Intermediate representation of an attribute which stores the name and the type of an attribute
 *
 *  Attribute has the only name attribute
 */
class AstAttribute : public AstNode {
public:
    AstAttribute(std::string n, AstQualifiedName t) : name(std::move(n)), typeName(std::move(t)) {}

    /** get attribute name */
    const std::string& getAttributeName() const {
        return name;
    }

    /** get type name */
    const AstQualifiedName& getTypeName() const {
        return typeName;
    }

    /** set type name */
    void setTypeName(const AstQualifiedName& name) {
        typeName = name;
    }

    AstAttribute* clone() const override {
        auto* res = new AstAttribute(name, typeName);
        res->setSrcLoc(getSrcLoc());
        return res;
    }

protected:
    void print(std::ostream& os) const override {
        os << name << ":" << typeName;
    }

    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstAttribute*>(&node));
        const auto& other = static_cast<const AstAttribute&>(node);
        return name == other.name && typeName == other.typeName;
    }

private:
    /** Attribute name */
    std::string name;

    /** Type name */
    AstQualifiedName typeName;
};

}  // end of namespace souffle
