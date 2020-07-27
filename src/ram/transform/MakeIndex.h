/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file MakeIndex.h
 *
 ***********************************************************************/

#pragma once

#include "ram/RamOperation.h"
#include "ram/RamTranslationUnit.h"
#include "ram/analysis/RamLevelAnalysis.h"
#include "ram/transform/RamTransformer.h"
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

class RamProgram;
class RamCondition;
class RamExpression;

/**
 * @class MakeIndexTransformer
 * @brief Make indexable operations to indexed operations.
 *
 * The transformer assumes that the RAM has been levelled before.
 * The conditions that could be used for an index must be located
 * immediately after the scan or aggregate operation.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *   FOR t1 IN A
 *    IF t1.x = 10 /\ t1.y = 20 /\ C
 *     ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * will be rewritten to
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    SEARCH t1 IN A INDEX t1.x=10 AND t1.y = 20
 *     IF C
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class MakeIndexTransformer : public RamTransformer {
public:
    std::string getName() const override {
        return "MakeIndexTransformer";
    }

    /**
     * @brief Get expression of RAM element access
     *
     * @param Equivalence constraints of the format t1.x = <expression> or <expression> = t1.x
     * @param Element that was accessed, e.g., for t1.x this would be the index of attribute x.
     * @param Tuple identifier
     *
     * The method retrieves expression the expression of an equivalence constraint of the
     * format t1.x = <expr> or <expr> = t1.x
     */
    using ExpressionPair = std::pair<std::unique_ptr<RamExpression>, std::unique_ptr<RamExpression>>;

    ExpressionPair getSignedExpressionPair(const RamConstraint* binRelOp, size_t& element, int identifier);
    ExpressionPair getUnsignedExpressionPair(const RamConstraint* binRelOp, size_t& element, int identifier);
    ExpressionPair getFloatExpressionPair(const RamConstraint* binRelOp, size_t& element, int identifier);
    ExpressionPair getLowerUpperExpression(RamCondition* c, size_t& element, int level);

    /**
     * @brief Construct query patterns for an indexable operation
     * @param Query pattern that is to be constructed
     * @param Flag to indicate whether operation is indexable
     * @param A list of conditions that will be transformed to query patterns
     * @param Tuple identifier of the indexable operation
     * @result Remaining conditions that could not be transformed to an index
     */
    std::unique_ptr<RamCondition> constructPattern(RamPattern& queryPattern, bool& indexable,
            std::vector<std::unique_ptr<RamCondition>> conditionList, int identifier);

    /**
     * @brief Rewrite a scan operation to an indexed scan operation
     * @param Scan operation that is potentially rewritten to an IndexScan
     * @result The result is null if the scan could not be rewritten to an IndexScan;
     *         otherwise the new IndexScan operation is returned.
     */
    std::unique_ptr<RamOperation> rewriteScan(const RamScan* scan);

    /**
     * @brief Rewrite an index scan operation to an amended index scan operation
     * @param An IndexScan that can be amended with new index values
     * @result The result is null if the index scan cannot be amended;
     *         otherwise the new IndexScan operation is returned.
     */
    std::unique_ptr<RamOperation> rewriteIndexScan(const RamIndexScan* iscan);

    /**
     * @brief Rewrite an aggregate operation to an indexed aggregate operation
     * @param Aggregate operation that is potentially rewritten to an indexed version
     * @result The result is null if the aggregate could not be rewritten to an indexed version;
     *         otherwise the new indexed version of the aggregate is returned.
     */
    std::unique_ptr<RamOperation> rewriteAggregate(const RamAggregate* agg);

    /**
     * @brief Make indexable RAM operation indexed
     * @param RAM program that is transformed
     * @result Flag that indicates whether the input program has changed
     */
    bool makeIndex(RamProgram& program);

protected:
    RamLevelAnalysis* rla{nullptr};
    bool transform(RamTranslationUnit& translationUnit) override {
        rla = translationUnit.getAnalysis<RamLevelAnalysis>();
        return makeIndex(translationUnit.getProgram());
    }
};

}  // end of namespace souffle
