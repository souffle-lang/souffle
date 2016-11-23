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

#include <string>
#include <list>
#include <memory>

#include "SymbolTable.h"
#include "AstNode.h"
#include "AstType.h"
#include "BinaryOperator.h"
#include "UnaryOperator.h"

#include "TypeSystem.h"

namespace souffle {

/* Forward declarations */
class AstClause;
class AstVariable;
class AstLiteral;

/**
 * @class Argument
 * @brief Intermediate representation of an argument of a Literal (e.g., a variable or a constant)
 */
class AstArgument : public AstNode {

public:

    virtual ~AstArgument() {}

    /** Obtains a list of all embedded child nodes */
    virtual std::vector<const AstNode*> getChildNodes() const {
        return std::vector<const AstNode*>();  // type is just cached, not essential
    }

    /** Creates a clone if this AST sub-structure */
    virtual AstArgument* clone() const =0;

};

/**
 * @class Variable
 * @brief Subclass of Argument that represents a named variable
 */
class AstVariable : public AstArgument {
protected:
    /** Variable name */
    std::string name;

public:
    AstVariable(const std::string& n) : AstArgument(), name(n)   { }

    /** Updates this variable name */
    void setName(const std::string& name) {
        this->name = name;
    }

    /** @return Variable name */
    const std::string &getName() const {
        return name; 
    }

    /** Print argument to the given output stream */
    virtual void print(std::ostream &os) const {
        os << name;
    }

    /** Creates a clone if this AST sub-structure */
    virtual AstVariable* clone() const {
        AstVariable* res = new AstVariable(name);
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** Mutates this node */
    virtual void apply(const AstNodeMapper& mapper) {
        // no sub-nodes to consider
    }

protected:

    /** Implements the node comparison for this node type */
    virtual bool equal(const AstNode& node) const {
        assert(dynamic_cast<const AstVariable*>(&node));
        const AstVariable& other = static_cast<const AstVariable&>(node);
        return name == other.name;
    }

};

/**
 * @class Variable
 * @brief Subclass of Argument that represents an unnamed variable
 */
class AstUnnamedVariable : public AstArgument {
protected:

public:
    AstUnnamedVariable() : AstArgument() {
    }

    /** Print argument to the given output stream */
    virtual void print(std::ostream &os) const {
        os << "_";
    }

    /** Creates a clone if this AST sub-structure */
    virtual AstUnnamedVariable* clone() const {
        AstUnnamedVariable* res = new AstUnnamedVariable();
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** Mutates this node */
    virtual void apply(const AstNodeMapper& mapper) {
        // no sub-nodes to consider
    }

protected:

    /** Implements the node comparison for this node type */
    virtual bool equal(const AstNode& node) const {
        assert(dynamic_cast<const AstUnnamedVariable*>(&node));
        return true;
    }

};

/**
 * @class Counter
 * @brief Subclass of Argument that represents a counter (for projections only)
 */
class AstCounter : public AstArgument {
protected:

public:
    AstCounter() : AstArgument() { 
    }

    /** Print argument to the given output stream */
    virtual void print(std::ostream &os) const {
        os << "$";
    }

    /** Creates a clone if this AST sub-structure */
    virtual AstCounter* clone() const {
        AstCounter* res = new AstCounter();
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** Mutates this node */
    virtual void apply(const AstNodeMapper& mapper) {
        // no sub-nodes to consider within constants
    }

protected:

    /** Implements the node comparison for this node type */
    virtual bool equal(const AstNode& node) const {
        assert(dynamic_cast<const AstCounter*>(&node));
        return true;
    }

};

/**
 * @class Constant
 * @brief Subclass of Argument that represents a datalog constant value
 */
class AstConstant : public AstArgument {
protected:
    /** Index of this Constant in the SymbolTable */
    size_t idx;

public:
    AstConstant(size_t i) : AstArgument(), idx(i) {
    }

    /** @return Return the index of this constant in the SymbolTable */
    size_t getIndex() const { return idx; }

    /** Mutates this node */
    virtual void apply(const AstNodeMapper& mapper) {
        // no sub-nodes to consider within constants
    }

protected:

    /** Implements the node comparison for this node type */
    virtual bool equal(const AstNode& node) const {
        assert(dynamic_cast<const AstConstant*>(&node));
        const AstConstant& other = static_cast<const AstConstant&>(node);
        return idx == other.idx;
    }

};

/**
 * @class Constant
 * @brief Subclass of Argument that represents a datalog constant value
 */
class AstStringConstant : public AstConstant {
    using SymbolTable = souffle::SymbolTable; // XXX pending namespace cleanup
    SymbolTable *symTable; 
    AstStringConstant(SymbolTable *symTable, size_t index) : AstConstant(index), symTable(symTable) {}

public:

    AstStringConstant(SymbolTable& symTable, const char *c)
        : AstConstant(symTable.lookup(c)), symTable(&symTable) {}

    /** @return String representation of this Constant */
    const std::string getConstant() const {
        return symTable->resolve(getIndex());
    }

    /** @brief Print argument to the given output stream */
    virtual void print(std::ostream &os) const {
        os << "\"" << getConstant() << "\"";
    }

    /** Creates a clone if this AST sub-structure */
    virtual AstStringConstant* clone() const {
        AstStringConstant* res = new AstStringConstant(symTable, getIndex());
        res->setSrcLoc(getSrcLoc());
        return res;
    }
};

/**
 * @class Constant
 * @brief Subclass of Argument that represents a datalog constant value
 */
class AstNumberConstant : public AstConstant {
public:

    AstNumberConstant(size_t num)
        : AstConstant(num) {}

    /** @brief Print argument to the given output stream */
    virtual void print(std::ostream &os) const {
        os << (int64_t)idx;
    }

    /** Creates a clone if this AST sub-structure */
    virtual AstNumberConstant* clone() const {
        AstNumberConstant* res = new AstNumberConstant(getIndex());
        res->setSrcLoc(getSrcLoc());
        return res;
    }
};

/**
 * @class AstNullConstant
 * @brief Subclass of AstConstant that represents a null-constant (no record)
 */
class AstNullConstant : public AstConstant {
public:

    AstNullConstant() : AstConstant(0) {}

    /** @brief Print argument to the given output stream */
    virtual void print(std::ostream &os) const {
        os << '-';
    }

    /** Creates a clone if this AST sub-structure */
    virtual AstNullConstant* clone() const {
        AstNullConstant* res = new AstNullConstant();
        res->setSrcLoc(getSrcLoc());
        return res;
    }
};

/**
 * @class AstFunctor
 * @brief A common base class for AST functors
 */
class AstFunctor : public AstArgument {
};

/**
 * @class UnaryFunctor
 * @brief Subclass of Argument that represents a unary function
 */
class AstUnaryFunctor : public AstFunctor {
protected:

    UnaryOp fun;

    std::unique_ptr<AstArgument> operand;

public:

    AstUnaryFunctor(UnaryOp fun, std::unique_ptr<AstArgument> o)
        : fun(fun), operand(std::move(o)) {}

    virtual ~AstUnaryFunctor() { }

    AstArgument *getOperand() const {
        return operand.get();
    }

    UnaryOp getFunction() const {
        return fun;
    }

    bool isNumerical() const {
        return isNumericUnaryOp(fun);
    }

    bool isSymbolic() const {
        return isSymbolicUnaryOp(fun);
    }

    bool acceptsNumbers() const {
        return unaryOpAcceptsNumbers(fun);
    }

    bool acceptsSymbols() const {
        return unaryOpAcceptsSymbols(fun);
    }

    /** Print argument to the given output stream */
    virtual void print(std::ostream &os) const {
        os << getSymbolForUnaryOp(fun);
        os << "(";
        operand->print(os);
        os << ")";
    }

    /** Creates a clone if this AST sub-structure */
    virtual AstUnaryFunctor* clone() const {
        auto res = new AstUnaryFunctor(fun, std::unique_ptr<AstArgument>(operand->clone()));
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** Mutates this node */
    virtual void apply(const AstNodeMapper& map) {
        operand = map(std::move(operand));
    }

    /** Obtains a list of all embedded child nodes */
    virtual std::vector<const AstNode*> getChildNodes() const {
        auto res = AstArgument::getChildNodes();
        res.push_back(operand.get());
        return res;
    }

protected:

    /** Implements the node comparison for this node type */
    virtual bool equal(const AstNode& node) const {
        assert(dynamic_cast<const AstUnaryFunctor*>(&node));
        const AstUnaryFunctor& other = static_cast<const AstUnaryFunctor&>(node);
        return fun == other.fun && *operand == *other.operand;
    }

};

/**
 * @class BinaryFunctor
 * @brief Subclass of Argument that represents a binary function
 */
class AstBinaryFunctor : public AstFunctor {
protected:

    BinaryOp fun;

    std::unique_ptr<AstArgument> lhs, rhs;

public:

    AstBinaryFunctor(BinaryOp fun, std::unique_ptr<AstArgument> l, std::unique_ptr<AstArgument> r)
        : fun(fun), lhs(std::move(l)), rhs(std::move(r)) {}

    virtual ~AstBinaryFunctor() { }

    AstArgument *getLHS() const {
        return lhs.get();
    }

    AstArgument *getRHS() const {
        return rhs.get();
    }

    BinaryOp getFunction() const {
        return fun;
    }

    bool isNumerical() const {
        return isNumericBinaryOp(fun);
    }

    bool isSymbolic() const {
        return isSymbolicBinaryOp(fun);
    }

    /** Print argument to the given output stream */
    virtual void print(std::ostream &os) const {
        if (isNumericBinaryOp(fun)) {
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

    /** Creates a clone if this AST sub-structure */
    virtual AstBinaryFunctor* clone() const {
        auto res = new AstBinaryFunctor(fun, std::unique_ptr<AstArgument>(lhs->clone()), std::unique_ptr<AstArgument>(rhs->clone()));
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
        auto res = AstArgument::getChildNodes();
        res.push_back(lhs.get());
        res.push_back(rhs.get());
        return res;
    }

protected:

    /** Implements the node comparison for this node type */
    virtual bool equal(const AstNode& node) const {
        assert(dynamic_cast<const AstBinaryFunctor*>(&node));
        const AstBinaryFunctor& other = static_cast<const AstBinaryFunctor&>(node);
        return fun == other.fun && *lhs == *other.lhs && *rhs == *other.rhs;
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

	~AstRecordInit() { }

	void add(std::unique_ptr<AstArgument> arg) {
		args.push_back(std::move(arg));
	}

	std::vector<AstArgument *> getArguments() const {
	    return toPtrVector(args);
	}

	virtual void print(std::ostream& os) const {
		os << "[" << join(args, ",", print_deref<std::unique_ptr<AstArgument>>()) << "]";
	}

	/** Creates a clone if this AST sub-structure */
    virtual AstRecordInit* clone() const {
        auto res = new AstRecordInit();
        for(auto &cur : args) {
            res->args.push_back(std::unique_ptr<AstArgument>(cur->clone()));
        }
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** Mutates this node */
    virtual void apply(const AstNodeMapper& map) {
        for(auto& arg : args) {
            arg = map(std::move(arg));
        }
    }

    /** Obtains a list of all embedded child nodes */
    virtual std::vector<const AstNode*> getChildNodes() const {
        auto res = AstArgument::getChildNodes();
        for(auto &cur : args) res.push_back(cur.get());
        return res;
    }

protected:

    /** Implements the node comparison for this node type */
    virtual bool equal(const AstNode& node) const {
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

	virtual void print(std::ostream& os) const {
		os << *value << " as " << type;
	}

	AstArgument* getValue() const {
		return value.get();
	}

    /** Obtains a list of all embedded child nodes */
    virtual std::vector<const AstNode*> getChildNodes() const {
        auto res = AstArgument::getChildNodes();
        res.push_back(value.get());
        return res;
    }

    /** Creates a clone if this AST sub-structure */
    virtual AstTypeCast* clone() const {
        auto res = new AstTypeCast(std::unique_ptr<AstArgument>(value->clone()), type);
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** Mutates this node */
    virtual void apply(const AstNodeMapper& map) {
        value = map(std::move(value));
    }

protected:

    /** Implements the node comparison for this node type */
    virtual bool equal(const AstNode& node) const {
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
    enum Op {
        min,
        max,
        count,
        sum
    };

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
    ~AstAggregator() { }

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

    std::vector<AstLiteral *> getBodyLiterals() const {
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
    virtual void print(std::ostream& os) const;

    /** Obtains a list of all embedded child nodes */
    virtual std::vector<const AstNode*> getChildNodes() const;

    /** Creates a clone if this AST sub-structure */
    virtual AstAggregator* clone() const;

    /** Mutates this node */
    virtual void apply(const AstNodeMapper& map) {
        if (expr) expr = map(std::move(expr));
        for(auto& cur : body) cur = map(std::move(cur));
    }

protected:

    /** Implements the node comparison for this node type */
    virtual bool equal(const AstNode& node) const {
        assert(dynamic_cast<const AstAggregator*>(&node));
        const AstAggregator& other = static_cast<const AstAggregator&>(node);
        return fun == other.fun && equal_ptr(expr, other.expr) && equal_targets(body, other.body);
    }

};

} // end of namespace souffle

