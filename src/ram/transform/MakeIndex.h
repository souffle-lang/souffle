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

#include "ram/Aggregate.h"
#include "ram/Condition.h"
#include "ram/Constraint.h"
#include "ram/Expression.h"
#include "ram/IndexOperation.h"
#include "ram/IndexScan.h"
#include "ram/Operation.h"
#include "ram/Program.h"
#include "ram/Scan.h"
#include "ram/TranslationUnit.h"
#include "ram/analysis/Level.h"
#include "ram/analysis/Relation.h"
#include "ram/transform/Transformer.h"
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram::transform {

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
class MakeIndexTransformer : public Transformer {
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
    using ExpressionPair = std::pair<Own<Expression>, Own<Expression>>;

    ExpressionPair getExpressionPair(
            const Constraint* binRelOp, std::size_t& element, const std::optional<std::size_t>& identifier);
    ExpressionPair getLowerUpperExpression(Condition* c, std::size_t& element,
            const std::optional<std::size_t>& level, RelationRepresentation rep);

    /**
     * @param AttributeTypes to indicate type of each attribute in the relation
     * @param Query pattern that is to be constructed
     * @param Flag to indicate whether operation is indexable
     * @param A list of conditions that will be transformed to query patterns
     * @param Tuple identifier of the indexable operation
     * @param RelationRepresentation identifying the data structure
     * @result Remaining conditions that could not be transformed to an index
     */
    Own<Condition> constructPattern(const std::vector<std::string>& attributeTypes, RamPattern& queryPattern,
            bool& indexable, VecOwn<Condition> conditionList, std::size_t identifier,
            RelationRepresentation rep);

    /**
     * @brief Rewrite a scan operation to an indexed scan operation
     * @param Scan operation that is potentially rewritten to an IndexScan
     * @result The result is null if the scan could not be rewritten to an IndexScan;
     *         otherwise the new IndexScan operation is returned.
     */
    Own<Operation> rewriteScan(const Scan* scan);

    /**
     * @brief Rewrite an index scan operation to an amended index scan operation
     * @param An IndexScan that can be amended with new index values
     * @result The result is null if the index scan cannot be amended;
     *         otherwise the new IndexScan operation is returned.
     */
    Own<Operation> rewriteIndexScan(const IndexScan* iscan);

    /**
     * @brief Rewrite an aggregate operation to an indexed aggregate operation
     * @param Aggregate operation that is potentially rewritten to an indexed version
     * @result The result is null if the aggregate could not be rewritten to an indexed version;
     *         otherwise the new indexed version of the aggregate is returned.
     */
    Own<Operation> rewriteAggregate(const Aggregate* agg);

    /**
     * @brief Make indexable RAM operation indexed
     * @param RAM program that is transformed
     * @result Flag that indicates whether the input program has changed
     */
    bool makeIndex(Program& program);

protected:
    analysis::LevelAnalysis* rla{nullptr};

    bool transform(TranslationUnit& translationUnit) override {
        rla = &translationUnit.getAnalysis<analysis::LevelAnalysis>();
        relAnalysis = &translationUnit.getAnalysis<analysis::RelationAnalysis>();
        return makeIndex(translationUnit.getProgram());
    }

    analysis::RelationAnalysis* relAnalysis{nullptr};
};

}  // namespace souffle::ram::transform
