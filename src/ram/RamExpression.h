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
#include "RamTypes.h"
#include "ram/RamNode.h"
#include "utility/ContainerUtil.h"
#include "utility/StreamUtil.h"
#include <cassert>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 * @class RamExpression
 * @brief Abstract class for describing scalar values in RAM
 */
class RamExpression : public RamNode {
public:
    RamExpression* clone() const override = 0;
};

/**
 * @class RamAbstractOperator
 * @brief Abstract class for an operator/functor
 */
class RamAbstractOperator : public RamExpression {
public:
    explicit RamAbstractOperator(std::vector<std::unique_ptr<RamExpression>> args)
            : arguments(std::move(args)) {
        for (auto const& arg : arguments) {
            assert(arg != nullptr && "argument is null-pointer");
        }
    }

    /** @brief Get argument values */
    std::vector<RamExpression*> getArguments() const {
        return toPtrVector(arguments);
    }

    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        for (const auto& cur : arguments) {
            res.push_back(cur.get());
        }
        return res;
    }

    void apply(const RamNodeMapper& map) override {
        for (auto& arg : arguments) {
            arg = map(std::move(arg));
        }
    }

protected:
    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamAbstractOperator&>(node);
        return equal_targets(arguments, other.arguments);
    }

    /** Arguments of user defined operator */
    std::vector<std::unique_ptr<RamExpression>> arguments;
};

/**
 * @class RamIntrinsicOperator
 * @brief Operator that represents an intrinsic (built-in) functor
 */
class RamIntrinsicOperator : public RamAbstractOperator {
public:
    template <typename... Args>
    RamIntrinsicOperator(FunctorOp op, Args... args)
            : RamAbstractOperator({std::move(args)...}), operation(op) {}

    RamIntrinsicOperator(FunctorOp op, std::vector<std::unique_ptr<RamExpression>> args)
            : RamAbstractOperator(std::move(args)), operation(op) {}

    /** @brief Get operator symbol */
    FunctorOp getOperator() const {
        return operation;
    }

    RamIntrinsicOperator* clone() const override {
        std::vector<std::unique_ptr<RamExpression>> argsCopy;
        for (auto& arg : arguments) {
            argsCopy.emplace_back(arg->clone());
        }
        return new RamIntrinsicOperator(operation, std::move(argsCopy));
    }

protected:
    void print(std::ostream& os) const override {
        if (isInfixFunctorOp(operation)) {
            os << "(" << join(arguments, tfm::format("%s", operation)) << ")";
        } else {
            os << operation << "(" << join(arguments) << ")";
        }
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamIntrinsicOperator&>(node);
        return RamAbstractOperator::equal(node) && operation == other.operation;
    }

    /** Operation symbol */
    const FunctorOp operation;
};

/**
 * @class RamUserDefinedOperator
 * @brief Operator that represents an extrinsic (user-defined) functor
 */
class RamUserDefinedOperator : public RamAbstractOperator {
public:
    RamUserDefinedOperator(std::string n, std::vector<TypeAttribute> argsTypes, TypeAttribute returnType,
            std::vector<std::unique_ptr<RamExpression>> args)
            : RamAbstractOperator(std::move(args)), name(std::move(n)), argsTypes(std::move(argsTypes)),
              returnType(returnType) {
        assert(argsTypes.size() == args.size());
    }

    /** @brief Get operator name */
    const std::string& getName() const {
        return name;
    }

    /** @brief Get types of arguments */
    const std::vector<TypeAttribute>& getArgsTypes() const {
        return argsTypes;
    }

    /** @brief Get return type */
    TypeAttribute getReturnType() const {
        return returnType;
    }

    RamUserDefinedOperator* clone() const override {
        auto* res = new RamUserDefinedOperator(name, argsTypes, returnType, {});
        for (auto& cur : arguments) {
            RamExpression* arg = cur->clone();
            res->arguments.emplace_back(arg);
        }
        return res;
    }

protected:
    void print(std::ostream& os) const override {
        os << "@" << name << "_" << argsTypes << "(";
        os << join(arguments, ",",
                [](std::ostream& out, const std::unique_ptr<RamExpression>& arg) { out << *arg; });
        os << ")";
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamUserDefinedOperator&>(node);
        return RamAbstractOperator::equal(node) && name == other.name && argsTypes == other.argsTypes &&
               returnType == other.returnType;
    }

    /** Name of user-defined operator */
    const std::string name;

    /** Argument types */
    const std::vector<TypeAttribute> argsTypes;

    const TypeAttribute returnType;
};

/**
 * @class RamTupleElement
 * @brief Access element from the current tuple in a tuple environment
 *
 * In the following example, the tuple element t0.1 is accessed:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * IF t0.1 in A
 * 	...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamTupleElement : public RamExpression {
public:
    RamTupleElement(size_t ident, size_t elem) : identifier(ident), element(elem) {}
    /** @brief Get identifier */
    int getTupleId() const {
        return identifier;
    }

    /** @brief Get element */
    size_t getElement() const {
        return element;
    }

    RamTupleElement* clone() const override {
        return new RamTupleElement(identifier, element);
    }

protected:
    void print(std::ostream& os) const override {
        os << "t" << identifier << "." << element;
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamTupleElement&>(node);
        return identifier == other.identifier && element == other.element;
    }

    /** Identifier for the tuple */
    const size_t identifier;

    /** Element number */
    const size_t element;
};

/**
 * @class RamConstant
 * @brief Represents a Ram Constant
 *
 */
class RamConstant : public RamExpression {
public:
    /** @brief Get constant */
    RamDomain getConstant() const {
        return constant;
    }

protected:
    explicit RamConstant(RamDomain constant) : constant(constant) {}

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamConstant&>(node);
        return constant == other.constant;
    }

    /** Constant value */
    const RamDomain constant;
};

/**
 * @class RamSignedConstant
 * @brief Represents a signed constant
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * number(5)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamSignedConstant : public RamConstant {
public:
    explicit RamSignedConstant(RamDomain val) : RamConstant(val) {}

    /** @brief Get value of the constant. */
    RamDomain getValue() const {
        return constant;
    }

    /** Create clone */
    RamSignedConstant* clone() const override {
        return new RamSignedConstant(getValue());
    }

protected:
    void print(std::ostream& os) const override {
        os << "number(" << constant << ")";
    }
};

/**
 * @class RamUnsignedConstant
 * @brief Represents a unsigned constant
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * unsigned(5)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamUnsignedConstant : public RamConstant {
public:
    explicit RamUnsignedConstant(RamUnsigned val) : RamConstant(ramBitCast(val)) {}

    /** @brief Get value of the constant. */
    RamUnsigned getValue() const {
        return ramBitCast<RamUnsigned>(constant);
    }

    /** Create clone */
    RamUnsignedConstant* clone() const override {
        return new RamUnsignedConstant(getValue());
    }

protected:
    void print(std::ostream& os) const override {
        os << "unsigned(" << getValue() << ")";
    }
};

/**
 * @class RamFloatConstant
 * @brief Represents a float constant
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * float(3.3)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamFloatConstant : public RamConstant {
public:
    explicit RamFloatConstant(RamFloat val) : RamConstant(ramBitCast(val)) {}

    /** @brief Get value of the constant. */
    RamFloat getValue() const {
        return ramBitCast<RamFloat>(constant);
    }

    /** Create clone */
    RamFloatConstant* clone() const override {
        return new RamFloatConstant(getValue());
    }

protected:
    void print(std::ostream& os) const override {
        os << "float(" << getValue() << ")";
    }
};

/**
 * @class RamAutoIncrement
 * @brief Increment a counter and return its value.
 *
 * Note that there exists a single counter only.
 */
class RamAutoIncrement : public RamExpression {
public:
    RamAutoIncrement* clone() const override {
        return new RamAutoIncrement();
    }

protected:
    void print(std::ostream& os) const override {
        os << "autoinc()";
    }
};

/**
 * @class RamUndefValue
 * @brief An undefined expression
 *
 * Output is ⊥
 */
class RamUndefValue : public RamExpression {
public:
    RamUndefValue* clone() const override {
        return new RamUndefValue();
    }

protected:
    void print(std::ostream& os) const override {
        os << "⊥";
    }
};

/**
 * @class RamPackRecord
 * @brief Packs a record's arguments into a reference
 */
class RamPackRecord : public RamExpression {
public:
    RamPackRecord(std::vector<std::unique_ptr<RamExpression>> args) : arguments(std::move(args)) {
        for (const auto& arg : arguments) {
            assert(arg != nullptr && "argument is a null-pointer");
        }
    }

    /** @brief Get record arguments */
    std::vector<RamExpression*> getArguments() const {
        return toPtrVector(arguments);
    }

    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        for (const auto& cur : arguments) {
            res.push_back(cur.get());
        }
        return res;
    }

    RamPackRecord* clone() const override {
        auto* res = new RamPackRecord({});
        for (auto& cur : arguments) {
            res->arguments.emplace_back(cur->clone());
        }
        return res;
    }

    void apply(const RamNodeMapper& map) override {
        for (auto& arg : arguments) {
            arg = map(std::move(arg));
        }
    }

protected:
    void print(std::ostream& os) const override {
        os << "[" << join(arguments, ",", [](std::ostream& out, const std::unique_ptr<RamExpression>& arg) {
            out << *arg;
        }) << "]";
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamPackRecord&>(node);
        return equal_targets(arguments, other.arguments);
    }

    /** Arguments */
    std::vector<std::unique_ptr<RamExpression>> arguments;
};

/**
 * @class RamSubroutineArgument
 * @brief Access argument of a subroutine
 *
 * Arguments are number from zero 0 to n-1
 * where n is the number of arguments of the
 * subroutine.
 */
class RamSubroutineArgument : public RamExpression {
public:
    RamSubroutineArgument(size_t number) : number(number) {}

    /** @brief Get argument */
    size_t getArgument() const {
        return number;
    }

    RamSubroutineArgument* clone() const override {
        return new RamSubroutineArgument(number);
    }

protected:
    void print(std::ostream& os) const override {
        os << "argument(" << number << ")";
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamSubroutineArgument&>(node);
        return number == other.number;
    }

    /** Argument number */
    const size_t number;
};

}  // end of namespace souffle
