/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstTranslator.cpp
 *
 * Translator from AST to RAM structures.
 *
 ***********************************************************************/

#include "AstTranslator.h"
#include "BinaryConstraintOps.h"
#include "DebugReport.h"
#include "FunctorOps.h"
#include "Global.h"
#include "LogStatement.h"
#include "RelationTag.h"
#include "SrcLocation.h"
#include "ast/AstAbstract.h"
#include "ast/AstArgument.h"
#include "ast/AstAttribute.h"
#include "ast/AstClause.h"
#include "ast/AstIO.h"
#include "ast/AstLiteral.h"
#include "ast/AstNode.h"
#include "ast/AstProgram.h"
#include "ast/AstRelation.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstType.h"
#include "ast/AstUtils.h"
#include "ast/AstVisitor.h"
#include "ast/TypeSystem.h"
#include "ast/analysis/AstTypeEnvironmentAnalysis.h"
#include "ast/analysis/AuxArityAnalysis.h"
#include "ast/analysis/PrecedenceGraph.h"
#include "json11.h"
#include "ram/RamCondition.h"
#include "ram/RamExpression.h"
#include "ram/RamNode.h"
#include "ram/RamOperation.h"
#include "ram/RamProgram.h"
#include "ram/RamRelation.h"
#include "ram/RamStatement.h"
#include "ram/RamTranslationUnit.h"
#include "ram/RamUtils.h"
#include "utility/ContainerUtil.h"
#include "utility/FunctionalUtil.h"
#include "utility/StreamUtil.h"
#include "utility/StringUtil.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <type_traits>
#include <utility>
#include <vector>

namespace souffle {

using json11::Json;

class ErrorReport;

/** append statement to a list of statements */
inline void appendStmt(
        std::vector<std::unique_ptr<RamStatement>>& stmtList, std::unique_ptr<RamStatement> stmt) {
    if (stmt) {
        stmtList.push_back(std::move(stmt));
    }
}

std::unique_ptr<RamTupleElement> AstTranslator::makeRamTupleElement(const Location& loc) {
    return std::make_unique<RamTupleElement>(loc.identifier, loc.element);
}

size_t AstTranslator::getEvaluationArity(const AstAtom* atom) const {
    if (atom->getQualifiedName().toString().find("@delta_") == 0) {
        const AstQualifiedName& originalRel = AstQualifiedName(atom->getQualifiedName().toString().substr(7));
        return auxArityAnalysis->getArity(getRelation(*program, originalRel));
    } else if (atom->getQualifiedName().toString().find("@new_") == 0) {
        const AstQualifiedName& originalRel = AstQualifiedName(atom->getQualifiedName().toString().substr(5));
        return auxArityAnalysis->getArity(getRelation(*program, originalRel));
    } else if (atom->getQualifiedName().toString().find("@info_") == 0) {
        return 0;
    } else {
        return auxArityAnalysis->getArity(atom);
    }
}

std::vector<std::map<std::string, std::string>> AstTranslator::getInputDirectives(const AstRelation* rel) {
    std::vector<std::map<std::string, std::string>> inputDirectives;

    for (const auto* load : program->getIOs()) {
        if (load->getQualifiedName() != rel->getQualifiedName() || load->getType() != AstIoType::input) {
            continue;
        }

        std::map<std::string, std::string> directives;
        for (const auto& currentPair : load->getDirectives()) {
            directives.insert(std::make_pair(currentPair.first, unescape(currentPair.second)));
        }
        inputDirectives.push_back(directives);
    }

    if (inputDirectives.empty()) {
        inputDirectives.emplace_back();
    }

    return inputDirectives;
}

std::vector<std::map<std::string, std::string>> AstTranslator::getOutputDirectives(const AstRelation* rel) {
    std::vector<std::map<std::string, std::string>> outputDirectives;

    for (const auto* store : program->getIOs()) {
        if (store->getQualifiedName() != rel->getQualifiedName() || store->getType() == AstIoType::input) {
            continue;
        }

        std::map<std::string, std::string> directives;
        for (const auto& currentPair : store->getDirectives()) {
            directives.insert(std::make_pair(currentPair.first, unescape(currentPair.second)));
        }
        outputDirectives.push_back(directives);
    }

    if (outputDirectives.empty()) {
        outputDirectives.emplace_back();
    }

    return outputDirectives;
}

std::unique_ptr<RamRelationReference> AstTranslator::createRelationReference(const std::string name) {
    auto it = ramRels.find(name);
    assert(it != ramRels.end() && "relation name not found");

    const RamRelation* relation = it->second.get();
    return std::make_unique<RamRelationReference>(relation);
}

std::unique_ptr<RamRelationReference> AstTranslator::translateRelation(const AstAtom* atom) {
    return createRelationReference(getRelationName(atom->getQualifiedName()));
}

std::unique_ptr<RamRelationReference> AstTranslator::translateRelation(
        const AstRelation* rel, const std::string relationNamePrefix) {
    return createRelationReference(relationNamePrefix + getRelationName(rel->getQualifiedName()));
}

std::unique_ptr<RamRelationReference> AstTranslator::translateDeltaRelation(const AstRelation* rel) {
    return translateRelation(rel, "@delta_");
}

std::unique_ptr<RamRelationReference> AstTranslator::translateNewRelation(const AstRelation* rel) {
    return translateRelation(rel, "@new_");
}

std::unique_ptr<RamExpression> AstTranslator::translateValue(
        const AstArgument* arg, const ValueIndex& index) {
    if (arg == nullptr) {
        return nullptr;
    }

    class ValueTranslator : public AstVisitor<std::unique_ptr<RamExpression>> {
        AstTranslator& translator;
        const ValueIndex& index;

    public:
        ValueTranslator(AstTranslator& translator, const ValueIndex& index)
                : translator(translator), index(index) {}

        std::unique_ptr<RamExpression> visitVariable(const AstVariable& var) override {
            assert(index.isDefined(var) && "variable not grounded");
            return makeRamTupleElement(index.getDefinitionPoint(var));
        }

        std::unique_ptr<RamExpression> visitUnnamedVariable(const AstUnnamedVariable&) override {
            return std::make_unique<RamUndefValue>();
        }

        std::unique_ptr<RamExpression> visitNumericConstant(const AstNumericConstant& c) override {
            assert(c.getType().has_value() && "At this points all constants should have type.");

            switch (*c.getType()) {
                case AstNumericConstant::Type::Int:
                    return std::make_unique<RamSignedConstant>(
                            RamSignedFromString(c.getConstant(), nullptr, 0));
                case AstNumericConstant::Type::Uint:
                    return std::make_unique<RamUnsignedConstant>(
                            RamUnsignedFromString(c.getConstant(), nullptr, 0));
                case AstNumericConstant::Type::Float:
                    return std::make_unique<RamFloatConstant>(RamFloatFromString(c.getConstant()));
            }

            fatal("unexpected numeric constant type");
        }

        std::unique_ptr<RamExpression> visitStringConstant(const AstStringConstant& c) override {
            return std::make_unique<RamSignedConstant>(translator.getSymbolTable().lookup(c.getConstant()));
        }

        std::unique_ptr<RamExpression> visitNilConstant(const AstNilConstant&) override {
            return std::make_unique<RamSignedConstant>(0);
        }

        std::unique_ptr<RamExpression> visitIntrinsicFunctor(const AstIntrinsicFunctor& inf) override {
            std::vector<std::unique_ptr<RamExpression>> values;
            for (const auto& cur : inf.getArguments()) {
                values.push_back(translator.translateValue(cur, index));
            }

            auto* info = inf.getFunctionInfo();
            assert(info && "no overload picked for instrinsic; missing transform pass?");
            if (info->multipleResults) {
                return translator.makeRamTupleElement(index.getGeneratorLoc(inf));
            } else {
                return std::make_unique<RamIntrinsicOperator>(info->op, std::move(values));
            }
        }

        std::unique_ptr<RamExpression> visitUserDefinedFunctor(const AstUserDefinedFunctor& udf) override {
            // Sanity check.
            assert(udf.getArguments().size() == udf.getArgsTypes().size());

            std::vector<std::unique_ptr<RamExpression>> values;
            for (const auto& cur : udf.getArguments()) {
                values.push_back(translator.translateValue(cur, index));
            }

            return std::make_unique<RamUserDefinedOperator>(
                    udf.getName(), udf.getArgsTypes(), udf.getReturnType(), std::move(values));
        }

        std::unique_ptr<RamExpression> visitCounter(const AstCounter&) override {
            return std::make_unique<RamAutoIncrement>();
        }

        std::unique_ptr<RamExpression> visitRecordInit(const AstRecordInit& init) override {
            std::vector<std::unique_ptr<RamExpression>> values;
            for (const auto& cur : init.getArguments()) {
                values.push_back(translator.translateValue(cur, index));
            }
            return std::make_unique<RamPackRecord>(std::move(values));
        }

        std::unique_ptr<RamExpression> visitAggregator(const AstAggregator& agg) override {
            // here we look up the location the aggregation result gets bound
            return translator.makeRamTupleElement(index.getGeneratorLoc(agg));
        }

        std::unique_ptr<RamExpression> visitSubroutineArgument(const AstSubroutineArgument& subArg) override {
            return std::make_unique<RamSubroutineArgument>(subArg.getNumber());
        }
    };

    return ValueTranslator(*this, index)(*arg);
}

std::unique_ptr<RamCondition> AstTranslator::translateConstraint(
        const AstLiteral* lit, const ValueIndex& index) {
    class ConstraintTranslator : public AstVisitor<std::unique_ptr<RamCondition>> {
        AstTranslator& translator;
        const ValueIndex& index;

    public:
        ConstraintTranslator(AstTranslator& translator, const ValueIndex& index)
                : translator(translator), index(index) {}

        /** for atoms */
        std::unique_ptr<RamCondition> visitAtom(const AstAtom&) override {
            return nullptr;  // covered already within the scan/lookup generation step
        }

        /** for binary relations */
        std::unique_ptr<RamCondition> visitBinaryConstraint(const AstBinaryConstraint& binRel) override {
            auto valLHS = translator.translateValue(binRel.getLHS(), index);
            auto valRHS = translator.translateValue(binRel.getRHS(), index);
            return std::make_unique<RamConstraint>(
                    binRel.getOperator(), std::move(valLHS), std::move(valRHS));
        }

        /** for negations */
        std::unique_ptr<RamCondition> visitNegation(const AstNegation& neg) override {
            const auto* atom = neg.getAtom();
            size_t auxiliaryArity = translator.getEvaluationArity(atom);
            assert(auxiliaryArity <= atom->getArity() && "auxiliary arity out of bounds");
            size_t arity = atom->getArity() - auxiliaryArity;
            std::vector<std::unique_ptr<RamExpression>> values;

            auto args = atom->getArguments();
            for (size_t i = 0; i < arity; i++) {
                values.push_back(translator.translateValue(args[i], index));
            }
            for (size_t i = 0; i < auxiliaryArity; i++) {
                values.push_back(std::make_unique<RamUndefValue>());
            }
            if (arity > 0) {
                return std::make_unique<RamNegation>(std::make_unique<RamExistenceCheck>(
                        translator.translateRelation(atom), std::move(values)));
            } else {
                return std::make_unique<RamEmptinessCheck>(translator.translateRelation(atom));
            }
        }

        /** for provenance negation */
        std::unique_ptr<RamCondition> visitProvenanceNegation(const AstProvenanceNegation& neg) override {
            const auto* atom = neg.getAtom();
            size_t auxiliaryArity = translator.getEvaluationArity(atom);
            assert(auxiliaryArity < atom->getArity() && "auxiliary arity out of bounds");
            size_t arity = atom->getArity() - auxiliaryArity;
            std::vector<std::unique_ptr<RamExpression>> values;

            auto args = atom->getArguments();
            for (size_t i = 0; i < arity; i++) {
                values.push_back(translator.translateValue(args[i], index));
            }
            // we don't care about the provenance columns when doing the existence check
            if (Global::config().has("provenance")) {
                // undefined value for rule number
                values.push_back(std::make_unique<RamUndefValue>());
                // add the height annotation for provenanceNotExists
                for (size_t h = 0; h + 1 < auxiliaryArity; h++) {
                    values.push_back(translator.translateValue(args[arity + h + 1], index));
                }
            }
            return std::make_unique<RamNegation>(std::make_unique<RamProvenanceExistenceCheck>(
                    translator.translateRelation(atom), std::move(values)));
        }
    };
    return ConstraintTranslator(*this, index)(*lit);
}

std::unique_ptr<AstClause> AstTranslator::ClauseTranslator::getReorderedClause(
        const AstClause& clause, const int version) const {
    const auto plan = clause.getExecutionPlan();

    // check whether there is an imposed order constraint
    if (plan == nullptr) {
        return nullptr;
    }
    auto orders = plan->getOrders();
    if (orders.find(version) == orders.end()) {
        return nullptr;
    }

    // get the imposed order
    const auto& order = orders[version];

    // create a copy and fix order
    std::unique_ptr<AstClause> reorderedClause(clause.clone());

    // Change order to start at zero
    std::vector<unsigned int> newOrder(order->getOrder().size());
    std::transform(order->getOrder().begin(), order->getOrder().end(), newOrder.begin(),
            [](unsigned int i) -> unsigned int { return i - 1; });

    // re-order atoms
    reorderedClause.reset(reorderAtoms(reorderedClause.get(), newOrder));

    // clear other order and fix plan
    reorderedClause->clearExecutionPlan();

    return reorderedClause;
}

AstTranslator::ClauseTranslator::arg_list* AstTranslator::ClauseTranslator::getArgList(
        const AstNode* curNode, std::map<const AstNode*, std::unique_ptr<arg_list>>& nodeArgs) const {
    if (nodeArgs.count(curNode) == 0u) {
        if (auto rec = dynamic_cast<const AstRecordInit*>(curNode)) {
            nodeArgs[curNode] = std::make_unique<arg_list>(rec->getArguments());
        } else if (auto atom = dynamic_cast<const AstAtom*>(curNode)) {
            nodeArgs[curNode] = std::make_unique<arg_list>(atom->getArguments());
        } else {
            fatal("node type doesn't have arguments!");
        }
    }
    return nodeArgs[curNode].get();
}

void AstTranslator::ClauseTranslator::indexValues(const AstNode* curNode,
        std::map<const AstNode*, std::unique_ptr<arg_list>>& nodeArgs,
        std::map<const arg_list*, int>& arg_level, RamRelationReference* relation) {
    arg_list* cur = getArgList(curNode, nodeArgs);
    for (size_t pos = 0; pos < cur->size(); ++pos) {
        // get argument
        auto& arg = (*cur)[pos];

        // check for variable references
        if (auto var = dynamic_cast<const AstVariable*>(arg)) {
            if (pos < relation->get()->getArity()) {
                valueIndex.addVarReference(*var, arg_level[cur], pos, souffle::clone(relation));
            } else {
                valueIndex.addVarReference(*var, arg_level[cur], pos);
            }
        }

        // check for nested records
        if (auto rec = dynamic_cast<const AstRecordInit*>(arg)) {
            // introduce new nesting level for unpack
            op_nesting.push_back(rec);
            arg_level[getArgList(rec, nodeArgs)] = level++;

            // register location of record
            valueIndex.setRecordDefinition(*rec, arg_level[cur], pos);

            // resolve nested components
            indexValues(rec, nodeArgs, arg_level, relation);
        }
    }
}

/** index values in rule */
void AstTranslator::ClauseTranslator::createValueIndex(const AstClause& clause) {
    for (const auto* atom : getBodyLiterals<AstAtom>(clause)) {
        // std::map<const arg_list*, int> arg_level;
        std::map<const AstNode*, std::unique_ptr<arg_list>> nodeArgs;

        std::map<const arg_list*, int> arg_level;
        nodeArgs[atom] = std::make_unique<arg_list>(atom->getArguments());
        // the atom is obtained at the current level
        // increment nesting level for the atom
        arg_level[nodeArgs[atom].get()] = level++;
        op_nesting.push_back(atom);

        indexValues(atom, nodeArgs, arg_level, translator.translateRelation(atom).get());
    }

    // add aggregation functions
    visitDepthFirstPostOrder(clause, [&](const AstArgument& arg) {
        // returns the write-location for this generator (or none if an equiv arg was already seen)
        auto addGenerator = [&]() -> std::optional<int> {
            // The by-value compare means that we're effectively doing CSE for any
            // generator args during code-gen. This is a weird place to do this.
            if (any_of(generators, [&](auto* x) { return *x == arg; })) return {};
            generators.push_back(&arg);

            int aggLoc = level++;
            valueIndex.setGeneratorLoc(arg, Location({aggLoc, 0}));
            return aggLoc;
        };

        if (auto agg = dynamic_cast<const AstAggregator*>(&arg)) {
            if (auto aggLoc = addGenerator()) {
                // bind aggregator variables to locations
                const AstAtom& atom = dynamic_cast<const AstAtom&>(*agg->getBodyLiterals()[0]);
                size_t pos = 0;
                for (auto* arg : atom.getArguments()) {
                    if (const auto* var = dynamic_cast<const AstVariable*>(arg)) {
                        valueIndex.addVarReference(
                                *var, *aggLoc, (int)pos, translator.translateRelation(&atom));
                    }
                    ++pos;
                }
            }
        }

        auto func = dynamic_cast<const AstIntrinsicFunctor*>(&arg);
        if (func && func->getFunctionInfo()->multipleResults) {
            addGenerator();
        }
    });
}

std::unique_ptr<RamOperation> AstTranslator::ClauseTranslator::createOperation(const AstClause& clause) {
    const auto head = clause.getHead();

    std::vector<std::unique_ptr<RamExpression>> values;
    for (AstArgument* arg : head->getArguments()) {
        values.push_back(translator.translateValue(arg, valueIndex));
    }

    std::unique_ptr<RamOperation> project =
            std::make_unique<RamProject>(translator.translateRelation(head), std::move(values));

    if (head->getArity() == 0) {
        project = std::make_unique<RamFilter>(
                std::make_unique<RamEmptinessCheck>(translator.translateRelation(head)), std::move(project));
    }

    // check existence for original tuple if we have provenance
    // only if we don't compile
    if (Global::config().has("provenance") &&
            ((!Global::config().has("compile") && !Global::config().has("dl-program") &&
                    !Global::config().has("generate")))) {
        size_t auxiliaryArity = translator.getEvaluationArity(head);
        auto arity = head->getArity() - auxiliaryArity;
        std::vector<std::unique_ptr<RamExpression>> values;
        bool isVolatile = true;
        auto args = head->getArguments();

        // add args for original tuple
        for (size_t i = 0; i < arity; i++) {
            auto arg = args[i];
            // don't add counters
            visitDepthFirst(*arg, [&](const AstCounter&) { isVolatile = false; });
            values.push_back(translator.translateValue(arg, valueIndex));
        }
        for (size_t i = 0; i < auxiliaryArity; i++) {
            values.push_back(std::make_unique<RamUndefValue>());
        }
        if (isVolatile) {
            return std::make_unique<RamFilter>(
                    std::make_unique<RamNegation>(std::make_unique<RamExistenceCheck>(
                            translator.translateRelation(head), std::move(values))),
                    std::move(project));
        }
    }

    // build up insertion call
    return project;  // start with innermost
}

std::unique_ptr<RamOperation> AstTranslator::ProvenanceClauseTranslator::createOperation(
        const AstClause& clause) {
    std::vector<std::unique_ptr<RamExpression>> values;

    // get all values in the body
    for (AstLiteral* lit : clause.getBodyLiterals()) {
        if (auto atom = dynamic_cast<AstAtom*>(lit)) {
            for (AstArgument* arg : atom->getArguments()) {
                values.push_back(translator.translateValue(arg, valueIndex));
            }
        } else if (auto neg = dynamic_cast<AstNegation*>(lit)) {
            for (AstArgument* arg : neg->getAtom()->getArguments()) {
                values.push_back(translator.translateValue(arg, valueIndex));
            }
        } else if (auto con = dynamic_cast<AstBinaryConstraint*>(lit)) {
            values.push_back(translator.translateValue(con->getLHS(), valueIndex));
            values.push_back(translator.translateValue(con->getRHS(), valueIndex));
        } else if (auto neg = dynamic_cast<AstProvenanceNegation*>(lit)) {
            size_t auxiliaryArity = translator.getEvaluationArity(neg->getAtom());
            for (size_t i = 0; i < neg->getAtom()->getArguments().size() - auxiliaryArity; ++i) {
                auto arg = neg->getAtom()->getArguments()[i];
                values.push_back(translator.translateValue(arg, valueIndex));
            }
            for (size_t i = 0; i < auxiliaryArity; ++i) {
                values.push_back(std::make_unique<RamSignedConstant>(-1));
            }
        }
    }

    return std::make_unique<RamSubroutineReturn>(std::move(values));
}

std::unique_ptr<RamCondition> AstTranslator::ClauseTranslator::createCondition(
        const AstClause& originalClause) {
    const auto head = originalClause.getHead();

    // add stopping criteria for nullary relations
    // (if it contains already the null tuple, don't re-compute)
    if (head->getArity() == 0) {
        return std::make_unique<RamEmptinessCheck>(translator.translateRelation(head));
    }
    return nullptr;
}

std::unique_ptr<RamCondition> AstTranslator::ProvenanceClauseTranslator::createCondition(
        const AstClause& /* originalClause */) {
    return nullptr;
}

std::unique_ptr<RamOperation> AstTranslator::ClauseTranslator::filterByConstraints(size_t const level,
        const std::vector<AstArgument*>& args, std::unique_ptr<RamOperation> op, bool constrainByFunctors) {
    size_t pos = 0;

    auto mkFilter = [&](bool isFloatArg, std::unique_ptr<RamExpression> rhs) {
        return std::make_unique<RamFilter>(
                std::make_unique<RamConstraint>(isFloatArg ? BinaryConstraintOp::FEQ : BinaryConstraintOp::EQ,
                        std::make_unique<RamTupleElement>(level, pos), std::move(rhs)),
                std::move(op));
    };

    for (auto* a : args) {
        if (auto* c = dynamic_cast<const AstConstant*>(a)) {
            auto* const c_num = dynamic_cast<const AstNumericConstant*>(c);
            assert((!c_num || c_num->getType()) && "numeric constant wasn't bound to a type");
            op = mkFilter(c_num && *c_num->getType() == AstNumericConstant::Type::Float,
                    translator.translateConstant(*c));
        } else if (auto* func = dynamic_cast<const AstFunctor*>(a)) {
            if (constrainByFunctors) {
                op = mkFilter(func->getReturnType() == TypeAttribute::Float,
                        translator.translateValue(func, valueIndex));
            }
        }

        ++pos;
    }

    return op;
}

/** generate RAM code for a clause */
std::unique_ptr<RamStatement> AstTranslator::ClauseTranslator::translateClause(
        const AstClause& clause, const AstClause& originalClause, const int version) {
    if (auto reorderedClause = getReorderedClause(clause, version)) {
        // translate reordered clause
        return translateClause(*reorderedClause, originalClause, version);
    }

    // get extract some details
    const AstAtom* head = clause.getHead();

    // handle facts
    if (isFact(clause)) {
        // translate arguments
        std::vector<std::unique_ptr<RamExpression>> values;
        for (auto& arg : head->getArguments()) {
            values.push_back(translator.translateValue(arg, ValueIndex()));
        }

        // create a fact statement
        return std::make_unique<RamQuery>(
                std::make_unique<RamProject>(translator.translateRelation(head), std::move(values)));
    }

    // the rest should be rules
    assert(isRule(clause));

    createValueIndex(clause);

    // -- create RAM statement --

    std::unique_ptr<RamOperation> op = createOperation(clause);

    /* add equivalence constraints imposed by variable binding */
    for (const auto& cur : valueIndex.getVariableReferences()) {
        // the first appearance
        const Location& first = *cur.second.begin();
        // all other appearances
        for (const Location& loc : cur.second) {
            if (first != loc && !valueIndex.isGenerator(loc.identifier)) {
                // FIXME: equiv' for float types (`FEQ`)
                op = std::make_unique<RamFilter>(
                        std::make_unique<RamConstraint>(
                                BinaryConstraintOp::EQ, makeRamTupleElement(first), makeRamTupleElement(loc)),
                        std::move(op));
            }
        }
    }

    /* add conditions caused by atoms, negations, and binary relations */
    for (const auto& lit : clause.getBodyLiterals()) {
        if (auto condition = translator.translateConstraint(lit, valueIndex)) {
            op = std::make_unique<RamFilter>(std::move(condition), std::move(op));
        }
    }

    // add aggregator conditions
    size_t curLevel = op_nesting.size() - 1;
    for (auto it = op_nesting.rbegin(); it != op_nesting.rend(); ++it, --curLevel) {
        const AstNode* cur = *it;

        if (const auto* atom = dynamic_cast<const AstAtom*>(cur)) {
            // add constraints
            size_t pos = 0;
            for (auto arg : atom->getArguments()) {
                if (auto* agg = dynamic_cast<AstAggregator*>(arg)) {
                    auto loc = valueIndex.getGeneratorLoc(*agg);
                    // FIXME: equiv' for float types (`FEQ`)
                    op = std::make_unique<RamFilter>(std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
                                                             std::make_unique<RamTupleElement>(curLevel, pos),
                                                             makeRamTupleElement(loc)),
                            std::move(op));
                }
                ++pos;
            }
        }
    }

    // add generator levels
    --level;
    for (auto* cur : reverse(generators)) {
        if (auto agg = dynamic_cast<const AstAggregator*>(cur)) {
            // condition for aggregate and helper function to add terms
            std::unique_ptr<RamCondition> aggCond;
            auto addAggCondition = [&](std::unique_ptr<RamCondition> arg) {
                aggCond = aggCond ? std::make_unique<RamConjunction>(std::move(aggCond), std::move(arg))
                                  : std::move(arg);
            };

            // translate constraints of sub-clause
            for (auto&& lit : agg->getBodyLiterals()) {
                if (auto newCondition = translator.translateConstraint(lit, valueIndex)) {
                    addAggCondition(std::move(newCondition));
                }
            }

            // get the first predicate of the sub-clause
            // NB: at most one atom is permitted in a sub-clause
            const AstAtom* atom = nullptr;
            for (auto&& lit : agg->getBodyLiterals()) {
                if (atom == nullptr) {
                    atom = dynamic_cast<const AstAtom*>(lit);
                } else {
                    assert(dynamic_cast<const AstAtom*>(lit) != nullptr &&
                            "Unsupported complex aggregation body encountered!");
                }
            }

            // translate arguments's of atom (if exists) to conditions
            if (atom != nullptr) {
                size_t pos = 0;
                auto addAggEqCondition = [&](std::unique_ptr<RamExpression> value) {
                    if (isRamUndefValue(value.get())) return;

                    // FIXME: equiv' for float types (`FEQ`)
                    addAggCondition(std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
                            std::make_unique<RamTupleElement>(level, pos), std::move(value)));
                };
                for (auto* arg : atom->getArguments()) {
                    // variable bindings are issued differently since we don't want self
                    // referential variable bindings
                    if (auto* var = dynamic_cast<const AstVariable*>(arg)) {
                        for (auto&& loc : valueIndex.getVariableReferences().find(var->getName())->second) {
                            if (level != loc.identifier || (int)pos != loc.element) {
                                addAggEqCondition(makeRamTupleElement(loc));
                                break;
                            }
                        }
                    } else if (auto value = translator.translateValue(arg, valueIndex)) {
                        addAggEqCondition(std::move(value));
                    }
                    ++pos;
                }
            }

            // translate aggregate expression
            auto expr = translator.translateValue(agg->getTargetExpression(), valueIndex);

            // add Ram-Aggregation layer
            op = std::make_unique<RamAggregate>(std::move(op), agg->getOperator(),
                    translator.translateRelation(atom),
                    expr ? std::move(expr) : std::make_unique<RamUndefValue>(),
                    aggCond ? std::move(aggCond) : std::make_unique<RamTrue>(), level);
        } else if (const auto* func = dynamic_cast<const AstIntrinsicFunctor*>(cur)) {
            std::vector<std::unique_ptr<RamExpression>> args;
            for (auto&& x : func->getArguments()) {
                args.push_back(translator.translateValue(x, valueIndex));
            }

            auto func_op = [&]() -> RamNestedIntrinsicOp {
                switch (func->getFunctionInfo()->op) {
                    case FunctorOp::RANGE: return RamNestedIntrinsicOp::RANGE;
                    case FunctorOp::URANGE: return RamNestedIntrinsicOp::URANGE;
                    case FunctorOp::FRANGE: return RamNestedIntrinsicOp::FRANGE;

                    default:
                        assert(func->getFunctionInfo()->multipleResults);
                        fatal("missing case handler or bad code-gen");
                }
            };

            op = std::make_unique<RamNestedIntrinsicOperator>(
                    func_op(), std::move(args), std::move(op), level);
        }

        --level;
    }

    // build operation bottom-up
    while (!op_nesting.empty()) {
        // get next operator
        const AstNode* cur = op_nesting.back();
        op_nesting.pop_back();

        // get current nesting level
        auto level = op_nesting.size();

        if (const auto* atom = dynamic_cast<const AstAtom*>(cur)) {
            // add constraints
            // TODO: do we wish to enable constraints by header functor? record inits do so...
            op = filterByConstraints(level, atom->getArguments(), std::move(op), false);

            // check whether all arguments are unnamed variables
            bool isAllArgsUnnamed = true;
            for (auto* argument : atom->getArguments()) {
                if (dynamic_cast<AstUnnamedVariable*>(argument) == nullptr) {
                    isAllArgsUnnamed = false;
                }
            }

            // add check for emptiness for an atom
            op = std::make_unique<RamFilter>(
                    std::make_unique<RamNegation>(
                            std::make_unique<RamEmptinessCheck>(translator.translateRelation(atom))),
                    std::move(op));

            // add a scan level
            if (atom->getArity() != 0 && !isAllArgsUnnamed) {
                if (head->getArity() == 0) {
                    op = std::make_unique<RamBreak>(
                            std::make_unique<RamNegation>(
                                    std::make_unique<RamEmptinessCheck>(translator.translateRelation(head))),
                            std::move(op));
                }
                if (Global::config().has("profile")) {
                    std::stringstream ss;
                    ss << head->getQualifiedName();
                    ss.str("");
                    ss << "@frequency-atom" << ';';
                    ss << originalClause.getHead()->getQualifiedName() << ';';
                    ss << version << ';';
                    ss << stringify(toString(clause)) << ';';
                    ss << stringify(toString(*atom)) << ';';
                    ss << stringify(toString(originalClause)) << ';';
                    ss << level << ';';
                    op = std::make_unique<RamScan>(
                            translator.translateRelation(atom), level, std::move(op), ss.str());
                } else {
                    op = std::make_unique<RamScan>(translator.translateRelation(atom), level, std::move(op));
                }
            }

            // TODO: support constants in nested records!
        } else if (const auto* rec = dynamic_cast<const AstRecordInit*>(cur)) {
            // add constant constraints
            op = filterByConstraints(level, rec->getArguments(), std::move(op));

            // add an unpack level
            const Location& loc = valueIndex.getDefinitionPoint(*rec);
            op = std::make_unique<RamUnpackRecord>(
                    std::move(op), level, makeRamTupleElement(loc), rec->getArguments().size());
        } else {
            fatal("Unsupported AST node for creation of scan-level!");
        }
    }

    /* generate the final RAM Insert statement */
    std::unique_ptr<RamCondition> cond = createCondition(originalClause);
    if (cond != nullptr) {
        return std::make_unique<RamQuery>(std::make_unique<RamFilter>(std::move(cond), std::move(op)));
    } else {
        return std::make_unique<RamQuery>(std::move(op));
    }
}

std::unique_ptr<RamExpression> AstTranslator::translateConstant(AstConstant const& c) {
    auto const rawConstant = getConstantRamRepresentation(c);

    if (auto* const c_num = dynamic_cast<const AstNumericConstant*>(&c)) {
        switch (*c_num->getType()) {
            case AstNumericConstant::Type::Int: return std::make_unique<RamSignedConstant>(rawConstant);
            case AstNumericConstant::Type::Uint: return std::make_unique<RamUnsignedConstant>(rawConstant);
            case AstNumericConstant::Type::Float: return std::make_unique<RamFloatConstant>(rawConstant);
        }
    }

    return std::make_unique<RamSignedConstant>(rawConstant);
}

/** generate RAM code for a non-recursive relation */
std::unique_ptr<RamStatement> AstTranslator::translateNonRecursiveRelation(
        const AstRelation& rel, const RecursiveClauses* recursiveClauses) {
    /* start with an empty sequence */
    std::vector<std::unique_ptr<RamStatement>> res;

    // the ram table reference
    std::unique_ptr<RamRelationReference> rrel = translateRelation(&rel);

    /* iterate over all clauses that belong to the relation */
    for (AstClause* clause : getClauses(*program, rel)) {
        // skip recursive rules
        if (recursiveClauses->recursive(clause)) {
            continue;
        }

        // translate clause
        std::unique_ptr<RamStatement> rule = ClauseTranslator(*this).translateClause(*clause, *clause);

        // add logging
        if (Global::config().has("profile")) {
            const std::string& relationName = toString(rel.getQualifiedName());
            const SrcLocation& srcLocation = clause->getSrcLoc();
            const std::string clauseText = stringify(toString(*clause));
            const std::string logTimerStatement =
                    LogStatement::tNonrecursiveRule(relationName, srcLocation, clauseText);
            const std::string logSizeStatement =
                    LogStatement::nNonrecursiveRule(relationName, srcLocation, clauseText);
            rule = std::make_unique<RamLogRelationTimer>(
                    std::move(rule), logTimerStatement, souffle::clone(rrel));
        }

        // add debug info
        std::ostringstream ds;
        ds << toString(*clause) << "\nin file ";
        ds << clause->getSrcLoc();
        rule = std::make_unique<RamDebugInfo>(std::move(rule), ds.str());

        // add rule to result
        appendStmt(res, std::move(rule));
    }

    // add logging for entire relation
    if (Global::config().has("profile")) {
        const std::string& relationName = toString(rel.getQualifiedName());
        const SrcLocation& srcLocation = rel.getSrcLoc();
        const std::string logSizeStatement = LogStatement::nNonrecursiveRelation(relationName, srcLocation);

        // add timer if we did any work
        if (!res.empty()) {
            const std::string logTimerStatement =
                    LogStatement::tNonrecursiveRelation(relationName, srcLocation);
            auto newStmt = std::make_unique<RamLogRelationTimer>(
                    std::make_unique<RamSequence>(std::move(res)), logTimerStatement, souffle::clone(rrel));
            res.clear();
            appendStmt(res, std::move(newStmt));
        } else {
            // add table size printer
            appendStmt(res, std::make_unique<RamLogSize>(souffle::clone(rrel), logSizeStatement));
        }
    }

    // done
    return std::make_unique<RamSequence>(std::move(res));
}

/**
 * A utility function assigning names to unnamed variables such that enclosing
 * constructs may be cloned without losing the variable-identity.
 */
void AstTranslator::nameUnnamedVariables(AstClause* clause) {
    // the node mapper conducting the actual renaming
    struct Instantiator : public AstNodeMapper {
        mutable int counter = 0;

        Instantiator() = default;

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            // apply recursive
            node->apply(*this);

            // replace unknown variables
            if (dynamic_cast<AstUnnamedVariable*>(node.get()) != nullptr) {
                auto name = " _unnamed_var" + toString(++counter);
                return std::make_unique<AstVariable>(name);
            }

            // otherwise nothing
            return node;
        }
    };

    // name all variables in the atoms
    Instantiator init;
    for (auto& atom : getBodyLiterals<AstAtom>(*clause)) {
        atom->apply(init);
    }
}

/** generate RAM code for recursive relations in a strongly-connected component */
std::unique_ptr<RamStatement> AstTranslator::translateRecursiveRelation(
        const std::set<const AstRelation*>& scc, const RecursiveClauses* recursiveClauses) {
    // initialize sections
    std::vector<std::unique_ptr<RamStatement>> preamble;
    std::vector<std::unique_ptr<RamStatement>> updateTable;
    std::vector<std::unique_ptr<RamStatement>> postamble;

    auto genMerge = [](const RamRelationReference* dest,
                            const RamRelationReference* src) -> std::unique_ptr<RamStatement> {
        std::vector<std::unique_ptr<RamExpression>> values;
        if (src->get()->getArity() == 0) {
            return std::make_unique<RamQuery>(std::make_unique<RamFilter>(
                    std::make_unique<RamNegation>(std::make_unique<RamEmptinessCheck>(souffle::clone(src))),
                    std::make_unique<RamProject>(souffle::clone(dest), std::move(values))));
        }
        for (std::size_t i = 0; i < dest->get()->getArity(); i++) {
            values.push_back(std::make_unique<RamTupleElement>(0, i));
        }
        auto stmt = std::make_unique<RamQuery>(std::make_unique<RamScan>(souffle::clone(src), 0,
                std::make_unique<RamProject>(souffle::clone(dest), std::move(values))));
        if (dest->get()->getRepresentation() == RelationRepresentation::EQREL) {
            return std::make_unique<RamSequence>(
                    std::make_unique<RamExtend>(souffle::clone(dest), souffle::clone(src)), std::move(stmt));
        }
        return stmt;
    };

    // --- create preamble ---

    /* Compute non-recursive clauses for relations in scc and push
       the results in their delta tables. */
    for (const AstRelation* rel : scc) {
        /* create update statements for fixpoint (even iteration) */
        std::unique_ptr<RamStatement> updateRelTable = std::make_unique<RamSequence>(
                genMerge(translateRelation(rel).get(), translateNewRelation(rel).get()),
                std::make_unique<RamSwap>(translateDeltaRelation(rel), translateNewRelation(rel)),
                std::make_unique<RamClear>(translateNewRelation(rel)));

        /* measure update time for each relation */
        if (Global::config().has("profile")) {
            updateRelTable = std::make_unique<RamLogRelationTimer>(std::move(updateRelTable),
                    LogStatement::cRecursiveRelation(toString(rel->getQualifiedName()), rel->getSrcLoc()),
                    translateNewRelation(rel));
        }

        /* drop temporary tables after recursion */
        appendStmt(postamble, std::make_unique<RamClear>(translateDeltaRelation(rel)));
        appendStmt(postamble, std::make_unique<RamClear>(translateNewRelation(rel)));

        /* Generate code for non-recursive part of relation */
        /* Generate merge operation for temp tables */
        appendStmt(preamble, translateNonRecursiveRelation(*rel, recursiveClauses));
        appendStmt(preamble, genMerge(translateDeltaRelation(rel).get(), translateRelation(rel).get()));

        /* Add update operations of relations to parallel statements */
        appendStmt(updateTable, std::move(updateRelTable));
    }

    // --- build main loop ---

    std::vector<std::unique_ptr<RamStatement>> loopSeq;

    // create a utility to check SCC membership
    auto isInSameSCC = [&](const AstRelation* rel) {
        return std::find(scc.begin(), scc.end(), rel) != scc.end();
    };

    /* Compute temp for the current tables */
    for (const AstRelation* rel : scc) {
        std::vector<std::unique_ptr<RamStatement>> loopRelSeq;

        /* Find clauses for relation rel */
        for (const auto& cl : getClauses(*program, *rel)) {
            // skip non-recursive clauses
            if (!recursiveClauses->recursive(cl)) {
                continue;
            }

            // each recursive rule results in several operations
            int version = 0;
            const auto& atoms = getBodyLiterals<AstAtom>(*cl);
            for (size_t j = 0; j < atoms.size(); ++j) {
                const AstAtom* atom = atoms[j];
                const AstRelation* atomRelation = getAtomRelation(atom, program);

                // only interested in atoms within the same SCC
                if (!isInSameSCC(atomRelation)) {
                    continue;
                }

                // modify the processed rule to use delta relation and write to new relation
                std::unique_ptr<AstClause> r1(cl->clone());
                r1->getHead()->setQualifiedName(translateNewRelation(rel)->get()->getName());
                getBodyLiterals<AstAtom>(*r1)[j]->setQualifiedName(
                        translateDeltaRelation(atomRelation)->get()->getName());
                if (Global::config().has("provenance")) {
                    r1->addToBody(std::make_unique<AstProvenanceNegation>(souffle::clone(cl->getHead())));
                } else {
                    if (r1->getHead()->getArity() > 0) {
                        r1->addToBody(std::make_unique<AstNegation>(souffle::clone(cl->getHead())));
                    }
                }

                // replace wildcards with variables (reduces indices when wildcards are used in recursive
                // atoms)
                nameUnnamedVariables(r1.get());

                // reduce R to P ...
                for (size_t k = j + 1; k < atoms.size(); k++) {
                    if (isInSameSCC(getAtomRelation(atoms[k], program))) {
                        auto cur = souffle::clone(getBodyLiterals<AstAtom>(*r1)[k]);
                        cur->setQualifiedName(
                                translateDeltaRelation(getAtomRelation(atoms[k], program))->get()->getName());
                        r1->addToBody(std::make_unique<AstNegation>(std::move(cur)));
                    }
                }

                std::unique_ptr<RamStatement> rule =
                        ClauseTranslator(*this).translateClause(*r1, *cl, version);

                /* add logging */
                if (Global::config().has("profile")) {
                    const std::string& relationName = toString(rel->getQualifiedName());
                    const SrcLocation& srcLocation = cl->getSrcLoc();
                    const std::string clauseText = stringify(toString(*cl));
                    const std::string logTimerStatement =
                            LogStatement::tRecursiveRule(relationName, version, srcLocation, clauseText);
                    const std::string logSizeStatement =
                            LogStatement::nRecursiveRule(relationName, version, srcLocation, clauseText);
                    rule = std::make_unique<RamLogRelationTimer>(
                            std::move(rule), logTimerStatement, translateNewRelation(rel));
                }

                // add debug info
                std::ostringstream ds;
                ds << toString(*cl) << "\nin file ";
                ds << cl->getSrcLoc();
                rule = std::make_unique<RamDebugInfo>(std::move(rule), ds.str());

                // add to loop body
                appendStmt(loopRelSeq, std::move(rule));

                // increment version counter
                version++;
            }

            if (cl->getExecutionPlan() != nullptr) {
                // ensure that all required versions have been created, as expected
                int maxVersion = -1;
                for (auto const& cur : cl->getExecutionPlan()->getOrders()) {
                    maxVersion = std::max(cur.first, maxVersion);
                }
                assert(version > maxVersion && "missing clause versions");
            }
        }

        // if there was no rule, continue
        if (loopRelSeq.size() == 0) {
            continue;
        }

        // label all versions
        if (Global::config().has("profile")) {
            const std::string& relationName = toString(rel->getQualifiedName());
            const SrcLocation& srcLocation = rel->getSrcLoc();
            const std::string logTimerStatement = LogStatement::tRecursiveRelation(relationName, srcLocation);
            const std::string logSizeStatement = LogStatement::nRecursiveRelation(relationName, srcLocation);
            auto newStmt = std::make_unique<RamLogRelationTimer>(
                    std::make_unique<RamSequence>(std::move(loopRelSeq)), logTimerStatement,
                    translateNewRelation(rel));
            loopRelSeq.clear();
            appendStmt(loopRelSeq, std::move(newStmt));
        }

        /* add rule computations of a relation to parallel statement */
        appendStmt(loopSeq, std::make_unique<RamSequence>(std::move(loopRelSeq)));
    }
    auto loop = std::make_unique<RamParallel>(std::move(loopSeq));

    /* construct exit conditions for odd and even iteration */
    auto addCondition = [](std::unique_ptr<RamCondition>& cond, std::unique_ptr<RamCondition> clause) {
        cond = ((cond) ? std::make_unique<RamConjunction>(std::move(cond), std::move(clause))
                       : std::move(clause));
    };

    std::unique_ptr<RamCondition> exitCond;
    for (const AstRelation* rel : scc) {
        addCondition(exitCond, std::make_unique<RamEmptinessCheck>(translateNewRelation(rel)));
    }

    /* construct fixpoint loop  */
    std::vector<std::unique_ptr<RamStatement>> res;
    if (preamble.size() > 0) {
        appendStmt(res, std::make_unique<RamSequence>(std::move(preamble)));
    }
    if (!loop->getStatements().empty() && exitCond && updateTable.size() > 0) {
        appendStmt(res, std::make_unique<RamLoop>(std::make_unique<RamSequence>(std::move(loop),
                                std::make_unique<RamExit>(std::move(exitCond)),
                                std::make_unique<RamSequence>(std::move(updateTable)))));
    }
    if (postamble.size() > 0) {
        appendStmt(res, std::make_unique<RamSequence>(std::move(postamble)));
    }
    if (res.size() > 0) {
        return std::make_unique<RamSequence>(std::move(res));
    }

    fatal("Not Implemented");
}

/** make a subroutine to search for subproofs */
std::unique_ptr<RamStatement> AstTranslator::makeSubproofSubroutine(const AstClause& clause) {
    auto intermediateClause = std::make_unique<AstClause>(souffle::clone(clause.getHead()));

    // create a clone where all the constraints are moved to the end
    for (auto bodyLit : clause.getBodyLiterals()) {
        // first add all the things that are not constraints
        if (dynamic_cast<AstConstraint*>(bodyLit) == nullptr) {
            intermediateClause->addToBody(souffle::clone(bodyLit));
        }
    }

    // now add all constraints
    for (auto bodyLit : getBodyLiterals<AstConstraint>(clause)) {
        intermediateClause->addToBody(souffle::clone(bodyLit));
    }

    // name unnamed variables
    nameUnnamedVariables(intermediateClause.get());

    // add constraint for each argument in head of atom
    AstAtom* head = intermediateClause->getHead();
    size_t auxiliaryArity = auxArityAnalysis->getArity(head);
    auto args = head->getArguments();
    for (size_t i = 0; i < head->getArity() - auxiliaryArity; i++) {
        auto arg = args[i];

        if (auto var = dynamic_cast<AstVariable*>(arg)) {
            // FIXME: float equiv (`FEQ`)
            intermediateClause->addToBody(std::make_unique<AstBinaryConstraint>(
                    BinaryConstraintOp::EQ, souffle::clone(var), std::make_unique<AstSubroutineArgument>(i)));
        } else if (auto func = dynamic_cast<AstFunctor*>(arg)) {
            auto opEq = func->getReturnType() == TypeAttribute::Float ? BinaryConstraintOp::FEQ
                                                                      : BinaryConstraintOp::EQ;
            intermediateClause->addToBody(std::make_unique<AstBinaryConstraint>(
                    opEq, souffle::clone(func), std::make_unique<AstSubroutineArgument>(i)));
        } else if (auto rec = dynamic_cast<AstRecordInit*>(arg)) {
            intermediateClause->addToBody(std::make_unique<AstBinaryConstraint>(
                    BinaryConstraintOp::EQ, souffle::clone(rec), std::make_unique<AstSubroutineArgument>(i)));
        }
    }

    if (Global::config().get("provenance") == "subtreeHeights") {
        // starting index of subtree level arguments in argument list
        // starts immediately after original arguments as height and rulenumber of tuple are not passed to
        // subroutine
        size_t levelIndex = head->getArguments().size() - auxiliaryArity;

        // add level constraints
        const auto& bodyLiterals = intermediateClause->getBodyLiterals();
        for (auto lit : bodyLiterals) {
            if (auto atom = dynamic_cast<AstAtom*>(lit)) {
                auto arity = atom->getArity();
                auto auxiliaryArity = auxArityAnalysis->getArity(atom);
                auto literalLevelIndex = arity - auxiliaryArity + 1;
                auto atomArgs = atom->getArguments();
                // FIXME: float equiv (`FEQ`)
                intermediateClause->addToBody(std::make_unique<AstBinaryConstraint>(BinaryConstraintOp::EQ,
                        souffle::clone(atomArgs[literalLevelIndex]),
                        std::make_unique<AstSubroutineArgument>(levelIndex)));
            }
            levelIndex++;
        }
    } else {
        // index of level argument in argument list
        size_t levelIndex = head->getArguments().size() - auxiliaryArity;

        // add level constraints
        const auto& bodyLiterals = intermediateClause->getBodyLiterals();
        for (auto lit : bodyLiterals) {
            if (auto atom = dynamic_cast<AstAtom*>(lit)) {
                auto arity = atom->getArity();
                auto atomArgs = atom->getArguments();
                // arity - 1 is the level number in body atoms
                intermediateClause->addToBody(std::make_unique<AstBinaryConstraint>(BinaryConstraintOp::LT,
                        souffle::clone(atomArgs[arity - 1]),
                        std::make_unique<AstSubroutineArgument>(levelIndex)));
            }
        }
    }
    return ProvenanceClauseTranslator(*this).translateClause(*intermediateClause, clause);
}

/** make a subroutine to search for subproofs for the non-existence of a tuple */
std::unique_ptr<RamStatement> AstTranslator::makeNegationSubproofSubroutine(const AstClause& clause) {
    // TODO (taipan-snake): Currently we only deal with atoms (no constraints or negations or aggregates
    // or anything else...)
    //
    // The resulting subroutine looks something like this:
    // IF (arg(0), arg(1), _, _) IN rel_1:
    //   return 1
    // IF (arg(0), arg(1), _ ,_) NOT IN rel_1:
    //   return 0
    // ...

    // clone clause for mutation, rearranging constraints to be at the end
    auto clauseReplacedAggregates = std::make_unique<AstClause>(souffle::clone(clause.getHead()));

    // create a clone where all the constraints are moved to the end
    for (auto bodyLit : clause.getBodyLiterals()) {
        // first add all the things that are not constraints
        if (dynamic_cast<AstConstraint*>(bodyLit) == nullptr) {
            clauseReplacedAggregates->addToBody(souffle::clone(bodyLit));
        }
    }

    // now add all constraints
    for (auto bodyLit : getBodyLiterals<AstConstraint>(clause)) {
        clauseReplacedAggregates->addToBody(souffle::clone(bodyLit));
    }

    int aggNumber = 0;
    struct AggregatesToVariables : public AstNodeMapper {
        int& aggNumber;

        AggregatesToVariables(int& aggNumber) : aggNumber(aggNumber) {}

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            if (dynamic_cast<AstAggregator*>(node.get()) != nullptr) {
                return std::make_unique<AstVariable>("agg_" + std::to_string(aggNumber++));
            }

            node->apply(*this);
            return node;
        }
    };

    AggregatesToVariables aggToVar(aggNumber);
    clauseReplacedAggregates->apply(aggToVar);

    // build a vector of unique variables
    std::vector<const AstVariable*> uniqueVariables;

    visitDepthFirst(*clauseReplacedAggregates, [&](const AstVariable& var) {
        if (var.getName().find("@level_num") == std::string::npos) {
            // use find_if since uniqueVariables stores pointers, and we need to dereference the pointer to
            // check equality
            if (std::find_if(uniqueVariables.begin(), uniqueVariables.end(),
                        [&](const AstVariable* v) { return *v == var; }) == uniqueVariables.end()) {
                uniqueVariables.push_back(&var);
            }
        }
    });

    // a mapper to replace variables with subroutine arguments
    struct VariablesToArguments : public AstNodeMapper {
        const std::vector<const AstVariable*>& uniqueVariables;

        VariablesToArguments(const std::vector<const AstVariable*>& uniqueVariables)
                : uniqueVariables(uniqueVariables) {}

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            // replace unknown variables
            if (auto varPtr = dynamic_cast<const AstVariable*>(node.get())) {
                if (varPtr->getName().find("@level_num") == std::string::npos) {
                    size_t argNum = std::find_if(uniqueVariables.begin(), uniqueVariables.end(),
                                            [&](const AstVariable* v) { return *v == *varPtr; }) -
                                    uniqueVariables.begin();

                    return std::make_unique<AstSubroutineArgument>(argNum);
                } else {
                    return std::make_unique<AstUnnamedVariable>();
                }
            }

            // apply recursive
            node->apply(*this);

            // otherwise nothing
            return node;
        }
    };

    // the structure of this subroutine is a sequence where each nested statement is a search in each
    // relation
    std::vector<std::unique_ptr<RamStatement>> searchSequence;

    // make a copy so that when we mutate clause, pointers to objects in newClause are not affected
    auto newClause = souffle::clone(clauseReplacedAggregates);

    // go through each body atom and create a return
    size_t litNumber = 0;
    for (const auto& lit : newClause->getBodyLiterals()) {
        if (auto atom = dynamic_cast<AstAtom*>(lit)) {
            size_t auxiliaryArity = auxArityAnalysis->getArity(atom);
            // get a RamRelationReference
            auto relRef = translateRelation(atom);
            // construct a query
            std::vector<std::unique_ptr<RamExpression>> query;

            // translate variables to subroutine arguments
            VariablesToArguments varsToArgs(uniqueVariables);
            atom->apply(varsToArgs);

            auto atomArgs = atom->getArguments();
            // add each value (subroutine argument) to the search query
            for (size_t i = 0; i < atom->getArity() - auxiliaryArity; i++) {
                auto arg = atomArgs[i];
                query.push_back(translateValue(arg, ValueIndex()));
            }

            // fill up query with nullptrs for the provenance columns
            for (size_t i = 0; i < auxiliaryArity; i++) {
                query.push_back(std::make_unique<RamUndefValue>());
            }

            // ensure the length of query tuple is correct
            assert(query.size() == atom->getArity() && "wrong query tuple size");

            // create existence checks to check if the tuple exists or not
            auto existenceCheck =
                    std::make_unique<RamExistenceCheck>(souffle::clone(relRef), std::move(query));
            auto negativeExistenceCheck = std::make_unique<RamNegation>(souffle::clone(existenceCheck));

            // return true if the tuple exists
            std::vector<std::unique_ptr<RamExpression>> returnTrue;
            returnTrue.push_back(std::make_unique<RamSignedConstant>(1));

            // return false if the tuple exists
            std::vector<std::unique_ptr<RamExpression>> returnFalse;
            returnFalse.push_back(std::make_unique<RamSignedConstant>(0));

            // create a RamQuery to return true/false
            appendStmt(searchSequence,
                    std::make_unique<RamQuery>(std::make_unique<RamFilter>(std::move(existenceCheck),
                            std::make_unique<RamSubroutineReturn>(std::move(returnTrue)))));
            appendStmt(searchSequence,
                    std::make_unique<RamQuery>(std::make_unique<RamFilter>(std::move(negativeExistenceCheck),
                            std::make_unique<RamSubroutineReturn>(std::move(returnFalse)))));
        } else if (auto neg = dynamic_cast<AstNegation*>(lit)) {
            auto atom = neg->getAtom();

            size_t auxiliaryArity = auxArityAnalysis->getArity(atom);
            // get a RamRelationReference
            auto relRef = translateRelation(atom);
            // construct a query
            std::vector<std::unique_ptr<RamExpression>> query;

            // translate variables to subroutine arguments
            VariablesToArguments varsToArgs(uniqueVariables);
            atom->apply(varsToArgs);

            auto atomArgs = atom->getArguments();
            // add each value (subroutine argument) to the search query
            for (size_t i = 0; i < atom->getArity() - auxiliaryArity; i++) {
                auto arg = atomArgs[i];
                query.push_back(translateValue(arg, ValueIndex()));
            }

            // fill up query with nullptrs for the provenance columns
            for (size_t i = 0; i < auxiliaryArity; i++) {
                query.push_back(std::make_unique<RamUndefValue>());
            }

            // ensure the length of query tuple is correct
            assert(query.size() == atom->getArity() && "wrong query tuple size");

            // create existence checks to check if the tuple exists or not
            auto existenceCheck =
                    std::make_unique<RamExistenceCheck>(souffle::clone(relRef), std::move(query));
            auto negativeExistenceCheck = std::make_unique<RamNegation>(souffle::clone(existenceCheck));

            // return true if the tuple exists
            std::vector<std::unique_ptr<RamExpression>> returnTrue;
            returnTrue.push_back(std::make_unique<RamSignedConstant>(1));

            // return false if the tuple exists
            std::vector<std::unique_ptr<RamExpression>> returnFalse;
            returnFalse.push_back(std::make_unique<RamSignedConstant>(0));

            // create a RamQuery to return true/false
            appendStmt(searchSequence,
                    std::make_unique<RamQuery>(std::make_unique<RamFilter>(std::move(existenceCheck),
                            std::make_unique<RamSubroutineReturn>(std::move(returnFalse)))));
            appendStmt(searchSequence,
                    std::make_unique<RamQuery>(std::make_unique<RamFilter>(std::move(negativeExistenceCheck),
                            std::make_unique<RamSubroutineReturn>(std::move(returnTrue)))));

        } else if (auto con = dynamic_cast<AstConstraint*>(lit)) {
            VariablesToArguments varsToArgs(uniqueVariables);
            con->apply(varsToArgs);

            // translate to a RamCondition
            auto condition = translateConstraint(con, ValueIndex());
            auto negativeCondition = std::make_unique<RamNegation>(souffle::clone(condition));

            // create a return true value
            std::vector<std::unique_ptr<RamExpression>> returnTrue;
            returnTrue.push_back(std::make_unique<RamSignedConstant>(1));

            // create a return false value
            std::vector<std::unique_ptr<RamExpression>> returnFalse;
            returnFalse.push_back(std::make_unique<RamSignedConstant>(0));

            appendStmt(searchSequence,
                    std::make_unique<RamQuery>(std::make_unique<RamFilter>(std::move(condition),
                            std::make_unique<RamSubroutineReturn>(std::move(returnTrue)))));
            appendStmt(searchSequence,
                    std::make_unique<RamQuery>(std::make_unique<RamFilter>(std::move(negativeCondition),
                            std::make_unique<RamSubroutineReturn>(std::move(returnFalse)))));
        }

        litNumber++;
    }

    return std::make_unique<RamSequence>(std::move(searchSequence));
}

/** translates the given datalog program into an equivalent RAM program  */
void AstTranslator::translateProgram(const AstTranslationUnit& translationUnit) {
    // obtain type environment from analysis
    typeEnv = &translationUnit.getAnalysis<TypeEnvironmentAnalysis>()->getTypeEnvironment();

    // obtain recursive clauses from analysis
    const auto* recursiveClauses = translationUnit.getAnalysis<RecursiveClauses>();

    // obtain strongly connected component (SCC) graph from analysis
    const auto& sccGraph = *translationUnit.getAnalysis<SCCGraph>();

    // obtain some topological order over the nodes of the SCC graph
    const auto& sccOrder = *translationUnit.getAnalysis<TopologicallySortedSCCGraph>();

    // obtain the schedule of relations expired at each index of the topological order
    const auto& expirySchedule = translationUnit.getAnalysis<RelationSchedule>()->schedule();

    // get auxiliary arity analysis
    auxArityAnalysis = translationUnit.getAnalysis<AuxiliaryArity>();

    // handle the case of an empty SCC graph
    if (sccGraph.getNumberOfSCCs() == 0) return;

    // a function to load relations
    const auto& makeRamLoad = [&](std::vector<std::unique_ptr<RamStatement>>& current,
                                      const AstRelation* relation) {
        for (auto directives : getInputDirectives(relation)) {
            std::unique_ptr<RamStatement> statement = std::make_unique<RamIO>(
                    std::unique_ptr<RamRelationReference>(translateRelation(relation)), directives);
            if (Global::config().has("profile")) {
                const std::string logTimerStatement = LogStatement::tRelationLoadTime(
                        toString(relation->getQualifiedName()), relation->getSrcLoc());
                statement = std::make_unique<RamLogRelationTimer>(std::move(statement), logTimerStatement,
                        std::unique_ptr<RamRelationReference>(translateRelation(relation)));
            }
            appendStmt(current, std::move(statement));
        }
    };

    // a function to store relations
    const auto& makeRamStore = [&](std::vector<std::unique_ptr<RamStatement>>& current,
                                       const AstRelation* relation) {
        for (auto directives : getOutputDirectives(relation)) {
            std::unique_ptr<RamStatement> statement = std::make_unique<RamIO>(
                    std::unique_ptr<RamRelationReference>(translateRelation(relation)), directives);
            if (Global::config().has("profile")) {
                const std::string logTimerStatement = LogStatement::tRelationSaveTime(
                        toString(relation->getQualifiedName()), relation->getSrcLoc());
                statement = std::make_unique<RamLogRelationTimer>(std::move(statement), logTimerStatement,
                        std::unique_ptr<RamRelationReference>(translateRelation(relation)));
            }
            appendStmt(current, std::move(statement));
        }
    };

    // a function to drop relations
    const auto& makeRamClear = [&](std::vector<std::unique_ptr<RamStatement>>& current,
                                       const AstRelation* relation) {
        appendStmt(current, std::make_unique<RamClear>(translateRelation(relation)));
    };

    // maintain the index of the SCC within the topological order
    size_t indexOfScc = 0;

    // create all Ram relations in ramRels
    for (const auto& scc : sccOrder.order()) {
        const auto& isRecursive = sccGraph.isRecursive(scc);
        const auto& allInterns = sccGraph.getInternalRelations(scc);
        for (const auto& rel : allInterns) {
            std::string name = rel->getQualifiedName().toString();
            auto arity = rel->getArity();
            auto auxiliaryArity = auxArityAnalysis->getArity(rel);
            auto representation = rel->getRepresentation();
            const auto& attributes = rel->getAttributes();
            std::vector<std::string> attributeNames;
            std::vector<std::string> attributeTypeQualifiers;
            for (size_t i = 0; i < rel->getArity(); ++i) {
                attributeNames.push_back(attributes[i]->getName());
                if (typeEnv != nullptr) {
                    attributeTypeQualifiers.push_back(
                            getTypeQualifier(typeEnv->getType(attributes[i]->getTypeName())));
                }
            }
            ramRels[name] = std::make_unique<RamRelation>(
                    name, arity, auxiliaryArity, attributeNames, attributeTypeQualifiers, representation);
            if (isRecursive) {
                std::string deltaName = "@delta_" + name;
                std::string newName = "@new_" + name;
                ramRels[deltaName] = std::make_unique<RamRelation>(deltaName, arity, auxiliaryArity,
                        attributeNames, attributeTypeQualifiers, representation);
                ramRels[newName] = std::make_unique<RamRelation>(newName, arity, auxiliaryArity,
                        attributeNames, attributeTypeQualifiers, representation);
            }
        }
    }
    // iterate over each SCC according to the topological order
    for (const auto& scc : sccOrder.order()) {
        // make a new ram statement for the current SCC
        std::vector<std::unique_ptr<RamStatement>> current;

        // find out if the current SCC is recursive
        const auto& isRecursive = sccGraph.isRecursive(scc);

        // make variables for particular sets of relations contained within the current SCC, and,
        // predecessors and successor SCCs thereof
        const auto& allInterns = sccGraph.getInternalRelations(scc);
        const auto& internIns = sccGraph.getInternalInputRelations(scc);
        const auto& internOuts = sccGraph.getInternalOutputRelations(scc);

        // make a variable for all relations that are expired at the current SCC
        const auto& internExps = expirySchedule.at(indexOfScc).expired();

        // load all internal input relations from the facts dir with a .facts extension
        for (const auto& relation : internIns) {
            makeRamLoad(current, relation);
        }

        // compute the relations themselves
        std::unique_ptr<RamStatement> bodyStatement =
                (!isRecursive) ? translateNonRecursiveRelation(
                                         *((const AstRelation*)*allInterns.begin()), recursiveClauses)
                               : translateRecursiveRelation(allInterns, recursiveClauses);
        appendStmt(current, std::move(bodyStatement));

        // store all internal output relations to the output dir with a .csv extension
        for (const auto& relation : internOuts) {
            makeRamStore(current, relation);
        }

        // if provenance is not enabled...
        if (!Global::config().has("provenance")) {
            // otherwise, drop all  relations expired as per the topological order
            for (const auto& relation : internExps) {
                makeRamClear(current, relation);
            }
        }

        // create subroutine for this stratum
        ramSubs["stratum_" + std::to_string(indexOfScc)] = std::make_unique<RamSequence>(std::move(current));
        indexOfScc++;
    }

    // invoke all strata
    std::vector<std::unique_ptr<RamStatement>> res;
    for (size_t i = 0; i < indexOfScc; i++) {
        appendStmt(res, std::make_unique<RamCall>("stratum_" + std::to_string(i)));
    }

    // add main timer if profiling
    if (res.size() > 0 && Global::config().has("profile")) {
        auto newStmt = std::make_unique<RamLogTimer>(
                std::make_unique<RamSequence>(std::move(res)), LogStatement::runtime());
        res.clear();
        appendStmt(res, std::move(newStmt));
    }

    // done for main prog
    ramMain = std::make_unique<RamSequence>(std::move(res));

    // add subroutines for each clause
    if (Global::config().has("provenance")) {
        visitDepthFirst(*program, [&](const AstClause& clause) {
            std::stringstream relName;
            relName << clause.getHead()->getQualifiedName();

            // do not add subroutines for info relations or facts
            if (relName.str().find("@info") != std::string::npos || clause.getBodyLiterals().empty()) {
                return;
            }

            std::string subroutineLabel =
                    relName.str() + "_" + std::to_string(getClauseNum(program, &clause)) + "_subproof";
            ramSubs[subroutineLabel] = makeSubproofSubroutine(clause);

            std::string negationSubroutineLabel = relName.str() + "_" +
                                                  std::to_string(getClauseNum(program, &clause)) +
                                                  "_negation_subproof";
            ramSubs[negationSubroutineLabel] = makeNegationSubproofSubroutine(clause);
        });
    }
}

std::unique_ptr<RamTranslationUnit> AstTranslator::translateUnit(AstTranslationUnit& tu) {
    auto ram_start = std::chrono::high_resolution_clock::now();
    program = tu.getProgram();

    translateProgram(tu);
    SymbolTable& symTab = getSymbolTable();
    ErrorReport& errReport = tu.getErrorReport();
    DebugReport& debugReport = tu.getDebugReport();
    std::vector<std::unique_ptr<RamRelation>> rels;
    for (auto& cur : ramRels) {
        rels.push_back(std::move(cur.second));
    }
    if (nullptr == ramMain) {
        ramMain = std::make_unique<RamSequence>();
    }
    auto ramProg = std::make_unique<RamProgram>(std::move(rels), std::move(ramMain), std::move(ramSubs));
    if (!Global::config().get("debug-report").empty()) {
        if (ramProg) {
            auto ram_end = std::chrono::high_resolution_clock::now();
            std::string runtimeStr =
                    "(" + std::to_string(std::chrono::duration<double>(ram_end - ram_start).count()) + "s)";
            std::stringstream ramProgStr;
            ramProgStr << *ramProg;
            debugReport.addSection("ram-program", "RAM Program " + runtimeStr, ramProgStr.str());
        }
    }
    return std::make_unique<RamTranslationUnit>(
            std::move(ramProg), std::move(symTab), errReport, debugReport);
}

}  // end of namespace souffle
