/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamRelation.h
 *
 * Defines the class for ram relations
 ***********************************************************************/

#pragma once

#include "RelationTag.h"
#include "ram/RamNode.h"
#include "utility/ContainerUtil.h"
#include <cassert>
#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 * @class RamRelation
 * @brief A RAM Relation in the RAM intermediate representation.
 */
class RamRelation : public RamNode {
public:
    RamRelation(std::string name, size_t arity, size_t auxiliaryArity,
            std::vector<std::string> attributeNames, std::vector<std::string> attributeTypes,
            RelationRepresentation representation)
            : representation(representation), name(std::move(name)), arity(arity),
              auxiliaryArity(auxiliaryArity), attributeNames(std::move(attributeNames)),
              attributeTypes(std::move(attributeTypes)) {
        assert(this->attributeNames.size() == arity && "arity mismatch for attributes");
        assert(this->attributeTypes.size() == arity && "arity mismatch for types");
        for (std::size_t i = 0; i < arity; i++) {
            assert(!this->attributeNames[i].empty() && "no attribute name specified");
            assert(!this->attributeTypes[i].empty() && "no attribute type specified");
        }
    }

    /** @brief Get name */
    const std::string& getName() const {
        return name;
    }

    /** @brief Get attribute types */
    const std::vector<std::string>& getAttributeTypes() const {
        return attributeTypes;
    }

    /** @brief Get attribute names */
    const std::vector<std::string>& getAttributeNames() const {
        return attributeNames;
    }

    /** @brief Is nullary relation */
    bool isNullary() const {
        return arity == 0;
    }

    /** @brief Relation representation type */
    RelationRepresentation getRepresentation() const {
        return representation;
    }

    /** @brief Is temporary relation (for semi-naive evaluation) */
    bool isTemp() const {
        return name.at(0) == '@';
    }

    /** @brief Get arity of relation */
    unsigned getArity() const {
        return arity;
    }

    /** @brief Get number of auxiliary attributes */
    unsigned getAuxiliaryArity() const {
        return auxiliaryArity;
    }

    /** @brief Compare two relations via their name */
    bool operator<(const RamRelation& other) const {
        return name < other.name;
    }

    RamRelation* clone() const override {
        return new RamRelation(name, arity, auxiliaryArity, attributeNames, attributeTypes, representation);
    }

protected:
    void print(std::ostream& out) const override {
        out << name;
        if (arity > 0) {
            out << "(" << attributeNames[0] << ":" << attributeTypes[0];
            for (unsigned i = 1; i < arity; i++) {
                out << ",";
                out << attributeNames[i] << ":" << attributeTypes[i];
                if (i >= arity - auxiliaryArity) {
                    out << " auxiliary";
                }
            }
            out << ")";
            out << " " << representation;
        } else {
            out << " nullary";
        }
    }

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamRelation*>(&node));
        const auto& other = static_cast<const RamRelation&>(node);
        return representation == other.representation && name == other.name && arity == other.arity &&
               auxiliaryArity == other.auxiliaryArity && attributeNames == other.attributeNames &&
               attributeTypes == other.attributeTypes;
    }

protected:
    /** Data-structure representation */
    const RelationRepresentation representation;

    /** Name of relation */
    const std::string name;

    /** Arity, i.e., number of attributes */
    const size_t arity;

    /** Number of auxiliary attributes (e.g. provenance attributes etc) */
    const size_t auxiliaryArity;

    /** Name of attributes */
    const std::vector<std::string> attributeNames;

    /** Type of attributes */
    const std::vector<std::string> attributeTypes;
};

/**
 * @class RamRelationReference
 * @brief A RAM Relation in the RAM intermediate representation.
 */
class RamRelationReference : public RamNode {
public:
    RamRelationReference(const RamRelation* relation) : relation(relation) {
        assert(relation != nullptr && "null relation");
    }

    /** @brief Get reference */
    const RamRelation* get() const {
        return relation;
    }

    RamRelationReference* clone() const override {
        return new RamRelationReference(relation);
    }

protected:
    void print(std::ostream& out) const override {
        out << relation->getName();
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamRelationReference&>(node);
        return equal_ptr(relation, other.relation);
    }

protected:
    /** Name of relation */
    const RamRelation* relation;
};

}  // end of namespace souffle
