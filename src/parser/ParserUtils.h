/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ParserUtils.h
 *
 * Defines a rewrite class for multi-heads and disjunction
 *
 ***********************************************************************/

#pragma once

#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/Constraint.h"
#include "ast/Literal.h"
#include "souffle/utility/MiscUtil.h"
#include <iosfwd>
#include <utility>
#include <vector>

namespace souffle {

class RuleBody {
public:
    RuleBody negated() const;

    void conjunct(RuleBody other);

    void disjunct(RuleBody other);

    VecOwn<ast::Clause> toClauseBodies() const;

    // -- factory functions --

    static RuleBody getTrue();

    static RuleBody getFalse();

    static RuleBody atom(Own<ast::Atom> atom);

    static RuleBody constraint(Own<ast::Constraint> constraint);

    friend std::ostream& operator<<(std::ostream& out, const RuleBody& body);

private:
    // a struct to represent literals
    struct literal {
        literal(bool negated, Own<ast::Literal> atom) : negated(negated), atom(std::move(atom)) {}

        // whether this literal is negated or not
        bool negated;

        // the atom referenced by tis literal
        Own<ast::Literal> atom;

        literal cloneImpl() const {
            return literal(negated, clone(atom));
        }
    };

    using clause = std::vector<literal>;

    std::vector<clause> dnf;

    static bool equal(const literal& a, const literal& b);

    static bool equal(const clause& a, const clause& b);

    static bool isSubsetOf(const clause& a, const clause& b);

    static void insert(clause& cl, literal&& lit);

    static void insert(std::vector<clause>& cnf, clause&& cls);
};

/**
 * Name unnamed variables in an atom
 */
Own<ast::Atom> nameUnnamedVariables(Own<ast::Atom> atom);

namespace parser {

template <typename K>
struct Ok {
    Ok(const K& k) : value(k) {}
    Ok(K&& k) : value(std::forward<K>(k)) {}
    std::decay_t<K> value;
};

template <typename E>
struct Err {
    Err(const E& e) : value(e) {}
    Err(E&& e) : value(std::forward<E>(e)) {}
    std::decay_t<E> value;
};

template <typename T, typename E>
struct Result {
    template <typename K>
    Result(Ok<K>&& ok) : value(std::in_place_type<T>, std::forward<K>(ok.value)) {}

    template <typename X>
    Result(Err<X>&& err) : value(std::in_place_type<E>, std::forward<X>(err.value)) {}

    T& result() {
        return std::get<T>(value);
    }
    const T& result() const {
        return std::get<T>(value);
    }
    E& error() {
        return std::get<E>(value);
    }
    const E& error() const {
        return std::get<E>(value);
    }

    operator bool() const {
        return std::holds_alternative<T>(value);
    }

    std::variant<T, E> value;
};

template <typename T>
using PResult = Result<T, std::string>;

class Parser {
public:
    virtual ~Parser() = default;

    /// Return a fresh parser over the given token stream
    static std::unique_ptr<Parser> make(const ast::TokenStream& ts);

    /// Parse a qualified name
    virtual PResult<ast::QualifiedName> parseQualifiedName() = 0;

    /// Return the current token without advancing
    virtual ast::Token token() const = 0;

    /// Return true if the current token is of the given type otherwise return
    /// false.
    virtual bool check(ast::TokenKind) = 0;

    /// If the current token is of the given type then advance to the next
    /// token and return true.  Otherwise return false.
    virtual bool eat(ast::TokenKind) = 0;

    /// Advance to next token
    virtual void bump() = 0;
};

}  // namespace parser

}  // namespace souffle
