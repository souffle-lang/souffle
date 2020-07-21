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
#include "utility/FunctionalUtil.h"
#include "utility/StreamUtil.h"
#include "utility/StringUtil.h"
#include <cassert>

namespace souffle {

void SubsetType::print(std::ostream& out) const {
    out << tfm::format("%s <: %s", getName(), baseType.getName());
}

void UnionType::print(std::ostream& out) const {
    out << tfm::format("%s = %s", getName(),
            join(elementTypes, " | ", [](std::ostream& out, const Type* type) { out << type->getName(); }));
}

void RecordType::print(std::ostream& out) const {
    out << tfm::format("%s = (%s)", getName(),
            join(fields, ", ",
                    [&](std::ostream& out, const Type* fieldType) { out << fieldType->getName(); }));
}

TypeSet TypeEnvironment::initializeConstantTypes() {
    auto& signedConstant = createType<ConstantType>("__numberConstant");
    auto& floatConstant = createType<ConstantType>("__floatConstant");
    auto& symbolConstant = createType<ConstantType>("__symbolConstant");
    auto& unsignedConstant = createType<ConstantType>("__unsignedConstant");

    return TypeSet(signedConstant, floatConstant, symbolConstant, unsignedConstant);
}

TypeSet TypeEnvironment::initializePrimitiveTypes() {
#define CREATE_PRIMITIVE(TYPE)                    \
    auto& TYPE##Type = createType<PrimitiveType>( \
            #TYPE, static_cast<const ConstantType&>(getType("__" #TYPE "Constant")));

    CREATE_PRIMITIVE(number);
    CREATE_PRIMITIVE(float);
    CREATE_PRIMITIVE(symbol);
    CREATE_PRIMITIVE(unsigned);

    return TypeSet(numberType, floatType, symbolType, unsignedType);

#undef CREATE_PRIMITIVE
}

bool TypeEnvironment::isType(const AstQualifiedName& ident) const {
    return types.find(ident) != types.end();
}

bool TypeEnvironment::isType(const Type& type) const {
    return this == &type.getTypeEnvironment();
}

const Type& TypeEnvironment::getType(const AstQualifiedName& ident) const {
    return *types.at(ident);
}

/**
 * A visitor for Types.
 */
template <typename R>
struct TypeVisitor {
    virtual ~TypeVisitor() = default;

    R operator()(const Type& type) const {
        return visit(type);
    }

#define FORWARD(TYPE) \
    if (auto* t = dynamic_cast<const TYPE##Type*>(&type)) return visit##TYPE##Type(*t);

    virtual R visit(const Type& type) const {
        FORWARD(Constant);
        FORWARD(Subset);
        FORWARD(Union);
        FORWARD(Record);

        fatal("Unsupported type encountered!");
    }
#undef FORWARD

#define VISIT(TYPE)                                             \
    virtual R visit##TYPE##Type(const TYPE##Type& type) const { \
        return visitType(type);                                 \
    }

    VISIT(Constant)
    VISIT(Subset)
    VISIT(Union)
    VISIT(Record)

    virtual R visitType(const Type& /*type*/) const {
        return R();
    }

#undef VISIT
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

/**
 * Determines whether the given type is a sub-type of the given root type.
 */
bool isOfRootType(const Type& type, const Type& root) {
    struct visitor : public VisitOnceTypeVisitor<bool> {
        const Type& root;

        explicit visitor(const Type& root) : root(root) {}

        bool visitConstantType(const ConstantType& type) const override {
            return type == root;
        }
        bool visitSubsetType(const SubsetType& type) const override {
            return type == root || isOfRootType(type.getBaseType(), root);
        }
        bool visitUnionType(const UnionType& type) const override {
            return type == root ||
                   all_of(type.getElementTypes(), [&](const Type* cur) { return this->visit(*cur); });
        }

        bool visitRecordType(const RecordType& type) const override {
            return type == root;
        }

        bool visitType(const Type& /*unused*/) const override {
            return false;
        }
    };

    return visitor(root).visit(type);
}

bool isOfKind(const Type& type, TypeAttribute kind) {
    if (kind == TypeAttribute::Record) {
        return isA<RecordType>(type);
    }
    return isOfRootType(type, type.getTypeEnvironment().getConstantType(kind));
}

bool isOfKind(const TypeSet& typeSet, TypeAttribute kind) {
    return !typeSet.empty() && !typeSet.isAll() &&
           all_of(typeSet, [&](const Type& type) { return isOfKind(type, kind); });
}

std::string getTypeQualifier(const Type& type) {
    struct visitor : public VisitOnceTypeVisitor<std::string> {
        std::string visitUnionType(const UnionType& type) const override {
            return tfm::format("%s[%s]", visitType(type),
                    join(type.getElementTypes(), ", ",
                            [&](std::ostream& out, const auto* elementType) { out << visit(*elementType); }));
        }

        std::string visitRecordType(const RecordType& type) const override {
            return tfm::format("%s{%s}", visitType(type),
                    join(type.getFields(), ", ",
                            [&](std::ostream& out, const auto* field) { out << visit(*field); }));
        }

        std::string visitType(const Type& type) const override {
            std::string str;

            if (isOfKind(type, TypeAttribute::Signed)) {
                str.append("i");
            } else if (isOfKind(type, TypeAttribute::Unsigned)) {
                str.append("u");
            } else if (isOfKind(type, TypeAttribute::Float)) {
                str.append("f");
            } else if (isOfKind(type, TypeAttribute::Symbol)) {
                str.append("s");
            } else if (isOfKind(type, TypeAttribute::Record)) {
                str.append("r");
            } else {
                fatal("Unsupported kind");
            }

            str.append(":");
            str.append(toString(type.getName()));
            seen[&type] = str;
            return str;
        }
    };

    return visitor().visit(type);
}

bool isSubtypeOf(const Type& a, const Type& b) {
    assert(&a.getTypeEnvironment() == &b.getTypeEnvironment() &&
            "Types must be in the same type environment");

    if (isOfRootType(a, b)) {
        return true;
    }

    if (isA<UnionType>(a)) {
        return all_of(static_cast<const UnionType&>(a).getElementTypes(),
                [&b](const Type* type) { return isSubtypeOf(*type, b); });
    }

    if (isA<UnionType>(b)) {
        return any_of(static_cast<const UnionType&>(b).getElementTypes(),
                [&a](const Type* type) { return isSubtypeOf(a, *type); });
    }

    return false;
}

void TypeEnvironment::print(std::ostream& out) const {
    out << "Types:\n";
    for (const auto& cur : types) {
        out << "\t" << *cur.second << "\n";
    }
}

TypeSet getGreatestCommonSubtypes(const Type& a, const Type& b) {
    assert(&a.getTypeEnvironment() == &b.getTypeEnvironment() &&
            "Types must be in the same type environment");

    if (isSubtypeOf(a, b)) {
        return TypeSet(a);
    }
    if (isSubtypeOf(b, a)) {
        return TypeSet(b);
    }

    TypeSet res;
    if (isA<UnionType>(a) && isA<UnionType>(b)) {
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
                    visit(*cur);
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
    // Edge cases.
    if (set.empty() || set.isAll()) {
        return TypeSet();
    }

    TypeSet greatestCommonSubtypes;
    greatestCommonSubtypes.insert(*set.begin());

    for (auto& type : set) {
        greatestCommonSubtypes = getGreatestCommonSubtypes(TypeSet(type), greatestCommonSubtypes);
    }

    return greatestCommonSubtypes;
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

bool haveCommonSupertype(const Type& a, const Type& b) {
    assert(&a.getTypeEnvironment() == &b.getTypeEnvironment() &&
            "Types must be in the same type environment");

    if (a == b) {
        return true;
    }

    if (isSubtypeOf(a, b) || isSubtypeOf(b, a)) {
        return true;
    }

    return any_of(a.getTypeEnvironment().getTypes(),
            [&](const Type& type) { return isSubtypeOf(a, type) && isSubtypeOf(b, type); });
}

TypeAttribute getTypeAttribute(const Type& type) {
    for (auto typeAttribute : {TypeAttribute::Signed, TypeAttribute::Unsigned, TypeAttribute::Float,
                 TypeAttribute::Record, TypeAttribute::Symbol}) {
        if (isOfKind(type, typeAttribute)) {
            return typeAttribute;
        }
    }
    fatal("Unknown type class");
}

std::optional<TypeAttribute> getTypeAttribute(const TypeSet& type) {
    for (auto typeAttribute : {TypeAttribute::Signed, TypeAttribute::Unsigned, TypeAttribute::Float,
                 TypeAttribute::Record, TypeAttribute::Symbol}) {
        if (isOfKind(type, typeAttribute)) {
            return typeAttribute;
        }
    }
    return {};
}

}  // end of namespace souffle
