/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstLiteral.h
 *
 * Define classes for Literals and its subclasses atoms, negated atoms,
 * and binary relations.
 *
 ***********************************************************************/

#pragma once

#include "AstArgument.h"
#include "AstNode.h"
#include "AstRelationIdentifier.h"
#include "BinaryConstraintOps.h"
#include "Util.h"

#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace souffle {

class AstRelation;
class AstClause;
class AstProgram;
class AstAtom;

/**
 * Intermediate representation of atoms, binary relations,
 * and negated atoms in the body and head of a clause.
 */
class AstLiteral : public AstNode {
public:
    AstLiteral() {}

    virtual ~AstLiteral() {}

    /** Obtains the atom referenced by this literal - if any */
    virtual const AstAtom* getAtom() const = 0;

    /** Creates a clone if this AST sub-structure */
    virtual AstLiteral* clone() const = 0;
};

/**
 * Subclass of Literal that represents the use of a relation
 * either in the head or in the body of a Clause, e.g., parent(x,y).
 * The arguments of the atom can be variables or constants.
 */
class AstAtom : public AstLiteral {
protected:
    /** Name of the atom */
    AstRelationIdentifier name;

    /** Arguments of the atom */
    std::vector<std::unique_ptr<AstArgument>> arguments;

public:
    AstAtom(const AstRelationIdentifier& name = AstRelationIdentifier()) : name(name) {}

    virtual ~AstAtom() {}

    /** Return the name of this atom */
    const AstRelationIdentifier& getName() const {
        return name;
    }

    /** Return the arity of the atom */
    size_t getArity() const {
        return arguments.size();
    }

    /** Set atom name */
    void setName(const AstRelationIdentifier& n) {
        name = n;
    }

    /** Returns this class as the referenced atom */
    const AstAtom* getAtom() const {
        return this;
    }

    /** Add argument to the atom */
    void addArgument(std::unique_ptr<AstArgument> arg) {
        arguments.push_back(std::move(arg));
    }

    /** Return the i-th argument of the atom */
    AstArgument* getArgument(size_t idx) const {
        return arguments[idx].get();
    }

    /** Replace the argument at the given index with the given argument */
    void setArgument(size_t idx, std::unique_ptr<AstArgument> newArg) {
        arguments[idx].swap(newArg);
    }

    /** Provides access to the list of arguments of this atom */
    std::vector<AstArgument*> getArguments() const {
        return toPtrVector(arguments);
    }

    /** Return the number of arguments */
    size_t argSize() const {
        return arguments.size();
    }

    /** Output to a given stream */
    virtual void print(std::ostream& os) const {
        os << getName() << "(";

        for (size_t i = 0; i < arguments.size(); ++i) {
            if (i != 0) os << ",";

            if (arguments[i] != NULL)
                arguments[i]->print(os);
            else
                os << "_";
        }
        os << ")";
    }

    /** Creates a clone if this AST sub-structure */
    virtual AstAtom* clone() const {
        auto res = new AstAtom(name);
        res->setSrcLoc(getSrcLoc());
        for (const auto& cur : arguments) {
            res->arguments.push_back(std::unique_ptr<AstArgument>(cur->clone()));
        }
        return res;
    }

    /** Mutates this node */
    virtual void apply(const AstNodeMapper& map) {
        for (auto& arg : arguments) {
            arg = map(std::move(arg));
        }
    }

    /** Obtains a list of all embedded child nodes */
    virtual std::vector<const AstNode*> getChildNodes() const {
        std::vector<const AstNode*> res;
        for (auto& cur : arguments) {
            res.push_back(cur.get());
        }
        return res;
    }

protected:
    /** Implements the node comparison for this node type */
    virtual bool equal(const AstNode& node) const {
        assert(dynamic_cast<const AstAtom*>(&node));
        const AstAtom& other = static_cast<const AstAtom&>(node);
        return name == other.name && equal_targets(arguments, other.arguments);
    }
};

/**
 * Subclass of Literal that represents a negated atom, * e.g., !parent(x,y).
 * A Negated atom occurs in a body of clause and cannot occur in a head of a clause.
 */
class AstNegation : public AstLiteral {
protected:
    /** A pointer to the negated Atom */
    std::unique_ptr<AstAtom> atom;

public:
    AstNegation(std::unique_ptr<AstAtom> a) : atom(std::move(a)) {}

    virtual ~AstNegation() {}

    /** Returns the nested atom as the referenced atom */
    const AstAtom* getAtom() const {
        return atom.get();
    }

    /** Return the negated atom */
    AstAtom* getAtom() {
        return atom.get();
    }

    /** Output to a given stream */
    virtual void print(std::ostream& os) const {
        os << "!";
        atom->print(os);
    }

    /** Creates a clone if this AST sub-structure */
    virtual AstNegation* clone() const {
        AstNegation* res = new AstNegation(std::unique_ptr<AstAtom>(atom->clone()));
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** Mutates this node */
    virtual void apply(const AstNodeMapper& map) {
        atom = map(std::move(atom));
    }

    /** Obtains a list of all embedded child nodes */
    virtual std::vector<const AstNode*> getChildNodes() const {
        return {atom.get()};
    }

protected:
    /** Implements the node comparison for this node type */
    virtual bool equal(const AstNode& node) const {
        assert(dynamic_cast<const AstNegation*>(&node));
        const AstNegation& other = static_cast<const AstNegation&>(node);
        return *atom == *other.atom;
    }
};

/**
 * Subclass of Literal that represents a binary constraint
 * e.g., x = y.
 */
class AstConstraint : public AstLiteral {
protected:
    /** The operator in this relation */
    BinaryRelOp operation;

    /** Left-hand side argument of a binary operation */
    std::unique_ptr<AstArgument> lhs;

    /** Right-hand side argument of a binary operation */
    std::unique_ptr<AstArgument> rhs;

public:
    AstConstraint(BinaryRelOp o, std::unique_ptr<AstArgument> ls, std::unique_ptr<AstArgument> rs)
            : operation(o), lhs(std::move(ls)), rhs(std::move(rs)) {}

    AstConstraint(const std::string& op, std::unique_ptr<AstArgument> ls, std::unique_ptr<AstArgument> rs)
            : operation(getBinaryRelOpForSymbol(op)), lhs(std::move(ls)), rhs(std::move(rs)) {}

    virtual ~AstConstraint() {}

    /** This kind of literal has no nested atom */
    const AstAtom* getAtom() const {
        return nullptr;
    }

    /** Return LHS argument */
    AstArgument* getLHS() const {
        return lhs.get();
    }

    /** Return RHS argument */
    AstArgument* getRHS() const {
        return rhs.get();
    }

    /** Return binary operator */
    BinaryRelOp getOperator() const {
        return operation;
    }

    /** Update the binary operator */
    void setOperator(BinaryRelOp op) {
        operation = op;
    }

    /** Negates the constraint */
    void negate() {
        setOperator(souffle::negate(operation));
    }

    /** Output the constraint to a given stream */
    virtual void print(std::ostream& os) const {
        lhs->print(os);
        os << " " << getSymbolForBinaryRelOp(operation) << " ";
        rhs->print(os);
    }

    /** Creates a clone if this AST sub-structure */
    virtual AstConstraint* clone() const {
        AstConstraint* res = new AstConstraint(operation, std::unique_ptr<AstArgument>(lhs->clone()),
                std::unique_ptr<AstArgument>(rhs->clone()));
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** Mutates this node */
    virtual void apply(const AstNodeMapper& map) {
        lhs = map(std::move(lhs));
        rhs = map(std::move(rhs));
    }

    /** Obtains a list of all embedded child nodes */
    virtual std::vector<const AstNode*> getChildNodes() const {
        return {lhs.get(), rhs.get()};
    }

protected:
    /** Implements the node comparison for this node type */
    virtual bool equal(const AstNode& node) const {
        assert(dynamic_cast<const AstConstraint*>(&node));
        const AstConstraint& other = static_cast<const AstConstraint&>(node);
        return operation == other.operation && *lhs == *other.lhs && *rhs == *other.rhs;
    }
};

}  // end of namespace souffle
