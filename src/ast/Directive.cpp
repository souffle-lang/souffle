/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/Directive.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <ostream>
#include <utility>

namespace souffle::ast {

std::ostream& operator<<(std::ostream& os, DirectiveType e) {
    switch (e) {
        case DirectiveType::input: return os << "input";
        case DirectiveType::output: return os << "output";
        case DirectiveType::printsize: return os << "printsize";
        case DirectiveType::limitsize: return os << "limitsize";
    }

    UNREACHABLE_BAD_CASE_ANALYSIS
}

Directive::Directive(DirectiveType type, QualifiedName name, SrcLocation loc)
        : Node(std::move(loc)), type(type), name(std::move(name)) {}

void Directive::setQualifiedName(QualifiedName name) {
    this->name = std::move(name);
}

void Directive::addParameter(const std::string& key, std::string value) {
    parameters[key] = std::move(value);
}

void Directive::print(std::ostream& os) const {
    os << "." << type << " " << name;
    if (!parameters.empty()) {
        os << "(" << join(parameters, ",", [](std::ostream& out, const auto& arg) {
            out << arg.first << "=\"" << arg.second << "\"";
        }) << ")";
    }
}

bool Directive::equal(const Node& node) const {
    const auto& other = asAssert<Directive>(node);
    return other.type == type && other.name == name && other.parameters == parameters;
}

Directive* Directive::cloning() const {
    auto res = new Directive(type, name, getSrcLoc());
    res->parameters = parameters;
    return res;
}

}  // namespace souffle::ast
