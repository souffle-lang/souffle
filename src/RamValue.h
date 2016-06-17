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

#include <stdlib.h>

#include <string>
#include <sstream>
#include <algorithm>

#include "RamRelation.h"
#include "RamIndex.h"
#include "RamRecords.h"
#include "RamNode.h"
#include "SymbolTable.h"
#include "BinaryOperator.h"

namespace souffle {

/** Abstract class for values in the relational algebra machine used for evaluating, printing, and book-keeping */ 
class RamValue : public RamNode {

    bool cnst;

public:

    RamValue(RamNodeType type, bool isCnst) : RamNode(type), cnst(isCnst) {}

    virtual ~RamValue() {}

    /** get level of value (which for-loop of a query) */ 
    virtual size_t getLevel() const = 0;

    /** Determines whether this value is a constant or not */
    bool isConstant() const {
        return cnst;
    }

}; 

/** convert a symbolic value to an ordinal value (there is a total order among all symbols) */
class RamOrd : public RamValue {
    std::unique_ptr<RamValue> symbol;
public:
    RamOrd(std::unique_ptr<RamValue> symbol)
        : RamValue(RN_Ord,symbol->isConstant()), symbol(std::move(symbol)) {}

    ~RamOrd() { }

    const RamValue& getSymbol() const {
        return *symbol;
    }

    void print(std::ostream &os) const {
        os << "ord(";
        symbol->print(os);
        os << ")";
    }

    size_t getLevel() const {
        return symbol->getLevel();
    }

    /** Obtains a list of child nodes */
    virtual std::vector<const RamNode*> getChildNodes() const {
        return toVector<const RamNode*>(symbol.get());
    }
};

/** logical not */
class RamNot : public RamValue {
    std::unique_ptr<RamValue> value;
public:
    RamNot(std::unique_ptr<RamValue> value)
        : RamValue(RN_Not,value->isConstant()), value(std::move(value)) {}

    ~RamNot() { }

    const RamValue& getValue() const {
        ASSERT(value != NULL && "no null value expected");
        return *value;
    }

    void print(std::ostream &os) const {
        os << "complement(";
        value->print(os);
        os << ")";
    }

    size_t getLevel() const {
        return value->getLevel();
    }

    /** Obtains a list of child nodes */
    virtual std::vector<const RamNode*> getChildNodes() const {
        return toVector<const RamNode*>(value.get());
    }
};
/** bitwise complement */
class RamComplement : public RamValue {
    std::unique_ptr<RamValue> value;
public:
    RamComplement(std::unique_ptr<RamValue> value)
        : RamValue(RN_Complement,value->isConstant()), value(std::move(value)) {}

    ~RamComplement() { }

    const RamValue& getValue() const {
        ASSERT(value != NULL && "no null value expected");
        return *value;
    }

    void print(std::ostream &os) const {
        os << "complement(";
        value->print(os);
        os << ")";
    }

    size_t getLevel() const {
        return value->getLevel();
    }

    /** Obtains a list of child nodes */
    virtual std::vector<const RamNode*> getChildNodes() const {
        return toVector<const RamNode*>(value.get());
    }
};

/** negate a value arithmetically */
class RamNegation : public RamValue {
    std::unique_ptr<RamValue> value;
public:
    RamNegation(std::unique_ptr<RamValue> value)
        : RamValue(RN_Negation,value->isConstant()), value(std::move(value)) {}

    ~RamNegation() { }

    const RamValue& getValue() const {
        ASSERT(value != NULL && "no null value expected");
        return *value;
    }

    void print(std::ostream &os) const {
        os << "neg(";
        value->print(os);
        os << ")";
    }

    size_t getLevel() const {
        return value->getLevel();
    }

    /** Obtains a list of child nodes */
    virtual std::vector<const RamNode*> getChildNodes() const {
        return toVector<const RamNode*>(value.get());
    }
};

class RamBinaryOperator : public RamValue {
private:
    BinaryOp op;
    std::unique_ptr<RamValue> lhs, rhs;

public:

    RamBinaryOperator(BinaryOp op, std::unique_ptr<RamValue> l, std::unique_ptr<RamValue> r)
        : RamValue(RN_BinaryOperator, l->isConstant() && r->isConstant()), op(op), lhs(std::move(l)), rhs(std::move(r)) {}

    virtual ~RamBinaryOperator() { }

    virtual void print(std::ostream &os) const {
        if (isNumericBinaryOp(op)) {
            os << "(";
            lhs->print(os);
            os << getSymbolForBinaryOp(op);
            rhs->print(os);
            os << ")";
        } else {
            os << getSymbolForBinaryOp(op);
            os << "(";
            lhs->print(os);
            os << ",";
            rhs->print(os);
            os << ")";
        }
    }

    const RamValue* getLHS() const {
        return lhs.get();
    }

    const RamValue* getRHS() const {
        return rhs.get();
    }

    BinaryOp getOperator() const {
        return op;
    }

    size_t getLevel() const {
        return std::max(lhs->getLevel(), rhs->getLevel());
    }

    /** Obtains a list of child nodes */
    virtual std::vector<const RamNode*> getChildNodes() const {
        return toVector<const RamNode*>(lhs.get(), rhs.get());
    }
};

/** Class to retrieve an element from the tuple environment */ 
class RamElementAccess : public RamValue {
    size_t level;
    size_t element;
    std::string name; 
public:
    RamElementAccess(size_t l, size_t e, const std::string &n="")
        : RamValue(RN_ElementAccess, false), level(l), element(e), name(n) {}

    void print(std::ostream &os) const {
        if (name == "") { 
           os << "env(t" << level << ", i" << element << ")";
        } else {
           os << "t" << level << "." << name;
        } 
    }

    size_t getLevel() const {
        return level; 
    }
    size_t getElement() const {
        return element;
    }
    /** Obtains a list of child nodes */
    virtual std::vector<const RamNode*> getChildNodes() const {
        return std::vector<const RamNode*>();  // no child nodes
    }
};

/** Constant value */ 
class RamNumber : public RamValue {
    RamDomain constant;
public:
    RamNumber(RamDomain c) : RamValue(RN_Number,true), constant(c) {}

    RamDomain getConstant() const {
        return constant;
    }
    void print(std::ostream &os) const {
        os << "number(" << constant << ")"; 
    }
    size_t getLevel() const {
        return 0;
    }
    /** Obtains a list of child nodes */
    virtual std::vector<const RamNode*> getChildNodes() const {
        return std::vector<const RamNode*>();  // no child nodes
    }
};

/** Constant value */ 
class RamNull : public RamValue {
public:
    RamNull() : RamValue(RN_Null,true) {}

    RamDomain getConstant() const {
        return '0';
    }
    void print(std::ostream &os) const {
        os << "null()"; 
    }
    size_t getLevel() const {
        return 0;
    }
    /** Obtains a list of child nodes */
    virtual std::vector<const RamNode*> getChildNodes() const {
        return std::vector<const RamNode*>();  // no child nodes
    }
};

/** Constant value */ 
/*class RamUnit : public RamValue {
public:
    RamUnit() : RamValue(RN_Unit,true) {}

    RamDomain getConstant() const {
        return '\n';
    }
    void print(std::ostream &os) const {
        os << "()"; 
    }
    size_t getLevel() const {
        return 0;
    }

    virtual std::vector<const RamNode*> getChildNodes() const {
        return std::vector<const RamNode*>();  // no child nodes
    }
};*/

/** Constant value */ 
class RamAutoIncrement : public RamValue {
public:
    RamAutoIncrement()
        : RamValue(RN_AutoIncrement,false) {}

    void print(std::ostream &os) const {
        os << "autoinc()"; 
    }

    size_t getLevel() const {
        return 0;
    }
    /** Obtains a list of child nodes */
    virtual std::vector<const RamNode*> getChildNodes() const {
        return std::vector<const RamNode*>();  // no child nodes
    }
};

/** Record pack operation */
class RamPack : public RamValue {

    std::vector<std::unique_ptr<RamValue>> values;

public:

    RamPack(std::vector<std::unique_ptr<RamValue>> values)
        : RamValue(RN_Pack, all_of(values, [](const std::unique_ptr<RamValue> &v) { return v && v->isConstant(); })),
          values(std::move(values)) { }

    ~RamPack() { }

    std::vector<RamValue*> getValues() const {
        return toPtrVector(values);
    }

    void print(std::ostream& os) const {
        os << "[" << join(values, ",", [](std::ostream& out, const std::unique_ptr<RamValue> &value) {
            if (value) out << *value;
            else out << "_";
        }) << "]";
    }

    size_t getLevel() const {
        size_t level = 0;
        for(const auto& value : values) {
            if(value) level = std::max(level, value->getLevel());
        }
        return level;
    }

    /** Obtains a list of child nodes */
    virtual std::vector<const RamNode*> getChildNodes() const {
        std::vector<const RamNode*> res;
        for(const auto& cur : values) if (cur) res.push_back(cur.get());
        return res;
    }
};

} // end of namespace souffle

