/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Attribute.h
 *
 * Defines the attribute class
 *
 ***********************************************************************/

#pragma once

#include "ast/Node.h"
#include "ast/QualifiedName.h"
#include "parser/SrcLocation.h"
#include <iosfwd>
#include <string>

namespace souffle::ast {

/**
 * @class Attribute
 * @brief Attribute class
 *
 * Example:
 *    x: number
 * An attribute consists of a name and its type name.
 */
class Attribute : public Node {
public:
    Attribute(std::string n, QualifiedName t, SrcLocation loc = {});
    Attribute(std::string n, QualifiedName t, std::optional<std::string> merger, SrcLocation loc = {});

    /** Return attribute name */
    const std::string& getName() const {
        return name;
    }

    /** Return type name */
    const QualifiedName& getTypeName() const {
        return typeName;
    }

    /** Set type name */
    void setTypeName(QualifiedName name);

    const std::optional<std::string>& getMerger() const {
        return merger;
    }

protected:
    void print(std::ostream& os) const override;

private:
    bool equal(const Node& node) const override;

    Attribute* cloning() const override;

private:
    /** Attribute name */
    std::string name;

    /** Type name */
    QualifiedName typeName;

    std::optional<std::string> merger;
};

}  // namespace souffle::ast
