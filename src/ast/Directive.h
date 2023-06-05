/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Directive.h
 *
 * Defines a directive for a relation
 *
 ***********************************************************************/

#pragma once

#include "ast/Node.h"
#include "ast/QualifiedName.h"
#include "parser/SrcLocation.h"
#include <iosfwd>
#include <map>
#include <string>

namespace souffle::ast {

enum class DirectiveType { input, output, printsize, limitsize };

// FIXME: I'm going crazy defining these. There has to be a library that does this boilerplate for us.
std::ostream& operator<<(std::ostream& os, DirectiveType e);

/**
 * @class Directive
 * @brief a directive has a type (e.g. input/output/printsize/limitsize), qualified relation name, and a key
 * value map for storing parameters of the directive.
 */
class Directive : public Node {
public:
    Directive(DirectiveType type, QualifiedName name, SrcLocation loc = {});

    /** Get directive type */
    DirectiveType getType() const {
        return type;
    }

    /** Set directive  type */
    void setType(DirectiveType type) {
        this->type = type;
    }

    /** Get relation name */
    const QualifiedName& getQualifiedName() const {
        return name;
    }

    /** Set relation name */
    void setQualifiedName(QualifiedName name);

    /** Get parameter */
    const std::string& getParameter(const std::string& key) const {
        return parameters.at(key);
    }

    /** Add new parameter */
    void addParameter(const std::string& key, std::string value);

    /** Check for a parameter */
    bool hasParameter(const std::string& key) const {
        return parameters.find(key) != parameters.end();
    }

    /** Get parameters */
    const std::map<std::string, std::string>& getParameters() const {
        return parameters;
    }

protected:
    void print(std::ostream& os) const override;

private:
    bool equal(const Node& node) const override;

    Directive* cloning() const override;

private:
    /** Type of directive */
    DirectiveType type;

    /** Relation name of the directive */
    QualifiedName name;

    /** Parameters of directive */
    std::map<std::string, std::string> parameters;
};

}  // namespace souffle::ast
