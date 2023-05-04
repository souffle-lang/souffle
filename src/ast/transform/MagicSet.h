/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file MagicSet.h
 *
 * Define classes and functionality related to the magic set transformation.
 *
 ***********************************************************************/

#pragma once

#include "ast/Atom.h"
#include "ast/BinaryConstraint.h"
#include "ast/Clause.h"
#include "ast/QualifiedName.h"
#include "ast/transform/Pipeline.h"
#include "ast/transform/RemoveRedundantRelations.h"
#include "ast/transform/Transformer.h"
#include "souffle/utility/ContainerUtil.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace souffle::ast {
class Program;
class TranslationUnit;
}  // namespace souffle::ast

namespace souffle::ast::transform {

/**
 * Magic Set Transformation.
 * Involves four stages:
 *      (1) NormaliseDatabaseTransformer, for assumptions to hold
 *      (2) LabelDatabaseTransformer, to support negation
 *      (3) AdornDatabaseTransformer, to annotate information flow
 *      (4) MagicSetCoreTransformer, to perform the core magifying transformation
 */
class MagicSetTransformer : public PipelineTransformer {
public:
    class NormaliseDatabaseTransformer;
    class LabelDatabaseTransformer;
    class AdornDatabaseTransformer;
    class MagicSetCoreTransformer;

    MagicSetTransformer()
            : PipelineTransformer(mk<NormaliseDatabaseTransformer>(), mk<LabelDatabaseTransformer>(),
                      mk<RemoveRedundantRelationsTransformer>(), mk<AdornDatabaseTransformer>(),
                      mk<RemoveRedundantRelationsTransformer>(), mk<MagicSetCoreTransformer>()) {}

    std::string getName() const override {
        return "MagicSetTransformer";
    }

private:
    MagicSetTransformer* cloning() const override {
        return new MagicSetTransformer();
    }

    bool transform(TranslationUnit& tu) override {
        return shouldRun(tu) ? PipelineTransformer::transform(tu) : false;
    }

    /** Determines whether any part of the MST should be run. */
    static bool shouldRun(const TranslationUnit& tu);

    /**
     * Gets the set of relations that are trivially computable,
     * and so should not be magic-set.
     */
    static UnorderedQualifiedNameSet getTriviallyIgnoredRelations(const TranslationUnit& tu);

    /**
     * Gets the set of relations to weakly ignore during the MST process.
     * Weakly-ignored relations cannot be adorned/magic'd.
     * Superset of strongly-ignored relations.
     */
    static UnorderedQualifiedNameSet getWeaklyIgnoredRelations(const TranslationUnit& tu);

    /**
     * Gets the set of relations to strongly ignore during the MST process.
     * Strongly-ignored relations cannot be safely duplicated without affecting semantics.
     */
    static UnorderedQualifiedNameSet getStronglyIgnoredRelations(const TranslationUnit& tu);

    /**
     * Gets the set of relations to not label.
     * The union of strongly and trivially ignored.
     */
    static UnorderedQualifiedNameSet getRelationsToNotLabel(const TranslationUnit& tu);
};

/**
 * Database normaliser for MST.
 * Effects:
 *  - Partitions database into [input|intermediate|queries]
 *  - Normalises all arguments and constraints
 * Prerequisite for adornment.
 */
class MagicSetTransformer::NormaliseDatabaseTransformer : public Transformer {
public:
    std::string getName() const override {
        return "NormaliseDatabaseTransformer";
    }

private:
    NormaliseDatabaseTransformer* cloning() const override {
        return new NormaliseDatabaseTransformer();
    }

    bool transform(TranslationUnit& translationUnit) override;

    /**
     * Partitions the input and output relations.
     * Program will no longer have relations that are both input and output.
     */
    static bool partitionIO(TranslationUnit& translationUnit);

    /**
     * Separates the IDB from the EDB, so that they are disjoint.
     * Program will no longer have input relations that appear as the head of clauses.
     */
    static bool extractIDB(TranslationUnit& translationUnit);

    /**
     * Extracts output relations into separate simple query relations,
     * so that they are unused in any other rules.
     * Programs will only contain output relations which:
     *      (1) have exactly one rule defining them
     *      (2) do not appear in other rules
     */
    static bool querifyOutputRelations(TranslationUnit& translationUnit);

    /**
     * Normalise all arguments within each clause.
     * All arguments in all clauses will now be either:
     *      (1) a variable, or
     *      (2) the RHS of a `<var> = <arg>` constraint
     */
    static bool normaliseArguments(TranslationUnit& translationUnit);
};

/**
 * Database labeller. Runs the magic-set labelling algorithm.
 * Necessary for supporting negation in MST.
 */
class MagicSetTransformer::LabelDatabaseTransformer : public PipelineTransformer {
public:
    class NegativeLabellingTransformer;
    class PositiveLabellingTransformer;

    LabelDatabaseTransformer()
            : PipelineTransformer(mk<NegativeLabellingTransformer>(), mk<PositiveLabellingTransformer>()) {}

    std::string getName() const override {
        return "LabelDatabaseTransformer";
    }

private:
    LabelDatabaseTransformer* cloning() const override {
        return new LabelDatabaseTransformer();
    }

    /** Check if a relation is negatively labelled. */
    static bool isNegativelyLabelled(const QualifiedName& name);
};

/**
 * Runs the first stage of the labelling algorithm.
 * Separates out negated appearances of relations from the main SCC graph, preventing them from affecting
 * stratification once magic dependencies are added.
 */
class MagicSetTransformer::LabelDatabaseTransformer::NegativeLabellingTransformer : public Transformer {
public:
    std::string getName() const override {
        return "NegativeLabellingTransformer";
    }

private:
    NegativeLabellingTransformer* cloning() const override {
        return new NegativeLabellingTransformer();
    }

    bool transform(TranslationUnit& translationUnit) override;

    /** Provide a unique name for negatively-labelled relations. */
    static QualifiedName getNegativeLabel(const QualifiedName& name);
};

/**
 * Runs the second stage of the labelling algorithm.
 * Separates out the dependencies of negatively labelled atoms from the main SCC graph, preventing them from
 * affecting stratification after magic.
 * Note: Negative labelling must have been run first.
 */
class MagicSetTransformer::LabelDatabaseTransformer::PositiveLabellingTransformer : public Transformer {
public:
    std::string getName() const override {
        return "PositiveLabellingTransformer";
    }

private:
    PositiveLabellingTransformer* cloning() const override {
        return new PositiveLabellingTransformer();
    }

    bool transform(TranslationUnit& translationUnit) override;

    /** Provide a unique name for a positively labelled relation copy. */
    static QualifiedName getPositiveLabel(const QualifiedName& name, std::size_t count);
};

/**
 * Database adornment.
 * Adorns the rules of a database with variable flow and binding information.
 * Prerequisite for the magic set transformation.
 */
class MagicSetTransformer::AdornDatabaseTransformer : public Transformer {
public:
    std::string getName() const override {
        return "AdornDatabaseTransformer";
    }

private:
    AdornDatabaseTransformer* cloning() const override {
        return new AdornDatabaseTransformer();
    }

    using adorned_predicate = std::pair<QualifiedName, std::string>;

    struct AdornedPredicateHash {
        std::size_t operator()(const adorned_predicate& pred) const {
            std::size_t seed = qnhasher(pred.first);
            seed ^= strhasher(pred.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
        std::hash<QualifiedName> qnhasher;
        std::hash<std::string> strhasher;
    };

    std::unordered_set<adorned_predicate, AdornedPredicateHash> headAdornmentsToDo;
    UnorderedQualifiedNameSet headAdornmentsSeen;
    UnorderedQualifiedNameSet weaklyIgnoredRelations;

    bool transform(TranslationUnit& translationUnit) override;

    /** Get the unique identifier corresponding to an adorned predicate. */
    static QualifiedName getAdornmentID(const QualifiedName& relName, const std::string& adornmentMarker);

    /** Add an adornment to the ToDo queue if it hasn't been processed before. */
    void queueAdornment(const QualifiedName& relName, const std::string& adornmentMarker) {
        auto adornmentID = getAdornmentID(relName, adornmentMarker);
        if (!contains(headAdornmentsSeen, adornmentID)) {
            headAdornmentsToDo.insert(std::make_pair(relName, adornmentMarker));
            headAdornmentsSeen.insert(adornmentID);
        }
    }

    /** Check if any more relations need to be adorned. */
    bool hasAdornmentToProcess() const {
        return !headAdornmentsToDo.empty();
    }

    /** Pop off the next predicate adornment to process. **/
    adorned_predicate nextAdornmentToProcess() {
        assert(hasAdornmentToProcess() && "no adornment to pop");
        auto headAdornment = *(headAdornmentsToDo.begin());
        headAdornmentsToDo.erase(headAdornmentsToDo.begin());
        return headAdornment;
    }

    /** Returns the adorned version of a clause. */
    Own<Clause> adornClause(const Clause* clause, const std::string& adornmentMarker);
};

/**
 * Core section of the magic set transformer.
 * Creates all magic rules and relations based on the preceding adornment, and adds them into rules as needed.
 * Assumes that Normalisation, Labelling, and Adornment have all been performed.
 */
class MagicSetTransformer::MagicSetCoreTransformer : public Transformer {
public:
    std::string getName() const override {
        return "MagicSetCoreTransformer";
    }

private:
    MagicSetCoreTransformer* cloning() const override {
        return new MagicSetCoreTransformer();
    }

    bool transform(TranslationUnit& translationUnit) override;

    /** Gets a unique magic identifier for a given adorned relation name */
    static QualifiedName getMagicName(const QualifiedName& name);

    /** Checks if a given relation name is adorned */
    static bool isAdorned(const QualifiedName& name);

    /** Retrieves an adornment encoded in a given relation name */
    static std::string getAdornment(const QualifiedName& name);

    /** Get all potentially-binding equality constraints in a clause */
    static std::vector<const BinaryConstraint*> getBindingEqualityConstraints(const Clause* clause);

    /** Get the closure of the given set of variables under appearance in the given eq constraints */
    static void addRelevantVariables(
            std::set<std::string>& variables, const std::vector<const BinaryConstraint*> eqConstraints);

    /** Creates the magic atom associatd with the given (rel, adornment) pair */
    static Own<Atom> createMagicAtom(const Atom* atom);

    /** Creates the magic clause centred around the given magic atom */
    static Own<Clause> createMagicClause(const Atom* atom, const VecOwn<Atom>& constrainingAtoms,
            const std::vector<const BinaryConstraint*> eqConstraints);
};

}  // namespace souffle::ast::transform
