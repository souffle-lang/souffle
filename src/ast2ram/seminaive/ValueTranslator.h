/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ValueTranslator.h
 *
 ***********************************************************************/

#pragma once

#include "ast2ram/ValueTranslator.h"

namespace souffle::ast {
class Argument;
class Aggregator;
class Counter;
class IntrinsicFunctor;
class NilConstant;
class NumericConstant;
class RecordInit;
class StringConstant;
class SubroutineArgument;
class UnnamedVariable;
class UserDefinedFunctor;
class Variable;
}  // namespace souffle::ast

namespace souffle::ram {
class Expression;
}

namespace souffle::ast2ram {
class TranslatorContext;
class ValueIndex;
}  // namespace souffle::ast2ram

namespace souffle::ast2ram::seminaive {

class ValueTranslator : public ast2ram::ValueTranslator {
public:
    ValueTranslator(const TranslatorContext& context, const ValueIndex& index)
            : ast2ram::ValueTranslator(context, index) {}

    Own<ram::Expression> translateValue(const ast::Argument* arg) override;

    /** -- Visitors -- */
    Own<ram::Expression> visit_(type_identity<ast::Variable>, const ast::Variable& var) override;
    Own<ram::Expression> visit_(
            type_identity<ast::UnnamedVariable>, const ast::UnnamedVariable& var) override;
    Own<ram::Expression> visit_(type_identity<ast::NumericConstant>, const ast::NumericConstant& c) override;
    Own<ram::Expression> visit_(type_identity<ast::StringConstant>, const ast::StringConstant& c) override;
    Own<ram::Expression> visit_(type_identity<ast::NilConstant>, const ast::NilConstant& c) override;
    Own<ram::Expression> visit_(type_identity<ast::TypeCast>, const ast::TypeCast& typeCast) override;
    Own<ram::Expression> visit_(
            type_identity<ast::IntrinsicFunctor>, const ast::IntrinsicFunctor& inf) override;
    Own<ram::Expression> visit_(
            type_identity<ast::UserDefinedFunctor>, const ast::UserDefinedFunctor& udf) override;
    Own<ram::Expression> visit_(type_identity<ast::Counter>, const ast::Counter& ctr) override;
    Own<ram::Expression> visit_(
            type_identity<ast::IterationCounter>, const ast::IterationCounter& ctr) override;
    Own<ram::Expression> visit_(type_identity<ast::RecordInit>, const ast::RecordInit& init) override;
    Own<ram::Expression> visit_(type_identity<ast::BranchInit>, const ast::BranchInit& init) override;
    Own<ram::Expression> visit_(type_identity<ast::Aggregator>, const ast::Aggregator& agg) override;
};

}  // namespace souffle::ast2ram::seminaive
