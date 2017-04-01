/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstType.h
 *
 * Defines a type, i.e., disjoint supersets of the universe
 *
 ***********************************************************************/

#pragma once

#include "AstNode.h"

#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace souffle {

/**
 * The type of identifier utilized for referencing types. Type
 * name identifiers are hierarchically qualified names, e.g.
 *
 *          problem.graph.edge
 *
 */
class AstTypeIdentifier {
    /**
     * The list of names forming this identifier.
     */
    std::vector<std::string> names;

public:
    // -- constructors --

    AstTypeIdentifier() : names() {}

    AstTypeIdentifier(const std::string& name) : names(toVector(name)) {}

    AstTypeIdentifier(const char* name) : AstTypeIdentifier(std::string(name)) {}

    AstTypeIdentifier(const AstTypeIdentifier&) = default;
    AstTypeIdentifier(AstTypeIdentifier&&) = default;

    // -- assignment operators --

    AstTypeIdentifier& operator=(const AstTypeIdentifier&) = default;
    AstTypeIdentifier& operator=(AstTypeIdentifier&&) = default;

    // -- mutators --

    void append(const std::string& name) {
        names.push_back(name);
    }

    void prepend(const std::string& name) {
        names.insert(names.begin(), name);
    }

    // -- getters and setters --

    bool empty() const {
        return names.empty();
    }

    const std::vector<std::string>& getNames() const {
        return names;
    }

    // -- comparison operators --

    bool operator==(const AstTypeIdentifier& other) const {
        return names == other.names;
    }

    bool operator!=(const AstTypeIdentifier& other) const {
        return !(*this == other);
    }

    bool operator<(const AstTypeIdentifier& other) const {
        return std::lexicographical_compare(
                names.begin(), names.end(), other.names.begin(), other.names.end());
    }

    void print(std::ostream& out) const {
        out << join(names, ".");
    }

    friend std::ostream& operator<<(std::ostream& out, const AstTypeIdentifier& id) {
        id.print(out);
        return out;
    }
};

/**
 * A overloaded operator to add a new prefix to a given relation identifier.
 */
inline AstTypeIdentifier operator+(const std::string& name, const AstTypeIdentifier& id) {
    AstTypeIdentifier res = id;
    res.prepend(name);
    return res;
}

/**
 *  @class Type
 *  @brief An abstract base class for types within the AST.
 *
 */
class AstType : public AstNode {
    /** In the AST each type has to have a name forming a unique identifier */
    AstTypeIdentifier name;

public:
    /** Creates a new type */
    AstType(const AstTypeIdentifier& name = "") : name(name) {}

    /** Obtains the name of this type */
    const AstTypeIdentifier& getName() const {
        return name;
    }

    /** Updates the name of this type */
    void setName(const AstTypeIdentifier& name) {
        this->name = name;
    }

    /** Obtains a list of all embedded child nodes */
    std::vector<const AstNode*> getChildNodes() const override {
        return {};
    }

    /** Creates a clone if this AST sub-structure */
    AstType* clone() const override = 0;

    /** Mutates this node */
    void apply(const AstNodeMapper& /*map*/) override {
        // no nested nodes in any type
    }
};

/**
 * A primitive type is named type that can either be a sub-type of
 * the build-in number or symbol type. Primitive types are the most
 * basic building blocks of souffle's type system.
 */
class AstPrimitiveType : public AstType {
    /** Indicates whether it is a number (true) or a symbol (false) */
    bool num;

public:
    /** Creates a new primitive type */
    AstPrimitiveType(const AstTypeIdentifier& name, bool num = false) : AstType(name), num(num) {}

    /** Tests whether this type is a numeric type */
    bool isNumeric() const {
        return num;
    }

    /** Tests whether this type is a symbolic type */
    bool isSymbolic() const {
        return !num;
    }

    /** Prints a summary of this type to the given stream */
    void print(std::ostream& os) const override {
        os << ".type " << getName() << (num ? "= number" : "");
    }

    /** Creates a clone if this AST sub-structure */
    AstPrimitiveType* clone() const override {
        return new AstPrimitiveType(getName(), num);
    }

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(dynamic_cast<const AstPrimitiveType*>(&node));
        const AstPrimitiveType& other = static_cast<const AstPrimitiveType&>(node);
        return getName() == other.getName() && num == other.num;
    }
};

/**
 * A union type combines multiple types into a new super type.
 * Each of the enumerated types become a sub-type of the new
 * union type.
 */
class AstUnionType : public AstType {
    /** The list of types aggregated by this union type */
    std::vector<AstTypeIdentifier> types;

public:
    /** Creates a new union type */
    AstUnionType() {}

    /** Obtains a reference to the list element types */
    const std::vector<AstTypeIdentifier>& getTypes() const {
        return types;
    }

    /** Adds another element type */
    void add(const AstTypeIdentifier& type) {
        types.push_back(type);
    }

    /** Prints a summary of this type to the given stream */
    void print(std::ostream& os) const override {
        os << ".type " << getName() << " = " << join(types, " | ");
    }

    /** Creates a clone if this AST sub-structure */
    AstUnionType* clone() const override {
        auto res = new AstUnionType();
        res->setName(getName());
        res->types = types;
        return res;
    }

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(dynamic_cast<const AstUnionType*>(&node));
        const AstUnionType& other = static_cast<const AstUnionType&>(node);
        return getName() == other.getName() && types == other.types;
    }
};

/**
 * A record type aggregates a list of fields into a new type.
 * Each record type has a name making it unique. Two record
 * types are unrelated to all other types (they do not have
 * any super or sub types).
 */
class AstRecordType : public AstType {
public:
    /** The type utilized to model a field */
    struct Field {
        std::string name;        // < the field name
        AstTypeIdentifier type;  // < the field type

        bool operator==(const Field& other) const {
            return this == &other || (name == other.name && type == other.type);
        }
    };

private:
    /** The list of fields constituting this record type */
    std::vector<Field> fields;

public:
    /** Creates a new record type */
    AstRecordType() {}

    /** Adds a new field to this record type */
    void add(const std::string& name, const AstTypeIdentifier& type) {
        fields.push_back(Field({name, type}));
    }

    /** Obtains the list of field constituting this record type */
    const std::vector<Field>& getFields() const {
        return fields;
    }

    /** Prints a summary of this type to the given stream */
    void print(std::ostream& os) const override {
        os << ".type " << getName() << " = "
           << "[";
        for (unsigned i = 0; i < fields.size(); i++) {
            if (i != 0) {
                os << ",";
            }
            os << fields[i].name;
            os << ":";
            os << fields[i].type;
        }
        os << "]";
    }

    /** Creates a clone if this AST sub-structure */
    AstRecordType* clone() const override {
        auto res = new AstRecordType();
        res->setName(getName());
        res->fields = fields;
        return res;
    }

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(dynamic_cast<const AstRecordType*>(&node));
        const AstRecordType& other = static_cast<const AstRecordType&>(node);
        return getName() == other.getName() && fields == other.fields;
    }
};

}  // end of namespace souffle
