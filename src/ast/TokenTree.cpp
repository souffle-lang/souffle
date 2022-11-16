/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2022, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "TokenTree.h"

#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace souffle::ast {
TokenTree makeTokenTree(TokenKind kind, std::string_view text) {
    Single s;
    s.token.kind = kind;
    s.token.text = std::string(text);
    return s;
}

TokenStream makeTokenStream(TokenKind kind, std::string_view text) {
    TokenStream ts;
    ts.emplace_back(makeTokenTree(kind, text));
    return ts;
}

TokenTree makeTokenTree(Delimiter delim, TokenStream ts) {
    Delimited d;
    d.delim = delim;
    d.tokens = std::move(ts);
    return d;
}

TokenTree makeTokenTree(Delimiter delim, TokenTree tt) {
    Delimited d;
    d.delim = delim;
    d.tokens.emplace_back(std::move(tt));
    return d;
}

Token openToken(Delimiter delim) {
    ast::Token t;
    switch (delim) {
        case Delimiter::Paren:
            t.kind = ast::TokenKind::LParen;
            t.text = "(";
            break;
        case Delimiter::Brace:
            t.kind = ast::TokenKind::LBrace;
            t.text = "{";
            break;
        case Delimiter::Bracket:
            t.kind = ast::TokenKind::LBracket;
            t.text = "[";
            break;
        case Delimiter::None: throw std::out_of_range("No delimiter");
    }
    return t;
}

Token closeToken(Delimiter delim) {
    ast::Token t;
    switch (delim) {
        case Delimiter::Paren:
            t.kind = ast::TokenKind::RParen;
            t.text = ")";
            break;
        case Delimiter::Brace:
            t.kind = ast::TokenKind::RBrace;
            t.text = "}";
            break;
        case Delimiter::Bracket:
            t.kind = ast::TokenKind::RBracket;
            t.text = "]";
            break;
        case Delimiter::None: throw std::out_of_range("No delimiter");
    }
    return t;
}

void printTokenStream(std::ostream& os, const TokenStream& ts) {
    std::size_t count = ts.size();
    for (std::size_t i = 0; i < count; ++i) {
        if (i > 0) {
            os << " ";
        }
        const TokenTree& tt = ts[i];
        if (std::holds_alternative<Single>(tt)) {
            const auto& token = std::get<Single>(tt).token;
            if (token.kind == TokenKind::Symbol) {
                os << '\"' << token.text << '\"';
            } else {
                os << token.text;
            }
        } else if (std::holds_alternative<Delimited>(tt)) {
            const auto& delimited = std::get<Delimited>(tt);
            if (delimited.delim != Delimiter::None) {
                os << openToken(delimited.delim).text;
            }
            printTokenStream(os, delimited.tokens);
            if (delimited.delim != Delimiter::None) {
                os << closeToken(delimited.delim).text;
            }
        }
    }
}

}  // namespace souffle::ast
