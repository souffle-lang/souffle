/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#pragma once

#include "Global.h"
#include "IndexSetAnalysis.h"
#include "RamRelation.h"

#include <memory>
#include <numeric>
#include <ostream>
#include <string>
#include <vector>

namespace souffle {

class SynthesiserRelation {
protected:
    /** Ram relation referred to by this */
    const RamRelationReference& relation;

    /** Indices used for this relation */
    const IndexSet& indices;

    /** The data structure used for the relation */
    std::string dataStructure;

    /** The final list of indices used */
    std::vector<IndexSet::LexicographicalOrder> computedIndices;

    /** The number of the master index */
    size_t masterIndex = -1;

    /** Is this relation used with provenance */
    const bool isProvenance;

public:
    SynthesiserRelation(
            const RamRelationReference& rel, const IndexSet& indices, const bool isProvenance = false)
            : relation(rel), indices(indices), isProvenance(isProvenance) {}

    virtual ~SynthesiserRelation() = default;

    /** Compute the final list of indices to be used */
    virtual void computeIndices() = 0;

    /** Get arity of relation */
    size_t getArity() const {
        return relation.getArity();
    }

    /** Get data structure of relation */
    const std::string& getDataStructure() const {
        return dataStructure;
    }

    /** Get list of indices used for relation,
     * guaranteed that original indices in IndexSet
     * come before any generated indices */
    std::vector<std::vector<int>> getIndices() const {
        return computedIndices;
    }

    /** Get stored IndexSet */
    const IndexSet& getIndexSet() const {
        return indices;
    }

    /** Get stored RamRelation */
    const RamRelationReference& getRamRelation() const {
        return relation;
    }

    /** Print type name */
    virtual std::string getTypeName() = 0;

    /** Generate relation type struct */
    virtual void generateTypeStruct(std::ostream& out) = 0;

    /** Factory method to generate a SynthesiserRelation */
    static std::unique_ptr<SynthesiserRelation> getSynthesiserRelation(
            const RamRelationReference& ramRel, const IndexSet& indexSet, bool isProvenance);
};

class SynthesiserNullaryRelation : public SynthesiserRelation {
public:
    SynthesiserNullaryRelation(
            const RamRelationReference& ramRel, const IndexSet& indexSet, bool isProvenance)
            : SynthesiserRelation(ramRel, indexSet, isProvenance) {}

    void computeIndices() override;
    std::string getTypeName() override;
    void generateTypeStruct(std::ostream& out) override;
};

class SynthesiserDirectRelation : public SynthesiserRelation {
public:
    SynthesiserDirectRelation(const RamRelationReference& ramRel, const IndexSet& indexSet, bool isProvenance)
            : SynthesiserRelation(ramRel, indexSet, isProvenance) {}

    void computeIndices() override;
    std::string getTypeName() override;
    void generateTypeStruct(std::ostream& out) override;
};

class SynthesiserIndirectRelation : public SynthesiserRelation {
public:
    SynthesiserIndirectRelation(
            const RamRelationReference& ramRel, const IndexSet& indexSet, bool isProvenance)
            : SynthesiserRelation(ramRel, indexSet, isProvenance) {}

    void computeIndices() override;
    std::string getTypeName() override;
    void generateTypeStruct(std::ostream& out) override;
};

class SynthesiserBrieRelation : public SynthesiserRelation {
public:
    SynthesiserBrieRelation(const RamRelationReference& ramRel, const IndexSet& indexSet, bool isProvenance)
            : SynthesiserRelation(ramRel, indexSet, isProvenance) {}

    void computeIndices() override;
    std::string getTypeName() override;
    void generateTypeStruct(std::ostream& out) override;
};

class SynthesiserEqrelRelation : public SynthesiserRelation {
public:
    SynthesiserEqrelRelation(const RamRelationReference& ramRel, const IndexSet& indexSet, bool isProvenance)
            : SynthesiserRelation(ramRel, indexSet, isProvenance) {}

    void computeIndices() override;
    std::string getTypeName() override;
    void generateTypeStruct(std::ostream& out) override;
};

}  // end of namespace souffle
