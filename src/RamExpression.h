/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamExpression.h
 *
 * Defines a class for evaluating values in the Relational Algebra Machine
 *
 ************************************************************************/

#pragma once

#include "FunctorOps.h"
#include "RamNode.h"
#include "RamRelation.h"
#include "SymbolTable.h"
#include "Util.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <string>

#include <cstdlib>
#include <utility>

namespace souffle {

/**
 * Abstract class for describing scalar values in RAM
 */
class RamExpression : public RamNode {
public:
    RamExpression* clone() const override = 0;
};

// TODO (azreika): create a common abstract base class for RAM operators

/**
 * Operator that represents an intrinsic (built-in) functor
 */
class RamIntrinsicOperator : public RamExpression {
public:
    template <typename... Args>
    RamIntrinsicOperator(FunctorOp op, Args... args) : operation(op) {
        std::unique_ptr<RamExpression> tmp[] = {std::move(args)...};
        for (auto& cur : tmp) {
            arguments.push_back(std::move(cur));
        }
    }

    RamIntrinsicOperator(FunctorOp op, std::vector<std::unique_ptr<RamExpression>> args)
            : operation(op), arguments(std::move(args)) {}

    void print(std::ostream& os) const override {
        if (isInfixFunctorOp(operation)) {
            os << "(";
            os << join(arguments, getSymbolForFunctorOp(operation),
                    print_deref<std::unique_ptr<RamExpression>>());
            os << ")";
        } else {
            os << getSymbolForFunctorOp(operation);
            os << "(";
            os << join(arguments, ",", print_deref<std::unique_ptr<RamExpression>>());
            os << ")";
        }
    }

    /** Get operator symbol */
    FunctorOp getOperator() const {
        return operation;
    }

    /** Get argument values */
    std::vector<RamExpression*> getArguments() const {
        return toPtrVector(arguments);
    }

    /** Get i-th argument value */
    const RamExpression& getArgument(size_t i) const {
        assert(i >= 0 && i < arguments.size() && "argument index out of bounds");
        return *arguments[i];
    }

    /** Get number of arguments */
    size_t getArgCount() const {
        return arguments.size();
    }

    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        for (const auto& cur : arguments) {
            res.push_back(cur.get());
        }
        return res;
    }

    RamIntrinsicOperator* clone() const override {
        std::vector<std::unique_ptr<RamExpression>> argsCopy;
        for (auto& arg : arguments) {
            argsCopy.emplace_back(arg->clone());
        }
        return new RamIntrinsicOperator(operation, std::move(argsCopy));
    }

    void apply(const RamNodeMapper& map) override {
        for (auto& arg : arguments) {
            arg = map(std::move(arg));
        }
    }

protected:
    /** Operation symbol */
    const FunctorOp operation;

    /** Arguments of the function */
    std::vector<std::unique_ptr<RamExpression>> arguments;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamIntrinsicOperator*>(&node));
        const auto& other = static_cast<const RamIntrinsicOperator&>(node);
        return getOperator() == other.getOperator() && equal_targets(arguments, other.arguments);
    }
};

/**
 * Operator that represents an extrinsic (user-defined) functor
 */
class RamUserDefinedOperator : public RamExpression {
public:
    RamUserDefinedOperator(std::string n, std::string t, std::vector<std::unique_ptr<RamExpression>> args)
            : arguments(std::move(args)), name(std::move(n)), type(std::move(t)) {}

    void print(std::ostream& os) const override {
        os << "@" << name << "_" << type << "(";
        os << join(arguments, ",",
                [](std::ostream& out, const std::unique_ptr<RamExpression>& arg) { out << *arg; });
        os << ")";
    }

    /** Get argument values */
    std::vector<RamExpression*> getArguments() const {
        return toPtrVector(arguments);
    }

    /** Get i-th argument value */
    const RamExpression& getArgument(size_t i) const {
        assert(i >= 0 && i < arguments.size() && "argument index out of bounds");
        return *arguments[i];
    }

    /** Get number of arguments */
    size_t getArgCount() const {
        return arguments.size();
    }

    /** Get operator name */
    const std::string& getName() const {
        return name;
    }

    /** Get types of arguments */
    const std::string& getType() const {
        return type;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        for (const auto& cur : arguments) {
            res.push_back(cur.get());
        }
        return res;
    }

    RamUserDefinedOperator* clone() const override {
        auto* res = new RamUserDefinedOperator(name, type, {});
        for (auto& cur : arguments) {
            RamExpression* arg = cur->clone();
            res->arguments.emplace_back(arg);
        }
        return res;
    }

    void apply(const RamNodeMapper& map) override {
        for (auto& arg : arguments) {
            arg = map(std::move(arg));
        }
    }

protected:
    /** Arguments of user defined operator */
    std::vector<std::unique_ptr<RamExpression>> arguments;

    /** Name of user-defined operator */
    const std::string name;

    /** Argument types */
    const std::string type;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamUserDefinedOperator*>(&node));
        const auto& other = static_cast<const RamUserDefinedOperator&>(node);
        return name == other.name && type == other.type && equal_targets(arguments, other.arguments);
    }
};

/**
 * Access element from the current tuple in a tuple environment
 */
class RamElementAccess : public RamExpression {
public:
    RamElementAccess(size_t ident, size_t elem, std::unique_ptr<RamRelationReference> relRef = nullptr)
            : identifier(ident), element(elem) {}

    void print(std::ostream& os) const override {
        os << "t" << identifier << "." << element;
    }

    /** Get identifier */
    int getTupleId() const {
        return identifier;
    }

    /** Get element */
    size_t getElement() const {
        return element;
    }

    RamElementAccess* clone() const override {
        return new RamElementAccess(identifier, element);
    }

protected:
    /** Identifier for the tuple */
    const size_t identifier;

    /** Element number */
    const size_t element;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamElementAccess*>(&node));
        const auto& other = static_cast<const RamElementAccess&>(node);
        return getTupleId() == other.getTupleId() && getElement() == other.getElement();
    }
};

/**
 * Number Constant
 */
class RamNumber : public RamExpression {
public:
    RamNumber(RamDomain c) : constant(c) {}

    /** Get constant */
    RamDomain getConstant() const {
        return constant;
    }

    void print(std::ostream& os) const override {
        os << "number(" << constant << ")";
    }

    /** Create clone */
    RamNumber* clone() const override {
        return new RamNumber(constant);
    }

protected:
    /** Constant value */
    const RamDomain constant;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamNumber*>(&node));
        const auto& other = static_cast<const RamNumber&>(node);
        return getConstant() == other.getConstant();
    }
};

/**
 * Counter
 *
 * Increment a counter and return its value. Note that
 * there exists a single counter only.
 */
class RamAutoIncrement : public RamExpression {
public:
    RamAutoIncrement() = default;

    void print(std::ostream& os) const override {
        os << "autoinc()";
    }

    RamAutoIncrement* clone() const override {
        return new RamAutoIncrement();
    }
};

/**
 * Undefined Expression
 */
class RamUndefValue : public RamExpression {
public:
    RamUndefValue() = default;

    void print(std::ostream& os) const override {
        os << "⊥";
    }

    RamUndefValue* clone() const override {
        return new RamUndefValue();
    }
};

inline bool isRamUndefValue(const RamExpression* expr) {
    return nullptr != dynamic_cast<const RamUndefValue*>(expr);
}

/**
 * Record pack operation
 */
class RamPackRecord : public RamExpression {
public:
    RamPackRecord(std::vector<std::unique_ptr<RamExpression>> args) : arguments(std::move(args)) {}

    /** Get arguments */
    std::vector<RamExpression*> getArguments() const {
        return toPtrVector(arguments);
    }

    void print(std::ostream& os) const override {
        os << "[" << join(arguments, ",", [](std::ostream& out, const std::unique_ptr<RamExpression>& arg) {
            out << *arg;
        }) << "]";
    }

    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        for (const auto& cur : arguments) {
            if (cur) {
                res.push_back(cur.get());
            }
        }
        return res;
    }

    RamPackRecord* clone() const override {
        auto* res = new RamPackRecord({});
        for (auto& cur : arguments) {
            RamExpression* arg = nullptr;
            if (cur != nullptr) {
                arg = cur->clone();
            }
            res->arguments.emplace_back(arg);
        }
        return res;
    }

    void apply(const RamNodeMapper& map) override {
        for (auto& arg : arguments) {
            if (arg != nullptr) {
                arg = map(std::move(arg));
            }
        }
    }

protected:
    /** Arguments */
    std::vector<std::unique_ptr<RamExpression>> arguments;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamPackRecord*>(&node));
        const auto& other = static_cast<const RamPackRecord&>(node);
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
class RamArgument : public RamExpression {
public:
    RamArgument(size_t number) : number(number) {}

    /** Get argument */
    size_t getArgument() const {
        return number;
    }

    void print(std::ostream& os) const override {
        os << "argument(" << number << ")";
    }

    RamArgument* clone() const override {
        return new RamArgument(number);
    }

protected:
    /** Argument number */
    const size_t number;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamArgument*>(&node));
        const auto& other = static_cast<const RamArgument&>(node);
        return getArgument() == other.getArgument();
    }
};

}  // end of namespace souffle
