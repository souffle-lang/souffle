/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ComponentType.h
 *
 * Defines the component-type class
 *
 ***********************************************************************/

#pragma once

#include "ast/Node.h"
#include "ast/QualifiedName.h"
#include "parser/SrcLocation.h"
#include <iosfwd>
#include <string>
#include <vector>

namespace souffle::ast {

/**
 * @class ComponentType
 * @brief Component type of a component
 *
 * Example:
 *    name < Type1, Type2, ... >
 * where name is the component name and < Type, Type, ... > is a
 * list of component type parameters (either actual or formal).
 */
class ComponentType : public Node {
public:
    ComponentType(std::string name = "", std::vector<QualifiedName> params = {}, SrcLocation loc = {});

    /** Return component name */
    const std::string& getName() const {
        return name;
    }

    /** Set component name */
    void setName(std::string n);

    /** Return component type parameters */
    const std::vector<QualifiedName>& getTypeParameters() const {
        return typeParams;
    }

    /** Set component type parameters */
    void setTypeParameters(const std::vector<QualifiedName>& params) {
        typeParams = params;
    }

    static bool classof(const Node*);

protected:
    void print(std::ostream& os) const override;

private:
    bool equal(const Node& node) const override;

    ComponentType* cloning() const override;

private:
    /** Component name */
    std::string name;

    /** Component type parameters */
    std::vector<QualifiedName> typeParams;
};

}  // namespace souffle::ast
