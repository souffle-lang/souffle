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
 */
class RamRelation : public RamNode {
protected:
    /** Name of relation */
    const std::string name;

    /** Arity, i.e., number of attributes */
    const size_t arity;

    /** Name of attributes */
    const std::vector<std::string> attributeNames;

    /** Type of attributes */
    const std::vector<std::string> attributeTypeQualifiers;

    /** TODO (#541): legacy, i.e., duplicated information */
    const SymbolMask mask;

    /** Relation qualifiers */
    // TODO: Simplify interface
    const bool input;     // input relation
    const bool output;    // output relation
    const bool computed;  // either output or printed

    const bool btree;  // btree data-structure
    const bool brie;   // brie data-structure
    const bool eqrel;  // equivalence relation

public:
    RamRelation(const std::string name, const size_t arity, const std::vector<std::string> attributeNames,
            const std::vector<std::string> attributeTypeQualifiers, const SymbolMask mask, const bool input,
            const bool computed, const bool output, const bool btree, const bool brie, const bool eqrel)
            : RamNode(RN_Relation), name(std::move(name)), arity(arity),
              attributeNames(std::move(attributeNames)),
              attributeTypeQualifiers(std::move(attributeTypeQualifiers)), mask(std::move(mask)),
              input(input), output(output), computed(computed), btree(btree), brie(brie), eqrel(eqrel) {
        assert(this->attributeNames.size() == arity || this->attributeNames.empty());
        assert(this->attributeTypeQualifiers.size() == arity || this->attributeTypeQualifiers.empty());
    }

    /** Get name */
    const std::string& getName() const {
        return name;
    }

    /** Get argument */
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

    /** Get symbol mask */
    const SymbolMask& getSymbolMask() const {
        return mask;
    }

    /** Is input relation */
    const bool isInput() const {
        return input;
    }

    /** Is relation computed */
    const bool isComputed() const {
        return computed;
    }

    /** Is output relation */
    const bool isOutput() const {
        return output;
    }

    /** Is nullary relation */
    const bool isNullary() const {
        return arity == 0;
    }

    /** Is BTree relation */
    const bool isBTree() const {
        return btree;
    }

    /** Is Brie relation */
    const bool isBrie() const {
        return brie;
    }

    /** Is equivalence relation */
    const bool isEqRel() const {
        return eqrel;
    }

    // Flag to check whether the data-structure
    const bool isCoverable() const {
        return true;
    }

    /** Is temporary relation (for semi-naive evaluation) */
    const bool isTemp() const {
        return name.at(0) == '@';
    }

    /* Get arity of relation */
    unsigned getArity() const {
        return arity;
    }

    /* Compare two relations via their name */
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
                computed, output, btree, brie, eqrel);
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
 */
class RamRelationReference : public RamNode {
protected:
    /** Name of relation */
    const RamRelation* relation;

public:
    RamRelationReference(const RamRelation* relation) : RamNode(RN_RelationReference), relation(relation) {}

    /** Get name */
    const std::string& getName() const {
        return relation->getName();
    }

    /** Get relation */
    const RamRelation* getRelation() const {
        return relation;
    }

    /** Get arity */
    unsigned getArity() const {
        return relation->getArity();
    }

    /** Is nullary relation */
    const bool isNullary() const {
        return relation->isNullary();
    }

    /* Is BTree relation */
    const bool isBTree() const {
        return relation->isBTree();
    }

    /** Is Brie relation */
    const bool isBrie() const {
        return relation->isBrie();
    }

    /** Is equivalence relation */
    const bool isEqRel() const {
        return relation->isEqRel();
    }

    /** Is temp relation */
    const bool isTemp() const {
        return relation->isTemp();
    }

    /** Is input relation */
    const bool isInput() const {
        return relation->isInput();
    }

    /** Is computed relation */
    const bool isComputed() const {
        return relation->isComputed();
    }

    /** Is output relation */
    const bool isOutput() const {
        return relation->isOutput();
    }

    /** Get symbol mask */
    const SymbolMask& getSymbolMask() const {
        return relation->getSymbolMask();
    }

    /** Get argument */
    const std::string getArg(uint32_t i) const {
        return relation->getArg(i);
    }

    /** Get argument qualifier */
    const std::string getArgTypeQualifier(uint32_t i) const {
        return relation->getArgTypeQualifier(i);
    }

    /** Comparator */
    bool operator<(const RamRelationReference& other) const {
        return relation->operator<(*other.getRelation());
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
    RamRelationReference* clone() const override {
        auto* res = new RamRelationReference(relation);
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {}

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamRelationReference*>(&node));
        const auto& other = static_cast<const RamRelationReference&>(node);
        return relation == other.relation;
    }
};

}  // end of namespace souffle
