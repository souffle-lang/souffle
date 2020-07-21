/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamTransformer.h
 *
 * Defines the interface for RAM transformation passes.
 *
 ***********************************************************************/

#pragma once

#include <cassert>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

class RamTranslationUnit;

/**
 * @Class RamTransformer
 * @Brief abstract transformer class for a translation unit
 *
 * This is an abstract class to implement transformers. A
 * transformer takes a translation unit and changes its
 * state.
 *
 * Transformers can be composed using other transformers.
 *
 * For debugging purposes, a transformer has a name
 * (this will show up in the debug report) and a
 * protected method transform(), that performs the
 * actual transformation.
 *
 * The method apply is used to call transform() and
 * does the reporting of the debug information.
 *
 */

class RamTransformer {
public:
    virtual ~RamTransformer() = default;

    /**
     * @Brief apply the transformer to a translation unit
     * @Param translationUnit that will be transformed.
     * @Return flag reporting whether the RAM program has changed
     */
    bool apply(RamTranslationUnit& translationUnit);

    /**
     * @Brief get name of the transformer
     */
    virtual std::string getName() const = 0;

protected:
    /**
     * @Brief transform the translation unit / used by apply
     * @Param translationUnit that will be transformed.
     * @Return flag reporting whether the RAM program has changed
     */
    virtual bool transform(RamTranslationUnit& translationUnit) = 0;
};

/**
 * @Class RamMetaTransformer
 * @Brief Abstract class to identifier meta transformer
 */
class RamMetaTransformer : public RamTransformer {};

/**
 * @Class RamTransformerSequence
 * @Brief Composite sequence transformer
 *
 * A sequence of transformations is applied to a translation unit
 * sequentially. The last transformation decides the outcome whether
 * the code has been changed.
 *
 */
class RamTransformerSequence : public RamMetaTransformer {
public:
    template <typename... Tfs>
    RamTransformerSequence(std::unique_ptr<Tfs>&&... tf) : RamTransformerSequence() {
        std::unique_ptr<RamTransformer> tmp[] = {std::move(tf)...};
        for (auto& cur : tmp) {
            transformers.emplace_back(std::move(cur));
        }
        for (const auto& cur : transformers) {
            (void)cur;
            assert(cur);
        }
    }
    RamTransformerSequence() = default;
    std::string getName() const override {
        return "RamTransformerSequence";
    }
    bool transform(RamTranslationUnit& tU) override {
        bool changed = false;
        // The last transformer decides the status
        // of the change flag.
        // Note that for other semantics, new transformer
        // sequence class needs to be introduced.
        for (auto const& cur : transformers) {
            changed = cur->apply(tU);
        }
        return changed;
    }

protected:
    /** sequence of transformers */
    std::vector<std::unique_ptr<RamTransformer>> transformers;
};

/**
 * @Class RamLoopTransformer
 * @Brief Composite loop transformer
 *
 * A transformation is invoked iteratively until no further change
 * is made.
 */
class RamLoopTransformer : public RamMetaTransformer {
public:
    RamLoopTransformer(std::unique_ptr<RamTransformer> tLoop) : loop(std::move(tLoop)) {}
    std::string getName() const override {
        return "RamLoopTransformer";
    }
    bool transform(RamTranslationUnit& tU) override {
        int ctr = 0;
        while (loop->apply(tU)) {
            ctr++;
        }
        return ctr > 0;
    }

protected:
    /** transformer of the loop */
    std::unique_ptr<RamTransformer> loop;
};

/**
 * @Class RamConditionalTransformer
 * @Brief Composite conditional transformer
 *
 * A transformation is invoked if a condition holds.
 */
class RamConditionalTransformer : public RamMetaTransformer {
public:
    RamConditionalTransformer(std::function<bool()> fn, std::unique_ptr<RamTransformer> tb)
            : func(std::move(fn)), body(std::move(tb)) {}
    std::string getName() const override {
        return "RamConditionalTransformer";
    }
    bool transform(RamTranslationUnit& tU) override {
        if (func()) {
            return body->apply(tU);
        } else {
            return false;
        }
    }

protected:
    std::function<bool()> func;
    std::unique_ptr<RamTransformer> body;
};

}  // end of namespace souffle
