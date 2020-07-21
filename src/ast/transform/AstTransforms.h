/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstTransforms.h
 *
 * Defines AST transformation passes.
 *
 ***********************************************************************/

#pragma once

#include "DebugReporter.h"
#include "ast/transform/AstTransformer.h"
#include "utility/ContainerUtil.h"
#include "utility/MiscUtil.h"
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

class AstAggregator;
class AstBinaryConstraint;
class AstClause;
class AstLiteral;
class AstProgram;
class AstRecordInit;
class AstRelation;
class AstTranslationUnit;

/**
 * Transformation pass to eliminate grounded aliases.
 * e.g. resolve: a(r) , r = [x,y]       => a(x,y)
 * e.g. resolve: a(x) , !b(y) , y = x   => a(x) , !b(x)
 */
class ResolveAliasesTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "ResolveAliasesTransformer";
    }

    /**
     * Converts the given clause into a version without variables aliasing
     * grounded variables.
     *
     * @param clause the clause to be processed
     * @return a modified clone of the processed clause
     */
    static std::unique_ptr<AstClause> resolveAliases(const AstClause& clause);

    /**
     * Removes trivial equalities of the form t = t from the given clause.
     *
     * @param clause the clause to be processed
     * @return a modified clone of the given clause
     */
    static std::unique_ptr<AstClause> removeTrivialEquality(const AstClause& clause);

    /**
     * Removes complex terms in atoms, replacing them with constrained variables.
     *
     * @param clause the clause to be processed
     * @return a modified clone of the processed clause
     */
    static std::unique_ptr<AstClause> removeComplexTermsInAtoms(const AstClause& clause);

    ResolveAliasesTransformer* clone() const override {
        return new ResolveAliasesTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

/**
 * Transformation pass to replaces copy of relations by their origin.
 * For instance, if a relation r is defined by
 *
 *      r(X,Y) :- s(X,Y)
 *
 * and no other clause, all occurrences of r will be replaced by s.
 */
class RemoveRelationCopiesTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "RemoveRelationCopiesTransformer";
    }

    /**
     * Replaces copies of relations by their origin in the given program.
     *
     * @param program the program to be processed
     * @return whether the program was modified
     */
    static bool removeRelationCopies(AstTranslationUnit& translationUnit);

    RemoveRelationCopiesTransformer* clone() const override {
        return new RemoveRelationCopiesTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override {
        return removeRelationCopies(translationUnit);
    }
};

/**
 * Transformation pass to rename aggregation variables to make them unique.
 */
class UniqueAggregationVariablesTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "UniqueAggregationVariablesTransformer";
    }

    UniqueAggregationVariablesTransformer* clone() const override {
        return new UniqueAggregationVariablesTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

/**
 * Replaces literals containing single-valued aggregates with
 * a synthesised relation
 */
class MaterializeSingletonAggregationTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "MaterializeSingletonAggregationTransformer";
    }

    MaterializeSingletonAggregationTransformer* clone() const override {
        return new MaterializeSingletonAggregationTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
    /**
     * Determines whether an aggregate is single-valued,
     * ie the aggregate does not depend on the outer scope.
     */
    static bool isSingleValued(const AstAggregator& agg, const AstClause& clause);
    /**
     * findUniqueVariableName returns a variable name that hasn't appeared
     * in the given clause.
     */
    static std::string findUniqueVariableName(const AstClause& clause);
    /**
     * findUniqueAggregateRelationName returns a synthesised aggregate
     * relation name that hasn't appeared
     * in the given clause.
     */
    static std::string findUniqueAggregateRelationName(const AstProgram& program);
};

/**
 * Transformation pass to create artificial relations for bodies of
 * aggregation functions consisting of more than a single atom.
 */
class MaterializeAggregationQueriesTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "MaterializeAggregationQueriesTransformer";
    }

    /**
     * Creates artificial relations for bodies of aggregation functions
     * consisting of more than a single atom, in the given program.
     *
     * @param program the program to be processed
     * @return whether the program was modified
     */
    static bool materializeAggregationQueries(AstTranslationUnit& translationUnit);

    MaterializeAggregationQueriesTransformer* clone() const override {
        return new MaterializeAggregationQueriesTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override {
        return materializeAggregationQueries(translationUnit);
    }

    /**
     * A test determining whether the body of a given aggregation needs to be
     * 'outlined' into an independent relation or can be kept inline.
     */
    static bool needsMaterializedRelation(const AstAggregator& agg);
};

/**
 * Transformation pass to remove all empty relations and rules that use empty relations.
 */
class RemoveEmptyRelationsTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "RemoveEmptyRelationsTransformer";
    }

    /**
     * Eliminate all empty relations (and their uses) in the given program.
     *
     * @param translationUnit the program to be processed
     * @return whether the program was modified
     */
    static bool removeEmptyRelations(AstTranslationUnit& translationUnit);

    RemoveEmptyRelationsTransformer* clone() const override {
        return new RemoveEmptyRelationsTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override {
        return removeEmptyRelations(translationUnit);
    }

    /**
     * Eliminate rules that contain empty relations and/or rewrite them.
     *
     * @param translationUnit the program to be processed
     * @param emptyRelation relation that is empty
     * @return whether the program was modified
     */
    static bool removeEmptyRelationUses(AstTranslationUnit& translationUnit, AstRelation* emptyRelation);
};

/**
 * Transformation pass to remove relations which are redundant (do not contribute to output).
 */
class RemoveRedundantRelationsTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "RemoveRedundantRelationsTransformer";
    }

    RemoveRedundantRelationsTransformer* clone() const override {
        return new RemoveRedundantRelationsTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

/**
 * Transformation pass to remove equivalent rules.
 */
class MinimiseProgramTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "MinimiseProgramTransformer";
    }

    // Check whether two clauses are bijectively equivalent.
    static bool areBijectivelyEquivalent(const AstClause* left, const AstClause* right);

    MinimiseProgramTransformer* clone() const override {
        return new MinimiseProgramTransformer();
    }

private:
    class NormalisedClauseRepr;

    bool transform(AstTranslationUnit& translationUnit) override;

    /** -- Bijective Equivalence Helper Methods -- */

    // Check whether two normalised clause representations are equivalent.
    static bool areBijectivelyEquivalent(const NormalisedClauseRepr& left, const NormalisedClauseRepr& right);

    // Check whether a valid variable mapping exists for the given permutation.
    static bool isValidPermutation(const NormalisedClauseRepr& left, const NormalisedClauseRepr& right,
            const std::vector<unsigned int>& permutation);

    /** -- Sub-Transformations -- */

    /**
     * Reduces locally-redundant clauses.
     * A clause is locally-redundant if there is another clause within the same relation
     * that computes the same set of tuples.
     */
    static bool reduceLocallyEquivalentClauses(AstTranslationUnit& translationUnit);

    /**
     * Remove clauses that are only satisfied if they are already satisfied.
     */
    static bool removeRedundantClauses(AstTranslationUnit& translationUnit);

    /**
     * Remove redundant literals within a clause.
     */
    static bool reduceClauseBodies(AstTranslationUnit& translationUnit);

    /**
     * Removes redundant singleton relations.
     * Singleton relations are relations with a single clause. A singleton relation is redundant
     * if there exists another singleton relation that computes the same set of tuples.
     * @return true iff the program was changed
     */
    static bool reduceSingletonRelations(AstTranslationUnit& translationUnit);
};

/**
 * Transformation pass to add provenance information
 */
class ProvenanceTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "ProvenanceTransformer";
    }

    ProvenanceTransformer* clone() const override {
        return new ProvenanceTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
    bool transformMaxHeight(AstTranslationUnit& translationUnit);
    bool transformSubtreeHeights(AstTranslationUnit& translationUnit);
};

/**
 * Transformation pass to remove constant boolean constraints
 * Should be called after any transformation that may generate boolean constraints
 */
class RemoveBooleanConstraintsTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "RemoveBooleanConstraintsTransformer";
    }

    RemoveBooleanConstraintsTransformer* clone() const override {
        return new RemoveBooleanConstraintsTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

/**
 * Transformation pass to inline marked relations
 */
class InlineRelationsTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "InlineRelationsTransformer";
    }

    InlineRelationsTransformer* clone() const override {
        return new InlineRelationsTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

/**
 * Transformation pass to move literals into new clauses
 * if they are independent of remaining literals.
 * E.g. a(x) :- b(x), c(y), d(y), e(z). is transformed into:
 *      - a(x) :- b(x), newrel1(), newrel2().
 *      - newrel1() :- c(y), d(y).
 *      - newrel2() :- e(z).
 */
class PartitionBodyLiteralsTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "PartitionBodyLiteralsTransformer";
    }

    PartitionBodyLiteralsTransformer* clone() const override {
        return new PartitionBodyLiteralsTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

/**
 * Transformation pass to reduce unnecessary computation for
 * relations that only appear in the form A(_,...,_).
 */
class ReduceExistentialsTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "ReduceExistentialsTransformer";
    }

    ReduceExistentialsTransformer* clone() const override {
        return new ReduceExistentialsTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

/**
 * Transformation pass to replace singleton variables
 * with unnamed variables.
 * E.g.: a() :- b(x). -> a() :- b(_).
 */
class ReplaceSingletonVariablesTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "ReplaceSingletonVariablesTransformer";
    }

    ReplaceSingletonVariablesTransformer* clone() const override {
        return new ReplaceSingletonVariablesTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

/**
 * Transformation pass to replace unnamed variables
 * with singletons.
 * E.g.: a() :- b(_). -> a() :- b(x).
 */
class NameUnnamedVariablesTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "NameUnnamedVariablesTransformer";
    }

    NameUnnamedVariablesTransformer* clone() const override {
        return new NameUnnamedVariablesTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

/**
 * Transformation pass to reorder body literals.
 */
class ReorderLiteralsTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "ReorderLiteralsTransformer";
    }

    ReorderLiteralsTransformer* clone() const override {
        return new ReorderLiteralsTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

/**
 * Transformation pass to normalise constraints.
 * E.g.: a(x) :- b(x, 1). -> a(x) :- b(x, tmp0), tmp0=1.
 */
class NormaliseConstraintsTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "NormaliseConstraintsTransformer";
    }

    NormaliseConstraintsTransformer* clone() const override {
        return new NormaliseConstraintsTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

/**
 * Transformation pass to remove expressions of the form
 * sum k : { ... } and replace them with
 * k * count : { ... }
 * where k is a constant.
 */
class RemoveRedundantSumsTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "RemoveRedundantSumsTransformer";
    }

    RemoveRedundantSumsTransformer* clone() const override {
        return new RemoveRedundantSumsTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

/**
 * Magic Set Transformation
 */
class MagicSetTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "MagicSetTransformer";
    }

    MagicSetTransformer* clone() const override {
        return new MagicSetTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

/**
 * Transformation to remove typecasts.
 */
class RemoveTypecastsTransformer : public AstTransformer {
private:
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    std::string getName() const override {
        return "RemoveTypecastsTransformer";
    }

    RemoveTypecastsTransformer* clone() const override {
        return new RemoveTypecastsTransformer();
    }
};

/**
 * Transformer that holds an arbitrary number of sub-transformations
 */
class PipelineTransformer : public MetaTransformer {
public:
    template <typename... Args>
    PipelineTransformer(Args... args) {
        std::unique_ptr<AstTransformer> tmp[] = {std::move(args)...};
        for (auto& cur : tmp) {
            pipeline.push_back(std::move(cur));
        }
    }

    PipelineTransformer(std::vector<std::unique_ptr<AstTransformer>> pipeline)
            : pipeline(std::move(pipeline)) {}

    std::vector<AstTransformer*> getSubtransformers() const override {
        return toPtrVector(pipeline);
    }

    void setDebugReport() override {
        for (auto& i : pipeline) {
            if (auto* mt = dynamic_cast<MetaTransformer*>(i.get())) {
                mt->setDebugReport();
            } else {
                i = std::make_unique<DebugReporter>(std::move(i));
            }
        }
    }

    void setVerbosity(bool verbose) override {
        this->verbose = verbose;
        for (auto& cur : pipeline) {
            if (auto* mt = dynamic_cast<MetaTransformer*>(cur.get())) {
                mt->setVerbosity(verbose);
            }
        }
    }

    void disableTransformers(const std::set<std::string>& transforms) override {
        for (auto& i : pipeline) {
            if (auto* mt = dynamic_cast<MetaTransformer*>(i.get())) {
                mt->disableTransformers(transforms);
            } else if (transforms.find(i->getName()) != transforms.end()) {
                i = std::make_unique<NullTransformer>();
            }
        }
    }

    std::string getName() const override {
        return "PipelineTransformer";
    }

    PipelineTransformer* clone() const override {
        std::vector<std::unique_ptr<AstTransformer>> transformers;
        for (const auto& transformer : pipeline) {
            transformers.push_back(souffle::clone(transformer));
        }
        return new PipelineTransformer(std::move(transformers));
    }

private:
    std::vector<std::unique_ptr<AstTransformer>> pipeline;
    bool transform(AstTranslationUnit& translationUnit) override;
};

/**
 * Transformer that executes a sub-transformer iff a condition holds
 */
class ConditionalTransformer : public MetaTransformer {
public:
    ConditionalTransformer(std::function<bool()> cond, std::unique_ptr<AstTransformer> transformer)
            : condition(std::move(cond)), transformer(std::move(transformer)) {}

    ConditionalTransformer(bool cond, std::unique_ptr<AstTransformer> transformer)
            : condition([=]() { return cond; }), transformer(std::move(transformer)) {}

    std::vector<AstTransformer*> getSubtransformers() const override {
        return {transformer.get()};
    }

    void setDebugReport() override {
        if (auto* mt = dynamic_cast<MetaTransformer*>(transformer.get())) {
            mt->setDebugReport();
        } else {
            transformer = std::make_unique<DebugReporter>(std::move(transformer));
        }
    }

    void setVerbosity(bool verbose) override {
        this->verbose = verbose;
        if (auto* mt = dynamic_cast<MetaTransformer*>(transformer.get())) {
            mt->setVerbosity(verbose);
        }
    }

    void disableTransformers(const std::set<std::string>& transforms) override {
        if (auto* mt = dynamic_cast<MetaTransformer*>(transformer.get())) {
            mt->disableTransformers(transforms);
        } else if (transforms.find(transformer->getName()) != transforms.end()) {
            transformer = std::make_unique<NullTransformer>();
        }
    }

    std::string getName() const override {
        return "ConditionalTransformer";
    }

    ConditionalTransformer* clone() const override {
        return new ConditionalTransformer(condition, souffle::clone(transformer));
    }

private:
    std::function<bool()> condition;
    std::unique_ptr<AstTransformer> transformer;
    bool transform(AstTranslationUnit& translationUnit) override;
};

/**
 * Transformer that repeatedly executes a sub-transformer while a condition is met
 */
class WhileTransformer : public MetaTransformer {
public:
    WhileTransformer(std::function<bool()> cond, std::unique_ptr<AstTransformer> transformer)
            : condition(std::move(cond)), transformer(std::move(transformer)) {}

    WhileTransformer(bool cond, std::unique_ptr<AstTransformer> transformer)
            : condition([=]() { return cond; }), transformer(std::move(transformer)) {}

    std::vector<AstTransformer*> getSubtransformers() const override {
        return {transformer.get()};
    }
    void setDebugReport() override {
        if (auto* mt = dynamic_cast<MetaTransformer*>(transformer.get())) {
            mt->setDebugReport();
        } else {
            transformer = std::make_unique<DebugReporter>(std::move(transformer));
        }
    }

    void setVerbosity(bool verbose) override {
        this->verbose = verbose;
        if (auto* mt = dynamic_cast<MetaTransformer*>(transformer.get())) {
            mt->setVerbosity(verbose);
        }
    }

    void disableTransformers(const std::set<std::string>& transforms) override {
        if (auto* mt = dynamic_cast<MetaTransformer*>(transformer.get())) {
            mt->disableTransformers(transforms);
        } else if (transforms.find(transformer->getName()) != transforms.end()) {
            transformer = std::make_unique<NullTransformer>();
        }
    }

    std::string getName() const override {
        return "WhileTransformer";
    }

    WhileTransformer* clone() const override {
        return new WhileTransformer(condition, souffle::clone(transformer));
    }

private:
    std::function<bool()> condition;
    std::unique_ptr<AstTransformer> transformer;
    bool transform(AstTranslationUnit& translationUnit) override;
};

/**
 * Transformer that repeatedly executes a sub-transformer until no changes are made
 */
class FixpointTransformer : public MetaTransformer {
public:
    FixpointTransformer(std::unique_ptr<AstTransformer> transformer) : transformer(std::move(transformer)) {}

    void setDebugReport() override {
        if (auto* mt = dynamic_cast<MetaTransformer*>(transformer.get())) {
            mt->setDebugReport();
        } else {
            transformer = std::make_unique<DebugReporter>(std::move(transformer));
        }
    }

    std::vector<AstTransformer*> getSubtransformers() const override {
        return {transformer.get()};
    }

    void setVerbosity(bool verbose) override {
        this->verbose = verbose;
        if (auto* mt = dynamic_cast<MetaTransformer*>(transformer.get())) {
            mt->setVerbosity(verbose);
        }
    }

    void disableTransformers(const std::set<std::string>& transforms) override {
        if (auto* mt = dynamic_cast<MetaTransformer*>(transformer.get())) {
            mt->disableTransformers(transforms);
        } else if (transforms.find(transformer->getName()) != transforms.end()) {
            transformer = std::make_unique<NullTransformer>();
        }
    }

    std::string getName() const override {
        return "FixpointTransformer";
    }

    FixpointTransformer* clone() const override {
        return new FixpointTransformer(souffle::clone(transformer));
    }

private:
    std::unique_ptr<AstTransformer> transformer;
    bool transform(AstTranslationUnit& translationUnit) override;
};

/**
 * Transformation pass to determine instances of polymorphic object
 * objects = Functors (plus, minus...) ∪ binary constraints (>, ≥ ...) ∪ aggregation ∪ numeric constants
 */

class PolymorphicObjectsTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "PolymorphicObjectsTransformer";
    }

    PolymorphicObjectsTransformer* clone() const override {
        return new PolymorphicObjectsTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

/**
 * Transformation that passes the type information from user functors
 * declaration to functors instances
 */
class AstUserDefinedFunctorsTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "AstUserDefinedFunctorsTransformer";
    }

    AstUserDefinedFunctorsTransformer* clone() const override {
        return new AstUserDefinedFunctorsTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

/**
 * Transformation pass that removes (binary) constraints on the anonymous records.
 * After resolving aliases this is equivalent to completely removing anonymous records.
 *
 * e.g.
 * [a, b, c] = [x, y, z] → a = x, b = y, c = z.
 * [a, b, c] != [x, y, z] →  a != x  b != y  c != z (expanded to three new clauses)
 *
 * In a single pass, in case of equalities  a transformation expands a single level
 * of records in every clause. (e.g. [[a]] = [[1]] => [a] = [1])
 * In case of inequalities, it expands at most a single inequality in every clause
 *
 *
 * This transformation does not resolve aliases.
 * E.g. A = [a, b], A = [c, d]
 * Thus it should be called in conjunction with ResolveAnonymousRecordsAliases.
 */
class FoldAnonymousRecords : public AstTransformer {
public:
    std::string getName() const override {
        return "FoldAnonymousRecords";
    }

    FoldAnonymousRecords* clone() const override {
        return new FoldAnonymousRecords();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;

    /**
     * Process a single clause.
     *
     * @parem clause Clause to be processed.
     * @param newClauses a destination for the newly produced clauses.
     */
    void transformClause(const AstClause& clause, std::vector<std::unique_ptr<AstClause>>& newClauses);

    /**
     * Expand constraint on records position-wise.
     *
     * eg.
     * [1, 2, 3] = [a, b, c] => vector(1 = a, 2 = b, 3 = c)
     * [x, y, z] != [a, b, c] => vector(x != a, x != b, z != c)
     *
     * Procedure assumes that argument has a valid operation,
     * that children are of type AstRecordInit and that the size
     * of both sides is the same
     */
    std::vector<std::unique_ptr<AstLiteral>> expandRecordBinaryConstraint(const AstBinaryConstraint&);

    /**
     * Determine if the clause contains at least one binary constraint which can be expanded.
     */
    bool containsValidRecordConstraint(const AstClause&);

    /**
     * Determine if binary constraint can be expanded.
     */
    bool isValidRecordConstraint(const AstLiteral* literal);
};

/**
 * Transformer resolving aliases for anonymous records.
 *
 * The transformer works by searching the clause for equalities
 * of the form a = [...], where a is an anonymous record, and replacing
 * all occurrences of a with the RHS.
 *
 * The transformer is to be called in conjunction with FoldAnonymousRecords.
 **/
class ResolveAnonymousRecordsAliases : public AstTransformer {
public:
    std::string getName() const override {
        return "FoldAnonymousRecords";
    }

    ResolveAnonymousRecordsAliases* clone() const override {
        return new ResolveAnonymousRecordsAliases();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;

    /**
     * Use mapping found by findVariablesRecordMapping to substitute
     * a records for each variable that operates on records.
     **/
    bool replaceNamedVariables(AstTranslationUnit&, AstClause&);

    /**
     * For each variable equal to some anonymous record,
     * assign a value of that record.
     **/
    std::map<std::string, const AstRecordInit*> findVariablesRecordMapping(
            AstTranslationUnit&, const AstClause&);

    /**
     * For unnamed variables, replace each equation _ op record with true.
     **/
    bool replaceUnnamedVariable(AstClause&);
};

}  // end of namespace souffle
