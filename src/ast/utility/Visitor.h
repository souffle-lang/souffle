/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Visitor.h
 *
 * Defines a visitor pattern for AST
 *
 ***********************************************************************/

#pragma once

#include "ast/Aggregator.h"
#include "ast/AlgebraicDataType.h"
#include "ast/AliasType.h"
#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/Attribute.h"
#include "ast/BinaryConstraint.h"
#include "ast/BooleanConstraint.h"
#include "ast/BranchInit.h"
#include "ast/BranchType.h"
#include "ast/Clause.h"
#include "ast/Component.h"
#include "ast/ComponentInit.h"
#include "ast/ComponentType.h"
#include "ast/Constant.h"
#include "ast/Constraint.h"
#include "ast/Counter.h"
#include "ast/FunctionalConstraint.h"
#include "ast/Functor.h"
#include "ast/FunctorDeclaration.h"
#include "ast/IntrinsicFunctor.h"
#include "ast/Literal.h"
#include "ast/Negation.h"
#include "ast/NilConstant.h"
#include "ast/Node.h"
#include "ast/NumericConstant.h"
#include "ast/Pragma.h"
#include "ast/Program.h"
#include "ast/RecordInit.h"
#include "ast/RecordType.h"
#include "ast/Relation.h"
#include "ast/StringConstant.h"
#include "ast/SubsetType.h"
#include "ast/SubsumptiveClause.h"
#include "ast/Term.h"
#include "ast/Type.h"
#include "ast/TypeCast.h"
#include "ast/UnionType.h"
#include "ast/UnnamedVariable.h"
#include "ast/UserDefinedFunctor.h"
#include "ast/Variable.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/Visitor.h"

namespace souffle::ast {
/**
 * The generic base type of all AstVisitors
 * @see souffle::Visitor
 */
template <typename R = void, typename NodeType = Node const, typename... Params>
struct Visitor : souffle::detail::VisitorBase<R, NodeType, Params...> {
    using Base = souffle::detail::VisitorBase<R, NodeType, Params...>;
    using Base::visit_;

    R dispatch(NodeType& node, Params const&... args) override {
        // dispatch node processing based on dynamic type

        // types
        SOUFFLE_VISITOR_FORWARD(AliasType);
        SOUFFLE_VISITOR_FORWARD(SubsetType);
        SOUFFLE_VISITOR_FORWARD(UnionType);
        SOUFFLE_VISITOR_FORWARD(RecordType);
        SOUFFLE_VISITOR_FORWARD(AlgebraicDataType);
        SOUFFLE_VISITOR_FORWARD(BranchType)

        // arguments
        SOUFFLE_VISITOR_FORWARD(Variable)
        SOUFFLE_VISITOR_FORWARD(UnnamedVariable)
        SOUFFLE_VISITOR_FORWARD(IntrinsicFunctor)
        SOUFFLE_VISITOR_FORWARD(UserDefinedFunctor)
        SOUFFLE_VISITOR_FORWARD(Counter)
        SOUFFLE_VISITOR_FORWARD(NumericConstant)
        SOUFFLE_VISITOR_FORWARD(StringConstant)
        SOUFFLE_VISITOR_FORWARD(NilConstant)
        SOUFFLE_VISITOR_FORWARD(TypeCast)
        SOUFFLE_VISITOR_FORWARD(RecordInit)
        SOUFFLE_VISITOR_FORWARD(BranchInit)
        SOUFFLE_VISITOR_FORWARD(Aggregator)

        // literals
        SOUFFLE_VISITOR_FORWARD(Atom)
        SOUFFLE_VISITOR_FORWARD(Negation)
        SOUFFLE_VISITOR_FORWARD(BooleanConstraint)
        SOUFFLE_VISITOR_FORWARD(BinaryConstraint)
        SOUFFLE_VISITOR_FORWARD(FunctionalConstraint)

        // components
        SOUFFLE_VISITOR_FORWARD(ComponentType);
        SOUFFLE_VISITOR_FORWARD(ComponentInit);
        SOUFFLE_VISITOR_FORWARD(Component);

        // rest
        SOUFFLE_VISITOR_FORWARD(Attribute);
        SOUFFLE_VISITOR_FORWARD(Clause);
        SOUFFLE_VISITOR_FORWARD(SubsumptiveClause);
        SOUFFLE_VISITOR_FORWARD(Relation);
        SOUFFLE_VISITOR_FORWARD(Program);
        SOUFFLE_VISITOR_FORWARD(Pragma);
        SOUFFLE_VISITOR_FORWARD(FunctorDeclaration);

        // did not work ...
        fatal("unsupported type: %s", typeid(node).name());
    }

    // -- types --
    SOUFFLE_VISITOR_LINK(ast::SubsetType, Type);
    SOUFFLE_VISITOR_LINK(ast::RecordType, Type);
    SOUFFLE_VISITOR_LINK(ast::AliasType, Type);
    SOUFFLE_VISITOR_LINK(ast::AlgebraicDataType, Type);
    SOUFFLE_VISITOR_LINK(ast::UnionType, Type);
    SOUFFLE_VISITOR_LINK(ast::Type, Node);
    SOUFFLE_VISITOR_LINK(ast::BranchType, Node);

    // -- arguments --
    SOUFFLE_VISITOR_LINK(Variable, Argument)
    SOUFFLE_VISITOR_LINK(UnnamedVariable, Argument)
    SOUFFLE_VISITOR_LINK(Counter, Argument)
    SOUFFLE_VISITOR_LINK(TypeCast, Argument)
    SOUFFLE_VISITOR_LINK(BranchInit, Argument)

    SOUFFLE_VISITOR_LINK(NumericConstant, Constant)
    SOUFFLE_VISITOR_LINK(StringConstant, Constant)
    SOUFFLE_VISITOR_LINK(NilConstant, Constant)
    SOUFFLE_VISITOR_LINK(Constant, Argument)

    SOUFFLE_VISITOR_LINK(IntrinsicFunctor, Functor)
    SOUFFLE_VISITOR_LINK(UserDefinedFunctor, Functor)

    SOUFFLE_VISITOR_LINK(RecordInit, Term)
    SOUFFLE_VISITOR_LINK(Functor, Term)

    SOUFFLE_VISITOR_LINK(Term, Argument)

    SOUFFLE_VISITOR_LINK(Aggregator, Argument)

    SOUFFLE_VISITOR_LINK(Argument, Node);

    // literals
    SOUFFLE_VISITOR_LINK(Atom, Literal)
    SOUFFLE_VISITOR_LINK(Negation, Literal)
    SOUFFLE_VISITOR_LINK(Literal, Node);

    SOUFFLE_VISITOR_LINK(BooleanConstraint, Constraint)
    SOUFFLE_VISITOR_LINK(BinaryConstraint, Constraint)
    SOUFFLE_VISITOR_LINK(FunctionalConstraint, Constraint)
    SOUFFLE_VISITOR_LINK(Constraint, Literal)

    // components
    SOUFFLE_VISITOR_LINK(ComponentType, Node);
    SOUFFLE_VISITOR_LINK(ComponentInit, Node);
    SOUFFLE_VISITOR_LINK(Component, Node);

    // -- others --
    SOUFFLE_VISITOR_LINK(Program, Node);
    SOUFFLE_VISITOR_LINK(Attribute, Node);
    SOUFFLE_VISITOR_LINK(SubsumptiveClause, Clause);
    SOUFFLE_VISITOR_LINK(Clause, Node);
    SOUFFLE_VISITOR_LINK(Relation, Node);
    SOUFFLE_VISITOR_LINK(Pragma, Node);
    SOUFFLE_VISITOR_LINK(FunctorDeclaration, Node);
};
}  // namespace souffle::ast

SOUFFLE_VISITOR_DEFINE_PARTIAL_SPECIALISATION(souffle::ast::Node, souffle::ast::Visitor);
