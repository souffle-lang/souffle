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

#include "AstType.h"
#include "IterUtils.h"
#include "Util.h"
#include <cassert>
#include <iostream>
#include <map>
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
    Type(const TypeEnvironment& environment, AstQualifiedName name)
            : environment(environment), name(std::move(name)) {}

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

    virtual void print(std::ostream& out = std::cout) const {
        out << name;
    }

    friend std::ostream& operator<<(std::ostream& out, const Type& t) {
        return t.print(out), out;
    }

    friend std::ostream& operator<<(std::ostream& out, const Type* t) {
        if (t == nullptr) {
            return out << "-null-";
        }
        return t->print(out), out;
    }

protected:
    /** A reference to the type environment this type is associated to. */
    const TypeEnvironment& environment;

private:
    /** The name of this type. */
    AstQualifiedName name;
};

/**
 * PredefinedType = Number/Unsigned/Float/Symbol
 */
struct PredefinedType : public Type {
    PredefinedType(const TypeEnvironment& environment, const AstQualifiedName& name)
            : Type(environment, name) {}
};

/**
 * A primitive type. The basic type construct to build new types.
 */
class PrimitiveType : public Type {
public:
    void print(std::ostream& out) const override;

    const Type& getBaseType() const {
        return baseType;
    }

private:
    // only allow type environments to create instances
    friend class TypeEnvironment;

    /** The base type -- may be symbol or numeric */
    const Type& baseType;

    PrimitiveType(const TypeEnvironment& environment, const AstQualifiedName& name, const Type& base)
            : Type(environment, name), baseType(base) {}
};

/**
 * A union type combining a list of types into a new, aggregated type.
 */
class UnionType : public Type {
public:
    void add(const Type& type);

    const std::vector<const Type*>& getElementTypes() const {
        return elementTypes;
    }

    void print(std::ostream& out) const override;

private:
    // only allow type environments to create instances
    friend class TypeEnvironment;

    /** The contained element types */
    std::vector<const Type*> elementTypes;

    UnionType(const TypeEnvironment& environment, const AstQualifiedName& name) : Type(environment, name) {}
};

/**
 * A record type combining a list of fields into a new, aggregated type.
 */
struct RecordType : public Type {
public:
    /** The type to model fields */
    struct Field {
        std::string name;  // < the name of the field
        const Type& type;  // < the type of the field
    };

    /** The list of contained fields */
    void add(const std::string& name, const Type& type);

    const std::vector<Field>& getFields() const {
        return fields;
    }

    void print(std::ostream& out) const override;

private:
    // only allow type environments to create instances
    friend class TypeEnvironment;

    std::vector<Field> fields;

    RecordType(const TypeEnvironment& environment, const AstQualifiedName& name) : Type(environment, name) {}
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

    // -- constructors, destructors and assignment operations --

    TypeSet(bool all = false) : all(all) {}

    TypeSet(const TypeSet& other) = default;

    TypeSet(TypeSet&& other) noexcept : all(other.all), types() {
        types.swap(other.types);
    }

    template <typename... Types>
    explicit TypeSet(const Types&... types) : all(false) {
        for (const Type* cur : toVector<const Type*>(&types...)) {
            this->types.insert(cur);
        }
    }

    TypeSet& operator=(const TypeSet& other) = default;

    /** A factory function for the all-types set */
    static TypeSet getAllTypes() {
        return TypeSet(true);
    }

    /** Emptiness check */
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
        if (all) {
            return;
        }
        types.insert(&type);
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
    // -- constructors / destructores --
    TypeEnvironment();

    TypeEnvironment(const TypeEnvironment&) = delete;

    ~TypeEnvironment();

    // -- create types in this environment --

    template <typename T, typename... Args>
    T& createType(const AstQualifiedName& name, const Args&... args) {
        auto* res = new T(*this, name, args...);
        addType(*res);
        return *res;
    }

    PrimitiveType& createNumericType(const AstQualifiedName& name) {
        return createType<PrimitiveType>(name, getNumberType());
    }

    PrimitiveType& createFloatType(const AstQualifiedName& name) {
        return createType<PrimitiveType>(name, getFloatType());
    }

    PrimitiveType& createUnsignedType(const AstQualifiedName& name) {
        return createType<PrimitiveType>(name, getUnsignedType());
    }

    PrimitiveType& createSymbolType(const AstQualifiedName& name) {
        return createType<PrimitiveType>(name, getSymbolType());
    }

    UnionType& createUnionType(const AstQualifiedName& name) {
        return createType<UnionType>(name);
    }

    RecordType& createRecordType(const AstQualifiedName& name) {
        return createType<RecordType>(name);
    }

    // -- query type information --

    bool isType(const AstQualifiedName&) const;

    bool isType(const Type& type) const;

    const Type& getType(const AstQualifiedName&) const;

    const Type& getNumberType() const {
        return getType("number");
    }

    const Type& getUnsignedType() const {
        return getType("unsigned");
    }

    const Type& getFloatType() const {
        return getType("float");
    }

    const Type& getSymbolType() const {
        return getType("symbol");
    }

    TypeSet predefinedTypes() const {
        return TypeSet(getType("number"), getType("unsigned"), getType("float"), getType("symbol"));
    }

    bool isPredefinedType(const AstQualifiedName& identifier) const {
        if (isType(identifier)) {
            return isPredefinedType(getType(identifier));
        } else {
            return false;
        }
    }

    bool isPredefinedType(const Type& typeName) const {
        return predefinedTypes().contains(typeName);
    }

    TypeSet getAllTypes() const;

    Type* getModifiableType(const AstQualifiedName& name);

    void clear();

    void print(std::ostream& out) const;

    friend std::ostream& operator<<(std::ostream& out, const TypeEnvironment& environment) {
        environment.print(out);
        return out;
    }

    void swap(TypeEnvironment& env) {
        types.swap(env.types);
    }

private:
    /** The list of covered types */
    std::map<AstQualifiedName, Type*> types;

    /** Register types created by one of the factory functions */
    void addType(Type& type);
};

// ---------------------------------------------------------------
//                          Type Utilities
// ---------------------------------------------------------------

/**
 * Returns full type qualifier for a given type
 */
std::string getTypeQualifier(const Type& type);

/**
 * Determine if a type analysis' result is equivalent to the given TypeAttribute.
 */
template <typename T>  // T = Type or T = Typeset
bool eqTypeTypeAttribute(const TypeAttribute ramType, const T& type) {
    switch (ramType) {
        case TypeAttribute::Signed:
            return isNumberType(type);
        case TypeAttribute::Unsigned:
            return isUnsignedType(type);
        case TypeAttribute::Float:
            return isFloatType(type);
        case TypeAttribute::Symbol:
            return isSymbolType(type);
        case TypeAttribute::Record:
            return isRecordType(type);
    }
    assert(false && "unhandled `TypeAttribute`");
    exit(EXIT_FAILURE);
}

/**
 * Convert a type analysis' type/set of type to the the TypeAttribute
 */
template <typename T>  // T = Type or T = Typeset
TypeAttribute getTypeAttribute(const T& type) {
    TypeAttribute primitiveType;
    if (isNumberType(type)) {
        primitiveType = TypeAttribute::Signed;
    } else if (isUnsignedType(type)) {
        primitiveType = TypeAttribute::Unsigned;
    } else if (isFloatType(type)) {
        primitiveType = TypeAttribute::Float;
    } else if (isRecordType(type)) {
        primitiveType = TypeAttribute::Record;
    } else if (isSymbolType(type)) {
        primitiveType = TypeAttribute::Symbol;
    } else {
        std::cerr << "Unknown type class" << std::endl;
        std::exit(EXIT_FAILURE);
    }
    return primitiveType;
}

/**
 * Determines whether the type is numeric.
 */
template <typename T>  // T = Type or T = Typeset
inline bool isNumericType(const T& type) {
    return isFloatType(type) || isNumberType(type) || isUnsignedType(type);
}

template <typename T>  // T = Type or T = Typeset
inline bool isOrderableType(const T& type) {
    return isNumericType(type) || isSymbolType(type);
}

/**
 * Is any value in the set signed
 **/
bool hasSignedType(const TypeSet& types);

/**
 * Is any value in the set unsigned
 **/
bool hasUnsignedType(const TypeSet& types);

/**
 * Is any value in the set float
 **/
bool hasFloatType(const TypeSet& types);

/**
 * Determines whether the given type is a float type.
 */
bool isFloatType(const Type& type);

/**
 * Determines whether all the types in the given set are float types.
 */
bool isFloatType(const TypeSet& s);

/**
 * Determines whether the given type is a number type.
 */
bool isNumberType(const Type& type);

/**
 * Determines whether all the types in the given set are number types.
 */
bool isNumberType(const TypeSet& s);

/**
 * Determines whether the given type is a number type.
 */
bool isUnsignedType(const Type& type);

/**
 * Determines whether all the types in the given set are number types.
 */
bool isUnsignedType(const TypeSet& s);

/**
 * Determines whether the given type is a symbol type.
 */
bool isSymbolType(const Type& type);

/**
 * Determines whether all the types in the given set are symbol types.
 */
bool isSymbolType(const TypeSet& s);

/**
 * Determines whether the given type is a record type.
 */
bool isRecordType(const Type& type);

/**
 * Determines whether all the types in the given set are record types.
 */
bool isRecordType(const TypeSet& s);

/**
 * Determines whether the given type is a recursive type.
 */
bool isRecursiveType(const Type& type);

/**
 * Determines whether type a is a subtype of type b.
 */
bool isSubtypeOf(const Type& a, const Type& b);

/**
 * Determines whether all types in s are subtypes of type b.
 */
bool areSubtypesOf(const TypeSet& s, const Type& b);

// -- Least Common Super Types ----------------------------------------

/**
 * Computes the least common super types of the two given types.
 */
TypeSet getLeastCommonSupertypes(const Type& a, const Type& b);

/**
 * Computes the least common super types of all the types in the given set.
 */
TypeSet getLeastCommonSupertypes(const TypeSet& set);

/**
 * The set of pair-wise least common super types of the types in the two given sets.
 */
TypeSet getLeastCommonSupertypes(const TypeSet& a, const TypeSet& b);

/**
 * Computes the least common super types of the given types.
 */
template <typename... Types>
TypeSet getLeastCommonSupertypes(const Types&... types) {
    return getLeastCommonSupertypes(TypeSet(types...));
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

}  // end namespace souffle
