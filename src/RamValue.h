/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamValue.h
 *
 * Defines a class for evaluating values in the Relational Algebra Machine
 *
 ************************************************************************/

#pragma once

#include "BinaryFunctorOps.h"
#include "RamNode.h"
#include "RamRelation.h"
#include "SymbolTable.h"
#include "TernaryFunctorOps.h"
#include "UnaryFunctorOps.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <string>

#include <stdlib.h>

namespace souffle {

/**
 * Abstract class for describing scalar values in RAM
 */
// TODO (#541): Remove isConstant() and make an analysis for RAM
class RamValue : public RamNode {
protected:
    bool cnst;

public:
    RamValue(RamNodeType type, bool isCnst) : RamNode(type), cnst(isCnst) {}

    /** get level of value (which for-loop of a query) */
    virtual size_t getLevel() const = 0;

    /** Determines whether this value is a constant or not */
    bool isConstant() const {
        return cnst;
    }

    /** Create clone */
    RamValue* clone() const override = 0;
};

/**
 * Unary function
 */
// TODO (#541): have a single n-ary function
class RamUnaryOperator : public RamValue {
private:
    /** Operation symbol */
    UnaryOp operation;

    /** Argument of unary function */
    std::unique_ptr<RamValue> argument;

public:
    RamUnaryOperator(UnaryOp op, std::unique_ptr<RamValue> v)
            : RamValue(RN_UnaryOperator, v->isConstant()), operation(op), argument(std::move(v)) {}

    /** Print */
    void print(std::ostream& os) const override {
        os << getSymbolForUnaryOp(operation) << "(";
        argument->print(os);
        os << ")";
    }

    /** Get Argument */
    // TODO (#541): rename to getArgument()
    const RamValue* getValue() const {
        return argument.get();
    }
    const RamValue& getArgument() const {
        return *argument;
    }

    /** Get operator */
    UnaryOp getOperator() const {
        return operation;
    }

    /** Get level */
    // TODO (#541): move to an analysis
    size_t getLevel() const override {
        return argument->getLevel();
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return toVector<const RamNode*>(argument.get());
    }

    /** Create clone */
    RamUnaryOperator* clone() const override {
        RamUnaryOperator* res = new RamUnaryOperator(operation, std::unique_ptr<RamValue>(argument->clone()));
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        argument = map(std::move(argument));
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamUnaryOperator*>(&node));
        const RamUnaryOperator& other = static_cast<const RamUnaryOperator&>(node);
        return getOperator() == other.getOperator() && getArgument() == other.getArgument();
    }
};

/**
 * Binary function
 */
// TODO (#541): have a single n-ary function
class RamBinaryOperator : public RamValue {
private:
    /** Operation symbol */
    BinaryOp operation;

    /** Left-hand side argument */
    std::unique_ptr<RamValue> lhsArgument;

    /** Right-hand side argument */
    std::unique_ptr<RamValue> rhsArgument;

public:
    RamBinaryOperator(BinaryOp op, std::unique_ptr<RamValue> l, std::unique_ptr<RamValue> r)
            : RamValue(RN_BinaryOperator, l->isConstant() && r->isConstant()), operation(op),
              lhsArgument(std::move(l)), rhsArgument(std::move(r)) {}

    /** Print */
    void print(std::ostream& os) const override {
        if (operation < BinaryOp::MAX) {
            // print infix notation
            os << "(";
            lhsArgument->print(os);
            os << getSymbolForBinaryOp(operation);
            rhsArgument->print(os);
            os << ")";
        } else {
            // print prefix notation
            os << getSymbolForBinaryOp(operation);
            os << "(";
            lhsArgument->print(os);
            os << ",";
            rhsArgument->print(os);
            os << ")";
        }
    }

    /** Get left-handside argument */
    // remove def below
    const RamValue* getLHS() const {
        return lhsArgument.get();
    }
    const RamValue& getLHSArgument() const {
        ASSERT(lhsArgument);
        return *lhsArgument;
    }

    /** Get right-handside argument */
    // remove def below
    const RamValue* getRHS() const {
        return rhsArgument.get();
    }
    const RamValue& getRHSArgument() const {
        ASSERT(rhsArgument);
        return *rhsArgument;
    }

    /** Get operator symbol */
    BinaryOp getOperator() const {
        return operation;
    }

    /** Get level */
    // TODO (#541): move to an analysis
    size_t getLevel() const override {
        return std::max(lhsArgument->getLevel(), rhsArgument->getLevel());
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return toVector<const RamNode*>(lhsArgument.get(), rhsArgument.get());
    }

    /** Create clone */
    RamBinaryOperator* clone() const override {
        RamBinaryOperator* res =
                new RamBinaryOperator(operation, std::unique_ptr<RamValue>(lhsArgument->clone()),
                        std::unique_ptr<RamValue>(rhsArgument->clone()));
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        lhsArgument = map(std::move(lhsArgument));
        rhsArgument = map(std::move(rhsArgument));
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamBinaryOperator*>(&node));
        const RamBinaryOperator& other = static_cast<const RamBinaryOperator&>(node);
        return getOperator() == other.getOperator() && getLHSArgument() == other.getLHSArgument() &&
               getRHSArgument() == other.getRHSArgument();
    }
};

/**
 * Ternary Function
 */
// TODO (#541): have a single n-ary function
class RamTernaryOperator : public RamValue {
private:
    /** Operation symbol */
    TernaryOp operation;

    /** Arguments */
    std::array<std::unique_ptr<RamValue>, 3> arguments;

public:
    RamTernaryOperator(TernaryOp op, std::unique_ptr<RamValue> a0, std::unique_ptr<RamValue> a1,
            std::unique_ptr<RamValue> a2)
            : RamValue(RN_TernaryOperator, a0->isConstant() && a1->isConstant() && a2->isConstant()),
              operation(op), arguments({{std::move(a0), std::move(a1), std::move(a2)}}) {}

    /** Print */
    void print(std::ostream& os) const override {
        os << getSymbolForTernaryOp(operation);
        os << "(";
        arguments[0]->print(os);
        os << ",";
        arguments[1]->print(os);
        os << ",";
        arguments[2]->print(os);
        os << ")";
    }

    /** Get argument */
    // TODO (#541): Remove old def
    const RamValue* getArg(int i) const {
        return arguments[i].get();
    }
    const RamValue& getArgument(int i) const {
        ASSERT(arguments[i]);
        return *arguments[i];
    }

    /** Get operation symbol */
    TernaryOp getOperator() const {
        return operation;
    }

    /** Get level */
    // TODO (#541): move to analysis
    size_t getLevel() const override {
        return std::max(
                std::max(arguments[0]->getLevel(), arguments[1]->getLevel()), arguments[2]->getLevel());
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return toVector<const RamNode*>(arguments[0].get(), arguments[1].get(), arguments[2].get());
    }

    /** Create clone */
    RamTernaryOperator* clone() const override {
        RamTernaryOperator* res =
                new RamTernaryOperator(operation, std::unique_ptr<RamValue>(arguments[0]->clone()),
                        std::unique_ptr<RamValue>(arguments[1]->clone()),
                        std::unique_ptr<RamValue>(arguments[2]->clone()));
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        for (int i = 0; i < 3; i++) {
            arguments[i] = map(std::move(arguments[i]));
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamTernaryOperator*>(&node));
        const RamTernaryOperator& other = static_cast<const RamTernaryOperator&>(node);
        return getOperator() == other.getOperator() && getArgument(0) == other.getArgument(0) &&
               getArgument(1) == other.getArgument(1) && getArgument(2) == other.getArgument(2);
    }
};

/**
 * Access element from the current tuple in a tuple environment
 */
// TODO (#541): add reference to attributes of a relation
class RamElementAccess : public RamValue {
private:
    /** Level information */
    // TODO (#541): move to analysis
    size_t level;

    /** Element number */
    size_t element;

    /** Name of attribute */
    std::string name;

public:
    RamElementAccess(size_t l, size_t e, const std::string& n = "")
            : RamValue(RN_ElementAccess, false), level(l), element(e), name(n) {}

    /** Print */
    void print(std::ostream& os) const override {
        if (name == "") {
            os << "env(t" << level << ", i" << element << ")";
        } else {
            os << "t" << level << "." << name;
        }
    }

    /** Get level */
    size_t getLevel() const override {
        return level;
    }

    /** Get element */
    size_t getElement() const {
        return element;
    }

    /** Get name */
    const std::string& getName() const {
        return name;
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>();  // no child nodes
    }

    /** Create clone */
    RamElementAccess* clone() const override {
        RamElementAccess* res = new RamElementAccess(level, element, name);
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {}

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamElementAccess*>(&node));
        const RamElementAccess& other = static_cast<const RamElementAccess&>(node);
        return getLevel() == other.getLevel() && getElement() == other.getElement() &&
               getName() == other.getName();
    }
};

/**
 * Number Constant
 */
class RamNumber : public RamValue {
    /** Constant value */
    RamDomain constant;

public:
    RamNumber(RamDomain c) : RamValue(RN_Number, true), constant(c) {}

    /** Get constant */
    // TODO (#541):  move to analysis
    RamDomain getConstant() const {
        return constant;
    }

    /** Print */
    void print(std::ostream& os) const override {
        os << "number(" << constant << ")";
    }

    /** Get level */
    // TODO (#541): move to analysis
    size_t getLevel() const override {
        return 0;
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>();  // no child nodes
    }

    /** Create clone */
    RamNumber* clone() const override {
        RamNumber* res = new RamNumber(constant);
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {}

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamNumber*>(&node));
        const RamNumber& other = static_cast<const RamNumber&>(node);
        return getConstant() == other.getConstant();
    }
};

/**
 * Counter
 *
 * Increment a counter and return its value. Note that
 * there exists a single counter only.
 */
class RamAutoIncrement : public RamValue {
public:
    RamAutoIncrement() : RamValue(RN_AutoIncrement, false) {}

    /** Print */
    void print(std::ostream& os) const override {
        os << "autoinc()";
    }

    /** Get level */
    // TODO (#541): move to analysis
    size_t getLevel() const override {
        return 0;
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>();  // no child nodes
    }

    /** Create clone */
    RamAutoIncrement* clone() const override {
        RamAutoIncrement* res = new RamAutoIncrement();
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {}

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamAutoIncrement*>(&node));
        return true;
    }
};

/**
 * Record pack operation
 */
class RamPack : public RamValue {
private:
    /** Arguments */
    // TODO (#541): use type for vector-ram-value
    std::vector<std::unique_ptr<RamValue>> arguments;

public:
    RamPack(std::vector<std::unique_ptr<RamValue>> args)
            : RamValue(RN_Pack,
                      all_of(args, [](const std::unique_ptr<RamValue>& a) { return a && a->isConstant(); })),
              arguments(std::move(args)) {}

    /** Get values */
    // TODO (#541): remove getter
    std::vector<RamValue*> getValues() const {
        return toPtrVector(arguments);
    }
    std::vector<RamValue*> getArguments() const {
        return toPtrVector(arguments);
    }

    /** Print */
    void print(std::ostream& os) const override {
        os << "[" << join(arguments, ",", [](std::ostream& out, const std::unique_ptr<RamValue>& arg) {
            if (arg) {
                out << *arg;
            } else {
                out << "_";
            }
        }) << "]";
    }

    /** Get level */
    // TODO (#541): move to analysis
    size_t getLevel() const override {
        size_t level = 0;
        for (const auto& arg : arguments) {
            if (arg) {
                level = std::max(level, arg->getLevel());
            }
        }
        return level;
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        for (const auto& cur : arguments) {
            if (cur) {
                res.push_back(cur.get());
            }
        }
        return res;
    }

    /** Create clone */
    RamPack* clone() const override {
        RamPack* res = new RamPack({});
        for (auto& cur : arguments) {
            RamValue* arg = nullptr;
            if (cur != nullptr) {
                arg = cur->clone();
            }
            res->arguments.push_back(std::unique_ptr<RamValue>(arg));
        }
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        for (auto& arg : arguments) {
            if (arg != nullptr) {
                arg = map(std::move(arg));
            }
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamPack*>(&node));
        const RamPack& other = static_cast<const RamPack&>(node);
        return equal_targets(arguments, other.arguments);
    }
};

/**
 * Access argument of a subroutine
 *
 * Arguments are number from zero 0 to n-1
 * where n is the number of arguments of the
 * subroutine.
 */
class RamArgument : public RamValue {
    /** Argument number */
    size_t number;

public:
    RamArgument(size_t number) : RamValue(RN_Argument, false), number(number) {}

    /** Get argument number */
    size_t getArgNumber() const {
        return number;
    }

    /** Print */
    void print(std::ostream& os) const override {
        os << "argument(" << number << ")";
    }

    /** Get level */
    // TODO (#541): move to an analysis
    size_t getLevel() const override {
        return 0;
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>();
    }

    /** Create clone */
    RamArgument* clone() const override {
        RamArgument* res = new RamArgument(getArgNumber());
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {}

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamArgument*>(&node));
        const RamArgument& other = static_cast<const RamArgument&>(node);
        return getArgNumber() == other.getArgNumber();
    }
};

}  // end of namespace souffle
