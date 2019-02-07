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

#include "FunctorOps.h"
#include "RamNode.h"
#include "RamRelation.h"
#include "SymbolTable.h"

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
class RamValue : public RamNode {
public:
    RamValue(RamNodeType type) : RamNode(type) {}

    /** Create clone */
    RamValue* clone() const override = 0;
};

// TODO (azreika): create a common abstract base class for RAM operators

/**
 * Operator that represents an intrinsic (built-in) functor
 */
class RamIntrinsicOperator : public RamValue {
private:
    /** Operation symbol */
    FunctorOp operation;

    /** Arguments of the function */
    std::vector<std::unique_ptr<RamValue>> arguments;

public:
    template <typename... Args>
    RamIntrinsicOperator(FunctorOp op, Args... args) : RamValue(RN_IntrinsicOperator), operation(op) {
        std::unique_ptr<RamValue> tmp[] = {std::move(args)...};
        for (auto& cur : tmp) {
            arguments.push_back(std::move(cur));
        }
    }

    RamIntrinsicOperator(FunctorOp op, std::vector<std::unique_ptr<RamValue>> args)
            : RamValue(RN_IntrinsicOperator), operation(op), arguments(std::move(args)) {}

    /** Print */
    void print(std::ostream& os) const override {
        if (isInfixFunctorOp(operation)) {
            os << "(";
            os << join(arguments, getSymbolForFunctorOp(operation), print_deref<std::unique_ptr<RamValue>>());
            os << ")";
        } else {
            os << getSymbolForFunctorOp(operation);
            os << "(";
            os << join(arguments, ",", print_deref<std::unique_ptr<RamValue>>());
            os << ")";
        }
    }

    FunctorOp getOperator() const {
        return operation;
    }

    /** Get values */
    std::vector<RamValue*> getArguments() const {
        return toPtrVector(arguments);
    }

    const RamValue* getArgument(size_t i) const {
        assert(i >= 0 && i < arguments.size() && "argument index out of bounds");
        return arguments[i].get();
    }

    size_t getArgCount() const {
        return arguments.size();
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        for (const auto& cur : arguments) {
            res.push_back(cur.get());
        }
        return res;
    }

    RamIntrinsicOperator* clone() const override {
        std::vector<std::unique_ptr<RamValue>> argsCopy;
        for (auto& arg : arguments) {
            argsCopy.push_back(std::unique_ptr<RamValue>(arg->clone()));
        }
        auto res = new RamIntrinsicOperator(operation, std::move(argsCopy));
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        for (auto& arg : arguments) {
            arg = map(std::move(arg));
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamIntrinsicOperator*>(&node));
        const auto& other = static_cast<const RamIntrinsicOperator&>(node);
        return getOperator() == other.getOperator() && equal_targets(arguments, other.arguments);
    }
};

/**
 * Operator that represents an extrinsic (user-defined) functor
 */
class RamUserDefinedOperator : public RamValue {
private:
    /** Argument of unary function */
    std::vector<std::unique_ptr<RamValue>> arguments;
    /** Name of user-defined unary functor */
    const std::string name;

    /** Argument type */
    const std::string type;

public:
    RamUserDefinedOperator(std::string n, std::string t, std::vector<std::unique_ptr<RamValue>> args)
            : RamValue(RN_UserDefinedOperator), arguments(std::move(args)), name(std::move(n)),
              type(std::move(t)) {}

    /** Print */
    void print(std::ostream& os) const override {
        os << "@" << name << "_" << type << "(";
        os << join(
                arguments, ",", [](std::ostream& out, const std::unique_ptr<RamValue>& arg) { out << *arg; });
        os << ")";
    }

    /** Get values */
    std::vector<RamValue*> getArguments() const {
        return toPtrVector(arguments);
    }

    const RamValue* getArgument(size_t i) const {
        assert(i >= 0 && i < arguments.size() && "argument index out of bounds");
        return arguments[i].get();
    }

    size_t getArgCount() const {
        return arguments.size();
    }

    const std::string& getName() const {
        return name;
    }

    const std::string& getType() const {
        return type;
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        for (const auto& cur : arguments) {
            res.push_back(cur.get());
        }
        return res;
    }

    /** Create clone */
    RamUserDefinedOperator* clone() const override {
        RamUserDefinedOperator* res = new RamUserDefinedOperator(name, type, {});
        for (auto& cur : arguments) {
            RamValue* arg = cur->clone();
            res->arguments.push_back(std::unique_ptr<RamValue>(arg));
        }
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        for (auto& arg : arguments) {
            arg = map(std::move(arg));
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamUserDefinedOperator*>(&node));
        const auto& other = static_cast<const RamUserDefinedOperator&>(node);
        return name == other.name && type == other.type && equal_targets(arguments, other.arguments);
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
    RamElementAccess(size_t l, size_t e, std::string n = "")
            : RamValue(RN_ElementAccess), level(l), element(e), name(std::move(n)) {}

    /** Print */
    void print(std::ostream& os) const override {
        if (name.empty()) {
            os << "env(t" << level << ", i" << element << ")";
        } else {
            os << "t" << level << "." << name;
        }
    }

    /** Get level */
    size_t getLevel() const {
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
        assert(nullptr != dynamic_cast<const RamElementAccess*>(&node));
        const auto& other = static_cast<const RamElementAccess&>(node);
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
    RamNumber(RamDomain c) : RamValue(RN_Number), constant(c) {}

    /** Get constant */
    // TODO (#541):  move to analysis
    RamDomain getConstant() const {
        return constant;
    }

    /** Print */
    void print(std::ostream& os) const override {
        os << "number(" << constant << ")";
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>();  // no child nodes
    }

    /** Create clone */
    RamNumber* clone() const override {
        auto* res = new RamNumber(constant);
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {}

protected:
    /** Check equality */
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
class RamAutoIncrement : public RamValue {
public:
    RamAutoIncrement() : RamValue(RN_AutoIncrement) {}

    /** Print */
    void print(std::ostream& os) const override {
        os << "autoinc()";
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>();  // no child nodes
    }

    /** Create clone */
    RamAutoIncrement* clone() const override {
        auto* res = new RamAutoIncrement();
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {}

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamAutoIncrement*>(&node));
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
    RamPack(std::vector<std::unique_ptr<RamValue>> args) : RamValue(RN_Pack), arguments(std::move(args)) {}

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
        assert(nullptr != dynamic_cast<const RamPack*>(&node));
        const auto& other = static_cast<const RamPack&>(node);
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
    RamArgument(size_t number) : RamValue(RN_Argument), number(number) {}

    /** Get argument number */
    size_t getArgCount() const {
        return number;
    }

    /** Print */
    void print(std::ostream& os) const override {
        os << "argument(" << number << ")";
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return std::vector<const RamNode*>();
    }

    /** Create clone */
    RamArgument* clone() const override {
        auto* res = new RamArgument(getArgCount());
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {}

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamArgument*>(&node));
        const auto& other = static_cast<const RamArgument&>(node);
        return getArgCount() == other.getArgCount();
    }
};

}  // end of namespace souffle
