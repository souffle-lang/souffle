/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#pragma once

#include "ram/Relation.h"
#include "ram/analysis/Index.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace souffle::synthesiser {

class Relation {
public:
    Relation(const ram::Relation& rel, const ram::analysis::IndexCluster& indexSelection)
            : relation(rel), indexSelection(indexSelection) {}

    virtual ~Relation() = default;

    /** Compute the final list of indices to be used */
    virtual void computeIndices() = 0;

    /** Get arity of relation */
    std::size_t getArity() const {
        return relation.getArity();
    }

    /** Get data structure of relation */
    const std::string& getDataStructure() const {
        return dataStructure;
    }

    /** Get list of indices used for relation,
     * guaranteed that original indices in analysis::MinIndexSelectionStrategy
     * come before any generated indices */
    ram::analysis::OrderCollection getIndices() const {
        return computedIndices;
    }

    std::set<std::size_t> getProvenenceIndexNumbers() const {
        return provenanceIndexNumbers;
    }

    /** Get stored ram::Relation */
    const ram::Relation& getRelation() const {
        return relation;
    }

    /** Print type name */
    virtual std::string getTypeName() = 0;

    /** Helper function to convert attribute types to a single string */
    std::string getTypeAttributeString(const std::vector<std::string>& attributeTypes,
            const std::unordered_set<std::size_t>& attributesUsed) const;

    /** Generate relation type struct */
    virtual void generateTypeStruct(std::ostream& out) = 0;

    /** Factory method to generate a SynthesiserRelation */
    static Own<Relation> getSynthesiserRelation(
            const ram::Relation& ramRel, const ram::analysis::IndexCluster& indexSelection);

protected:
    /** Ram relation referred to by this */
    const ram::Relation& relation;

    /** Indices used for this relation */
    const ram::analysis::IndexCluster indexSelection;

    /** The data structure used for the relation */
    std::string dataStructure;

    /** The final list of indices used */
    ram::analysis::OrderCollection computedIndices;

    /** The list of indices added for provenance computation */
    std::set<std::size_t> provenanceIndexNumbers;

    /** The number of the master index */
    std::size_t masterIndex = -1;
};

class NullaryRelation : public Relation {
public:
    NullaryRelation(const ram::Relation& ramRel, const ram::analysis::IndexCluster& indexSelection)
            : Relation(ramRel, indexSelection) {}

    void computeIndices() override;
    std::string getTypeName() override;
    void generateTypeStruct(std::ostream& out) override;
};

class InfoRelation : public Relation {
public:
    InfoRelation(const ram::Relation& ramRel, const ram::analysis::IndexCluster& indexSelection)
            : Relation(ramRel, indexSelection) {}

    void computeIndices() override;
    std::string getTypeName() override;
    void generateTypeStruct(std::ostream& out) override;
};

class DirectRelation : public Relation {
public:
    DirectRelation(const ram::Relation& ramRel, const ram::analysis::IndexCluster& indexSelection,
            bool isProvenance, bool isIncremental, bool hasErase)
            : Relation(ramRel, indexSelection), isProvenance(isProvenance), isIncremental(isIncremental),
              hasErase(hasErase) {}

    void computeIndices() override;
    std::string getTypeName() override;
    void generateTypeStruct(std::ostream& out) override;

private:
    const bool isProvenance;
    const bool isIncremental;
    const bool hasErase;
};

class IndirectRelation : public Relation {
public:
    IndirectRelation(const ram::Relation& ramRel, const ram::analysis::IndexCluster& indexSelection)
            : Relation(ramRel, indexSelection) {}

    void computeIndices() override;
    std::string getTypeName() override;
    void generateTypeStruct(std::ostream& out) override;
};

class BrieRelation : public Relation {
public:
    BrieRelation(const ram::Relation& ramRel, const ram::analysis::IndexCluster& indexSelection)
            : Relation(ramRel, indexSelection) {}

    void computeIndices() override;
    std::string getTypeName() override;
    void generateTypeStruct(std::ostream& out) override;
};

class EqrelRelation : public Relation {
public:
    EqrelRelation(const ram::Relation& ramRel, const ram::analysis::IndexCluster& indexSelection)
            : Relation(ramRel, indexSelection) {}

    void computeIndices() override;
    std::string getTypeName() override;
    void generateTypeStruct(std::ostream& out) override;
};
}  // namespace souffle::synthesiser
