/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ComponentInit.h
 *
 * Defines the component-initialization class
 *
 ***********************************************************************/

#pragma once

#include "ast/ComponentType.h"
#include "ast/Node.h"
#include "parser/SrcLocation.h"
#include <iosfwd>
#include <string>

namespace souffle::ast {

/**
 * @class ComponentInit
 * @brief Component initialization class
 *
 * Example:
 *  .init X=B<T1,T2>
 *
 * Intialization of a component with type parameters
 */
class ComponentInit : public Node {
public:
    ComponentInit(std::string name, Own<ComponentType> type, SrcLocation loc = {});

    /** Return instance name */
    const std::string& getInstanceName() const {
        return instanceName;
    }

    /** Set instance name */
    void setInstanceName(std::string name);

    /** Return component type */
    const ComponentType* getComponentType() const {
        return componentType.get();
    }

    /** Set component type */
    void setComponentType(Own<ComponentType> type);

    void apply(const NodeMapper& mapper) override;

protected:
    void print(std::ostream& os) const override;

    bool equal(const Node& node) const override;

private:
    NodeVec getChildren() const override;

    ComponentInit* cloning() const override;

private:
    /** Instance name */
    std::string instanceName;

    /** Actual component arguments for instantiation */
    Own<ComponentType> componentType;
};

}  // namespace souffle::ast
