/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file NormaliseGenerators.cpp
 *
 * Transform pass to normalise all appearances of generators.
 * Generators include multi-result functors + aggregators.
 *
 ***********************************************************************/

#include "ast/transform/NormaliseGenerators.h"
#include "ast/Aggregator.h"
#include "ast/BinaryConstraint.h"
#include "ast/IntrinsicFunctor.h"
#include "ast/Program.h"
#include "ast/TranslationUnit.h"
#include "ast/Variable.h"
#include "ast/analysis/Functor.h"
#include "souffle/utility/ContainerUtil.h"

namespace souffle::ast::transform {

bool NormaliseGeneratorsTransformer::transform(TranslationUnit& translationUnit) {
    bool changed = false;
    auto& program = translationUnit.getProgram();

    // Assign a unique name to each generator
    struct name_generators : public NodeMapper {
        mutable int count{0};
        mutable std::vector<std::pair<std::string, Own<Argument>>> generatorNames{};
        name_generators() = default;

        std::vector<std::pair<std::string, Own<Argument>>> getGeneratorNames() {
            return std::move(generatorNames);
        }

        std::string getUniqueName() const {
            std::stringstream newName;
            newName << "@generator_" << count++;
            return newName.str();
        }

        Own<Node> operator()(Own<Node> node) const override {
            node->apply(*this);

            if (auto* inf = as<IntrinsicFunctor>(node)) {
                // Multi-result functors
                if (analysis::FunctorAnalysis::isMultiResult(*inf)) {
                    std::string name = getUniqueName();
                    generatorNames.emplace_back(name, clone(inf));
                    return mk<Variable>(name);
                }
            } else if (auto* agg = as<Aggregator>(node)) {
                // Aggregators
                std::string name = getUniqueName();
                generatorNames.emplace_back(name, clone(agg));
                return mk<Variable>(name);
            }

            return node;
        }
    };

    // Apply the mapper to each clause
    for (auto* clause : program.getClauses()) {
        name_generators update;
        clause->apply(update);
        for (auto& [name, generator] : update.getGeneratorNames()) {
            changed = true;
            clause->addToBody(
                    mk<BinaryConstraint>(BinaryConstraintOp::EQ, mk<Variable>(name), std::move(generator)));
        }
    }
    return changed;
}

}  // namespace souffle::ast::transform
