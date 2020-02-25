/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstArgument.h
 *
 * Define the classes Argument, Variable, and Constant to represent
 * variables and constants in literals. Variable and Constant are
 * sub-classes of class argument.
 *
 ***********************************************************************/

#pragma once

#include "AstAbstract.h"
#include "AstNode.h"
#include "AstType.h"
#include "FunctorOps.h"
#include "SymbolTable.h"
#include "Util.h"
#include <cassert>
#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 * Named Variable
 */
class AstVariable : public AstArgument {
public:
    AstVariable(std::string name) : name(std::move(name)) {}

    void print(std::ostream& os) const override {
        os << name;
    }

    /** set variable name */
    void setName(const std::string& name) {
        this->name = name;
    }

    /** @return variable name */
    const std::string& getName() const {
        return name;
    }

    AstVariable* clone() const override {
        auto* res = new AstVariable(name);
        res->setSrcLoc(getSrcLoc());
        return res;
    }

protected:
    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstVariable*>(&node));
        const auto& other = static_cast<const AstVariable&>(node);
        return name == other.name;
    }

    /** variable name */
    std::string name;
};

/**
 * Unnamed Variable
 */
class AstUnnamedVariable : public AstArgument {
public:
    void print(std::ostream& os) const override {
        os << "_";
    }

    AstUnnamedVariable* clone() const override {
        auto* res = new AstUnnamedVariable();
        res->setSrcLoc(getSrcLoc());
        return res;
    }
};

/**
 * Counter
 */
class AstCounter : public AstArgument {
public:
    void print(std::ostream& os) const override {
        os << "$";
    }

    AstCounter* clone() const override {
        auto* res = new AstCounter();
        res->setSrcLoc(getSrcLoc());
        return res;
    }
};

/**
 * Abstract Constant
 */
class AstConstant : public AstArgument {
public:
    AstConstant* clone() const override = 0;
};

/**
 * String Constant
 */
class AstStringConstant : public AstConstant {
public:
    explicit AstStringConstant(std::string value) : value(std::move(value)) {}

    void print(std::ostream& os) const override {
        os << "\"" << value << "\"";
    }

    /** @return String representation of this Constant */
    const std::string& getValue() const {
        return value;
    }

    AstStringConstant* clone() const override {
        auto* res = new AstStringConstant(value);
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    bool operator==(const AstStringConstant& other) const {
        return getValue() == other.getValue();
    }

protected:
    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstStringConstant*>(&node));
        const auto& other = static_cast<const AstStringConstant&>(node);
        return getValue() == other.getValue();
    }

private:
    const std::string value;
};

/**
 * Numeric Constant
 */
template <typename NumericType>  // NumericType ⲉ {RamSigned, RamUnsigned, RamFloat}
class AstNumericConstant : public AstConstant {
public:
    explicit AstNumericConstant(NumericType value) : value(value) {}

    void print(std::ostream& os) const override {
        os << value;
    }

    /** Get the value of the constant. */
    NumericType getValue() const {
        return value;
    }

    AstNumericConstant<NumericType>* clone() const override {
        auto* copy = new AstNumericConstant<NumericType>(value);
        copy->setSrcLoc(getSrcLoc());
        return copy;
    }

    bool operator==(const AstNumericConstant<NumericType>& other) const {
        return getValue() == other.getValue();
    }

protected:
    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstNumericConstant<NumericType>*>(&node));
        const auto& other = static_cast<const AstNumericConstant<NumericType>&>(node);
        return value == other.value;
    }

private:
    const NumericType value;
};

// This definitions are used by AstVisitor.
using AstNumberConstant = AstNumericConstant<RamSigned>;
using AstFloatConstant = AstNumericConstant<RamFloat>;
using AstUnsignedConstant = AstNumericConstant<RamUnsigned>;

/**
 * Nil Constant
 */
class AstNilConstant : public AstConstant {
public:
    AstNilConstant() = default;

    void print(std::ostream& os) const override {
        os << "nil";
    }

    AstNilConstant* clone() const override {
        auto* res = new AstNilConstant();
        res->setSrcLoc(getSrcLoc());
        return res;
    }
};

/**
 * Abstract Term
 */
class AstTerm : public AstArgument {
protected:
    AstTerm() = default;
    AstTerm(std::vector<std::unique_ptr<AstArgument>> operands) : args(std::move(operands)){};

public:
    /** get arguments */
    std::vector<AstArgument*> getArguments() const {
        return toPtrVector(args);
    }

    /** add argument to argument list */
    void addArgument(std::unique_ptr<AstArgument> arg) {
        args.push_back(std::move(arg));
    }

    std::vector<const AstNode*> getChildNodes() const override {
        auto res = AstArgument::getChildNodes();
        for (auto& cur : args) {
            res.push_back(cur.get());
        }
        return res;
    }

    void apply(const AstNodeMapper& map) override {
        for (auto& arg : args) {
            arg = map(std::move(arg));
        }
    }

protected:
    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstTerm*>(&node));
        const auto& other = static_cast<const AstTerm&>(node);
        return equal_targets(args, other.args);
    }

    /** Arguments */
    std::vector<std::unique_ptr<AstArgument>> args;
};

/**
 * Functor class
 */

class AstFunctor : public AstTerm {
public:
    virtual TypeAttribute getReturnType() const = 0;
    virtual TypeAttribute getArgType(const size_t arg) const = 0;

protected:
    AstFunctor() = default;
    explicit AstFunctor(std::vector<std::unique_ptr<AstArgument>> operands) : AstTerm(std::move(operands)) {}
};

/**
 * Intrinsic Functor
 */
class AstIntrinsicFunctor : public AstFunctor {
public:
    template <typename... Operands>
    AstIntrinsicFunctor(FunctorOp function, Operands... operands) : function(function) {
        std::unique_ptr<AstArgument> tmp[] = {std::move(operands)...};
        for (auto& cur : tmp) {
            addArgument(std::move(cur));
        }
        assert(isValidFunctorOpArity(function, args.size()) && "invalid number of arguments for functor");
    }

    AstIntrinsicFunctor(FunctorOp function, std::vector<std::unique_ptr<AstArgument>> operands)
            : AstFunctor(std::move(operands)), function(function) {
        assert(isValidFunctorOpArity(function, args.size()) && "invalid number of arguments for functor");
    }

    void print(std::ostream& os) const override {
        if (isInfixFunctorOp(function)) {
            os << "(";
            os << join(args, getSymbolForFunctorOp(function), print_deref<std::unique_ptr<AstArgument>>());
            os << ")";
        } else {
            os << getSymbolForFunctorOp(function);
            os << "(";
            os << join(args, ",", print_deref<std::unique_ptr<AstArgument>>());
            os << ")";
        }
    }

    /** get function */
    FunctorOp getFunction() const {
        return function;
    }

    /** set function */
    void setFunction(const FunctorOp functor) {
        function = functor;
    }

    /** get the return type of the functor. */
    TypeAttribute getReturnType() const override {
        return functorReturnType(function);
    }

    /** get type of the functor argument*/
    TypeAttribute getArgType(const size_t arg) const override {
        return functorOpArgType(arg, function);
    }

    AstIntrinsicFunctor* clone() const override {
        std::vector<std::unique_ptr<AstArgument>> argsCopy;
        for (auto& arg : args) {
            argsCopy.emplace_back(arg->clone());
        }
        auto res = new AstIntrinsicFunctor(function, std::move(argsCopy));
        res->setSrcLoc(getSrcLoc());
        return res;
    }

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstIntrinsicFunctor*>(&node));
        const auto& other = static_cast<const AstIntrinsicFunctor&>(node);
        return function == other.function && AstFunctor::equal(node);
    }

    /** Function */
    FunctorOp function;
};

/**
 * User-Defined Functor
 */
class AstUserDefinedFunctor : public AstFunctor {
public:
    explicit AstUserDefinedFunctor(std::string name) : AstFunctor(), name(std::move(name)){};
    AstUserDefinedFunctor(std::string name, std::vector<std::unique_ptr<AstArgument>> args)
            : AstFunctor(std::move(args)), name(std::move(name)){};

    /** print user-defined functor */
    void print(std::ostream& os) const override {
        os << '@' << name << "(" << join(args, ",", print_deref<std::unique_ptr<AstArgument>>()) << ")";
    }

    /** get name */
    const std::string& getName() const {
        return name;
    }

    /** get type of the functor argument*/
    TypeAttribute getArgType(const size_t arg) const override {
        return argTypes.at(arg);
    }

    /** get type of the functor argument*/
    TypeAttribute getReturnType() const override {
        return returnType;
    }

    void setArgsTypes(std::vector<TypeAttribute> types) {
        assert(types.size() == args.size() && "Size of types must match size of arguments");
        argTypes = types;
    }

    const std::vector<TypeAttribute>& getArgsTypes() const {
        return argTypes;
    }

    void setReturnType(TypeAttribute type) {
        returnType = type;
    }

    AstUserDefinedFunctor* clone() const override {
        auto res = new AstUserDefinedFunctor(name);
        // Set args
        for (auto& arg : args) {
            res->args.emplace_back(arg->clone());
        }
        // Set types
        // Only copy types if they have already been set.
        if (!argTypes.empty()) {
            res->setArgsTypes(argTypes);
        }
        res->setReturnType(returnType);

        res->setSrcLoc(getSrcLoc());
        return res;
    }

protected:
    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstUserDefinedFunctor*>(&node));
        const auto& other = static_cast<const AstUserDefinedFunctor&>(node);
        return name == other.name && AstFunctor::equal(node);
    }

    std::vector<TypeAttribute> argTypes;
    TypeAttribute returnType;

    /** name of user-defined functor */
    const std::string name;
};

/**
 * Record
 */
class AstRecordInit : public AstTerm {
public:
    AstRecordInit(std::optional<AstQualifiedName> ty) : type(std::move(ty)) {}

    void print(std::ostream& os) const override {
        if (type.has_value()) {
            os << *type << " ";
        }

        os << "[" << join(args, ",", print_deref<std::unique_ptr<AstArgument>>()) << "]";
    }

    AstRecordInit* clone() const override {
        auto res = new AstRecordInit(type);
        for (auto& cur : args) {
            res->args.emplace_back(cur->clone());
        }
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** The type of the record in question, if specified.
     *  If not specified, we'll try to infer.
     */
    std::optional<AstQualifiedName> type;

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstRecordInit*>(&node));
        const auto& other = static_cast<const AstRecordInit&>(node);
        return equal_targets(args, other.args) && type == other.type;
    }
};

/**
 * An argument that takes a values and converts it into a new sum type branch.
 */
class AstSumInit : public AstArgument {
public:
    AstSumInit(AstQualifiedName ty, std::string branch, std::unique_ptr<AstArgument> arg)
            : type(std::move(ty)), branch(std::move(branch)), arg(std::move(arg)) {
        assert(this->arg);
    }

    const AstArgument* getArgument() const {
        return arg.get();
    }

    const std::string& getBranch() const {
        return branch;
    }

    void print(std::ostream& os) const override {
        os << "@" << type << " " << branch << "[" << *arg << "]";
    }

    /** Creates a clone of this AST sub-structure */
    AstSumInit* clone() const override {
        auto res = new AstSumInit(type, branch, std::unique_ptr<AstArgument>(arg->clone()));
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** Mutates this node */
    void apply(const AstNodeMapper& map) override {
        arg = map(std::move(arg));
    }

    /** Obtains a list of all embedded child nodes */
    std::vector<const AstNode*> getChildNodes() const override {
        auto res = AstArgument::getChildNodes();
        res.push_back(arg.get());
        return res;
    }

    /** The type of the record in question */
    AstQualifiedName type;

protected:
    /** The sum type branch name */
    std::string branch;

    /** The list of components to be aggregated into a record */
    std::unique_ptr<AstArgument> arg;

    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstSumInit*>(&node));
        const auto& other = static_cast<const AstSumInit&>(node);
        return type == other.type && branch == other.branch && *arg == *other.arg;
    }
};

/**
 * An argument capable of casting a value of one type into another.
 */
class AstTypeCast : public AstArgument {
public:
    AstTypeCast(std::unique_ptr<AstArgument> value, AstQualifiedName type)
            : value(std::move(value)), type(std::move(type)) {}

    void print(std::ostream& os) const override {
        os << "as(" << *value << "," << type << ")";
    }

    /** Get value */
    AstArgument* getValue() const {
        return value.get();
    }

    /** Get type */
    const AstQualifiedName& getType() const {
        return type;
    }

    /** Set type */
    void setType(const AstQualifiedName& type) {
        this->type = type;
    }

    std::vector<const AstNode*> getChildNodes() const override {
        auto res = AstArgument::getChildNodes();
        res.push_back(value.get());
        return res;
    }

    AstTypeCast* clone() const override {
        auto res = new AstTypeCast(std::unique_ptr<AstArgument>(value->clone()), type);
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    void apply(const AstNodeMapper& map) override {
        value = map(std::move(value));
    }

protected:
    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstTypeCast*>(&node));
        const auto& other = static_cast<const AstTypeCast&>(node);
        return type == other.type && equal_ptr(value, other.value);
    }

    /** The value to be casted */
    std::unique_ptr<AstArgument> value;

    /** The target type name */
    AstQualifiedName type;
};

/**
 * An argument aggregating a value from a sub-query.
 * TODO (b-scholz): fix body literal interface;
 * remove getters/setters for individual literals
 */
class AstAggregator : public AstArgument {
public:
    /**
     * The kind of utilised aggregation operator.
     * Note: lower-case is utilized due to a collision with
     *  constants in the parser.
     */
    enum Op { min, max, count, sum };

    /** Creates a new aggregation node */
    AstAggregator(Op fun) : fun(fun), expression(nullptr) {}

    void print(std::ostream& os) const override {
        switch (fun) {
            case sum:
                os << "sum";
                break;
            case min:
                os << "min";
                break;
            case max:
                os << "max";
                break;
            case count:
                os << "count";
                break;
            default:
                break;
        }
        if (expression) {
            os << " " << *expression;
        }
        os << " : ";
        if (body.size() > 1) {
            os << "{ ";
        }
        os << join(body, ", ", print_deref<std::unique_ptr<AstLiteral>>());
        if (body.size() > 1) {
            os << " }";
        }
    }

    /** Get aggregate operator */
    Op getOperator() const {
        return fun;
    }

    /** Set target expression */
    void setTargetExpression(std::unique_ptr<AstArgument> arg) {
        expression = std::move(arg);
    }

    /** Get target expression */
    const AstArgument* getTargetExpression() const {
        return expression.get();
    }

    /** Get body literals */
    std::vector<AstLiteral*> getBodyLiterals() const {
        return toPtrVector(body);
    }

    /** Clear body literals */
    void clearBodyLiterals() {
        body.clear();
    }

    /** Add body literal */
    void addBodyLiteral(std::unique_ptr<AstLiteral> lit) {
        body.push_back(std::move(lit));
    }

    std::vector<const AstNode*> getChildNodes() const override {
        auto res = AstArgument::getChildNodes();
        if (expression) {
            res.push_back(expression.get());
        }
        for (auto& cur : body) {
            res.push_back(cur.get());
        }
        return res;
    }

    AstAggregator* clone() const override {
        auto res = new AstAggregator(fun);
        res->expression = (expression) ? std::unique_ptr<AstArgument>(expression->clone()) : nullptr;
        for (const auto& cur : body) {
            res->body.emplace_back(cur->clone());
        }
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    void apply(const AstNodeMapper& map) override {
        if (expression) {
            expression = map(std::move(expression));
        }
        for (auto& cur : body) {
            cur = map(std::move(cur));
        }
    }

protected:
    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstAggregator*>(&node));
        const auto& other = static_cast<const AstAggregator&>(node);
        return fun == other.fun && equal_ptr(expression, other.expression) && equal_targets(body, other.body);
    }

private:
    /** The aggregation operator of this aggregation step */
    Op fun;

    /** The expression to be aggregated */
    std::unique_ptr<AstArgument> expression;

    /** A list of body-literals forming a sub-query which's result is projected and aggregated */
    std::vector<std::unique_ptr<AstLiteral>> body;
};

/**
 * Subroutine Argument
 */
class AstSubroutineArgument : public AstArgument {
public:
    AstSubroutineArgument(size_t index) : index(index) {}

    void print(std::ostream& os) const override {
        os << "arg_" << index;
    }

    /** Return argument index */
    size_t getNumber() const {
        return index;
    }

    AstSubroutineArgument* clone() const override {
        auto* res = new AstSubroutineArgument(index);
        res->setSrcLoc(getSrcLoc());
        return res;
    }

protected:
    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstSubroutineArgument*>(&node));
        const auto& other = static_cast<const AstSubroutineArgument&>(node);
        return index == other.index;
    }

private:
    /** Index of argument in argument list*/
    size_t index;
};

}  // end of namespace souffle
