/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2022, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */
#pragma once

#include "QualifiedName.h"
#include "TokenTree.h"
#include "parser/SrcLocation.h"

#include <list>

namespace souffle::ast {

class Annotation {
public:
    enum class Kind { Normal, DocComment };

    enum class Style { Outer, Inner };

    Annotation();
    Annotation(Kind kind, Style style, QualifiedName lbl, TokenStream ts, SrcLocation loc = {});
    Annotation(Annotation&&) = default;
    Annotation(const Annotation&) = default;
    Annotation& operator=(Annotation&&) = default;
    Annotation& operator=(const Annotation&) = default;

    bool operator==(const Annotation& other) const;

    Kind getKind() const;

    Style getStyle() const;

    const SrcLocation& getSrcLoc() const;

    const QualifiedName& getLabel() const;

    const TokenStream& getTokens() const;

    void print(std::ostream&) const;

    void printAsOuter(std::ostream&) const;

private:
    Kind kind;

    Style style;

    QualifiedName label;

    TokenStream tokens;

    SrcLocation location;
};

using AnnotationList = std::list<Annotation>;

}  // namespace souffle::ast
