/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2023, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/Lattice.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"

#include <utility>

namespace souffle::ast {

std::optional<LatticeOperator> latticeOperatorFromString(const std::string& str) {
    if (str == "Bottom") return Bottom;
    if (str == "Top") return Top;
    if (str == "Lub") return Lub;
    if (str == "Glb") return Glb;
    if (str == "Leq") return Leq;
    return std::nullopt;
}

std::string latticeOperatorToString(const LatticeOperator op) {
    switch (op) {
        case Bottom: return "Bottom";
        case Top: return "Top";
        case Lub: return "Lub";
        case Glb: return "Glb";
        case Leq: return "Leq";
        default: assert(false && "unknown lattice operator");
    }
    return "";
}

Lattice::Lattice(QualifiedName name, std::map<LatticeOperator, Own<ast::Argument>> ops, SrcLocation loc)
        : Node(NK_Lattice, std::move(loc)), name(std::move(name)), operators(std::move(ops)) {}

void Lattice::setQualifiedName(QualifiedName name) {
    this->name = std::move(name);
}

const std::map<LatticeOperator, const ast::Argument*> Lattice::getOperators() const {
    std::map<LatticeOperator, const ast::Argument*> ops;
    for (const auto& [op, arg] : operators) {
        ops.emplace(std::make_pair(op, arg.get()));
    }
    return ops;
}

bool Lattice::hasGlb() const {
    return operators.count(Glb) > 0;
}

bool Lattice::hasLub() const {
    return operators.count(Lub) > 0;
}

bool Lattice::hasBottom() const {
    return operators.count(Bottom) > 0;
}

bool Lattice::hasTop() const {
    return operators.count(Top) > 0;
}

const ast::Argument* Lattice::getLub() const {
    return operators.at(Lub).get();
}

const ast::Argument* Lattice::getGlb() const {
    return operators.at(Glb).get();
}

const ast::Argument* Lattice::getBottom() const {
    return operators.at(Bottom).get();
}

const ast::Argument* Lattice::getTop() const {
    return operators.at(Top).get();
}

void Lattice::print(std::ostream& os) const {
    os << ".lattice " << getQualifiedName() << " {\n   ";
    bool first = true;
    for (const auto& [op, arg] : operators) {
        if (!first) {
            os << ",\n   ";
        }
        os << latticeOperatorToString(op) << " -> " << *arg;
        first = false;
    }
    os << "\n}";
}

bool Lattice::equal(const Node& node) const {
    const auto& other = asAssert<Lattice>(node);
    return getQualifiedName() == other.getQualifiedName() && equal_targets(operators, other.operators);
}

Lattice* Lattice::cloning() const {
    return new Lattice(getQualifiedName(), clone(operators), getSrcLoc());
}

bool Lattice::classof(const Node* n) {
    return n->getKind() == NK_Lattice;
}

}  // namespace souffle::ast
