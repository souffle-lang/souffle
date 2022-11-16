/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2022, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */
#include "Annotation.h"

#include <cassert>

namespace souffle::ast {

Annotation::Annotation() : kind(), style(), label(), tokens(), location() {}

Annotation::Annotation(Kind k, Style sty, QualifiedName lbl, TokenStream ts, SrcLocation loc)
        : kind(k), style(sty), label(lbl), tokens(ts), location(loc) {}

bool Annotation::operator==(const Annotation& other) const {
    return (kind == other.kind) && (style == other.style) && (label == other.label) &&
           (tokens == other.tokens);
}

void Annotation::print(std::ostream& os) const {
    os << "@";
    if (style == Style::Inner) {
        os << "!";
    }
    os << "[" << label;
    if (!tokens.empty()) {
        os << " ";
    }
    printTokenStream(os, tokens);
    os << "]";
}

void Annotation::printAsOuter(std::ostream& os) const {
    os << "@[" << label;
    if (!tokens.empty()) {
        os << " ";
    }
    printTokenStream(os, tokens);
    os << "]";
}

Annotation::Kind Annotation::getKind() const {
    return kind;
}

Annotation::Style Annotation::getStyle() const {
    return style;
}

const SrcLocation& Annotation::getSrcLoc() const {
    return location;
}

const QualifiedName& Annotation::getLabel() const {
    return label;
}

const TokenStream& Annotation::getTokens() const {
    return tokens;
}
}  // namespace souffle::ast
