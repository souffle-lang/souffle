/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ParserUtils.cpp
 *
 * Defines class RuleBody to represents rule bodies.
 *
 ***********************************************************************/

#include "parser/ParserUtils.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/Constraint.h"
#include "ast/Literal.h"
#include "ast/Negation.h"
#include "ast/Node.h"
#include "ast/UnnamedVariable.h"
#include "ast/Variable.h"
#include "ast/utility/Utils.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <algorithm>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

namespace souffle {

RuleBody RuleBody::negated() const {
    RuleBody res = getTrue();

    for (const clause& cur : dnf) {
        RuleBody step;
        for (const literal& lit : cur) {
            step.dnf.push_back(clause());
            step.dnf.back().emplace_back(literal{!lit.negated, clone(lit.atom)});
        }

        res.conjunct(std::move(step));
    }

    return res;
}

void RuleBody::conjunct(RuleBody other) {
    // avoid making clones if possible
    if (dnf.size() == 1 && other.dnf.size() == 1) {
        for (auto&& rhs : other.dnf[0]) {
            insert(dnf[0], std::move(rhs));
        }

        return;
    }

    // compute the product of the disjunctions
    std::vector<clause> res;

    for (const auto& clauseA : dnf) {
        for (const auto& clauseB : other.dnf) {
            clause cur;

            for (const auto& lit : clauseA) {
                cur.emplace_back(lit.cloneImpl());
            }
            for (const auto& lit : clauseB) {
                insert(cur, lit.cloneImpl());
            }

            insert(res, std::move(cur));
        }
    }

    dnf = std::move(res);
}

void RuleBody::disjunct(RuleBody other) {
    // append the clauses of the other body to this body
    for (auto& cur : other.dnf) {
        insert(dnf, std::move(cur));
    }
}

VecOwn<ast::Clause> RuleBody::toClauseBodies() const {
    // collect clause results
    VecOwn<ast::Clause> bodies;
    for (const clause& cur : dnf) {
        bodies.push_back(mk<ast::Clause>(ast::QualifiedName::fromString("*")));
        ast::Clause& clause = *bodies.back();

        for (const literal& lit : cur) {
            // extract literal
            auto base = clone(lit.atom);
            // negate if necessary
            if (lit.negated) {
                // negate
                if (auto* atom = as<ast::Atom>(*base)) {
                    base.release();
                    base = mk<ast::Negation>(Own<ast::Atom>(atom), atom->getSrcLoc());
                } else if (auto* cstr = as<ast::Constraint>(*base)) {
                    negateConstraintInPlace(*cstr);
                }
            }

            // add to result
            clause.addToBody(std::move(base));
        }
    }

    // done
    return bodies;
}

// -- factory functions --

RuleBody RuleBody::getTrue() {
    RuleBody body;
    body.dnf.push_back(clause());
    return body;
}

RuleBody RuleBody::getFalse() {
    return RuleBody();
}

RuleBody RuleBody::atom(Own<ast::Atom> atom) {
    RuleBody body;
    body.dnf.push_back(clause());
    body.dnf.back().emplace_back(literal{false, std::move(atom)});
    return body;
}

RuleBody RuleBody::constraint(Own<ast::Constraint> constraint) {
    RuleBody body;
    body.dnf.push_back(clause());
    body.dnf.back().emplace_back(literal{false, std::move(constraint)});
    return body;
}

std::ostream& operator<<(std::ostream& out, const RuleBody& body) {
    return out << join(body.dnf, ";", [](std::ostream& out, const RuleBody::clause& cur) {
        out << join(cur, ",", [](std::ostream& out, const RuleBody::literal& l) {
            if (l.negated) {
                out << "!";
            }
            out << *l.atom;
        });
    });
}

bool RuleBody::equal(const literal& a, const literal& b) {
    return a.negated == b.negated && *a.atom == *b.atom;
}

bool RuleBody::equal(const clause& a, const clause& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (const auto& i : a) {
        bool found = false;
        for (const auto& j : b) {
            if (equal(i, j)) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

bool RuleBody::isSubsetOf(const clause& a, const clause& b) {
    if (a.size() > b.size()) {
        return false;
    }
    for (const auto& i : a) {
        bool found = false;
        for (const auto& j : b) {
            if (equal(i, j)) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

void RuleBody::insert(clause& cl, literal&& lit) {
    for (const auto& cur : cl) {
        if (equal(cur, lit)) {
            return;
        }
    }
    cl.emplace_back(std::move(lit));
}

void RuleBody::insert(std::vector<clause>& cnf, clause&& cls) {
    for (const auto& cur : cnf) {
        if (isSubsetOf(cur, cls)) {
            return;
        }
    }
    std::vector<clause> res;
    for (auto& cur : cnf) {
        if (!isSubsetOf(cls, cur)) {
            res.push_back(std::move(cur));
        }
    }
    res.swap(cnf);
    cnf.push_back(std::move(cls));
}

Own<ast::Atom> nameUnnamedVariables(Own<ast::Atom> atom) {
    assert(atom != nullptr && "Atom is a null-pointer");
    auto const& args = atom->getArguments();
    auto arity = atom->getArity();
    VecOwn<ast::Argument> newArgs;
    for (std::size_t i = 0; i < arity; i++) {
        if (isA<ast::UnnamedVariable>(args[i])) {
            std::stringstream varName;
            varName << "@var" << i;
            newArgs.push_back(mk<ast::Variable>(varName.str(), args[i]->getSrcLoc()));
        } else {
            newArgs.push_back(clone(args[i]));
        }
    }
    return mk<ast::Atom>(atom->getQualifiedName(), std::move(newArgs), atom->getSrcLoc());
}

namespace parser {

namespace {

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
// explicit deduction guide (not needed as of C++20)
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

struct Cursor {
    explicit Cursor(const ast::TokenStream& ts) : stream(ts), index(0) {}
    Cursor(const Cursor& other) = default;
    Cursor(Cursor&& other) = default;
    Cursor& operator=(const Cursor& other) = default;
    Cursor& operator=(Cursor&& other) = default;

    /// Return the next token tree and advance.
    std::optional<std::reference_wrapper<const ast::TokenTree>> maybeNext() {
        if (index < stream.get().size()) {
            return stream.get()[index++];
        }
        return std::nullopt;
    }

private:
    std::reference_wrapper<const ast::TokenStream> stream;
    std::size_t index;
};

struct TokenCursor {
    using Frame = std::pair<ast::Delimiter, Cursor>;

    TokenCursor(const ast::TokenStream& ts) : frame{ast::Delimiter::None, Cursor(ts)} {}

    ast::Token next() {
        while (true) {
            auto optTT = frame.second.maybeNext();
            if (optTT) {
                const ast::TokenTree& tt = optTT->get();

                if (std::holds_alternative<ast::Single>(tt)) {
                    return std::get<ast::Single>(tt).token;
                }

                if (std::holds_alternative<ast::Delimited>(tt)) {
                    const ast::Delimited& d = std::get<ast::Delimited>(tt);
                    const ast::Delimiter delim = d.delim;
                    Frame newframe = std::make_pair(delim, Cursor(d.tokens));
                    this->frames.emplace(std::move(this->frame));
                    std::swap(newframe, this->frame);
                    if (delim != ast::Delimiter::None) {
                        return openToken(delim);
                    }
                }
            } else if (!frames.empty()) {
                const ast::Delimiter delim = this->frame.first;
                std::swap(this->frame, this->frames.top());
                this->frames.pop();

                if (delim != ast::Delimiter::None) {
                    return closeToken(delim);
                }
            } else {
                ast::Token t;
                t.kind = ast::TokenKind::Eof;
                t.text = "";
                return t;
            }
        }
    }

private:
    /// The top frame
    Frame frame;

    /// The remaining frames
    std::stack<Frame> frames;
};

class ParserImpl : public Parser {
public:
    ParserImpl(const ast::TokenStream& ts) : cursor(ts) {
        bump();
    }

    PResult<ast::QualifiedName> parseQualifiedName() override {
        if (check(ast::TokenKind::Ident)) {
            ast::QualifiedName qn = ast::QualifiedName::fromString(curToken.text);
            bump();
            while (eat(ast::TokenKind::Dot)) {
                if (check(ast::TokenKind::Ident)) {
                    qn.append(curToken.text);
                    bump();
                } else {
                    return Err(std::string("expected identifier"));
                }
            }
            return Ok(qn);
        }

        return Err(std::string("expected identifier"));
    }

    ast::Token token() const override {
        return curToken;
    }

    bool check(const ast::TokenKind kind) override {
        const bool result = (curToken.kind == kind);
        if (!result) {
            expectedTokens.emplace_back(kind);
        }
        return result;
    }

    bool eat(const ast::TokenKind kind) override {
        if (check(kind)) {
            bump();
            return true;
        } else {
            return false;
        }
    }

    void bump() override {
        curToken = cursor.next();
        expectedTokens.clear();
    }

private:
    /// Current token.
    ast::Token curToken;

    /// Position in the token stream.
    TokenCursor cursor;

    /// Tokens that were expected but not found.
    std::vector<ast::TokenKind> expectedTokens;
};

}  // namespace

std::unique_ptr<Parser> Parser::make(const ast::TokenStream& ts) {
    return std::make_unique<ParserImpl>(ts);
}
}  // namespace parser

}  // end of namespace souffle
