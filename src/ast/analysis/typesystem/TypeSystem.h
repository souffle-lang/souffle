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
 * Souffle's Type System
 *
 ***********************************************************************/

#pragma once

#include "ast/QualifiedName.h"
#include "ast/Type.h"
#include "souffle/TypeAttribute.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/tinyformat.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ast::analysis {

class TypeEnvironment;

/**
 * Abstract Type Class
 *
 * A type in Souffle can be either a primitive type,
 * a constant type, a subset type, a union type, a
 * record type, an algebraic data-type.
 *
 * Type = PrimitiveType | ConstantType | SubsetType | AliasType |
 *        UnionType | RecordType | AlgebraicDataType
 */
class Type {
public:
    Type(const Type& other) = delete;

    virtual ~Type() = default;

    const QualifiedName& getName() const {
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
    Type(const TypeEnvironment& environment, QualifiedName name)
            : environment(environment), name(std::move(name)) {}

    /** Type environment of type */
    const TypeEnvironment& environment;

    /** Qualified type name */
    QualifiedName name;
};

/**
 * Constant Type Class
 *
 * This type class represents the type of a constant. Currently, we have
 * in Souffle following constant types: number, unsigned, float, and symbol.
 *
 * ConstantType =  NumberConstant | UnsignedConstant |
 *                 FloatConstant | SymbolConstant
 */
class ConstantType : public Type {
    ConstantType(const TypeEnvironment& environment, const QualifiedName& name) : Type(environment, name) {}

private:
    friend class TypeEnvironment;
};

/*
 * Subset Type Class
 *
 * A subset-type of a type supports substitutability
 * (also written as Subtype <: Type).
 *
 * SubsetType = subset(T)
 *
 * where T is a type.
 */
class SubsetType : virtual public Type {
public:
    void print(std::ostream& out) const override;

    const Type& getBaseType() const {
        return baseType;
    }

protected:
    SubsetType(const TypeEnvironment& environment, const QualifiedName& name, const Type& base)
            : Type(environment, name), baseType(base){};

private:
    friend class TypeEnvironment;

    /** Base type */
    const Type& baseType;
};

/*
 * Alias Type Class
 *
 * A alias-type of a type supports type equivalence
 * (also written as AliasType = Type).
 *
 * AliasType = alias(T)
 *
 * where T is a type.
 */
class AliasType : public Type {
public:
    void print(std::ostream& out) const override;

    const Type& getAliasType() const {
        return aliasType;
    }

protected:
    AliasType(const TypeEnvironment& environment, const QualifiedName& name, const Type& alias)
            : Type(environment, name), aliasType(alias){};

private:
    friend class TypeEnvironment;

    /** Base type */
    const Type& aliasType;
};

/**
 * Primitive Type Class
 *
 * Primitive types in Souffle are the actual computational
 * domains. Currently, we have number, unsigned, float,
 * and symbol. This class are pre-built and are concrete
 * types in the RAM (not user-defined types).
 *
 * PrimitiveType = Number | Unsigned |
 *                 Float | Symbol
 */
class PrimitiveType : public SubsetType {
public:
    void print(std::ostream& out) const override {
        out << name;
    }

protected:
    PrimitiveType(const TypeEnvironment& environment, const QualifiedName& name, const ConstantType& base)
            : Type(environment, name), SubsetType(environment, name, base) {}

private:
    friend class TypeEnvironment;
};

/**
 * Union Type Class
 *
 * A union type is a type that supports a union of types
 * and behaves as a least common super-type of them.
 *
 * UnionType = union(T1, ..., Tn)
 *
 * where T1, ..., Tn are types.
 *
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
    UnionType(const TypeEnvironment& environment, const QualifiedName& name,
            std::vector<const Type*> elementTypes = {})
            : Type(environment, name), elementTypes(std::move(elementTypes)) {}

private:
    friend class TypeEnvironment;

    /** List of types */
    std::vector<const Type*> elementTypes;
};

/**
 * Record Type Class
 *
 * A record type is a type of tuples. The elements of the tuple
 * have types themselves.
 *
 * RecordType = record(T1, ..., Tn)
 *
 * where T1, ..., Tn are the types of the tuple elements.
 */
struct RecordType : public Type {
public:
    void setFields(std::vector<const Type*> newFields) {
        fields = std::move(newFields);
    }

    const std::vector<const Type*>& getFields() const {
        return fields;
    }

    void print(std::ostream& out) const override;

protected:
    RecordType(const TypeEnvironment& environment, const QualifiedName& name,
            const std::vector<const Type*> fields = {})
            : Type(environment, name), fields(fields) {}

private:
    friend class TypeEnvironment;

    /* Fields of record type */
    std::vector<const Type*> fields;
};

/**
 * Algebraic Data Type Class
 *
 * An algebraic datatype is a polymorphic datatype that
 * consists of several branches.
 *
 * AlgebraicDataType = adt(BranchType1, ...,  BranchTypem).
 *
 * A branch is assigned to a unique algebraic datatype
 * (i.e., a branch cannot be used for several types).
 * A symbolic label identifies a branch and it has a
 * tuple type.
 *
 * BranchType = branch(S, T1, ..., Tn)
 *
 * where S is a symbol and T1, ..., Tn are types.
 *
 * Implementation invariant: branches are in stored in
 * lexicographical order.
 */
class AlgebraicDataType : public Type {
public:
    struct Branch {
        QualifiedName name;              // < the name of the branch
        std::vector<const Type*> types;  // < Product type associated with this branch.

        void print(std::ostream& out) const {
            out << tfm::format("%s {%s}", this->name,
                    join(types, ", ", [](std::ostream& out, const Type* t) { out << t->getName(); }));
        }
    };

    void print(std::ostream& out) const override {
        out << tfm::format("%s = %s", name,
                join(branches, " | ", [](std::ostream& out, const Branch& branch) { branch.print(out); }));
    }

    void setBranches(std::vector<Branch> bs) {
        branches = std::move(bs);
        std::sort(branches.begin(), branches.end(),
                [](const Branch& left, const Branch& right) { return left.name < right.name; });
    }

    const std::vector<const Type*>& getBranchTypes(const QualifiedName& name) const {
        for (auto& branch : branches) {
            if (branch.name == name) return branch.types;
        }
        // Branch doesn't exist.
        throw std::out_of_range("Trying to access non-existing branch.");
    }

    /** Return the branches as a sorted vector */
    const std::vector<Branch>& getBranches() const {
        return branches;
    }

protected:
    AlgebraicDataType(const TypeEnvironment& env, QualifiedName name) : Type(env, std::move(name)) {}

private:
    friend class TypeEnvironment;

    /** Branches of ADT */
    std::vector<Branch> branches;
};

/**
 * Type Set Class
 *
 * A sets of types with the ability to express the set of all
 * types without being capable of iterating over those.
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

    /** Empty check */
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

    /** Membership of a type */
    bool contains(const Type& type) const {
        return all || types.find(&type) != types.end();
    }

    /** Insert a new type */
    void insert(const Type& type) {
        if (!all) {
            types.insert(&type);
        }
    }

    /** Intersection of two type sets */
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

    /** Filter a typeset with a filter expression */
    template <typename F>
    TypeSet filter(TypeSet whenAll, F&& f) const {
        if (all) return whenAll;

        TypeSet cpy;
        for (auto&& t : *this)
            if (f(t)) cpy.insert(t);
        return cpy;
    }

    /** Union two type sets */
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

    /** Returns iterator pointing to the first type in the type set (only if not universal) */
    const_iterator begin() const {
        assert(!all && "Unable to enumerate universe.");
        return derefIter(types.begin());
    }

    /** Returns iterator pointing to the past-the-end type in the type set (only if not universal) */
    const_iterator end() const {
        assert(!all && "Unable to enumerate universe.");
        return derefIter(types.end());
    }

    /** Checks whether the set is a subset */
    bool isSubsetOf(const TypeSet& b) const {
        if (all) {
            return b.isAll();
        }
        return all_of(*this, [&](const Type& cur) { return b.contains(cur); });
    }

    /** Equality */
    bool operator==(const TypeSet& other) const {
        return all == other.all && types == other.types;
    }

    /** Inequality */
    bool operator!=(const TypeSet& other) const {
        return !(*this == other);
    }

    /** Print type set */
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

    /** Set of types if flag "all" is not set; otherwise we assume an empty set */
    std::set<const Type*, deref_less<Type>> types;
};

/**
 * Type Environment Class
 *
 * Stores named types for a given program instance.
 */
class TypeEnvironment {
public:
    TypeEnvironment() = default;
    TypeEnvironment(const TypeEnvironment&) = delete;
    virtual ~TypeEnvironment() = default;

    /** Create a new named type in this environment */
    template <typename T, typename... Args>
    T& createType(const QualifiedName& name, Args&&... args) {
        assert(types.find(name) == types.end() && "Error: registering present type!");
        auto newType = Own<T>(new T(*this, name, std::forward<Args>(args)...));
        T& res = *newType;
        types[name] = std::move(newType);
        return res;
    }

    bool isType(const QualifiedName&) const;
    bool isType(const Type& type) const;

    const Type& getType(const QualifiedName&) const;
    const Type& getType(const ast::Type&) const;

    const Type& getConstantType(TypeAttribute type) const {
        switch (type) {
            case TypeAttribute::Signed: return getType("__numberConstant");
            case TypeAttribute::Unsigned: return getType("__unsignedConstant");
            case TypeAttribute::Float: return getType("__floatConstant");
            case TypeAttribute::Symbol: return getType("__symbolConstant");
            case TypeAttribute::Record: return getType("__numberConstant");
            case TypeAttribute::ADT: return getType("__numberConstant");
        }
        fatal("Unhandled type");
    }

    bool isPrimitiveType(const QualifiedName& identifier) const {
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

    /** Map for storing named types */
    std::map<QualifiedName, Own<Type>> types;

    const TypeSet constantTypes = initializeConstantTypes();
    const TypeSet constantNumericTypes =
            TypeSet(getType("__numberConstant"), getType("__unsignedConstant"), getType("__floatConstant"));

    const TypeSet primitiveTypes = initializePrimitiveTypes();
};

// ---------------------------------------------------------------
//                          Type Utilities
// ---------------------------------------------------------------

/** Check subtype relationship between two types */
bool isSubtypeOf(const Type& a, const Type& b);

/** Returns fully qualified name for a given type */
std::string getTypeQualifier(const Type& type);

/** Check if the type is of a kind corresponding to the TypeAttribute (does not traverse sub-type and
 * alias-type if kind is ADT or Record) */
bool isOfKind(const Type& type, TypeAttribute kind);
bool isOfKind(const TypeSet& typeSet, TypeAttribute kind);

/** Check if the type is a direct or a sub-type or an alias-type of a kind corresponding to the TypeAttribute
 */
bool isBaseOfKind(const Type& type, TypeAttribute kind);

/** Get type attributes */
TypeAttribute getTypeAttribute(const Type&);

/** Get type attribute */
std::optional<TypeAttribute> getTypeAttribute(const TypeSet&);

/** Check whether it is a numeric type */
inline bool isNumericType(const TypeSet& type) {
    return isOfKind(type, TypeAttribute::Signed) || isOfKind(type, TypeAttribute::Unsigned) ||
           isOfKind(type, TypeAttribute::Float);
}

/**
 * Determine if ADT is enumerations (are all constructors empty)
 */
bool isADTEnum(const AlgebraicDataType& type);

/** Check whether it is a oderable type */
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

/** Return the base type of type, skipping aliases and ascending sub-types. */
const Type& getBaseType(const Type* type);

/** Return the un-aliased type of type. */
const Type& skipAliasesType(const Type* type);

/** Return the un-aliased type of type. */
const Type& skipAliasesType(const Type& type);

/**
 * Determine if two types are equivalent.
 * That is, check if a <: b and b <: a
 */
bool areEquivalentTypes(const Type& a, const Type& b);

}  // namespace souffle::ast::analysis
