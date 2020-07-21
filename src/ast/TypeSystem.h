/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TypeSystem.h
 *
 * Covers basic operations constituting Souffle's type system.
 *
 ***********************************************************************/

#pragma once

#include "RamTypes.h"
#include "ast/AstQualifiedName.h"
#include "utility/ContainerUtil.h"
#include "utility/FunctionalUtil.h"
#include "utility/MiscUtil.h"
#include "utility/StreamUtil.h"
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

// forward declaration
class TypeEnvironment;

/**
 * An abstract base class for types to be covered within a type environment.
 */
class Type {
public:
    Type(const Type& other) = delete;

    virtual ~Type() = default;

    const AstQualifiedName& getName() const {
        return name;
    }

    const TypeEnvironment& getTypeEnvironment() const {
        return environment;
    }

    bool operator==(const Type& other) const {
        return this == &other;
    }

    bool operator!=(const Type& other) const {
        return !(*this == other);
    }

    bool operator<(const Type& other) const {
        return name < other.name;
    }

    virtual void print(std::ostream& out) const {
        out << name;
    }

    friend std::ostream& operator<<(std::ostream& out, const Type& t) {
        return t.print(out), out;
    }

protected:
    Type(const TypeEnvironment& environment, AstQualifiedName name)
            : environment(environment), name(std::move(name)) {}

    /** A reference to the type environment this type is associated to. */
    const TypeEnvironment& environment;

    AstQualifiedName name;
};

/**
 * Representing the type assigned to a constant.
 * ConstantType = NumberConstant/UnsignedConstant/FloatConstant/SymbolConstant
 */
class ConstantType : public Type {
    ConstantType(const TypeEnvironment& environment, const AstQualifiedName& name)
            : Type(environment, name) {}

    friend class TypeEnvironment;
};

/**
 * A type being a subset of another type.
 */
class SubsetType : virtual public Type {
public:
    void print(std::ostream& out) const override;

    const Type& getBaseType() const {
        return baseType;
    }

protected:
    SubsetType(const TypeEnvironment& environment, const AstQualifiedName& name, const Type& base)
            : Type(environment, name), baseType(base){};

private:
    friend class TypeEnvironment;

    const Type& baseType;
};

/**
 * PrimitiveType = Number/Unsigned/Float/Symbol
 * The class representing pre-built, concrete types.
 */
class PrimitiveType : public SubsetType {
public:
    void print(std::ostream& out) const override {
        out << name;
    }

private:
    PrimitiveType(const TypeEnvironment& environment, const AstQualifiedName& name, const ConstantType& base)
            : Type(environment, name), SubsetType(environment, name, base) {}

    friend class TypeEnvironment;
};

/**
 * A union type combining a list of types into a new, aggregated type.
 */
class UnionType : public Type {
public:
    const std::vector<const Type*>& getElementTypes() const {
        return elementTypes;
    }

    void setElements(std::vector<const Type*> elements) {
        elementTypes = std::move(elements);
    }

    void print(std::ostream& out) const override;

protected:
    friend class TypeEnvironment;
    std::vector<const Type*> elementTypes;

    UnionType(const TypeEnvironment& environment, const AstQualifiedName& name,
            std::vector<const Type*> elementTypes = {})
            : Type(environment, name), elementTypes(std::move(elementTypes)) {}
};

/**
 * A record type combining a list of fields into a new, aggregated type.
 */
struct RecordType : virtual public Type {
public:
    void setFields(std::vector<const Type*> newFields) {
        fields = std::move(newFields);
    }

    const std::vector<const Type*>& getFields() const {
        return fields;
    }

    void print(std::ostream& out) const override;

protected:
    friend class TypeEnvironment;

    std::vector<const Type*> fields;

    RecordType(const TypeEnvironment& environment, const AstQualifiedName& name,
            const std::vector<const Type*> fields = {})
            : Type(environment, name), fields(fields) {}
};

/**
 * Type representing a subset type derived from the record type.
 */
struct SubsetRecordType : public SubsetType, public RecordType {
public:
    void print(std::ostream& out) const override {
        SubsetType::print(out);
    }

protected:
    friend class TypeEnvironment;

    SubsetRecordType(
            const TypeEnvironment& environment, const AstQualifiedName& name, const RecordType& baseType)
            : Type(environment, name), SubsetType(environment, name, baseType),
              RecordType(environment, name, baseType.getFields()) {
        // Update fields, replacing each occurrence of base with derived.
        // so that if .type base = [a, base] and derived <: base, then derived = [a, derived].
        std::replace(fields.begin(), fields.end(), dynamic_cast<const Type*>(&baseType),
                dynamic_cast<const Type*>(this));
    };
};

/**
 * A collection to represent sets of types. In addition to ordinary set capabilities
 * it may also represent the set of all types -- without being capable of iterating over those.
 *
 * It is the basic entity to conduct sub- and super-type computations.
 */
struct TypeSet {
public:
    using const_iterator = IterDerefWrapper<typename std::set<const Type*>::const_iterator>;

    TypeSet(bool all = false) : all(all) {}

    template <typename... Types>
    explicit TypeSet(const Types&... types) : all(false) {
        for (const Type* cur : toVector<const Type*>(&types...)) {
            this->types.insert(cur);
        }
    }

    TypeSet(const TypeSet& other) = default;
    TypeSet(TypeSet&& other) = default;
    TypeSet& operator=(const TypeSet& other) = default;
    TypeSet& operator=(TypeSet&& other) = default;

    bool empty() const {
        return !all && types.empty();
    }

    /** Universality check */
    bool isAll() const {
        return all;
    }

    /** Determines the size of this set unless it is the universal set */
    std::size_t size() const {
        assert(!all && "Unable to give size of universe.");
        return types.size();
    }

    /** Determines whether a given type is included or not */
    bool contains(const Type& type) const {
        return all || types.find(&type) != types.end();
    }

    /** Adds the given type to this set */
    void insert(const Type& type) {
        if (!all) {
            types.insert(&type);
        }
    }

    /** Calculate intersection of two TypeSet */
    static TypeSet intersection(const TypeSet& left, const TypeSet& right) {
        TypeSet result;

        if (left.isAll()) {
            return right;
        } else if (right.isAll()) {
            return left;
        }

        for (const auto& element : left) {
            if (right.contains(element)) {
                result.insert(element);
            }
        }

        return result;
    }

    template <typename F>
    TypeSet filter(TypeSet whenAll, F&& f) const {
        if (all) return whenAll;

        TypeSet cpy;
        for (auto&& t : *this)
            if (f(t)) cpy.insert(t);
        return cpy;
    }

    /** Inserts all the types of the given set into this set */
    void insert(const TypeSet& set) {
        if (all) {
            return;
        }

        // if the other set is universal => make this one universal
        if (set.isAll()) {
            all = true;
            types.clear();
            return;
        }

        // add types one by one
        for (const auto& t : set) {
            insert(t);
        }
    }

    /** Allows to iterate over the types contained in this set (only if not universal) */
    const_iterator begin() const {
        assert(!all && "Unable to enumerate universe.");
        return derefIter(types.begin());
    }

    /** Allows to iterate over the types contained in this set (only if not universal) */
    const_iterator end() const {
        assert(!all && "Unable to enumerate universe.");
        return derefIter(types.end());
    }

    /** Determines whether this set is a subset of the given set */
    bool isSubsetOf(const TypeSet& b) const {
        if (all) {
            return b.isAll();
        }
        return all_of(*this, [&](const Type& cur) { return b.contains(cur); });
    }

    /** Determines equality between type sets */
    bool operator==(const TypeSet& other) const {
        return all == other.all && types == other.types;
    }

    /** Determines inequality between type sets */
    bool operator!=(const TypeSet& other) const {
        return !(*this == other);
    }

    /** Adds print support for type sets */
    void print(std::ostream& out) const {
        if (all) {
            out << "{ - all types - }";
        } else {
            out << "{"
                << join(types, ",", [](std::ostream& out, const Type* type) { out << type->getName(); })
                << "}";
        }
    }

    friend std::ostream& operator<<(std::ostream& out, const TypeSet& set) {
        set.print(out);
        return out;
    }

private:
    /** True if it is the all-types set, false otherwise */
    bool all;

    /** The enumeration of types in case it is not the all-types set */
    std::set<const Type*, deref_less<Type>> types;
};

/**
 * A type environment is a set of types. It's main purpose is to provide an enumeration
 * of all all types within a given program. Additionally, it manages the life cycle of
 * type instances.
 */
class TypeEnvironment {
public:
    TypeEnvironment()
            : constantTypes(initializeConstantTypes()),
              constantNumericTypes(TypeSet(getType("__numberConstant"), getType("__unsignedConstant"),
                      getType("__floatConstant"))),
              primitiveTypes(initializePrimitiveTypes()){};

    TypeEnvironment(const TypeEnvironment&) = delete;

    virtual ~TypeEnvironment() = default;

    /** create type in this environment */
    template <typename T, typename... Args>
    T& createType(const AstQualifiedName& name, Args&&... args) {
        assert(types.find(name) == types.end() && "Error: registering present type!");
        auto* newType = new T(*this, name, std::forward<Args>(args)...);
        types[name] = std::unique_ptr<Type>(newType);
        return *newType;
    }

    bool isType(const AstQualifiedName&) const;
    bool isType(const Type& type) const;

    const Type& getType(const AstQualifiedName&) const;

    const Type& getConstantType(TypeAttribute type) const {
        switch (type) {
            case TypeAttribute::Signed: return getType("__numberConstant");
            case TypeAttribute::Unsigned: return getType("__unsignedConstant");
            case TypeAttribute::Float: return getType("__floatConstant");
            case TypeAttribute::Symbol: return getType("__symbolConstant");
            case TypeAttribute::Record: break;
        }

        fatal("There is no constant record type");
    }

    bool isPrimitiveType(const AstQualifiedName& identifier) const {
        if (isType(identifier)) {
            return isPrimitiveType(getType(identifier));
        }
        return false;
    }

    bool isPrimitiveType(const Type& type) const {
        return primitiveTypes.contains(type);
    }

    const TypeSet& getConstantTypes() const {
        return constantTypes;
    }

    const TypeSet& getPrimitiveTypes() const {
        return primitiveTypes;
    }

    const TypeSet& getConstantNumericTypes() const {
        return constantNumericTypes;
    }

    void print(std::ostream& out) const;

    friend std::ostream& operator<<(std::ostream& out, const TypeEnvironment& environment) {
        environment.print(out);
        return out;
    }

    TypeSet getTypes() const {
        TypeSet types;
        for (auto& type : types) {
            types.insert(type);
        }
        return types;
    }

private:
    TypeSet initializePrimitiveTypes();
    TypeSet initializeConstantTypes();

    /** The list of covered types. */
    std::map<AstQualifiedName, std::unique_ptr<Type>> types;

    const TypeSet constantTypes;
    const TypeSet constantNumericTypes;

    const TypeSet primitiveTypes;
};

// ---------------------------------------------------------------
//                          Type Utilities
// ---------------------------------------------------------------

/**
 * Determines whether type a is a subtype of type b.
 */
bool isSubtypeOf(const Type& a, const Type& b);

/**
 * Returns full type qualifier for a given type
 */
std::string getTypeQualifier(const Type& type);

/**
 * Check if the type is of a kind corresponding to the TypeAttribute.
 */
bool isOfKind(const Type& type, TypeAttribute kind);
bool isOfKind(const TypeSet& typeSet, TypeAttribute kind);

TypeAttribute getTypeAttribute(const Type&);
std::optional<TypeAttribute> getTypeAttribute(const TypeSet&);

inline bool isNumericType(const TypeSet& type) {
    return isOfKind(type, TypeAttribute::Signed) || isOfKind(type, TypeAttribute::Unsigned) ||
           isOfKind(type, TypeAttribute::Float);
}

inline bool isOrderableType(const TypeSet& type) {
    return isNumericType(type) || isOfKind(type, TypeAttribute::Symbol);
}

// -- Greatest Common Sub Types --------------------------------------

/**
 * Computes the greatest common sub types of the two given types.
 */
TypeSet getGreatestCommonSubtypes(const Type& a, const Type& b);

/**
 * Computes the greatest common sub types of all the types in the given set.
 */
TypeSet getGreatestCommonSubtypes(const TypeSet& set);

/**
 * The set of pair-wise greatest common sub types of the types in the two given sets.
 */
TypeSet getGreatestCommonSubtypes(const TypeSet& a, const TypeSet& b);

/**
 * Computes the greatest common sub types of the given types.
 */
template <typename... Types>
TypeSet getGreatestCommonSubtypes(const Types&... types) {
    return getGreatestCommonSubtypes(TypeSet(types...));
}

/**
 * Determine if there exist a type t such that a <: t and b <: t
 */
bool haveCommonSupertype(const Type& a, const Type& b);

}  // end namespace souffle
