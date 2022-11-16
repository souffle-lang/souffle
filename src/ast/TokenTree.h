/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2022, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */
#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace souffle::ast {

/// Delimiters of a delimited token tree
enum class Delimiter {
    None = 0,
    Brace,
    Bracket,
    Paren,
};

/// Kind of tokens
enum class TokenKind {
    None = 0,

    At,
    AtNot,
    Caret,
    Colon,
    Comma,
    Dollar,
    Dot,
    DoubleColon,
    Eq,
    Exclamation,
    Float,
    Ge,
    Gt,
    Ident,
    If,
    LBrace,
    LBracket,
    LParen,
    Le,
    Lt,
    MapsTo,
    Minus,
    Ne,
    Number,
    Percent,
    Pipe,
    Plus,
    RBrace,
    RBracket,
    RParen,
    Semicolon,
    Slash,
    Star,
    Symbol,
    Subtype,
    Underscore,
    Unsigned,

    Eof
};

/// A token
struct Token {
    TokenKind kind;
    std::string text;
    bool operator==(const Token& other) const {
        return kind == other.kind && text == other.text;
    }
};

struct Delimited;
struct Single;

/// Either a single token or a delimited token stream
using TokenTree = std::variant<Single, Delimited>;

/// A stream of tokens
using TokenStream = std::vector<TokenTree>;

/// A single token
struct Single {
    Token token;
    bool operator==(const Single& other) const {
        return token == other.token;
    }
};

/// A delimited token stream
struct Delimited {
    Delimiter delim;
    TokenStream tokens;
    bool operator==(const Delimited& other) const {
        return delim == other.delim && tokens == other.tokens;
    }
};

/// Build and return a token tree with a single token
TokenTree makeTokenTree(TokenKind kind, std::string_view text);

/// Build and return a token stream with a single token
TokenStream makeTokenStream(TokenKind kind, std::string_view text);

/// Build and return a delimited token tree from a token stream
TokenTree makeTokenTree(Delimiter delim, TokenStream ts);

/// Build and return a delimited token tree from a single token tree
TokenTree makeTokenTree(Delimiter delim, TokenTree tt);

/// Return the opening token for the given delimiter class.
Token openToken(Delimiter delim);

/// Return the closing token for the given delimiter class.
Token closeToken(Delimiter delim);

/// Print the tokens stream to the output stream.
void printTokenStream(std::ostream& os, const TokenStream& ts);

}  // namespace souffle::ast
