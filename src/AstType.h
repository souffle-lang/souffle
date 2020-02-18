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
#include "AstQualifiedName.h"
#include "RamTypes.h"

#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 *  @class Type
 *  @brief An abstract base class for types within the AST.
 */
class AstType : public AstNode {
public:
    AstType(AstQualifiedName name = {""}) : name(std::move(name)) {}

    /** get type name */
    const AstQualifiedName& getQualifiedName() const {
        return name;
    }

    /** set type name */
    void setQualifiedName(AstQualifiedName name) {
        this->name = std::move(name);
    }

    AstType* clone() const override = 0;

private:
    /** type name */
    AstQualifiedName name;
};

/**
 * A primitive type is named type that can either be a sub-type of
 * the build-in number or symbol type. Primitive types are the most
 * basic building blocks of souffle's type system.
 */
class AstPrimitiveType : public AstType {
public:
    /** Creates a new primitive type */
    AstPrimitiveType(const AstQualifiedName& name, TypeAttribute type) : AstType(name), type(type) {}

    /** Prints a summary of this type to the given stream */
    void print(std::ostream& os) const override {
        os << ".type " << getQualifiedName() << (type == TypeAttribute::Signed ? "= number" : "");
    }

    /** Tests whether this type is a numeric type */
    bool isNumeric() const {
        return isNumericType(type);
    }

    /** Tests whether this type is a symbolic type */
    bool isSymbolic() const {
        return type == TypeAttribute::Symbol;
    }

    AstPrimitiveType* clone() const override {
        auto res = new AstPrimitiveType(getQualifiedName(), type);
        res->setSrcLoc(getSrcLoc());
        return res;
    }

protected:
    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstPrimitiveType*>(&node));
        const auto& other = static_cast<const AstPrimitiveType&>(node);
        return getQualifiedName() == other.getQualifiedName() && type == other.type;
    }

private:
    /** type attribute */
    TypeAttribute type;
};

/**
 * A union type combines multiple types into a new super type.
 * Each of the enumerated types become a sub-type of the new
 * union type.
 */
class AstUnionType : public AstType {
public:
    void print(std::ostream& os) const override {
        os << ".type " << getQualifiedName() << " = " << join(types, " | ");
    }

    /** Obtains a reference to the list element types */
    const std::vector<AstQualifiedName>& getTypes() const {
        return types;
    }

    /** Adds another element type */
    void add(const AstQualifiedName& type) {
        types.push_back(type);
    }

    /** Set variant type */
    void setVariantType(size_t idx, const AstQualifiedName& type) {
        assert(idx < types.size() && "union variant index out of bounds");
        types[idx] = type;
    }

    AstUnionType* clone() const override {
        auto res = new AstUnionType();
        res->setSrcLoc(getSrcLoc());
        res->setQualifiedName(getQualifiedName());
        res->types = types;
        return res;
    }

protected:
    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstUnionType*>(&node));
        const auto& other = static_cast<const AstUnionType&>(node);
        return getQualifiedName() == other.getQualifiedName() && types == other.types;
    }

private:
    /** The list of types aggregated by this union type */
    std::vector<AstQualifiedName> types;
};

/**
 * A sum type represents one alternative from many.
 */
class AstSumType : public AstType {
public:
    struct Branch {
        std::string name;       // < the branch name
        AstQualifiedName type;  // < the branch type

        bool operator==(const Branch& other) const {
            return this == &other || (name == other.name && type == other.type);
        }

        friend std::ostream& operator<<(std::ostream& os, const Branch& br) {
            return os << br.name << " = " << br.type;
        }
    };

    /** Creates a new sum type */
    AstSumType() = default;

    /** Obtains a reference to the list element types */
    const std::vector<Branch>& getBranches() const {
        return branches;
    }

    /** Adds another element type */
    void add(Branch branch) {
        branches.emplace_back(std::move(branch));
    }

    void setVariantType(size_t idx, AstQualifiedName type) {
        assert(idx < branches.size() && "sum variant index out of bounds");
        branches[idx].type = std::move(type);
    }

    /** Prints a summary of this type to the given stream */
    void print(std::ostream& os) const override {
        os << ".type " << getQualifiedName() << " = " << join(branches, " | ");
    }

    /** Creates a clone of this AST sub-structure */
    AstSumType* clone() const override {
        auto res = new AstSumType();
        res->setSrcLoc(getSrcLoc());
        res->setQualifiedName(getQualifiedName());
        res->branches = branches;
        return res;
    }

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstSumType*>(&node));
        const auto& other = static_cast<const AstSumType&>(node);
        return getQualifiedName() == other.getQualifiedName() && branches == other.branches;
    }

private:
    /**
     * The list of branches for this sum type. */
    std::vector<Branch> branches;
};

/**
 * A record type aggregates a list of fields into a new type.
 * Each record type has a name making it unique. Two record
 * types are unrelated to all other types (they do not have
 * any super or sub types).
 */
class AstRecordType : public AstType {
public:
    /** record field */
    struct Field {
        std::string name;       // < the field name
        AstQualifiedName type;  // < the field type

        bool operator==(const Field& other) const {
            return this == &other || (name == other.name && type == other.type);
        }
    };

    void print(std::ostream& os) const override {
        os << ".type " << getQualifiedName() << " = "
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

    /** add field to record type */
    void add(const std::string& name, const AstQualifiedName& type) {
        fields.push_back(Field({name, type}));
    }

    /** get fields of record */
    const std::vector<Field>& getFields() const {
        return fields;
    }

    /** set field type */
    void setFieldType(size_t idx, const AstQualifiedName& type) {
        assert(idx < fields.size() && "record field index out of bounds");
        fields[idx].type = type;
    }

    AstRecordType* clone() const override {
        auto res = new AstRecordType();
        res->setSrcLoc(getSrcLoc());
        res->setQualifiedName(getQualifiedName());
        res->fields = fields;
        return res;
    }

protected:
    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstRecordType*>(&node));
        const auto& other = static_cast<const AstRecordType&>(node);
        return getQualifiedName() == other.getQualifiedName() && fields == other.fields;
    }

private:
    /** record fields */
    std::vector<Field> fields;
};

}  // end of namespace souffle
