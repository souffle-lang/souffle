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

#include "AstArgument.h"
#include "AstTransformer.h"
#include "AstTranslationUnit.h"
#include "DebugReport.h"
#include "Util.h"
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

class AstClause;
class AstProgram;
class AstRelation;

/**
 * Transformation pass to eliminate grounded aliases.
 * e.g. resolve: a(r) , r = [x,y]       => a(x,y)
 * e.g. resolve: a(x) , !b(y) , y = x   => a(x) , !b(x)
 */
class ResolveAliasesTransformer : public AstTransformer {
private:
    bool transform(AstTranslationUnit& translationUnit) override;

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
private:
    bool transform(AstTranslationUnit& translationUnit) override {
        return removeRelationCopies(*translationUnit.getProgram());
    }

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
    static bool removeRelationCopies(AstProgram& program);
};

/**
 * Transformation pass to rename aggregation variables to make them unique.
 */
class UniqueAggregationVariablesTransformer : public AstTransformer {
private:
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    std::string getName() const override {
        return "UniqueAggregationVariablesTransformer";
    }
};

/**
 * Transformation pass to create artificial relations for bodies of
 * aggregation functions consisting of more than a single atom.
 */
class MaterializeAggregationQueriesTransformer : public AstTransformer {
private:
    bool transform(AstTranslationUnit& translationUnit) override {
        return materializeAggregationQueries(translationUnit);
    }

    /**
     * A test determining whether the body of a given aggregation needs to be
     * 'outlined' into an independent relation or can be kept inline.
     */
    static bool needsMaterializedRelation(const AstAggregator& agg);

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
};

/**
 * Transformation pass to remove all empty relations and rules that use empty relations.
 */
class RemoveEmptyRelationsTransformer : public AstTransformer {
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
};

/**
 * Transformation pass to remove relations which are redundant (do not contribute to output).
 */
class RemoveRedundantRelationsTransformer : public AstTransformer {
private:
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    std::string getName() const override {
        return "RemoveRedundantRelationsTransformer";
    }
};

/**
 * Transformation pass to remove equivalent rules.
 */
class MinimiseProgramTransformer : public AstTransformer {
private:
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    std::string getName() const override {
        return "MinimiseProgramTransformer";
    }
};

/**
 * Transformation pass to add provenance information via guided SLD
 */
class ProvenanceTransformer : public AstTransformer {
private:
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    std::string getName() const override {
        return "ProvenanceTransformer";
    }
};

/**
 * Transformation pass to remove constant boolean constraints
 * Should be called after any transformation that may generate boolean constraints
 */
class RemoveBooleanConstraintsTransformer : public AstTransformer {
private:
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    std::string getName() const override {
        return "RemoveBooleanConstraintsTransformer";
    }
};

/**
 * Transformation pass to inline marked relations
 */
class InlineRelationsTransformer : public AstTransformer {
private:
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    std::string getName() const override {
        return "InlineRelationsTransformer";
    }
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
private:
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    std::string getName() const override {
        return "PartitionBodyLiteralsTransformer";
    }
};

/**
 * Transformation pass to reduce unnecessary computation for
 * relations that only appear in the form A(_,...,_).
 */
class ReduceExistentialsTransformer : public AstTransformer {
private:
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    std::string getName() const override {
        return "ReduceExistentialsTransformer";
    }
};

/**
 * Transformation pass to replace singleton variables
 * with unnamed variables.
 * E.g.: a() :- b(x). -> a() :- b(_).
 */
class ReplaceSingletonVariablesTransformer : public AstTransformer {
private:
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    std::string getName() const override {
        return "ReplaceSingletonVariablesTransformer";
    }
};

/**
 * Transformation pass to reorder body literals.
 */
class ReorderLiteralsTransformer : public AstTransformer {
private:
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    std::string getName() const override {
        return "ReorderLiteralsTransformer";
    }
};

/**
 * Transformation pass to normalise constraints.
 * E.g.: a(x) :- b(x, 1). -> a(x) :- b(x, tmp0), tmp0=1.
 */
class NormaliseConstraintsTransformer : public AstTransformer {
private:
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    std::string getName() const override {
        return "NormaliseConstraintsTransformer";
    }
};

/**
 * Magic Set Transformation
 */
class MagicSetTransformer : public AstTransformer {
private:
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    std::string getName() const override {
        return "MagicSetTransformer";
    }
};

/**
 * Transformer that holds an arbitrary number of sub-transformations
 */
class PipelineTransformer : public MetaTransformer {
private:
    std::vector<std::unique_ptr<AstTransformer>> pipeline;
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    template <typename... Args>
    PipelineTransformer(Args... args) {
        std::unique_ptr<AstTransformer> tmp[] = {std::move(args)...};
        for (auto& cur : tmp) {
            pipeline.push_back(std::move(cur));
        }
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
};

/**
 * Transformer that executes a sub-transformer iff a condition holds
 */
class ConditionalTransformer : public MetaTransformer {
private:
    std::function<bool()> condition;
    std::unique_ptr<AstTransformer> transformer;
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    ConditionalTransformer(std::function<bool()> cond, std::unique_ptr<AstTransformer> transformer)
            : condition(std::move(cond)), transformer(std::move(transformer)) {}

    ConditionalTransformer(bool cond, std::unique_ptr<AstTransformer> transformer)
            : condition([=]() { return cond; }), transformer(std::move(transformer)) {}

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
};

/**
 * Transformer that repeatedly executes a sub-transformer while a condition is met
 */
class WhileTransformer : public MetaTransformer {
private:
    std::function<bool()> condition;
    std::unique_ptr<AstTransformer> transformer;
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    WhileTransformer(std::function<bool()> cond, std::unique_ptr<AstTransformer> transformer)
            : condition(std::move(cond)), transformer(std::move(transformer)) {}

    WhileTransformer(bool cond, std::unique_ptr<AstTransformer> transformer)
            : condition([=]() { return cond; }), transformer(std::move(transformer)) {}

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
};

/**
 * Transformer that repeatedly executes a sub-transformer until no changes are made
 */
class FixpointTransformer : public MetaTransformer {
private:
    std::unique_ptr<AstTransformer> transformer;
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    FixpointTransformer(std::unique_ptr<AstTransformer> transformer) : transformer(std::move(transformer)) {}

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
        return "FixpointTransformer";
    }
};

}  // end of namespace souffle
