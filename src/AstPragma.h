/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstPragma.h
 *
 * Define the class AstPragma to update global options based on parameter.
 *
 ***********************************************************************/

#pragma once

#include "AstNode.h"
#include "AstTransformer.h"

#include <string>

namespace souffle {

/**
 * @class AstPragma
 * @brief Representation of a global option
 */
class AstPragma : public AstNode {
protected:
    /** An internal function to determine equality to another node */
    virtual bool equal(const AstNode& node) const {
        assert(dynamic_cast<const AstPragma*>(&node));
        const AstPragma& other = static_cast<const AstPragma&>(node);
        return other.key == key && other.value == value;
    }

public:
    virtual ~AstPragma() {}

    AstPragma() {}

    AstPragma(const std::string& k, const std::string& v) : key(k), value(v) {}

    /** Obtains a list of all embedded child nodes */
    virtual std::vector<const AstNode*> getChildNodes() const {
        // type is just cached, not essential
        return std::vector<const AstNode*>();
    }

    /** Creates a clone of this AST sub-structure */
    virtual AstPragma* clone() const {
        auto res = new AstPragma();
        res->key = key;
        res->value = value;
        res->setSrcLoc(getSrcLoc());
        return res;
    }

    /** No nested nodes to apply to */
    virtual void apply(const AstNodeMapper& mapper) {}

    /** Output to a given output stream */
    virtual void print(std::ostream& os) const {
        os << ".pragma " << key << " " << value << "\n";
    }

    std::pair<std::string, std::string> getkvp() const {
        return std::pair<std::string, std::string>(key, value);
    }

    friend std::ostream& operator<<(std::ostream& out, const AstPragma& arg) {
        out << arg.key << " \"" << arg.value << "\"";
        return out;
    }

    /** Name of the key */
    std::string key;

    /** Value */
    std::string value;
};

class AstPragmaChecker : public AstTransformer {
private:
    virtual bool transform(AstTranslationUnit&);

public:
    virtual std::string getName() const {
        return "AstPragmaChecker";
    }
};

}  // end of namespace souffle
