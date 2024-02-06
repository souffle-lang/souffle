/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2023, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Lattice.h
 *
 * Defines the Lattice class
 *
 ***********************************************************************/

#pragma once

#include "ast/Argument.h"
#include "ast/Node.h"
#include "ast/QualifiedName.h"
#include "parser/SrcLocation.h"

#include <map>
#include <optional>

namespace souffle::ast {

enum LatticeOperator { Bottom = 0, Top, Lub, Glb, Leq };

std::optional<LatticeOperator> latticeOperatorFromString(const std::string& str);

/**
 *  @class Lattice
 *  @brief An class to define Lattice attributes for a type
 */
class Lattice : public Node {
public:
    Lattice(QualifiedName name, std::map<LatticeOperator, Own<ast::Argument>> operators,
            SrcLocation loc = {});

    /** Return type name */
    const QualifiedName& getQualifiedName() const {
        return name;
    }

    /** Set type name */
    void setQualifiedName(QualifiedName name);

    std::map<LatticeOperator, const ast::Argument*> getOperators() const;

    bool hasGlb() const;
    bool hasLub() const;
    bool hasBottom() const;
    bool hasTop() const;

    const ast::Argument* getLub() const;
    const ast::Argument* getGlb() const;
    const ast::Argument* getBottom() const;
    const ast::Argument* getTop() const;

    static bool classof(const Node*);

protected:
    void print(std::ostream& os) const override;

private:
    bool equal(const Node& node) const override;

    Lattice* cloning() const override;

private:
    /** type name */
    QualifiedName name;

    const std::map<LatticeOperator, Own<ast::Argument>> operators;
};

}  // namespace souffle::ast
