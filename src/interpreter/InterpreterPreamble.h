/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file InterpreterPreamble
 *
 * Declares the InterpreterPreamble class.
 * Each Query operation has an InterpreterPreamble assoicated with it.
 * The preamble contains information about views creation.
 ***********************************************************************/

#pragma once

#include <array>
#include <memory>
#include <vector>

namespace souffle {

class InterpreterNode;

/**
 * @class InterpreterPreamble
 * @brief This class contains information for views (Hints) creation for RamQuery and RamParallel operation.
 */
class InterpreterPreamble {
public:
    /** @brief Add outer-most filter operation which requires a view.  */
    void addViewOperationForFilter(std::unique_ptr<InterpreterNode> node) {
        outerFilterViewOps.push_back(std::move(node));
    }

    /** @brief Add outer-most filter operation which does not require a view.  */
    void addViewFreeOperationForFilter(std::unique_ptr<InterpreterNode> node) {
        outerFilterViewFreeOps.push_back(std::move(node));
    }

    /** @brief Add nested operation which require a View (Hints).  */
    void addViewOperationForNested(std::unique_ptr<InterpreterNode> op) {
        nestedViewOps.push_back(std::move(op));
    }

    /** @brief Return outer-most filter operations.  */
    const std::vector<std::unique_ptr<InterpreterNode>>& getOuterFilterViewOps() {
        return outerFilterViewOps;
    }

    /** @brief Return views for outer-most filter operations.  */
    const std::vector<std::unique_ptr<InterpreterNode>>& getOuterFilterViewFreeOps() {
        return outerFilterViewFreeOps;
    }

    /** @brief Return nested operations */
    std::vector<std::unique_ptr<InterpreterNode>>& getViewsInNestedOperation() {
        return nestedViewOps;
    }

    /** @brief Return Views information for outer filter operation */
    std::vector<std::array<size_t, 3>>& getViewInfoForFilter() {
        return viewInfoForFilter;
    }

    /** @brief Return Views information for nested operation */
    std::vector<std::array<size_t, 3>>& getViewInfoForNested() {
        return viewInfoForNested;
    }

    /** @brief Add View creation information into the list for outer filter.  */
    void addViewInfoForFilter(size_t relId, size_t indexPos, size_t viewPos) {
        viewInfoForFilter.push_back({relId, indexPos, viewPos});
    }

    /** @brief Add View creation information into the list for nested oprations. */
    void addViewInfoForNested(size_t relId, size_t indexPos, size_t viewPos) {
        viewInfoForNested.push_back({relId, indexPos, viewPos});
    }

    /** If this preamble contains parallel operation.  */
    bool isParallel = false;

private:
    /** Vector of filter operation, views required */
    std::vector<std::unique_ptr<InterpreterNode>> outerFilterViewOps;
    /** Vector of filter operations, no views required. */
    std::vector<std::unique_ptr<InterpreterNode>> outerFilterViewFreeOps;
    /** Vector of nested operations */
    std::vector<std::unique_ptr<InterpreterNode>> nestedViewOps;
    /** Vector of View information in filter operations */
    std::vector<std::array<size_t, 3>> viewInfoForFilter;
    /** Vector of View information in nested operations */
    std::vector<std::array<size_t, 3>> viewInfoForNested;
};

}  // namespace souffle
