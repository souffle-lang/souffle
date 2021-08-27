/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Relation.h
 *
 * Defines the relation class and helper its classes
 *
 ***********************************************************************/

#pragma once

#include "RelationTag.h"
#include "ast/Attribute.h"
#include "ast/FunctionalConstraint.h"
#include "ast/Node.h"
#include "ast/QualifiedName.h"
#include "parser/SrcLocation.h"
#include <cstddef>
#include <iosfwd>
#include <set>
#include <vector>

namespace souffle::ast {

/**
 * @class Relation
 * @brief Defines a relation with a name, attributes, qualifiers, and internal representation.
 */
class Relation : public Node {
public:
    Relation() = default;
    Relation(QualifiedName name, SrcLocation loc = {});

    /** Get qualified relation name */
    const QualifiedName& getQualifiedName() const {
        return name;
    }

    /** Set name for this relation */
    void setQualifiedName(QualifiedName n);

    /** Add a new used type to this relation */
    void addAttribute(Own<Attribute> attr);

    /** Return the arity of this relation */
    std::size_t getArity() const {
        return attributes.size();
    }

    /** Set relation attributes */
    void setAttributes(VecOwn<Attribute> attrs);

    /** Get relation attributes */
    std::vector<Attribute*> getAttributes() const;

    /** Get relation qualifiers */
    const std::set<RelationQualifier>& getQualifiers() const {
        return qualifiers;
    }

    /** Add qualifier to this relation */
    bool addQualifier(RelationQualifier q) {
        return qualifiers.insert(q).second;
    }

    /** Remove qualifier from this relation */
    bool removeQualifier(RelationQualifier q) {
        return qualifiers.erase(q) != 0;
    }

    /** Get relation representation */
    RelationRepresentation getRepresentation() const {
        return representation;
    }

    /** Set relation representation */
    void setRepresentation(RelationRepresentation representation) {
        this->representation = representation;
    }

    /** Check for a relation qualifier */
    bool hasQualifier(RelationQualifier q) const {
        return qualifiers.find(q) != qualifiers.end();
    }

    /** Add functional dependency to this relation */
    void addDependency(Own<FunctionalConstraint> fd);

    std::vector<FunctionalConstraint*> getFunctionalDependencies() const;

    void apply(const NodeMapper& map) override;

protected:
    void print(std::ostream& os) const override;

    NodeVec getChildren() const override;

private:
    bool equal(const Node& node) const override;

    Relation* cloning() const override;

private:
    /** Name of relation */
    QualifiedName name;

    /** Attributes of the relation */
    VecOwn<Attribute> attributes;

    /** Qualifiers of relation */
    std::set<RelationQualifier> qualifiers;

    /** Functional dependencies of the relation */
    VecOwn<FunctionalConstraint> functionalDependencies;

    /** Datastructure to use for this relation */
    RelationRepresentation representation{RelationRepresentation::DEFAULT};
};

/**
 * @class NameComparison
 * @brief Comparator for relations
 *
 * Lexicographical order for Relation
 * using the qualified name as an ordering criteria.
 */
struct NameComparison {
    bool operator()(const Relation* x, const Relation* y) const {
        if (x != nullptr && y != nullptr) {
            return x->getQualifiedName() < y->getQualifiedName();
        }
        return y != nullptr;
    }
};

/** Relation set */
using RelationSet = std::set<const Relation*, NameComparison>;

}  // namespace souffle::ast
