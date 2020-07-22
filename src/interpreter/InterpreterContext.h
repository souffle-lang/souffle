/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file InterpreterContext.h
 *
 * Defines Interpreter interpreter context
 *
 ***********************************************************************/

#pragma once

#include "interpreter/InterpreterIndex.h"
#include "interpreter/InterpreterRelation.h"
#include "souffle/RamTypes.h"
#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace souffle {

/**
 * Evaluation context for Interpreter operations
 */
class InterpreterContext {
    using ViewPtr = std::unique_ptr<IndexView>;

    /** @brief Run-time value */
    std::vector<const RamDomain*> data;
    /** @brief Subroutine return value */
    std::vector<RamDomain>* returnValues = nullptr;
    /** @brief Subroutine arguments */
    const std::vector<RamDomain>* args = nullptr;
    /** @bref Allocated data */
    std::vector<std::unique_ptr<RamDomain[]>> allocatedDataContainer;
    /** @brief Views */
    std::vector<std::unique_ptr<IndexView>> views;

public:
    InterpreterContext(size_t size = 0) : data(size) {}

    /** This constructor is used when program enter a new scope.
     * Only Subroutine value needs to be copied */
    InterpreterContext(InterpreterContext& ctxt) : returnValues(ctxt.returnValues), args(ctxt.args) {}
    virtual ~InterpreterContext() = default;

    const RamDomain*& operator[](size_t index) {
        if (index >= data.size()) {
            data.resize((index + 1));
        }
        return data[index];
    }

    const RamDomain* const& operator[](size_t index) const {
        return data[index];
    }

    /** @brief Allocate a tuple.
     *  allocatedDataContainer has the ownership of those tuples. */
    RamDomain* allocateNewTuple(size_t size) {
        std::unique_ptr<RamDomain[]> newTuple(new RamDomain[size]);
        allocatedDataContainer.push_back(std::move(newTuple));

        // Return the reference as raw pointer.
        return allocatedDataContainer.back().get();
    }

    /** @brief Get subroutine return value */
    std::vector<RamDomain>& getReturnValues() const {
        return *returnValues;
    }

    /** @brief Set subroutine return value */
    void setReturnValues(std::vector<RamDomain>& retVals) {
        returnValues = &retVals;
    }

    /** @brief Add subroutine return value */
    void addReturnValue(RamDomain val) {
        assert(returnValues != nullptr);
        returnValues->push_back(val);
    }

    /** @brief Get subroutine Arguments */
    const std::vector<RamDomain>& getArguments() const {
        return *args;
    }

    /** @brief Set subroutine Arguments */
    void setArguments(const std::vector<RamDomain>& a) {
        args = &a;
    }

    /** @brief Get subroutine Arguments */
    RamDomain getArgument(size_t i) const {
        assert(args != nullptr && i < args->size() && "argument out of range");
        return (*args)[i];
    }

    /** @brief Create a view in the environment */
    void createView(const InterpreterRelation& rel, size_t indexPos, size_t viewPos) {
        ViewPtr view;
        if (views.size() < viewPos + 1) {
            views.resize(viewPos + 1);
        }
        views[viewPos] = rel.getView(indexPos);
    }

    /** @brief Return a view */
    ViewPtr& getView(size_t id) {
        assert(id < views.size());
        return views[id];
    }
};

}  // end of namespace souffle
