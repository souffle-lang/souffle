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
#include <vector>

#include <ctype.h>

namespace souffle {

// forward declaration
class Type;

/**
 *  Intermediate representation of an attribute which stores the name and the type of an attribute
 *
 *  Attribute has the only name attribute
 */
class AstAttribute : public AstNode {
    /** Attribute name */
    std::string name;

    /** Type name */
    AstTypeIdentifier typeName;

public:
    AstAttribute(const std::string& n, const AstTypeIdentifier& t, const Type* type = NULL)
            : name(n), typeName(t) {}

    const std::string& getAttributeName() const {
        return name;
    }

    const AstTypeIdentifier& getTypeName() const {
        return typeName;
    }

    void setTypeName(const AstTypeIdentifier& name) {
        typeName = name;
    }

    virtual void print(std::ostream& os) const {
        os << name << ":" << typeName;
    }

    /** Creates a clone if this AST sub-structure */
    virtual AstAttribute* clone() const {
        AstAttribute* res = new AstAttribute(name, typeName);
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** Mutates this node */
    virtual void apply(const AstNodeMapper& map) {
        // no nested AST nodes
    }

    /** Obtains a list of all embedded child nodes */
    virtual std::vector<const AstNode*> getChildNodes() const {
        return std::vector<const AstNode*>();
    }

protected:
    /** Implements the node comparison for this node type */
    virtual bool equal(const AstNode& node) const {
        assert(dynamic_cast<const AstAttribute*>(&node));
        const AstAttribute& other = static_cast<const AstAttribute&>(node);
        return name == other.name && typeName == other.typeName;
    }
};

}  // end of namespace souffle
