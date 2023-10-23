/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Context.h
 *
 * Defines Interpreter interpreter context
 *
 ***********************************************************************/

#pragma once

#include "interpreter/Index.h"
#include "interpreter/Relation.h"
#include "souffle/RamTypes.h"
#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace souffle::interpreter {

/**
 * Evaluation context for Interpreter operations
 */
class Context {
    using ViewPtr = Own<ViewWrapper>;

public:
    Context(std::size_t size = 0) : data(size) {}

    /** This constructor is used when program enter a new scope.
     * Only Subroutine value needs to be copied */
    Context(Context& ctxt) : returnValues(ctxt.returnValues), args(ctxt.args) {}
    virtual ~Context() = default;

    const RamDomain*& operator[](std::size_t index) {
        if (index >= data.size()) {
            data.resize((index + 1));
        }
        return data[index];
    }

    const RamDomain* const& operator[](std::size_t index) const {
        return data[index];
    }

    /** @brief Allocate a tuple.
     *  allocatedDataContainer has the ownership of those tuples. */
    RamDomain* allocateNewTuple(std::size_t size) {
        Own<RamDomain[]> newTuple(new RamDomain[size]);
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
    RamDomain getArgument(std::size_t i) const {
        assert(args != nullptr && i < args->size() && "argument out of range");
        return (*args)[i];
    }

    /** @brief Create a view in the environment */
    void createView(const RelationWrapper& rel, std::size_t indexPos, std::size_t viewPos) {
        ViewPtr view;
        if (views.size() < viewPos + 1) {
            views.resize(viewPos + 1);
        }
        views[viewPos] = rel.createView(indexPos);
    }

    /** @brief Return a view */
    ViewWrapper* getView(std::size_t id) {
        assert(id < views.size());
        return views[id].get();
    }

    RamDomain getVariable(const std::string& name) {
        return variables[name];
    }

    void setVariable(const std::string& name, RamDomain value) {
        variables[name] = value;
    }

private:
    /** @brief Run-time value */
    std::vector<const RamDomain*> data;
    /** @brief Subroutine return value */
    std::vector<RamDomain>* returnValues = nullptr;
    /** @brief Subroutine arguments */
    const std::vector<RamDomain>* args = nullptr;
    /** @bref Allocated data */
    VecOwn<RamDomain[]> allocatedDataContainer;
    /** @brief Views */
    VecOwn<ViewWrapper> views;
    std::map<std::string, RamDomain> variables;
};

}  // namespace souffle::interpreter
