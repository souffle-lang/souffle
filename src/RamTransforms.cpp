/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamTransforms.cpp
 *
 * Implementation of RAM transformation passes.
 *
 ***********************************************************************/

#include "RamTransforms.h"
#include "RamVisitor.h"

namespace souffle {

bool LevelConditionsTransformer::levelConditions(RamProgram& program) {
    // Node-mapper that collects nested conditions which apply to a given scan level
    // TODO: Change these to LambdaRamNodeMapper lambdas
    struct RamFilterCapturer : public RamNodeMapper {
        mutable std::unique_ptr<RamCondition> condition;

        /** identifier for the tuple */
        const size_t identifier;

        RamFilterCapturer(const size_t ident) : identifier(ident) {}

        std::unique_ptr<RamCondition> getCondition() const {
            return std::move(condition);
        }

        void addCondition(std::unique_ptr<RamCondition> c) const {
            if (condition != nullptr) {
                condition = std::make_unique<RamAnd>(std::move(condition), std::move(c));
            } else {
                condition = std::move(c);
            }
        }

        using RamNodeMapper::operator();

        std::unique_ptr<RamNode> operator()(std::unique_ptr<RamNode> node) const override {
            if (RamNestedOperation* nested = dynamic_cast<RamNestedOperation*>(node.get())) {
                if (const RamFilter* filter = dynamic_cast<const RamFilter*>(&nested->getOperation())) {
                    const RamCondition& condition = filter->getCondition();

                    if (condition.getLevel() == identifier) {
                        addCondition(std::unique_ptr<RamCondition>(condition.clone()));

                        // skip this filter
                        nested->setOperation(std::unique_ptr<RamOperation>(filter->getOperation().clone()));
                        return (*this)(std::move(node));
                    }
                }
            }

            node->apply(*this);
            return node;
        }
    };

    // Node-mapper that searches for and updates RAM scans nested in RAM inserts
    struct RamScanCapturer : public RamNodeMapper {
        mutable bool modified = false;

        bool getModified() const {
            return modified;
        }

        std::unique_ptr<RamNode> operator()(std::unique_ptr<RamNode> node) const override {
            if (RamScan* scan = dynamic_cast<RamScan*>(node.get())) {
                RamFilterCapturer filterUpdate(scan->getIdentifier());
                std::unique_ptr<RamScan> newScan = filterUpdate(std::unique_ptr<RamScan>(scan->clone()));

                // If a condition applies to this scan level, filter the scan based on the condition
                if (std::unique_ptr<RamCondition> condition = filterUpdate.getCondition()) {
                    newScan->setOperation(std::make_unique<RamFilter>(std::move(condition),
                            std::unique_ptr<RamOperation>(newScan->getOperation().clone())));
                    modified = true;
                }

                node = std::move(newScan);
            }

            node->apply(*this);
            return node;
        }
    };

    // Node-mapper that searches for and updates RAM inserts
    struct RamInsertCapturer : public RamNodeMapper {
        mutable bool modified = false;

        bool getModified() const {
            return modified;
        }

        std::unique_ptr<RamNode> operator()(std::unique_ptr<RamNode> node) const override {
            // get all RAM inserts
            if (RamInsert* insert = dynamic_cast<RamInsert*>(node.get())) {
                RamScanCapturer scanUpdate;
                insert->apply(scanUpdate);

                if (scanUpdate.getModified()) {
                    modified = true;
                }
            } else {
                // no need to search for nested RAM inserts
                node->apply(*this);
            }

            return node;
        }
    };

    // level all RAM inserts
    RamInsertCapturer insertUpdate;
    program.getMain()->apply(insertUpdate);

    return insertUpdate.getModified();
}

bool CreateIndicesTransformer::createIndices(RamProgram& program) {
    return false;
}

}  // end of namespace souffle
