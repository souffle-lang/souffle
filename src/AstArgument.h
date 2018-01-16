/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Argument.h
 *
 * Define the classes Argument, Variable, and Constant to represent
 * variables and constants in literals. Variable and Constant are
 * sub-classes of class argument.
 *
 ***********************************************************************/

#pragma once

#include "AstNode.h"
#include "AstType.h"
#include "BinaryFunctorOps.h"
#include "SymbolTable.h"
#include "TernaryFunctorOps.h"
#include "TypeSystem.h"
#include "UnaryFunctorOps.h"

#include <array>
#include <list>
#include <memory>
#include <string>

namespace souffle {

/* Forward declarations */
class AstClause;
class AstVariable;
class AstLiteral;

/**
 * Intermediate representation of an argument
 */
class AstArgument : public AstNode {
public:
    ~AstArgument() override = default;

    /** Obtains a list of all embedded child nodes */
    std::vector<const AstNode*> getChildNodes() const override {
        return std::vector<const AstNode*>();  // type is just cached, not essential
    }

    /** Create clone */
    AstArgument* clone() const override = 0;
};

/**
 * Subclass of Argument that represents a named variable
 */
class AstVariable : public AstArgument {
protected:
    /** Variable name */
    std::string name;

public:
    AstVariable(const std::string& n) : AstArgument(), name(n) {}

    /** Updates this variable name */
    void setName(const std::string& name) {
        this->name = name;
    }

    /** @return Variable name */
    const std::string& getName() const {
        return name;
    }

    /** Print argument to the given output stream */
    void print(std::ostream& os) const override {
        os << name;
    }

    /** Creates a clone if this AST sub-structure */
    AstVariable* clone() const override {
        AstVariable* res = new AstVariable(name);
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** Mutates this node */
    void apply(const AstNodeMapper& /*mapper*/) override {
        // no sub-nodes to consider
    }

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(dynamic_cast<const AstVariable*>(&node));
        const AstVariable& other = static_cast<const AstVariable&>(node);
        return name == other.name;
    }
};

/**
 * Subclass of Argument that represents an unnamed variable
 */
class AstUnnamedVariable : public AstArgument {
protected:
public:
    AstUnnamedVariable() : AstArgument() {}

    /** Print argument to the given output stream */
    void print(std::ostream& os) const override {
        os << "_";
    }

    /** Creates a clone if this AST sub-structure */
    AstUnnamedVariable* clone() const override {
        AstUnnamedVariable* res = new AstUnnamedVariable();
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** Mutates this node */
    void apply(const AstNodeMapper& /*mapper*/) override {
        // no sub-nodes to consider
    }

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(dynamic_cast<const AstUnnamedVariable*>(&node));
        return true;
    }
};

/**
 * Subclass of Argument that represents a counter (for projections only)
 */
class AstCounter : public AstArgument {
protected:
public:
    AstCounter() : AstArgument() {}

    /** Print argument to the given output stream */
    void print(std::ostream& os) const override {
        os << "$";
    }

    /** Creates a clone if this AST sub-structure */
    AstCounter* clone() const override {
        AstCounter* res = new AstCounter();
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** Mutates this node */
    void apply(const AstNodeMapper& /*mapper*/) override {
        // no sub-nodes to consider within constants
    }

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(dynamic_cast<const AstCounter*>(&node));
        return true;
    }
};

/**
 * Subclass of Argument that represents a datalog constant value
 */
class AstConstant : public AstArgument {
protected:
    /** Index of this Constant in the SymbolTable */
    AstDomain idx;

public:
    AstConstant(AstDomain i) : AstArgument(), idx(i) {}

    /** @return Return the index of this constant in the SymbolTable */
    AstDomain getIndex() const {
        return idx;
    }

    /** Mutates this node */
    void apply(const AstNodeMapper& /*mapper*/) override {
        // no sub-nodes to consider within constants
    }

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(dynamic_cast<const AstConstant*>(&node));
        const AstConstant& other = static_cast<const AstConstant&>(node);
        return idx == other.idx;
    }
};

/**
 * Subclass of Argument that represents a datalog constant value
 */
class AstStringConstant : public AstConstant {
    SymbolTable* symTable;
    AstStringConstant(SymbolTable* symTable, size_t index) : AstConstant(index), symTable(symTable) {}

public:
    AstStringConstant(SymbolTable& symTable, const char* c)
            : AstConstant(symTable.lookup(c)), symTable(&symTable) {}

    /** @return String representation of this Constant */
    const std::string getConstant() const {
        return symTable->resolve(getIndex());
    }

    /**  Print argument to the given output stream */
    void print(std::ostream& os) const override {
        os << "\"" << getConstant() << "\"";
    }

    /** Creates a clone if this AST sub-structure */
    AstStringConstant* clone() const override {
        AstStringConstant* res = new AstStringConstant(symTable, getIndex());
        res->setSrcLoc(getSrcLoc());
        return res;
    }
};

/**
 * Subclass of Argument that represents a datalog constant value
 */
class AstNumberConstant : public AstConstant {
public:
    AstNumberConstant(AstDomain num) : AstConstant(num) {}

    /**  Print argument to the given output stream */
    void print(std::ostream& os) const override {
        os << idx;
    }

    /** Creates a clone if this AST sub-structure */
    AstNumberConstant* clone() const override {
        AstNumberConstant* res = new AstNumberConstant(getIndex());
        res->setSrcLoc(getSrcLoc());
        return res;
    }
};

/**
 * Subclass of AstConstant that represents a null-constant (no record)
 */
class AstNullConstant : public AstConstant {
public:
    AstNullConstant() : AstConstant(0) {}

    /**  Print argument to the given output stream */
    void print(std::ostream& os) const override {
        os << '-';
    }

    /** Creates a clone if this AST sub-structure */
    AstNullConstant* clone() const override {
        AstNullConstant* res = new AstNullConstant();
        res->setSrcLoc(getSrcLoc());
        return res;
    }
};

/**
 * A common base class for AST functors
 */
class AstFunctor : public AstArgument {};

/**
 * Subclass of Argument that represents a unary function
 */
class AstUnaryFunctor : public AstFunctor {
protected:
    UnaryOp fun;

    std::unique_ptr<AstArgument> operand;

public:
    AstUnaryFunctor(UnaryOp fun, std::unique_ptr<AstArgument> o) : fun(fun), operand(std::move(o)) {}

    ~AstUnaryFunctor() override = default;

    AstArgument* getOperand() const {
        return operand.get();
    }

    UnaryOp getFunction() const {
        return fun;
    }

    /** Check if the return value of this functor is a number type. */
    bool isNumerical() const {
        return isNumericUnaryOp(fun);
    }

    /** Check if the return value of this functor is a symbol type. */
    bool isSymbolic() const {
        return isSymbolicUnaryOp(fun);
    }

    /** Check if the argument of this functor is a number type. */
    bool acceptsNumbers() const {
        return unaryOpAcceptsNumbers(fun);
    }

    /** Check if the argument of this functor is a symbol type. */
    bool acceptsSymbols() const {
        return unaryOpAcceptsSymbols(fun);
    }

    /** Print argument to the given output stream */
    void print(std::ostream& os) const override {
        os << getSymbolForUnaryOp(fun);
        os << "(";
        operand->print(os);
        os << ")";
    }

    /** Creates a clone */
    AstUnaryFunctor* clone() const override {
        auto res = new AstUnaryFunctor(fun, std::unique_ptr<AstArgument>(operand->clone()));
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** Mutates this node */
    void apply(const AstNodeMapper& map) override {
        operand = map(std::move(operand));
    }

    /** Obtains a list of all embedded child nodes */
    std::vector<const AstNode*> getChildNodes() const override {
        auto res = AstArgument::getChildNodes();
        res.push_back(operand.get());
        return res;
    }

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(dynamic_cast<const AstUnaryFunctor*>(&node));
        const AstUnaryFunctor& other = static_cast<const AstUnaryFunctor&>(node);
        return fun == other.fun && *operand == *other.operand;
    }
};

/**
 * Subclass of Argument that represents a binary function
 */
class AstBinaryFunctor : public AstFunctor {
protected:
    BinaryOp fun;                      // binary operator
    std::unique_ptr<AstArgument> lhs;  // first argument
    std::unique_ptr<AstArgument> rhs;  // second argument

public:
    AstBinaryFunctor(BinaryOp fun, std::unique_ptr<AstArgument> l, std::unique_ptr<AstArgument> r)
            : fun(fun), lhs(std::move(l)), rhs(std::move(r)) {}

    ~AstBinaryFunctor() override = default;

    AstArgument* getLHS() const {
        return lhs.get();
    }

    AstArgument* getRHS() const {
        return rhs.get();
    }

    BinaryOp getFunction() const {
        return fun;
    }

    /** Check if the return value of this functor is a number type. */
    bool isNumerical() const {
        return isNumericBinaryOp(fun);
    }

    /** Check if the return value of this functor is a symbol type. */
    bool isSymbolic() const {
        return isSymbolicBinaryOp(fun);
    }

    /** Check if the arguments of this functor are number types. */
    bool acceptsNumbers(int arg) const {
        return binaryOpAcceptsNumbers(arg, fun);
    }

    /** Check if the arguments of this functor are symbol types. */
    bool acceptsSymbols(int arg) const {
        return binaryOpAcceptsSymbols(arg, fun);
    }

    /** Print argument to the given output stream */
    void print(std::ostream& os) const override {
        if (fun < BinaryOp::MAX) {
            os << "(";
            lhs->print(os);
            os << getSymbolForBinaryOp(fun);
            rhs->print(os);
            os << ")";
        } else {
            os << getSymbolForBinaryOp(fun);
            os << "(";
            lhs->print(os);
            os << ",";
            rhs->print(os);
            os << ")";
        }
    }

    /** Creates a clone */
    AstBinaryFunctor* clone() const override {
        auto res = new AstBinaryFunctor(
                fun, std::unique_ptr<AstArgument>(lhs->clone()), std::unique_ptr<AstArgument>(rhs->clone()));
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** Mutates this node */
    void apply(const AstNodeMapper& map) override {
        lhs = map(std::move(lhs));
        rhs = map(std::move(rhs));
    }

    /** Obtains a list of all embedded child nodes */
    std::vector<const AstNode*> getChildNodes() const override {
        auto res = AstArgument::getChildNodes();
        res.push_back(lhs.get());
        res.push_back(rhs.get());
        return res;
    }

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(dynamic_cast<const AstBinaryFunctor*>(&node));
        const AstBinaryFunctor& other = static_cast<const AstBinaryFunctor&>(node);
        return fun == other.fun && *lhs == *other.lhs && *rhs == *other.rhs;
    }
};

/**
 * @class TernaryFunctor
 * @brief Subclass of Argument that represents a binary functor
 */
class AstTernaryFunctor : public AstFunctor {
protected:
    TernaryOp fun;
    std::array<std::unique_ptr<AstArgument>, 3> arg;

public:
    AstTernaryFunctor(TernaryOp fun, std::unique_ptr<AstArgument> a1, std::unique_ptr<AstArgument> a2,
            std::unique_ptr<AstArgument> a3)
            : fun(fun), arg({{std::move(a1), std::move(a2), std::move(a3)}}) {}

    ~AstTernaryFunctor() override = default;

    AstArgument* getArg(int idx) const {
        assert(idx >= 0 && idx < 3 && "wrong argument");
        return arg[idx].get();
    }

    TernaryOp getFunction() const {
        return fun;
    }

    /** Check if the return value of this functor is a number type. */
    bool isNumerical() const {
        return isNumericTernaryOp(fun);
    }

    /** Check if the return value of this functor is a symbol type. */
    bool isSymbolic() const {
        return isSymbolicTernaryOp(fun);
    }

    /** Check if the arguments of this functor are number types. */
    bool acceptsNumbers(int arg) const {
        return ternaryOpAcceptsNumbers(arg, fun);
    }

    /** Check if the arguments of this functor are symbol types. */
    bool acceptsSymbols(int arg) const {
        return ternaryOpAcceptsSymbols(arg, fun);
    }

    /** Print argument to the given output stream */
    void print(std::ostream& os) const override {
        os << getSymbolForTernaryOp(fun);
        os << "(";
        arg[0]->print(os);
        os << ",";
        arg[1]->print(os);
        os << ",";
        arg[2]->print(os);
        os << ")";
    }

    /** Clone this node  */
    AstTernaryFunctor* clone() const override {
        auto res = new AstTernaryFunctor(fun, std::unique_ptr<AstArgument>(arg[0]->clone()),
                std::unique_ptr<AstArgument>(arg[1]->clone()), std::unique_ptr<AstArgument>(arg[2]->clone()));
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** Mutates this node */
    void apply(const AstNodeMapper& map) override {
        arg[0] = map(std::move(arg[0]));
        arg[1] = map(std::move(arg[1]));
        arg[2] = map(std::move(arg[2]));
    }

    /** Obtains a list of all embedded child nodes */
    std::vector<const AstNode*> getChildNodes() const override {
        auto res = AstArgument::getChildNodes();
        res.push_back(arg[0].get());
        res.push_back(arg[1].get());
        res.push_back(arg[2].get());
        return res;
    }

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(dynamic_cast<const AstTernaryFunctor*>(&node));
        const AstTernaryFunctor& other = static_cast<const AstTernaryFunctor&>(node);
        return fun == other.fun && *arg[0] == *other.arg[0] && *arg[1] == *other.arg[1] &&
               *arg[2] == *other.arg[2];
    }
};

/**
 * An argument that takes a list of values and combines them into a
 * new record.
 */
class AstRecordInit : public AstArgument {
    /** The list of components to be aggregated into a record */
    std::vector<std::unique_ptr<AstArgument>> args;

public:
    AstRecordInit() {}

    ~AstRecordInit() override = default;

    void add(std::unique_ptr<AstArgument> arg) {
        args.push_back(std::move(arg));
    }

    std::vector<AstArgument*> getArguments() const {
        return toPtrVector(args);
    }

    void print(std::ostream& os) const override {
        os << "[" << join(args, ",", print_deref<std::unique_ptr<AstArgument>>()) << "]";
    }

    /** Creates a clone if this AST sub-structure */
    AstRecordInit* clone() const override {
        auto res = new AstRecordInit();
        for (auto& cur : args) {
            res->args.push_back(std::unique_ptr<AstArgument>(cur->clone()));
        }
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** Mutates this node */
    void apply(const AstNodeMapper& map) override {
        for (auto& arg : args) {
            arg = map(std::move(arg));
        }
    }

    /** Obtains a list of all embedded child nodes */
    std::vector<const AstNode*> getChildNodes() const override {
        auto res = AstArgument::getChildNodes();
        for (auto& cur : args) {
            res.push_back(cur.get());
        }
        return res;
    }

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(dynamic_cast<const AstRecordInit*>(&node));
        const AstRecordInit& other = static_cast<const AstRecordInit&>(node);
        return equal_targets(args, other.args);
    }
};

/**
 * An argument capable of casting a value of one type into another.
 */
class AstTypeCast : public AstArgument {
    /** The value to be casted */
    std::unique_ptr<AstArgument> value;

    /** The target type name */
    std::string type;

public:
    AstTypeCast(std::unique_ptr<AstArgument> value, const std::string& type)
            : value(std::move(value)), type(type) {}

    void print(std::ostream& os) const override {
        os << *value << " as " << type;
    }

    AstArgument* getValue() const {
        return value.get();
    }

    const std::string& getType() const {
        return type;
    }

    /** Obtains a list of all embedded child nodes */
    std::vector<const AstNode*> getChildNodes() const override {
        auto res = AstArgument::getChildNodes();
        res.push_back(value.get());
        return res;
    }

    /** Creates a clone if this AST sub-structure */
    AstTypeCast* clone() const override {
        auto res = new AstTypeCast(std::unique_ptr<AstArgument>(value->clone()), type);
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** Mutates this node */
    void apply(const AstNodeMapper& map) override {
        value = map(std::move(value));
    }

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(dynamic_cast<const AstTypeCast*>(&node));
        const AstTypeCast& other = static_cast<const AstTypeCast&>(node);
        return type == other.type && *value == *other.value;
    }
};

/**
 * An argument aggregating a value from a sub-query.
 */
class AstAggregator : public AstArgument {
public:
    /**
     * The kind of utilized aggregation operator.
     * Note: lower-case is utilized due to a collision with
     *  constants in the parser.
     */
    enum Op { min, max, count, sum };

private:
    /** The aggregation operator of this aggregation step */
    Op fun;

    /** The expression to be aggregated */
    std::unique_ptr<AstArgument> expr;

    /** A list of body-literals forming a sub-query which's result is projected and aggregated */
    std::vector<std::unique_ptr<AstLiteral>> body;

public:
    /** Creates a new aggregation node */
    AstAggregator(Op fun) : fun(fun), expr(nullptr) {}

    /** Destructor */
    ~AstAggregator() override = default;

    // -- getters and setters --

    Op getOperator() const {
        return fun;
    }

    void setTargetExpression(std::unique_ptr<AstArgument> arg) {
        expr = std::move(arg);
    }

    const AstArgument* getTargetExpression() const {
        return expr.get();
    }

    std::vector<AstLiteral*> getBodyLiterals() const {
        return toPtrVector(body);
    }

    void clearBodyLiterals() {
        body.clear();
    }

    void addBodyLiteral(std::unique_ptr<AstLiteral> lit) {
        body.push_back(std::move(lit));
    }

    // -- others --

    /** Prints this instance in a parse-able format */
    void print(std::ostream& os) const override;

    /** Obtains a list of all embedded child nodes */
    std::vector<const AstNode*> getChildNodes() const override;

    /** Creates a clone if this AST sub-structure */
    AstAggregator* clone() const override;

    /** Mutates this node */
    void apply(const AstNodeMapper& map) override {
        if (expr) {
            expr = map(std::move(expr));
        }
        for (auto& cur : body) {
            cur = map(std::move(cur));
        }
    }

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(dynamic_cast<const AstAggregator*>(&node));
        const AstAggregator& other = static_cast<const AstAggregator&>(node);
        return fun == other.fun && equal_ptr(expr, other.expr) && equal_targets(body, other.body);
    }
};

/**
 * An argument taking its value from an argument of a RAM subroutine
 */
class AstSubroutineArgument : public AstArgument {
private:
    /** Index of argument in argument list*/
    size_t number;

public:
    AstSubroutineArgument(size_t n) : AstArgument(), number(n) {}

    /** Return argument number */
    size_t getNumber() const {
        return number;
    }

    /** Print argument to the given output stream */
    void print(std::ostream& os) const override {
        os << "arg_" << number;
    }

    /** Creates a clone if this AST sub-structure */
    AstSubroutineArgument* clone() const override {
        AstSubroutineArgument* res = new AstSubroutineArgument(number);
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** Mutates this node */
    void apply(const AstNodeMapper& /*mapper*/) override {
        // no sub-nodes to consider
    }

protected:
    /** Implements the node comparison for this node type */
    bool equal(const AstNode& node) const override {
        assert(dynamic_cast<const AstSubroutineArgument*>(&node));
        const AstSubroutineArgument& other = static_cast<const AstSubroutineArgument&>(node);
        return number == other.number;
    }
};

}  // end of namespace souffle
