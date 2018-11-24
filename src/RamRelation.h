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

#include "IODirectives.h"
#include "ParallelUtils.h"
#include "RamNode.h"
#include "RamTypes.h"
#include "SymbolMask.h"
#include "SymbolTable.h"
#include "Table.h"
#include "Util.h"

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 * A RAM Relation in the RAM intermediate representation.
 * TODO (#541): Make RamRelation a sub-class of RAM node.
 * TODO (#541): Tidy-up interface and attributes
 */
class RamRelation : public RamNode {
protected:
    /** Name of relation */
    std::string name;

    /** Arity, i.e., number of attributes */
    unsigned arity = 0;

    /** Name of attributes */
    std::vector<std::string> attributeNames;

    /** Type of attributes */
    std::vector<std::string> attributeTypeQualifiers;

    /** TODO (#541): legacy, i.e., duplicated information */
    SymbolMask mask;

    /** Relation qualifiers */
    // TODO: Simplify interface
    bool input = false;     // input relation
    bool output = false;    // output relation
    bool computed = false;  // either output or printed

    bool btree = false;  // btree data-structure
    bool brie = false;   // brie data-structure
    bool eqrel = false;  // equivalence relation

    bool istemp = false;  // Temporary relation for semi-naive evaluation

public:
    RamRelation() : RamNode(RN_Relation), mask(arity) {}

    RamRelation(const std::string& name, unsigned arity, const bool istemp)
            : RamRelation(
                      name, arity, {}, {}, SymbolMask(0), false, false, false, false, false, false, false) {
        this->istemp = istemp;
    }

    RamRelation(std::string name, unsigned arity, std::vector<std::string> attributeNames,
            std::vector<std::string> attributeTypeQualifiers, SymbolMask mask, bool input, bool computed,
            bool output, bool btree, bool brie, bool eqrel, bool istemp)
            : RamNode(RN_Relation), name(std::move(name)), arity(arity),
              attributeNames(std::move(attributeNames)),
              attributeTypeQualifiers(std::move(attributeTypeQualifiers)), mask(std::move(mask)),
              input(input), output(output), computed(computed), btree(btree), brie(brie), eqrel(eqrel),
              istemp(istemp) {
        assert(this->attributeNames.size() == arity || this->attributeNames.empty());
        assert(this->attributeTypeQualifiers.size() == arity || this->attributeTypeQualifiers.empty());
    }

    const std::string& getName() const {
        return name;
    }

    const std::string getArg(uint32_t i) const {
        if (!attributeNames.empty()) {
            return attributeNames[i];
        }
        if (arity == 0) {
            return "";
        }
        return "c" + std::to_string(i);
    }

    const std::string getArgTypeQualifier(uint32_t i) const {
        return (i < attributeTypeQualifiers.size()) ? attributeTypeQualifiers[i] : "";
    }

    const SymbolMask& getSymbolMask() const {
        return mask;
    }

    const bool isInput() const {
        return input;
    }

    const bool isComputed() const {
        return computed;
    }

    const bool isOutput() const {
        return output;
    }

    const bool isBTree() const {
        return btree;
    }

    const bool isBrie() const {
        return brie;
    }

    const bool isEqRel() const {
        return eqrel;
    }

    // data-structures that can server various searches
    const bool isCoverable() const {
        return true;
    }

    const bool isTemp() const {
        return istemp;
    }

    unsigned getArity() const {
        return arity;
    }

    bool operator<(const RamRelation& other) const {
        return name < other.name;
    }

    /* Print */
    void print(std::ostream& out) const override {
        out << name << "(";
        out << getArg(0);
        for (unsigned i = 1; i < arity; i++) {
            out << ",";
            out << getArg(i);
        }
        out << ")";

        if (isBTree()) out << " btree";
        if (isBrie()) out << " brie";
        if (isEqRel()) out << " eqrel";
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>();  // no child nodes
    }

    /** Create clone */
    RamRelation* clone() const override {
        RamRelation* res = new RamRelation(name, arity, attributeNames, attributeTypeQualifiers, mask, input,
                computed, output, btree, brie, eqrel, istemp);
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {}

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamRelation*>(&node));
        const auto& other = static_cast<const RamRelation&>(node);
        return name == other.name && arity == other.arity && attributeNames == other.attributeNames &&
               attributeTypeQualifiers == other.attributeTypeQualifiers && mask == other.mask &&
               isInput() == other.isInput() && isOutput() == other.isOutput() &&
               isComputed() == other.isComputed() && isBTree() == other.isBTree() &&
               isBrie() == other.isBrie() && isEqRel() == other.isEqRel() && isTemp() == other.isTemp();
    }
};

/**
 * A RAM Relation in the RAM intermediate representation.
 * TODO: Make RamRelation a sub-class of RAM node.
 * TODO: Tidy-up interface and attributes
 */
class RamRelationRef : public RamNode {
protected:
    /** Name of relation */
    const RamRelation* relation;

public:
    RamRelationRef(const RamRelation* r) : RamNode(RN_RelationRef), relation(r) {}

    /** Get name */
    const std::string& getName() const {
        return relation->getName();
    }

    /** Get Relation */
    const RamRelation* getRelation() {
        return relation;
    }

    /* Print */
    void print(std::ostream& out) const override {
        out << getName();
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>();  // no child nodes
    }

    /** Create clone */
    RamRelationRef* clone() const override {
        RamRelationRef* res = new RamRelationRef(relation);
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {}

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamRelationRef*>(&node));
        const auto& other = static_cast<const RamRelationRef&>(node);
        return relation == other.relation;
    }
};

}  // end of namespace souffle
