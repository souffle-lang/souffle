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

namespace {

std::vector<RamCondition*> getConditions(const RamCondition* condition) {
    std::vector<RamCondition*> conditions;
    while (condition != nullptr) {
        if (const RamAnd* ramAnd = dynamic_cast<const RamAnd*>(condition)) {
            conditions.push_back(ramAnd->getLHS().clone());
            condition = &ramAnd->getRHS();
        } else {
            conditions.push_back(condition->clone());
            break;
        }
    }
    return conditions;
}

}  // namespace

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

/** Get indexable element */
std::unique_ptr<RamValue> CreateIndicesTransformer::getIndexElement(
        RamCondition* c, size_t& element, size_t identifier) {
    if (RamBinaryRelation* binRelOp = dynamic_cast<RamBinaryRelation*>(c)) {
        if (binRelOp->getOperator() == BinaryConstraintOp::EQ) {
            if (auto* lhs = dynamic_cast<RamElementAccess*>(binRelOp->getLHS())) {
                RamValue* rhs = binRelOp->getRHS();
                if (lhs->getLevel() == identifier && (rhs->isConstant() || rhs->getLevel() < identifier)) {
                    element = lhs->getElement();
                    return binRelOp->takeRHS();
                }
            }
            if (auto* rhs = dynamic_cast<RamElementAccess*>(binRelOp->getRHS())) {
                RamValue* lhs = binRelOp->getLHS();
                if (rhs->getLevel() == identifier && (lhs->isConstant() || lhs->getLevel() < identifier)) {
                    element = rhs->getElement();
                    return binRelOp->takeLHS();
                }
            }
        }
    }
    return nullptr;
}

std::unique_ptr<RamOperation> CreateIndicesTransformer::rewriteScan(const RamScan* scan) {
    if (const RamFilter* filter = dynamic_cast<const RamFilter*>(&scan->getOperation())) {
        const RamRelationReference& rel = scan->getRelation();
        const size_t identifier = scan->getIdentifier();

        // Values of index per column of table (if indexable)
        std::vector<std::unique_ptr<RamValue>> queryPattern(rel.getArity());

        // Indexable columns for a range query
        SearchColumns keys = 0;

        // Remaining conditions which weren't handled by an index
        std::unique_ptr<RamCondition> condition;

        auto addCondition = [&](std::unique_ptr<RamCondition> c) {
            if (condition != nullptr) {
                condition = std::make_unique<RamAnd>(std::move(condition), std::move(c));
            } else {
                condition = std::move(c);
            }
        };

        for (RamCondition* c : getConditions(filter->getCondition().clone())) {
            size_t element = 0;
            if (std::unique_ptr<RamValue> value = getIndexElement(c, element, identifier)) {
                keys |= (1 << element);
                if (queryPattern[element] == nullptr) {
                    queryPattern[element] = std::move(value);
                } else {
                    addCondition(std::unique_ptr<RamCondition>(c));
                }
            } else {
                addCondition(std::unique_ptr<RamCondition>(c));
            }
        }

        if (keys != 0) {
            // replace scan by index scan
            return std::make_unique<RamIndexScan>(std::unique_ptr<RamRelationReference>(rel.clone()),
                    identifier, std::move(queryPattern), keys,
                    condition == nullptr
                            ? std::unique_ptr<RamOperation>(filter->getOperation().clone())
                            : std::make_unique<RamFilter>(std::move(condition),
                                      std::unique_ptr<RamOperation>(filter->getOperation().clone())),
                    scan->getProfileText());
        }
    }

    return nullptr;
}

bool CreateIndicesTransformer::createIndices(RamProgram& program) {
    // TODO: Change these to LambdaRamNodeMapper lambdas
    // Node-mapper that searches for and updates RAM scans nested in RAM inserts
    struct RamScanCapturer : public RamNodeMapper {
        mutable bool modified = false;

        bool getModified() const {
            return modified;
        }

        std::unique_ptr<RamNode> operator()(std::unique_ptr<RamNode> node) const override {
            if (RamNestedOperation* nested = dynamic_cast<RamNestedOperation*>(node.get())) {
                if (const RamScan* scan = dynamic_cast<const RamScan*>(&nested->getOperation())) {
                    if (std::unique_ptr<RamOperation> op = rewriteScan(scan)) {
                        modified = true;
                        nested->setOperation(std::move(op));
                    }
                }
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
                // TODO: better way to modify the child of a RAM insert
                if (const RamScan* scan = dynamic_cast<const RamScan*>(&insert->getOperation())) {
                    if (std::unique_ptr<RamOperation> op = rewriteScan(scan)) {
                        modified = true;
                        insert->setOperation(std::move(op));
                    }
                }

                RamScanCapturer scanUpdate;
                insert->apply(scanUpdate);

                if (!modified && scanUpdate.getModified()) {
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

bool ConvertExistenceChecksTransformer::convertExistenceChecks(RamProgram& program) {
    // TODO: Change these to LambdaRamNodeMapper lambdas
    // Node-mapper that searches for and updates RAM scans nested in RAM inserts
    struct RamScanCapturer : public RamNodeMapper {
        mutable bool modified = false;

        bool getModified() const {
            return modified;
        }

        bool dependsOn(const RamCondition* condition, const size_t identifier) const {
            if (const RamBinaryRelation* binRel = dynamic_cast<const RamBinaryRelation*>(condition)) {
                std::vector<const RamValue*> queue = {binRel->getLHS(), binRel->getRHS()};
                while (!queue.empty()) {
                    const RamValue* val = queue.back();
                    queue.pop_back();
                    if (const RamValue* elemAccess = dynamic_cast<const RamElementAccess*>(val)) {
                        if (elemAccess->getLevel() == identifier) {
                            return true;
                        }
                    } else if (const RamBinaryOperator* binOp = dynamic_cast<const RamBinaryOperator*>(val)) {
                        queue.push_back(binOp->getLHS());
                        queue.push_back(binOp->getRHS());
                    }
                }
            }
            return false;
        }

        std::unique_ptr<RamNode> operator()(std::unique_ptr<RamNode> node) const override {
            if (RamRelationSearch* scan = dynamic_cast<RamRelationSearch*>(node.get())) {
                if (!scan->getRelation().isNullary()) {
                    const size_t identifier = scan->getIdentifier();
                    bool isExistCheck = true;
                    visitDepthFirst(scan->getOperation(), [&](const RamFilter& filter) {
                        if (isExistCheck) {
                            for (const RamCondition* c : getConditions(filter.getCondition().clone())) {
                                if (dependsOn(c, identifier)) {
                                    isExistCheck = false;
                                    break;
                                }
                            }
                        }
                    });
                    if (isExistCheck) {
                        visitDepthFirst(scan->getOperation(), [&](const RamIndexScan& indexScan) {
                            if (isExistCheck) {
                                for (const RamValue* value : indexScan.getRangePattern()) {
                                    if (value != nullptr && !value->isConstant() &&
                                            value->getLevel() == identifier) {
                                        isExistCheck = false;
                                        break;
                                    }
                                }
                            }
                        });
                    }
                    if (isExistCheck) {
                        visitDepthFirst(scan->getOperation(), [&](const RamProject& project) {
                            if (isExistCheck) {
                                std::vector<const RamValue*> values;
                                // TODO: function to extend vectors
                                const std::vector<RamValue*> initialVals = project.getValues();
                                values.insert(values.end(), initialVals.begin(), initialVals.end());

                                while (!values.empty()) {
                                    const RamValue* value = values.back();
                                    values.pop_back();

                                    if (const RamPack* pack = dynamic_cast<const RamPack*>(value)) {
                                        const std::vector<RamValue*> args = pack->getArguments();
                                        values.insert(values.end(), args.begin(), args.end());
                                    } else if (const RamBinaryOperator* binOp =
                                                       dynamic_cast<const RamBinaryOperator*>(value)) {
                                        values.push_back(binOp->getLHS());
                                        values.push_back(binOp->getRHS());
                                    } else if (value != nullptr && !value->isConstant() &&
                                               value->getLevel() == identifier) {
                                        isExistCheck = false;
                                        break;
                                    }
                                }
                            }
                        });
                    }
                    if (isExistCheck) {
                        visitDepthFirst(scan->getOperation(), [&](const RamLookup& lookup) {
                            if (isExistCheck) {
                                if (lookup.getReferenceLevel() == identifier) {
                                    isExistCheck = false;
                                }
                            }
                        });
                    }
                    if (isExistCheck) {
                        visitDepthFirst(scan->getOperation(), [&](const RamNotExists& notExists) {
                            if (isExistCheck) {
                                if (notExists.getLevel() == identifier) {
                                    isExistCheck = false;
                                }
                            }
                        });
                    }
                    scan->setIsPureExistenceCheck(isExistCheck);
                }
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

}  // end of namespace souffle
