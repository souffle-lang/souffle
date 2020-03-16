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

#include "AstAbstract.h"
#include "AstNode.h"
#include "AstQualifiedName.h"
#include "BinaryConstraintOps.h"
#include "Util.h"

#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "AstAbstract.h"
#include "AstNode.h"

namespace souffle {

class AstRelation;
class AstClause;
class AstProgram;
class AstAtom;

/**
 * Subclass of Literal that represents the use of a relation
 * either in the head or in the body of a Clause, e.g., parent(x,y).
 * The arguments of the atom can be variables or constants.
 */
class AstAtom : public AstLiteral {
public:
    AstAtom(AstQualifiedName name = AstQualifiedName()) : name(std::move(name)) {}

    AstAtom(AstQualifiedName name, std::vector<std::unique_ptr<AstArgument>> args, SrcLocation srcLoc)
            : name(std::move(name)), arguments(std::move(args)) {
        setSrcLoc(srcLoc);
    }

    /** get qualified name */
    const AstQualifiedName& getQualifiedName() const {
        return name;
    }

    /** get arity of the atom */
    size_t getArity() const {
        return arguments.size();
    }

    /** set qualified name */
    void setQualifiedName(const AstQualifiedName& n) {
        name = n;
    }

    /** add argument to the atom */
    void addArgument(std::unique_ptr<AstArgument> arg) {
        arguments.push_back(std::move(arg));
    }

    /** get arguments */
    std::vector<AstArgument*> getArguments() const {
        return toPtrVector(arguments);
    }

    AstAtom* clone() const override {
        auto res = new AstAtom(name);
        res->setSrcLoc(getSrcLoc());
        for (const auto& cur : arguments) {
            res->arguments.emplace_back(cur->clone());
        }
        return res;
    }

    void apply(const AstNodeMapper& map) override {
        for (auto& arg : arguments) {
            arg = map(std::move(arg));
        }
    }

    std::vector<const AstNode*> getChildNodes() const override {
        std::vector<const AstNode*> res;
        for (auto& cur : arguments) {
            res.push_back(cur.get());
        }
        return res;
    }

protected:
    void print(std::ostream& os) const override {
        os << getQualifiedName() << "(";
        os << join(arguments, ",", print_deref<std::unique_ptr<AstArgument>>());
        os << ")";
    }

    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstAtom*>(&node));
        const auto& other = static_cast<const AstAtom&>(node);
        return name == other.name && equal_targets(arguments, other.arguments);
    }

    /** name */
    AstQualifiedName name;

    /** arguments */
    std::vector<std::unique_ptr<AstArgument>> arguments;
};

/**
 * Subclass of Literal that represents a negated atom, * e.g., !parent(x,y).
 * A Negated atom occurs in a body of clause and cannot occur in a head of a clause.
 */
class AstNegation : public AstLiteral {
public:
    AstNegation(std::unique_ptr<AstAtom> atom) : atom(std::move(atom)) {}

    /** get negated atom */
    AstAtom* getAtom() const {
        return atom.get();
    }

    AstNegation* clone() const override {
        auto* res = new AstNegation(std::unique_ptr<AstAtom>(atom->clone()));
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    void apply(const AstNodeMapper& map) override {
        atom = map(std::move(atom));
    }

    std::vector<const AstNode*> getChildNodes() const override {
        return {atom.get()};
    }

protected:
    void print(std::ostream& os) const override {
        os << "!" << *atom;
    }

    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstNegation*>(&node));
        const auto& other = static_cast<const AstNegation&>(node);
        return equal_ptr(atom, other.atom);
    }

    /** negated atom */
    std::unique_ptr<AstAtom> atom;
};

/**
 * Subclass of Literal that represents a negated atom, * e.g., !parent(x,y).
 * A Negated atom occurs in a body of clause and cannot occur in a head of a clause.
 *
 * Specialised for provenance: used for existence check that tuple doesn't already exist
 */
class AstProvenanceNegation : public AstNegation {
public:
    AstProvenanceNegation(std::unique_ptr<AstAtom> atom) : AstNegation(std::move(atom)) {}

    AstProvenanceNegation* clone() const override {
        auto* res = new AstProvenanceNegation(std::unique_ptr<AstAtom>(atom->clone()));
        res->setSrcLoc(getSrcLoc());
        return res;
    }

protected:
    void print(std::ostream& os) const override {
        os << "prov!" << *atom;
    }
};

/**
 * Boolean Constraint
 *
 * Representing either 'true' or 'false' values
 */
class AstBooleanConstraint : public AstConstraint {
public:
    AstBooleanConstraint(bool truthValue) : truthValue(truthValue) {}

    /** check whether constraint holds */
    bool isTrue() const {
        return truthValue;
    }

    /** set truth value */
    void set(bool value) {
        truthValue = value;
    }

    AstBooleanConstraint* clone() const override {
        auto* res = new AstBooleanConstraint(truthValue);
        res->setSrcLoc(getSrcLoc());
        return res;
    }

protected:
    void print(std::ostream& os) const override {
        os << (truthValue ? "true" : "false");
    }

    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstBooleanConstraint*>(&node));
        const auto& other = static_cast<const AstBooleanConstraint&>(node);
        return truthValue == other.truthValue;
    }

    /** truth value */
    bool truthValue;
};

/**
 * Subclass of Constraint that represents a binary constraint
 * e.g., x = y.
 */
class AstBinaryConstraint : public AstConstraint {
public:
    AstBinaryConstraint(
            BinaryConstraintOp o, std::unique_ptr<AstArgument> ls, std::unique_ptr<AstArgument> rs)
            : operation(o), lhs(std::move(ls)), rhs(std::move(rs)) {}

    /** get LHS argument */
    AstArgument* getLHS() const {
        return lhs.get();
    }

    /** get RHS argument */
    AstArgument* getRHS() const {
        return rhs.get();
    }

    /** get binary operator */
    BinaryConstraintOp getOperator() const {
        return operation;
    }

    /** set binary operator */
    void setOperator(BinaryConstraintOp op) {
        operation = op;
    }

    AstBinaryConstraint* clone() const override {
        auto* res = new AstBinaryConstraint(operation, std::unique_ptr<AstArgument>(lhs->clone()),
                std::unique_ptr<AstArgument>(rhs->clone()));
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    void apply(const AstNodeMapper& map) override {
        lhs = map(std::move(lhs));
        rhs = map(std::move(rhs));
    }

    std::vector<const AstNode*> getChildNodes() const override {
        return {lhs.get(), rhs.get()};
    }

protected:
    void print(std::ostream& os) const override {
        os << *lhs;
        os << " " << toBinaryConstraintSymbol(operation) << " ";
        os << *rhs;
    }

    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstBinaryConstraint*>(&node));
        const auto& other = static_cast<const AstBinaryConstraint&>(node);
        return operation == other.operation && equal_ptr(lhs, other.lhs) && equal_ptr(rhs, other.rhs);
    }

    /** constraint operator */
    BinaryConstraintOp operation;

    /** left-hand side of binary constraint */
    std::unique_ptr<AstArgument> lhs;

    /** right-hand side of binary constraint */
    std::unique_ptr<AstArgument> rhs;
};

}  // end of namespace souffle
