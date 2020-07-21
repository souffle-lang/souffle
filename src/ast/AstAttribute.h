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

#include "SrcLocation.h"
#include "ast/AstNode.h"
#include "ast/AstQualifiedName.h"
#include <ostream>
#include <string>
#include <utility>

namespace souffle {

/**
 *  Intermediate representation of an attribute which stores the name and the type of an attribute
 *
 *  Attribute has the only name attribute
 */
class AstAttribute : public AstNode {
public:
    AstAttribute(std::string n, AstQualifiedName t, SrcLocation loc = {})
            : AstNode(std::move(loc)), name(std::move(n)), typeName(std::move(t)) {}

    /** get attribute name */
    const std::string& getName() const {
        return name;
    }

    /** get type name */
    const AstQualifiedName& getTypeName() const {
        return typeName;
    }

    /** set type name */
    void setTypeName(AstQualifiedName name) {
        typeName = std::move(name);
    }

    AstAttribute* clone() const override {
        return new AstAttribute(name, typeName, getSrcLoc());
    }

protected:
    void print(std::ostream& os) const override {
        os << name << ":" << typeName;
    }

    bool equal(const AstNode& node) const override {
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
