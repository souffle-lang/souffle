/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstRelation.h
 *
 * Defines class Relation that represents relations in a Datalog program.
 * A relation can either be an IDB or EDB relation.
 *
 ***********************************************************************/

#pragma once

#include "RelationTag.h"
#include "SrcLocation.h"
#include "ast/AstAttribute.h"
#include "ast/AstNode.h"
#include "ast/AstQualifiedName.h"
#include "utility/ContainerUtil.h"
#include "utility/StreamUtil.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/*!
 * @class Relation
 * @brief Intermediate representation of a datalog relation
 *
 * A relation has a name, types of its arguments, qualifier type,
 * and dependencies to other relations.
 *
 */
class AstRelation : public AstNode {
public:
    AstRelation() = default;
    AstRelation(AstQualifiedName name, SrcLocation loc = {}) : name(std::move(name)) {
        setSrcLoc(std::move(loc));
    }

    /** get qualified relation name */
    const AstQualifiedName& getQualifiedName() const {
        return name;
    }

    /** Set name for this relation */
    void setQualifiedName(AstQualifiedName n) {
        name = std::move(n);
    }

    /** Add a new used type to this relation */
    void addAttribute(Own<AstAttribute> attr) {
        assert(attr && "Undefined attribute");
        attributes.push_back(std::move(attr));
    }

    /** Return the arity of this relation */
    size_t getArity() const {
        return attributes.size();
    }

    /** Set relation attributes */
    void setAttributes(VecOwn<AstAttribute> attrs) {
        attributes = std::move(attrs);
    }

    /** Get relation attributes */
    std::vector<AstAttribute*> getAttributes() const {
        return toPtrVector(attributes);
    }

    /** Get relation qualifiers */
    const std::set<RelationQualifier>& getQualifiers() const {
        return qualifiers;
    }

    /** Add qualifier to this relation */
    void addQualifier(RelationQualifier q) {
        qualifiers.insert(q);
    }

    /** Remove qualifier from this relation */
    void removeQualifier(RelationQualifier q) {
        qualifiers.erase(q);
    }

    /** Get relation representation */
    RelationRepresentation getRepresentation() const {
        return representation;
    }

    /** Set relation representation */
    void setRepresentation(RelationRepresentation representation) {
        this->representation = representation;
    }

    /** check for a relation qualifier */
    bool hasQualifier(RelationQualifier q) const {
        return qualifiers.find(q) != qualifiers.end();
    }

    AstRelation* clone() const override {
        auto res = new AstRelation(name, getSrcLoc());
        res->attributes = souffle::clone(attributes);
        res->qualifiers = qualifiers;
        res->representation = representation;
        return res;
    }

    void apply(const AstNodeMapper& map) override {
        for (auto& cur : attributes) {
            cur = map(std::move(cur));
        }
    }

    std::vector<const AstNode*> getChildNodes() const override {
        std::vector<const AstNode*> res;
        for (const auto& cur : attributes) {
            res.push_back(cur.get());
        }
        return res;
    }

protected:
    void print(std::ostream& os) const override {
        os << ".decl " << getQualifiedName() << "(" << join(attributes, ", ") << ")" << join(qualifiers, " ")
           << " " << representation;
    }

    bool equal(const AstNode& node) const override {
        const auto& other = static_cast<const AstRelation&>(node);
        return name == other.name && equal_targets(attributes, other.attributes);
    }

    /** Name of relation */
    AstQualifiedName name;

    /** Attributes of the relation */
    VecOwn<AstAttribute> attributes;

    /** Qualifiers of relation */
    std::set<RelationQualifier> qualifiers;

    /** Datastructure to use for this relation */
    RelationRepresentation representation{RelationRepresentation::DEFAULT};
};

/**
 * Lexicographical order for AstRelation
 * using the qualified name as an ordering criteria.
 */
struct AstNameComparison {
    bool operator()(const AstRelation* x, const AstRelation* y) const {
        if (x != nullptr && y != nullptr) {
            return x->getQualifiedName() < y->getQualifiedName();
        }
        return y != nullptr;
    }
};

using AstRelationSet = std::set<const AstRelation*, AstNameComparison>;

}  // end of namespace souffle
