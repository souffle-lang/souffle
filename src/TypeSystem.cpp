/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TypeSystem.cpp
 *
 * Covers basic operations constituting Souffle's type system.
 *
 ***********************************************************************/

#include "TypeSystem.h"
#include "RamTypes.h"
#include "Util.h"
#include <cassert>

namespace souffle {

/**
 * A special, internal type for the predefined symbolic and numeric types.
 */
struct PredefinedType : public Type {
    PredefinedType(const TypeEnvironment& environment, const AstQualifiedName& name)
            : Type(environment, name) {}
};

void PrimitiveType::print(std::ostream& out) const {
    out << getName() << " <: " << baseType;
}

void UnionType::add(const Type& type) {
    assert(environment.isType(type));
    elementTypes.emplace_back(type);
}

void UnionType::print(std::ostream& out) const {
    out << getName() << " = "
        << join(elementTypes, " | ", [](auto&& out, auto&& type) { out << type.get().getName(); });
}

void RecordType::add(const std::string& name, const Type& type) {
    assert(environment.isType(type));
    fields.push_back(Field({name, type}));
}

void RecordType::print(std::ostream& out) const {
    out << getName() << " = ";
    if (fields.empty()) {
        out << "()";
        return;
    }
    out << "( " << join(fields, " , ", [](std::ostream& out, const RecordType::Field& f) {
        out << f.name << " : " << f.type.getName();
    }) << " )";
}

void SumType::add(Branch branch) {
    assert(environment.isType(branch.type));
    branches.emplace_back(std::move(branch));
}

void SumType::print(std::ostream& out) const {
    out << getName() << " = "
        << join(branches, " | ", [](auto&& os, auto&& br) { os << br.name << " = " << br.type; });
}

TypeEnvironment::TypeEnvironment() {
    clear();
}

TypeEnvironment::~TypeEnvironment() {
    for (const auto& cur : types) {
        delete cur.second;
    }
}

void TypeEnvironment::clear() {
    // clear list of stored types
    for (const auto& cur : types) {
        delete cur.second;
    }
    types.clear();

    // re-initialize type environment
    createType<PredefinedType>("number");
    createType<PredefinedType>("float");
    createType<PredefinedType>("symbol");
    createType<PredefinedType>("unsigned");
}

bool TypeEnvironment::isType(const identifier& ident) const {
    return types.find(ident) != types.end();
}

bool TypeEnvironment::isType(const Type& type) const {
    const Type& t = getType(type.getName());
    return t == type;
}

Type* TypeEnvironment::getModifiableType(const identifier& name) {
    auto pos = types.find(name);
    return (pos == types.end()) ? nullptr : pos->second;
}

const Type& TypeEnvironment::getType(const identifier& ident) const {
    assert(isType(ident));
    return *(types.find(ident)->second);
}

TypeSet TypeEnvironment::getAllTypes() const {
    TypeSet res;
    for (const auto& cur : types) {
        res.insert(*cur.second);
    }
    return res;
}

void TypeEnvironment::addType(Type& type) {
    const identifier& name = type.getName();
    assert(types.find(name) == types.end() && "Error: registering present type!");
    types[name] = &type;
}

namespace {

/**
 * A visitor for Types.
 */
template <typename R>
struct TypeVisitor {
    virtual ~TypeVisitor() = default;

    R operator()(const Type& type) const {
        return visit(type);
    }

    virtual R visit(const Type& type) const {
        // check all kinds of types and dispatch
        if (auto* t = dynamic_cast<const PredefinedType*>(&type)) {
            return visitPredefinedType(*t);
        }
        if (auto* t = dynamic_cast<const PrimitiveType*>(&type)) {
            return visitPrimitiveType(*t);
        }
        if (auto* t = dynamic_cast<const UnionType*>(&type)) {
            return visitUnionType(*t);
        }
        if (auto* t = dynamic_cast<const RecordType*>(&type)) {
            return visitRecordType(*t);
        }
        if (auto* t = dynamic_cast<const SumType*>(&type)) {
            return visitSumType(*t);
        }
        assert(false && "Unsupported type encountered!");
        return R();
    }

    virtual R visitPredefinedType(const PredefinedType& type) const {
        return visitType(type);
    }

    virtual R visitPrimitiveType(const PrimitiveType& type) const {
        return visitType(type);
    }

    virtual R visitUnionType(const UnionType& type) const {
        return visitType(type);
    }

    virtual R visitRecordType(const RecordType& type) const {
        return visitType(type);
    }

    virtual R visitSumType(const SumType& type) const {
        return visitType(type);
    }

    virtual R visitType(const Type& /*type*/) const {
        return R();
    }
};

/**
 * A visitor for types visiting each type only once (effectively breaking
 * recursive cycles).
 */
template <typename R>
class VisitOnceTypeVisitor : public TypeVisitor<R> {
protected:
    mutable std::map<const Type*, R> seen;

public:
    R visit(const Type& type) const override {
        auto pos = seen.find(&type);
        if (pos != seen.end()) {
            return pos->second;
        }
        auto& res = seen[&type];  // mark as seen
        return res = TypeVisitor<R>::visit(type);
    }
};

template <typename T>
bool isA(const Type& type) {
    return dynamic_cast<const T*>(&type);
}

template <typename T>
const T& as(const Type& type) {
    return static_cast<const T&>(type);
}

/**
 * Determines whether the given type is a sub-type of the given root type.
 */
bool isOfRootType(const Type& type, const Type& root) {
    struct visitor : public VisitOnceTypeVisitor<bool> {
        const Type& root;

        visitor(const Type& root) : root(root) {}

        bool visitPredefinedType(const PredefinedType& type) const override {
            return type == root;
        }
        bool visitPrimitiveType(const PrimitiveType& type) const override {
            return type == root || type.getBaseType() == root || isOfRootType(type.getBaseType(), root);
        }
        bool visitUnionType(const UnionType& type) const override {
            if (type.getElementTypes().empty()) {
                return false;
            }
            return all_of(type.getElementTypes(), [&](auto&& ty) { return visit(ty); });
        }
        bool visitType(const Type& /*unused*/) const override {
            return false;
        }
    };

    return visitor(root).visit(type);
}

bool isUnion(const Type& type) {
    return isA<UnionType>(type);
}

bool isSubType(const Type& a, const UnionType& b) {
    // A is a subtype of b if it is in the transitive closure of b

    struct visitor : public VisitOnceTypeVisitor<bool> {
        const Type& trg;
        visitor(const Type& trg) : trg(trg) {}
        bool visit(const Type& type) const override {
            if (trg == type) {
                return true;
            }
            return VisitOnceTypeVisitor<bool>::visit(type);
        }
        bool visitUnionType(const UnionType& type) const override {
            return any_of(type.getElementTypes(), [&](auto&& ty) { return visit(ty); });
        }
    };

    return visitor(a).visit(b);
}
}  // namespace

/* generate unique type qualifier string for a type */
std::string getTypeQualifier(const Type& type) {
    struct visitor : public VisitOnceTypeVisitor<std::string> {
        std::string visitUnionType(const UnionType& type) const override {
            std::ostringstream output;
            output << visitType(type) << "["
                   << join(type.getElementTypes(), ",", [&](auto& os, auto& x) { os << visit(x); }) << "]";
            return output.str();
        }

        std::string visitSumType(const SumType& type) const override {
            std::ostringstream output;
            output << visitType(type) << "[" << join(type.getBranches(), ";", [&](auto& os, auto& x) {
                os << x.name << " = " << visit(x.type);
            }) << "]";
            return output.str();
        }

        std::string visitRecordType(const RecordType& type) const override {
            std::string str = visitType(type);
            str += "{";
            bool first = true;
            for (auto field : type.getFields()) {
                if (first) {
                    first = false;
                } else {
                    str += ",";
                }
                str += field.name;
                str += "#";
                str += visit(field.type);
            }
            str += "}";
            return str;
        }

        std::string visitType(const Type& type) const override {
            std::string str;

            switch (getTypeAttribute(type)) {
                case TypeAttribute::Signed:
                    str.append("i");
                    break;
                case TypeAttribute::Unsigned:
                    str.append("u");
                    break;
                case TypeAttribute::Float:
                    str.append("f");
                    break;
                case TypeAttribute::Symbol:
                    str.append("s");
                    break;
                case TypeAttribute::Record:
                    str.append("r");
                    break;
                case TypeAttribute::Sum:
                    str.append("+");
                    break;
            }
            str.append(":");
            str.append(toString(type.getName()));
            seen[&type] = str;
            return str;
        }
    };

    return visitor().visit(type);
}

bool isFloatType(const Type& type) {
    return isOfRootType(type, type.getTypeEnvironment().getFloatType());
}

bool isFloatType(const TypeSet& s) {
    return !s.empty() && !s.isAll() && all_of(s, (bool (*)(const Type&)) & isFloatType);
}

bool isNumberType(const Type& type) {
    return isOfRootType(type, type.getTypeEnvironment().getNumberType());
}

bool isNumberType(const TypeSet& s) {
    return !s.empty() && !s.isAll() && all_of(s, (bool (*)(const Type&)) & isNumberType);
}

bool isUnsignedType(const Type& type) {
    return isOfRootType(type, type.getTypeEnvironment().getUnsignedType());
}

bool isUnsignedType(const TypeSet& s) {
    return !s.empty() && !s.isAll() && all_of(s, (bool (*)(const Type&)) & isUnsignedType);
}

bool isSymbolType(const Type& type) {
    return isOfRootType(type, type.getTypeEnvironment().getSymbolType());
}

bool isSymbolType(const TypeSet& s) {
    return !s.empty() && !s.isAll() && all_of(s, (bool (*)(const Type&)) & isSymbolType);
}

bool isRecordType(const Type& type) {
    return dynamic_cast<const RecordType*>(&type) != nullptr;
}

bool isRecordType(const TypeSet& s) {
    return !s.empty() && !s.isAll() && all_of(s, (bool (*)(const Type&)) & isRecordType);
}

bool isSumType(const Type& type) {
    return dynamic_cast<const SumType*>(&type);
}

bool isSumType(const TypeSet& s) {
    return !s.empty() && !s.isAll() && all_of(s, (bool (*)(const Type&)) & isSumType);
}

bool isRecursiveType(const Type& type) {
    struct visitor : public VisitOnceTypeVisitor<bool> {
        const Type& trg;
        visitor(const Type& trg) : trg(trg) {}
        bool visit(const Type& type) const override {
            if (trg == type) {
                return true;
            }
            return VisitOnceTypeVisitor<bool>::visit(type);
        }
        bool visitUnionType(const UnionType& type) const override {
            return any_of(type.getElementTypes(), [&](auto&& ty) { return visit(ty); });
        }
        bool visitRecordType(const RecordType& type) const override {
            return any_of(type.getFields(), [&](auto&& x) { return visit(x.type); });
        }
        bool visitSumType(const SumType& type) const override {
            return any_of(type.getBranches(), [&](auto&& x) { return visit(x.type); });
        }
    };

    // record types are recursive if they contain themselves
    if (const auto* r = dynamic_cast<const RecordType*>(&type)) {
        return visitor(type).visitRecordType(*r);
    }

    // sum types are recursive if any of their branches (indirectly) contains them
    if (const auto* s = dynamic_cast<const SumType*>(&type)) {
        return visitor(type).visitSumType(*s);
    }

    return false;
}

bool isSubtypeOf(const Type& a, const Type& b) {
    // make sure they are both in the same environment
    auto& environment = a.getTypeEnvironment();
    assert(environment.isType(a) && environment.isType(b));

    // first check - a type is a sub-type of itself
    if (a == b) {
        return true;
    }

    // check for predefined types
    if (b == environment.getNumberType()) {
        return isNumberType(a);
    }
    if (b == environment.getSymbolType()) {
        return isSymbolType(a);
    }

    // check primitive type chains
    if (isA<PrimitiveType>(a)) {
        if (isSubtypeOf(as<PrimitiveType>(a).getBaseType(), b)) {
            return true;
        }
    }

    // next - if b is a union type
    if (isUnion(b)) {
        return isSubType(a, as<UnionType>(b));
    }

    // done
    return false;
}

bool areSubtypesOf(const TypeSet& s, const Type& b) {
    return all_of(s, [&](const Type& t) { return isSubtypeOf(t, b); });
}

void TypeEnvironment::print(std::ostream& out) const {
    out << "Types:\n";
    for (const auto& cur : types) {
        out << "\t" << *cur.second << "\n";
    }
}

TypeSet getLeastCommonSupertypes(const Type& a, const Type& b) {
    // make sure they are in the same type environment
    assert(a.getTypeEnvironment().isType(a) && a.getTypeEnvironment().isType(b));

    // if they are equal it is easy
    if (a == b) {
        return TypeSet(a);
    }

    // equally simple - check whether one is a sub-type of the other
    if (isSubtypeOf(a, b)) {
        return TypeSet(b);
    }
    if (isSubtypeOf(b, a)) {
        return TypeSet(a);
    }

    // harder: no obvious relation => hard way
    TypeSet superTypes;
    TypeSet all = a.getTypeEnvironment().getAllTypes();
    for (const Type& cur : all) {
        if (isSubtypeOf(a, cur) && isSubtypeOf(b, cur)) {
            superTypes.insert(cur);
        }
    }

    // filter out non-least super types
    TypeSet res;
    for (const Type& cur : superTypes) {
        bool least = !any_of(superTypes, [&](const Type& t) { return t != cur && isSubtypeOf(t, cur); });
        if (least) {
            res.insert(cur);
        }
    }

    return res;
}

TypeSet getLeastCommonSupertypes(const TypeSet& set) {
    // handle the empty set
    if (set.empty()) {
        return set;
    }

    // handle the all set => empty set (since no common super-type)
    if (set.isAll()) {
        return TypeSet();
    }

    TypeSet res;
    auto it = set.begin();
    res.insert(*it);
    ++it;

    // refine sub-set step by step
    for (; it != set.end(); ++it) {
        TypeSet tmp;
        for (const Type& cur : res) {
            tmp.insert(getLeastCommonSupertypes(cur, *it));
        }
        res = tmp;
    }

    // done
    return res;
}

// pairwise
TypeSet getLeastCommonSupertypes(const TypeSet& a, const TypeSet& b) {
    // special cases
    if (a.empty()) {
        return a;
    }
    if (b.empty()) {
        return b;
    }

    if (a.isAll()) {
        return b;
    }
    if (b.isAll()) {
        return a;
    }

    // compute pairwise least common super types
    TypeSet res;
    for (const Type& x : a) {
        for (const Type& y : b) {
            res.insert(getLeastCommonSupertypes(x, y));
        }
    }
    return res;
}

TypeSet getGreatestCommonSubtypes(const Type& a, const Type& b) {
    // make sure they are in the same type environment
    assert(a.getTypeEnvironment().isType(a) && a.getTypeEnvironment().isType(b));

    // if they are equal it is easy
    if (a == b) {
        return TypeSet(a);
    }

    // equally simple - check whether one is a sub-type of the other
    if (isSubtypeOf(a, b)) {
        return TypeSet(a);
    }
    if (isSubtypeOf(b, a)) {
        return TypeSet(b);
    }

    // last option: if both are unions with common sub-types
    TypeSet res;
    if (isUnion(a) && isUnion(b)) {
        // collect common sub-types of union types

        struct collector : public TypeVisitor<void> {
            const Type& b;
            TypeSet& res;
            collector(const Type& b, TypeSet& res) : b(b), res(res) {}
            void visit(const Type& type) const override {
                if (isSubtypeOf(type, b)) {
                    res.insert(type);
                } else {
                    TypeVisitor<void>::visit(type);
                }
            }
            void visitUnionType(const UnionType& type) const override {
                for (const auto& cur : type.getElementTypes()) {
                    visit(cur);
                }
            }
        };

        // collect all common sub-types
        collector(b, res).visit(a);
    }

    // otherwise there is no common super type
    return res;
}

TypeSet getGreatestCommonSubtypes(const TypeSet& set) {
    // handle the empty set
    if (set.empty()) {
        return set;
    }

    // handle the all set => empty set (since no common sub-type)
    if (set.isAll()) {
        return TypeSet();
    }

    TypeSet res;
    auto it = set.begin();
    res.insert(*it);
    ++it;

    // refine sub-set step by step
    for (; it != set.end(); ++it) {
        TypeSet tmp;
        for (const Type& cur : res) {
            tmp.insert(getGreatestCommonSubtypes(cur, *it));
        }
        res = tmp;
    }

    // done
    return res;
}

TypeSet getGreatestCommonSubtypes(const TypeSet& a, const TypeSet& b) {
    // special cases
    if (a.empty()) {
        return a;
    }
    if (b.empty()) {
        return b;
    }

    if (a.isAll()) {
        return b;
    }
    if (b.isAll()) {
        return a;
    }

    // compute pairwise greatest common sub types
    TypeSet res;
    for (const Type& x : a) {
        for (const Type& y : b) {
            res.insert(getGreatestCommonSubtypes(x, y));
        }
    }
    return res;
}

}  // end of namespace souffle
