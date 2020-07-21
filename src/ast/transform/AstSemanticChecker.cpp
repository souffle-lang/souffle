/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstSemanticChecker.cpp
 *
 * Implementation of the semantic checker pass.
 *
 ***********************************************************************/

#include "ast/transform/AstSemanticChecker.h"
#include "AggregateOp.h"
#include "BinaryConstraintOps.h"
#include "ErrorReport.h"
#include "FunctorOps.h"
#include "Global.h"
#include "GraphUtils.h"
#include "RamTypes.h"
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
#include "ast/AstQualifiedName.h"
#include "ast/AstRelation.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstType.h"
#include "ast/AstUtils.h"
#include "ast/AstVisitor.h"
#include "ast/TypeSystem.h"
#include "ast/analysis/AstGroundAnalysis.h"
#include "ast/analysis/AstIOTypeAnalysis.h"
#include "ast/analysis/AstTypeAnalysis.h"
#include "ast/analysis/AstTypeEnvironmentAnalysis.h"
#include "ast/analysis/PrecedenceGraph.h"
#include "utility/ContainerUtil.h"
#include "utility/FunctionalUtil.h"
#include "utility/MiscUtil.h"
#include "utility/StreamUtil.h"
#include "utility/StringUtil.h"
#include "utility/tinyformat.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <typeinfo>
#include <utility>
#include <vector>

namespace souffle {

struct AstSemanticCheckerImpl {
    AstTranslationUnit& tu;
    AstSemanticCheckerImpl(AstTranslationUnit& tu);

private:
    const IOType& ioTypes = *tu.getAnalysis<IOType>();
    const PrecedenceGraph& precedenceGraph = *tu.getAnalysis<PrecedenceGraph>();
    const RecursiveClauses& recursiveClauses = *tu.getAnalysis<RecursiveClauses>();
    const TypeEnvironmentAnalysis& typeEnvAnalysis = *tu.getAnalysis<TypeEnvironmentAnalysis>();
    const SCCGraph& sccGraph = *tu.getAnalysis<SCCGraph>();

    const TypeEnvironment& typeEnv = typeEnvAnalysis.getTypeEnvironment();
    const AstProgram& program = *tu.getProgram();
    ErrorReport& report = tu.getErrorReport();

    void checkAtom(const AstAtom& atom);
    void checkLiteral(const AstLiteral& literal);
    void checkAggregator(const AstAggregator& aggregator);
    bool isDependent(const AstClause& agg1, const AstClause& agg2);
    void checkArgument(const AstArgument& arg);
    void checkConstant(const AstArgument& argument);
    void checkFact(const AstClause& fact);
    void checkClause(const AstClause& clause);
    void checkComplexRule(std::set<const AstClause*> multiRule);
    void checkRelationDeclaration(const AstRelation& relation);
    void checkRelation(const AstRelation& relation);

    void checkType(const AstType& type);
    void checkRecordType(const AstRecordType& type);
    void checkSubsetType(const AstSubsetType& type);
    void checkUnionType(const AstUnionType& type);

    void checkNamespaces();
    void checkIO();
    void checkWitnessProblem();
    void checkInlining();
};

bool AstSemanticChecker::transform(AstTranslationUnit& translationUnit) {
    AstSemanticCheckerImpl{translationUnit};
    return false;
}

class TypeChecker : AstVisitor<void> {
public:
    TypeChecker(AstTranslationUnit& tu) : tu(tu){};

    /** Analyse types, clause by clause */
    void run() {
        for (auto* clause : tu.getProgram()->getClauses()) {
            visitDepthFirstPreOrder(*clause, *this);
        }
    }

private:
    AstTranslationUnit& tu;
    ErrorReport& report = tu.getErrorReport();
    const TypeAnalysis& typeAnalysis = *tu.getAnalysis<TypeAnalysis>();
    const TypeEnvironment& typeEnv = tu.getAnalysis<TypeEnvironmentAnalysis>()->getTypeEnvironment();
    const AstProgram& program = *tu.getProgram();

    void visitAtom(const AstAtom& atom) override;
    void visitVariable(const AstVariable& var) override;
    void visitStringConstant(const AstStringConstant& constant) override;
    void visitNumericConstant(const AstNumericConstant& constant) override;
    void visitNilConstant(const AstNilConstant& constant) override;
    void visitRecordInit(const AstRecordInit& rec) override;
    void visitTypeCast(const AstTypeCast& cast) override;
    void visitIntrinsicFunctor(const AstIntrinsicFunctor& fun) override;
    void visitUserDefinedFunctor(const AstUserDefinedFunctor& fun) override;
    void visitBinaryConstraint(const AstBinaryConstraint& constraint) override;
    void visitAggregator(const AstAggregator& aggregator) override;
};

AstSemanticCheckerImpl::AstSemanticCheckerImpl(AstTranslationUnit& tu) : tu(tu) {
    // suppress warnings for given relations
    if (Global::config().has("suppress-warnings")) {
        std::vector<std::string> suppressedRelations =
                splitString(Global::config().get("suppress-warnings"), ',');

        if (std::find(suppressedRelations.begin(), suppressedRelations.end(), "*") !=
                suppressedRelations.end()) {
            // mute all relations
            for (AstRelation* rel : program.getRelations()) {
                rel->addQualifier(RelationQualifier::SUPPRESSED);
            }
        } else {
            // mute only the given relations (if they exist)
            for (auto& relname : suppressedRelations) {
                const std::vector<std::string> comps = splitString(relname, '.');
                if (!comps.empty()) {
                    // generate the relation identifier
                    AstQualifiedName relid(comps[0]);
                    for (size_t i = 1; i < comps.size(); i++) {
                        relid.append(comps[i]);
                    }

                    // update suppressed qualifier if the relation is found
                    if (AstRelation* rel = getRelation(program, relid)) {
                        rel->addQualifier(RelationQualifier::SUPPRESSED);
                    }
                }
            }
        }
    }

    // Check types in AST.
    for (const auto* astType : program.getTypes()) {
        checkType(*astType);
    }

    // check rules
    for (auto* rel : program.getRelations()) {
        checkRelation(*rel);
    }
    for (auto* clause : program.getClauses()) {
        checkClause(*clause);
    }

    // Group clauses that stem from a single complex rule
    // with multiple headers/disjunction etc. The grouping
    // is performed via their source-location.
    std::map<SrcLocation, std::set<const AstClause*>> multiRuleMap;
    for (auto* clause : program.getClauses()) {
        // collect clauses of a multi rule, i.e., they have the same source locator
        multiRuleMap[clause->getSrcLoc()].insert(clause);
    }

    // check complex rule
    for (const auto& multiRule : multiRuleMap) {
        checkComplexRule(multiRule.second);
    }

    checkNamespaces();
    checkIO();
    checkWitnessProblem();
    checkInlining();

    // Run grounded terms checker
    GroundedTermsChecker().verify(tu);

    // Check types
    TypeChecker{tu}.run();

    // - stratification --
    // check for cyclic dependencies
    for (AstRelation* cur : program.getRelations()) {
        size_t scc = sccGraph.getSCC(cur);
        if (sccGraph.isRecursive(scc)) {
            for (const AstRelation* cyclicRelation : sccGraph.getInternalRelations(scc)) {
                // Negations and aggregations need to be stratified
                const AstLiteral* foundLiteral = nullptr;
                bool hasNegation = hasClauseWithNegatedRelation(cyclicRelation, cur, &program, foundLiteral);
                if (hasNegation ||
                        hasClauseWithAggregatedRelation(cyclicRelation, cur, &program, foundLiteral)) {
                    auto const& relSet = sccGraph.getInternalRelations(scc);
                    std::set<const AstRelation*, AstNameComparison> sortedRelSet(
                            relSet.begin(), relSet.end());
                    // Negations and aggregations need to be stratified
                    std::string relationsListStr = toString(join(sortedRelSet, ",",
                            [](std::ostream& out, const AstRelation* r) { out << r->getQualifiedName(); }));
                    std::vector<DiagnosticMessage> messages;
                    messages.push_back(DiagnosticMessage(
                            "Relation " + toString(cur->getQualifiedName()), cur->getSrcLoc()));
                    std::string negOrAgg = hasNegation ? "negation" : "aggregation";
                    messages.push_back(
                            DiagnosticMessage("has cyclic " + negOrAgg, foundLiteral->getSrcLoc()));
                    report.addDiagnostic(Diagnostic(Diagnostic::ERROR,
                            DiagnosticMessage("Unable to stratify relation(s) {" + relationsListStr + "}"),
                            messages));
                    break;
                }
            }
        }
    }
}

void AstSemanticCheckerImpl::checkAtom(const AstAtom& atom) {
    // check existence of relation
    auto* r = getRelation(program, atom.getQualifiedName());
    if (r == nullptr) {
        report.addError("Undefined relation " + toString(atom.getQualifiedName()), atom.getSrcLoc());
        return;
    }

    if (r->getArity() != atom.getArity()) {
        report.addError(
                "Mismatching arity of relation " + toString(atom.getQualifiedName()), atom.getSrcLoc());
    }

    for (const AstArgument* arg : atom.getArguments()) {
        checkArgument(*arg);
    }
}

namespace {

/**
 * Get unnamed variables except those that appear inside aggregates.
 */
std::set<const AstUnnamedVariable*> getUnnamedVariables(const AstNode& node) {
    std::set<const AstUnnamedVariable*> unnamedInAggregates;
    visitDepthFirst(node, [&](const AstAggregator& agg) {
        visitDepthFirst(agg, [&](const AstUnnamedVariable& var) { unnamedInAggregates.insert(&var); });
    });

    std::set<const AstUnnamedVariable*> unnamed;
    visitDepthFirst(node, [&](const AstUnnamedVariable& var) {
        if (!contains(unnamedInAggregates, &var)) {
            unnamed.insert(&var);
        }
    });

    return unnamed;
}

}  // namespace

void AstSemanticCheckerImpl::checkLiteral(const AstLiteral& literal) {
    // check potential nested atom
    if (const auto* atom = as<AstAtom>(literal)) {
        checkAtom(*atom);
    }

    if (const auto* neg = as<AstNegation>(literal)) {
        checkAtom(*neg->getAtom());
    }

    if (const auto* constraint = as<AstBinaryConstraint>(literal)) {
        checkArgument(*constraint->getLHS());
        checkArgument(*constraint->getRHS());

        std::set<const AstUnnamedVariable*> unnamedInRecord;
        visitDepthFirst(*constraint, [&](const AstRecordInit& record) {
            for (auto* arg : record.getArguments()) {
                if (auto* unnamed = as<AstUnnamedVariable>(arg)) {
                    unnamedInRecord.insert(unnamed);
                }
            }
        });

        // Check if constraint contains unnamed variables.
        for (auto* unnamed : getUnnamedVariables(*constraint)) {
            if (!contains(unnamedInRecord, unnamed)) {
                report.addError("Underscore in binary relation", unnamed->getSrcLoc());
            }
        }
    }
}

/**
 * agg1, agg2 are clauses which contain no head, and consist of a single literal
 * that contains an aggregate.
 * agg1 is dependent on agg2 if agg1 contains a variable which is grounded by agg2, and not by agg1.
 */
bool AstSemanticCheckerImpl::isDependent(const AstClause& agg1, const AstClause& agg2) {
    auto groundedInAgg1 = getGroundedTerms(tu, agg1);
    auto groundedInAgg2 = getGroundedTerms(tu, agg2);
    bool dependent = false;
    // For each variable X in the first aggregate
    visitDepthFirst(agg1, [&](const AstVariable& searchVar) {
        // Try to find the corresponding variable X in the second aggregate
        // by string comparison
        const AstVariable* matchingVarPtr = nullptr;
        visitDepthFirst(agg2, [&](const AstVariable& var) {
            if (var == searchVar) {
                matchingVarPtr = &var;
                return;
            }
        });
        // If the variable occurs in both clauses (a match was found)
        if (matchingVarPtr != nullptr) {
            if (!groundedInAgg1[&searchVar] && groundedInAgg2[matchingVarPtr]) {
                dependent = true;
            }
        }
    });
    return dependent;
}

void AstSemanticCheckerImpl::checkAggregator(const AstAggregator& aggregator) {
    auto& report = tu.getErrorReport();
    auto& program = *tu.getProgram();
    const AstAggregator* inner = nullptr;

    // check for disallowed nested aggregates
    visitDepthFirst(aggregator, [&](const AstAggregator& innerAgg) {
        if (aggregator != innerAgg) {
            inner = &innerAgg;
        }
    });

    if (inner != nullptr) {
        report.addError("Unsupported nested aggregate", inner->getSrcLoc());
    }

    AstClause dummyClauseAggregator;

    visitDepthFirst(program, [&](const AstLiteral& parentLiteral) {
        visitDepthFirst(parentLiteral, [&](const AstAggregator& candidateAggregate) {
            if (candidateAggregate != aggregator) {
                return;
            }
            // Get the literal containing the aggregator and put it into a dummy clause
            // so we can get information about groundedness
            dummyClauseAggregator.addToBody(souffle::clone(&parentLiteral));
        });
    });

    visitDepthFirst(program, [&](const AstLiteral& parentLiteral) {
        visitDepthFirst(parentLiteral, [&](const AstAggregator& /* otherAggregate */) {
            // Create the other aggregate's dummy clause
            AstClause dummyClauseOther;
            dummyClauseOther.addToBody(souffle::clone(&parentLiteral));
            // Check dependency between the aggregator and this one
            if (isDependent(dummyClauseAggregator, dummyClauseOther) &&
                    isDependent(dummyClauseOther, dummyClauseAggregator)) {
                report.addError("Mutually dependent aggregate", aggregator.getSrcLoc());
            }
        });
    });

    for (AstLiteral* literal : aggregator.getBodyLiterals()) {
        checkLiteral(*literal);
    }
}

void AstSemanticCheckerImpl::checkArgument(const AstArgument& arg) {
    if (const auto* agg = dynamic_cast<const AstAggregator*>(&arg)) {
        checkAggregator(*agg);
    } else if (const auto* func = dynamic_cast<const AstFunctor*>(&arg)) {
        for (auto arg : func->getArguments()) {
            checkArgument(*arg);
        }
    }
}

namespace {

/**
 * Check if the argument can be statically evaluated
 * and thus in particular, if it should be allowed to appear as argument in facts.
 **/
bool isConstantArgument(const AstArgument* arg) {
    assert(arg != nullptr);

    if (isA<AstVariable>(arg) || isA<AstUnnamedVariable>(arg)) {
        return false;
    } else if (isA<AstUserDefinedFunctor>(arg)) {
        return false;
    } else if (isA<AstCounter>(arg)) {
        return false;
    } else if (auto* typeCast = as<AstTypeCast>(arg)) {
        return isConstantArgument(typeCast->getValue());
    } else if (auto* term = as<AstTerm>(arg)) {
        // Term covers intrinsic functor and records. User-functors are handled earlier.
        return all_of(term->getArguments(), isConstantArgument);
    } else if (isA<AstConstant>(arg)) {
        return true;
    } else {
        fatal("unsupported argument type: %s", typeid(arg).name());
    }
}

}  // namespace

/* Check if facts contain only constants */
void AstSemanticCheckerImpl::checkFact(const AstClause& fact) {
    assert(isFact(fact));

    AstAtom* head = fact.getHead();
    if (head == nullptr) {
        return;  // checked by clause
    }

    AstRelation* rel = getRelation(program, head->getQualifiedName());
    if (rel == nullptr) {
        return;  // checked by clause
    }

    // facts must only contain constants
    for (auto* arg : head->getArguments()) {
        if (!isConstantArgument(arg)) {
            report.addError("Argument in fact is not constant", arg->getSrcLoc());
        }
    }
}

void AstSemanticCheckerImpl::checkClause(const AstClause& clause) {
    // check head atom
    checkAtom(*clause.getHead());

    // Check for absence of underscores in head
    for (auto* unnamed : getUnnamedVariables(*clause.getHead())) {
        report.addError("Underscore in head of rule", unnamed->getSrcLoc());
    }

    // check body literals
    for (AstLiteral* lit : clause.getBodyLiterals()) {
        checkLiteral(*lit);
    }

    // check facts
    if (isFact(clause)) {
        checkFact(clause);
    }

    // check whether named unnamed variables of the form _<ident>
    // are only used once in a clause; if not, warnings will be
    // issued.
    std::map<std::string, int> var_count;
    std::map<std::string, const AstVariable*> var_pos;
    visitDepthFirst(clause, [&](const AstVariable& var) {
        var_count[var.getName()]++;
        var_pos[var.getName()] = &var;
    });
    for (const auto& cur : var_count) {
        int numAppearances = cur.second;
        const auto& varName = cur.first;
        const auto& varLocation = var_pos[varName]->getSrcLoc();
        if (varName[0] == '_') {
            assert(varName.size() > 1 && "named variable should not be a single underscore");
            if (numAppearances > 1) {
                report.addWarning("Variable " + varName + " marked as singleton but occurs more than once",
                        varLocation);
            }
        }
    }

    // check execution plan
    if (clause.getExecutionPlan() != nullptr) {
        auto numAtoms = getBodyLiterals<AstAtom>(clause).size();
        for (const auto& cur : clause.getExecutionPlan()->getOrders()) {
            bool isComplete = true;
            auto order = cur.second->getOrder();
            for (unsigned i = 1; i <= order.size(); i++) {
                if (!contains(order, i)) {
                    isComplete = false;
                    break;
                }
            }
            if (order.size() != numAtoms || !isComplete) {
                report.addError("Invalid execution order in plan", cur.second->getSrcLoc());
            }
        }
    }

    // check auto-increment
    if (recursiveClauses.recursive(&clause)) {
        visitDepthFirst(clause, [&](const AstCounter& ctr) {
            report.addError("Auto-increment functor in a recursive rule", ctr.getSrcLoc());
        });
    }
}

void AstSemanticCheckerImpl::checkComplexRule(std::set<const AstClause*> multiRule) {
    std::map<std::string, int> var_count;
    std::map<std::string, const AstVariable*> var_pos;

    // Count the variable occurrence for the body of a
    // complex rule only once.
    // TODO (b-scholz): for negation / disjunction this is not quite
    // right; we would need more semantic information here.
    for (auto literal : (*multiRule.begin())->getBodyLiterals()) {
        visitDepthFirst(*literal, [&](const AstVariable& var) {
            var_count[var.getName()]++;
            var_pos[var.getName()] = &var;
        });
    }

    // Count variable occurrence for each head separately
    for (auto clause : multiRule) {
        visitDepthFirst(*(clause->getHead()), [&](const AstVariable& var) {
            var_count[var.getName()]++;
            var_pos[var.getName()] = &var;
        });
    }

    // Check that a variables occurs more than once
    for (const auto& cur : var_count) {
        int numAppearances = cur.second;
        const auto& varName = cur.first;
        const auto& varLocation = var_pos[varName]->getSrcLoc();
        if (varName[0] != '_' && numAppearances == 1) {
            report.addWarning("Variable " + varName + " only occurs once", varLocation);
        }
    }
}

void AstSemanticCheckerImpl::checkRelationDeclaration(const AstRelation& relation) {
    const auto& attributes = relation.getAttributes();
    assert(attributes.size() == relation.getArity() && "mismatching attribute size and arity");

    for (size_t i = 0; i < relation.getArity(); i++) {
        AstAttribute* attr = attributes[i];
        auto&& typeName = attr->getTypeName();

        /* check whether type exists */
        if (!typeEnv.isPrimitiveType(typeName) && !getType(program, typeName)) {
            report.addError(tfm::format("Undefined type in attribute %s", *attr), attr->getSrcLoc());
        }

        /* check whether name occurs more than once */
        for (size_t j = 0; j < i; j++) {
            if (attr->getName() == attributes[j]->getName()) {
                report.addError(tfm::format("Doubly defined attribute name %s", *attr), attr->getSrcLoc());
            }
        }
    }
}

void AstSemanticCheckerImpl::checkRelation(const AstRelation& relation) {
    if (relation.getRepresentation() == RelationRepresentation::EQREL) {
        if (relation.getArity() == 2) {
            const auto& attributes = relation.getAttributes();
            assert(attributes.size() == 2 && "mismatching attribute size and arity");
            if (attributes[0]->getTypeName() != attributes[1]->getTypeName()) {
                report.addError("Domains of equivalence relation " + toString(relation.getQualifiedName()) +
                                        " are different",
                        relation.getSrcLoc());
            }
        } else {
            report.addError(
                    "Equivalence relation " + toString(relation.getQualifiedName()) + " is not binary",
                    relation.getSrcLoc());
        }
    }

    // start with declaration
    checkRelationDeclaration(relation);

    // check whether this relation is empty
    if (getClauses(program, relation).empty() && !ioTypes.isInput(&relation) &&
            !relation.hasQualifier(RelationQualifier::SUPPRESSED)) {
        report.addWarning("No rules/facts defined for relation " + toString(relation.getQualifiedName()),
                relation.getSrcLoc());
    }
}

// ----- types --------

void AstSemanticCheckerImpl::checkUnionType(const AstUnionType& type) {
    // check presence of all the element types and that all element types are based off a primitive
    for (const AstQualifiedName& sub : type.getTypes()) {
        if (typeEnv.isPrimitiveType(sub)) {
            continue;
        }
        const AstType* subt = getType(program, sub);
        if (subt == nullptr) {
            report.addError(tfm::format("Undefined type %s in definition of union type %s", sub,
                                    type.getQualifiedName()),
                    type.getSrcLoc());
        } else if (!isA<AstUnionType>(subt) && !isA<AstSubsetType>(subt)) {
            report.addError(tfm::format("Union type %s contains the non-primitive type %s",
                                    type.getQualifiedName(), sub),
                    type.getSrcLoc());
        }
    }

    // Check if the union is recursive.
    if (typeEnvAnalysis.isCyclic(type.getQualifiedName())) {
        report.addError("Infinite descent in the definition of type " + toString(type.getQualifiedName()),
                type.getSrcLoc());
    }

    /* check that union types do not mix different primitive types */
    for (const auto* type : program.getTypes()) {
        // We are only interested in unions here.
        if (dynamic_cast<const AstUnionType*>(type) == nullptr) {
            continue;
        }

        const auto& name = type->getQualifiedName();

        const auto& predefinedTypesInUnion = typeEnvAnalysis.getPrimitiveTypesInUnion(name);

        // Report error (if size == 0, then the union is cyclic)
        if (predefinedTypesInUnion.size() > 1) {
            report.addError(
                    tfm::format("Union type %s is defined over {%s} (multiple primitive types in union)",
                            name, join(predefinedTypesInUnion, ", ")),
                    type->getSrcLoc());
        }
    }
}

void AstSemanticCheckerImpl::checkRecordType(const AstRecordType& type) {
    auto&& fields = type.getFields();
    // check proper definition of all field types
    for (auto&& field : fields) {
        if (!typeEnv.isType(field->getTypeName())) {
            report.addError(tfm::format("Undefined type %s in definition of field %s", field->getTypeName(),
                                    field->getName()),
                    field->getSrcLoc());
        }
    }

    // check that field names are unique
    for (std::size_t i = 0; i < fields.size(); i++) {
        auto&& cur_name = fields[i]->getName();
        for (std::size_t j = 0; j < i; j++) {
            if (fields[j]->getName() == cur_name) {
                report.addError(tfm::format("Doubly defined field name %s in definition of type %s", cur_name,
                                        type.getQualifiedName()),
                        fields[i]->getSrcLoc());
            }
        }
    }
}

void AstSemanticCheckerImpl::checkSubsetType(const AstSubsetType& astType) {
    if (typeEnvAnalysis.isCyclic(astType.getQualifiedName())) {
        report.addError(
                tfm::format("Infinite descent in the definition of type %s", astType.getQualifiedName()),
                astType.getSrcLoc());
        return;
    }

    if (!typeEnv.isType(astType.getBaseType())) {
        report.addError(tfm::format("Undefined base type %s in definition of type %s", astType.getBaseType(),
                                astType.getQualifiedName()),
                astType.getSrcLoc());
        return;
    }

    auto& rootType = typeEnv.getType(astType.getBaseType());

    if (isA<UnionType>(rootType)) {
        report.addError(tfm::format("Subset type %s can't be derived from union %s",
                                astType.getQualifiedName(), rootType.getName()),
                astType.getSrcLoc());
    }
}

void AstSemanticCheckerImpl::checkType(const AstType& type) {
    if (typeEnv.isPrimitiveType(type.getQualifiedName())) {
        report.addError("Redefinition of the predefined type", type.getSrcLoc());
        return;
    }

    if (isA<AstUnionType>(type)) {
        checkUnionType(*as<AstUnionType>(type));
    } else if (isA<AstRecordType>(type)) {
        checkRecordType(*as<AstRecordType>(type));
    } else if (isA<AstSubsetType>(type)) {
        checkSubsetType(*as<AstSubsetType>(type));
    } else {
        fatal("unsupported type construct: %s", typeid(type).name());
    }
}

void AstSemanticCheckerImpl::checkIO() {
    auto checkIO = [&](const AstIO* directive) {
        auto* r = getRelation(program, directive->getQualifiedName());
        if (r == nullptr) {
            report.addError(
                    "Undefined relation " + toString(directive->getQualifiedName()), directive->getSrcLoc());
        }
    };
    for (const auto* directive : program.getIOs()) {
        checkIO(directive);
    }
}

static const std::vector<SrcLocation> usesInvalidWitness(AstTranslationUnit& tu,
        const std::vector<AstLiteral*>& literals,
        const std::set<std::unique_ptr<AstArgument>>& groundedArguments) {
    // Node-mapper that replaces aggregators with new (unique) variables
    struct M : public AstNodeMapper {
        // Variables introduced to replace aggregators
        mutable std::set<std::string> aggregatorVariables;

        const std::set<std::string>& getAggregatorVariables() {
            return aggregatorVariables;
        }

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            static int numReplaced = 0;
            if (dynamic_cast<AstAggregator*>(node.get()) != nullptr) {
                // Replace the aggregator with a variable
                std::stringstream newVariableName;
                newVariableName << "+aggr_var_" << numReplaced++;

                // Keep track of which variables are bound to aggregators
                aggregatorVariables.insert(newVariableName.str());

                return std::make_unique<AstVariable>(newVariableName.str());
            }
            node->apply(*this);
            return node;
        }
    };

    std::vector<SrcLocation> result;

    // Create two versions of the original clause

    // Clause 1 - will remain equivalent to the original clause in terms of variable groundedness
    auto originalClause = std::make_unique<AstClause>();
    originalClause->setHead(std::make_unique<AstAtom>("*"));

    // Clause 2 - will have aggregators replaced with intrinsically grounded variables
    auto aggregatorlessClause = std::make_unique<AstClause>();
    aggregatorlessClause->setHead(std::make_unique<AstAtom>("*"));

    // Construct both clauses in the same manner to match the original clause
    // Must keep track of the subnode in Clause 1 that each subnode in Clause 2 matches to
    std::map<const AstArgument*, const AstArgument*> identicalSubnodeMap;
    for (const AstLiteral* lit : literals) {
        auto firstClone = souffle::clone(lit);
        auto secondClone = souffle::clone(lit);

        // Construct the mapping between equivalent literal subnodes
        std::vector<const AstArgument*> firstCloneArguments;
        visitDepthFirst(*firstClone, [&](const AstArgument& arg) { firstCloneArguments.push_back(&arg); });

        std::vector<const AstArgument*> secondCloneArguments;
        visitDepthFirst(*secondClone, [&](const AstArgument& arg) { secondCloneArguments.push_back(&arg); });

        for (size_t i = 0; i < firstCloneArguments.size(); i++) {
            identicalSubnodeMap[secondCloneArguments[i]] = firstCloneArguments[i];
        }

        // Actually add the literal clones to each clause
        originalClause->addToBody(std::move(firstClone));
        aggregatorlessClause->addToBody(std::move(secondClone));
    }

    // Replace the aggregators in Clause 2 with variables
    M update;
    aggregatorlessClause->apply(update);

    // Create a dummy atom to force certain arguments to be grounded in the aggregatorlessClause
    auto groundingAtomAggregatorless = std::make_unique<AstAtom>("grounding_atom");
    auto groundingAtomOriginal = std::make_unique<AstAtom>("grounding_atom");

    // Force the new aggregator variables to be grounded in the aggregatorless clause
    const std::set<std::string>& aggregatorVariables = update.getAggregatorVariables();
    for (const std::string& str : aggregatorVariables) {
        groundingAtomAggregatorless->addArgument(std::make_unique<AstVariable>(str));
    }

    // Force the given grounded arguments to be grounded in both clauses
    for (const std::unique_ptr<AstArgument>& arg : groundedArguments) {
        groundingAtomAggregatorless->addArgument(souffle::clone(arg));
        groundingAtomOriginal->addArgument(souffle::clone(arg));
    }

    aggregatorlessClause->addToBody(std::move(groundingAtomAggregatorless));
    originalClause->addToBody(std::move(groundingAtomOriginal));

    // Compare the grounded analysis of both generated clauses
    // All added arguments in Clause 2 were forced to be grounded, so if an ungrounded argument
    // appears in Clause 2, it must also appear in Clause 1. Consequently, have two cases:
    //   - The argument is also ungrounded in Clause 1 - handled by another check
    //   - The argument is grounded in Clause 1 => the argument was grounded in the
    //     first clause somewhere along the line by an aggregator-body - not allowed!
    std::set<std::unique_ptr<AstArgument>> newlyGroundedArguments;
    auto originalGrounded = getGroundedTerms(tu, *originalClause);
    for (auto&& pair : getGroundedTerms(tu, *aggregatorlessClause)) {
        if (!pair.second && originalGrounded[identicalSubnodeMap[pair.first]]) {
            result.push_back(pair.first->getSrcLoc());
        }

        // Otherwise, it can now be considered grounded
        newlyGroundedArguments.insert(souffle::clone(pair.first));
    }

    // All previously grounded are still grounded
    for (const std::unique_ptr<AstArgument>& arg : groundedArguments) {
        newlyGroundedArguments.insert(souffle::clone(arg));
    }

    // Everything on this level is fine, check subaggregators of each literal
    for (const AstLiteral* lit : literals) {
        visitDepthFirst(*lit, [&](const AstAggregator& aggr) {
            // Check recursively if an invalid witness is used
            for (auto&& argloc : usesInvalidWitness(tu, aggr.getBodyLiterals(), newlyGroundedArguments)) {
                result.push_back(argloc);
            }
        });
    }

    return result;
}

void AstSemanticCheckerImpl::checkWitnessProblem() {
    // Visit each clause to check if an invalid aggregator witness is used
    visitDepthFirst(program, [&](const AstClause& clause) {
        // Body literals of the clause to check
        std::vector<AstLiteral*> bodyLiterals = clause.getBodyLiterals();

        // Add in all head variables as new ungrounded body literals
        auto headVariables = std::make_unique<AstAtom>("*");
        visitDepthFirst(*clause.getHead(),
                [&](const AstVariable& var) { headVariables->addArgument(souffle::clone(&var)); });
        auto headNegation = std::make_unique<AstNegation>(std::move(headVariables));
        bodyLiterals.push_back(headNegation.get());

        // Perform the check
        std::set<std::unique_ptr<AstArgument>> groundedArguments;
        for (auto&& invalidArgument : usesInvalidWitness(tu, bodyLiterals, groundedArguments)) {
            report.addError(
                    "Witness problem: argument grounded by an aggregator's inner scope is used ungrounded in "
                    "outer scope",
                    invalidArgument);
        }
    });
}

/**
 * Find a cycle consisting entirely of inlined relations.
 * If no cycle exists, then an empty vector is returned.
 */
std::vector<AstQualifiedName> findInlineCycle(const PrecedenceGraph& precedenceGraph,
        std::map<const AstRelation*, const AstRelation*>& origins, const AstRelation* current,
        AstRelationSet& unvisited, AstRelationSet& visiting, AstRelationSet& visited) {
    std::vector<AstQualifiedName> result;

    if (current == nullptr) {
        // Not looking at any nodes at the moment, so choose any node from the unvisited list

        if (unvisited.empty()) {
            // Nothing left to visit - so no cycles exist!
            return result;
        }

        // Choose any element from the unvisited set
        current = *unvisited.begin();
        origins[current] = nullptr;

        // Move it to "currently visiting"
        unvisited.erase(current);
        visiting.insert(current);

        // Check if we can find a cycle beginning from this node
        std::vector<AstQualifiedName> subresult =
                findInlineCycle(precedenceGraph, origins, current, unvisited, visiting, visited);

        if (subresult.empty()) {
            // No cycle found, try again from another node
            return findInlineCycle(precedenceGraph, origins, nullptr, unvisited, visiting, visited);
        } else {
            // Cycle found! Return it
            return subresult;
        }
    }

    // Check neighbours
    const AstRelationSet& successors = precedenceGraph.graph().successors(current);
    for (const AstRelation* successor : successors) {
        // Only care about inlined neighbours in the graph
        if (successor->hasQualifier(RelationQualifier::INLINE)) {
            if (visited.find(successor) != visited.end()) {
                // The neighbour has already been visited, so move on
                continue;
            }

            if (visiting.find(successor) != visiting.end()) {
                // Found a cycle!!
                // Construct the cycle in reverse
                while (current != nullptr) {
                    result.push_back(current->getQualifiedName());
                    current = origins[current];
                }
                return result;
            }

            // Node has not been visited yet
            origins[successor] = current;

            // Move from unvisited to visiting
            unvisited.erase(successor);
            visiting.insert(successor);

            // Visit recursively and check if a cycle is formed
            std::vector<AstQualifiedName> subgraphCycle =
                    findInlineCycle(precedenceGraph, origins, successor, unvisited, visiting, visited);

            if (!subgraphCycle.empty()) {
                // Found a cycle!
                return subgraphCycle;
            }
        }
    }

    // Visited all neighbours with no cycle found, so done visiting this node.
    visiting.erase(current);
    visited.insert(current);
    return result;
}

void AstSemanticCheckerImpl::checkInlining() {
    auto isInline = [&](const AstRelation* rel) { return rel->hasQualifier(RelationQualifier::INLINE); };

    // Find all inlined relations
    AstRelationSet inlinedRelations;
    for (const auto& relation : program.getRelations()) {
        if (isInline(relation)) {
            inlinedRelations.insert(relation);
            if (ioTypes.isIO(relation)) {
                report.addError(
                        "IO relation " + toString(relation->getQualifiedName()) + " cannot be inlined",
                        relation->getSrcLoc());
            }
        }
    }

    // Check 1:
    // Let G' be the subgraph of the precedence graph G containing only those nodes
    // which are marked with the inline directive.
    // If G' contains a cycle, then inlining cannot be performed.

    AstRelationSet unvisited;  // nodes that have not been visited yet
    AstRelationSet visiting;   // nodes that we are currently visiting
    AstRelationSet visited;    // nodes that have been completely explored

    // All nodes are initially unvisited
    for (const AstRelation* rel : inlinedRelations) {
        unvisited.insert(rel);
    }

    // Remember the parent node of each visited node to construct the found cycle
    std::map<const AstRelation*, const AstRelation*> origins;

    std::vector<AstQualifiedName> result =
            findInlineCycle(precedenceGraph, origins, nullptr, unvisited, visiting, visited);

    // If the result contains anything, then a cycle was found
    if (!result.empty()) {
        AstRelation* cycleOrigin = getRelation(program, result[result.size() - 1]);

        // Construct the string representation of the cycle
        std::stringstream cycle;
        cycle << "{" << cycleOrigin->getQualifiedName();

        // Print it backwards to preserve the initial cycle order
        for (int i = result.size() - 2; i >= 0; i--) {
            cycle << ", " << result[i];
        }

        cycle << "}";

        report.addError(
                "Cannot inline cyclically dependent relations " + cycle.str(), cycleOrigin->getSrcLoc());
    }

    // Check 2:
    // Cannot use the counter argument ('$') in inlined relations

    // Check if an inlined literal ever takes in a $
    visitDepthFirst(program, [&](const AstAtom& atom) {
        AstRelation* associatedRelation = getRelation(program, atom.getQualifiedName());
        if (associatedRelation != nullptr && isInline(associatedRelation)) {
            visitDepthFirst(atom, [&](const AstArgument& arg) {
                if (dynamic_cast<const AstCounter*>(&arg) != nullptr) {
                    report.addError(
                            "Cannot inline literal containing a counter argument '$'", arg.getSrcLoc());
                }
            });
        }
    });

    // Check if an inlined clause ever contains a $
    for (const AstRelation* rel : inlinedRelations) {
        for (AstClause* clause : getClauses(program, *rel)) {
            visitDepthFirst(*clause, [&](const AstArgument& arg) {
                if (dynamic_cast<const AstCounter*>(&arg) != nullptr) {
                    report.addError(
                            "Cannot inline clause containing a counter argument '$'", arg.getSrcLoc());
                }
            });
        }
    }

    // Check 3:
    // Suppose the relation b is marked with the inline directive, but appears negated
    // in a clause. Then, if b introduces a new variable in its body, we cannot inline
    // the relation b.

    // Find all relations with the inline declarative that introduce new variables in their bodies
    AstRelationSet nonNegatableRelations;
    for (const AstRelation* rel : inlinedRelations) {
        bool foundNonNegatable = false;
        for (const AstClause* clause : getClauses(program, *rel)) {
            // Get the variables in the head
            std::set<std::string> headVariables;
            visitDepthFirst(
                    *clause->getHead(), [&](const AstVariable& var) { headVariables.insert(var.getName()); });

            // Get the variables in the body
            std::set<std::string> bodyVariables;
            visitDepthFirst(clause->getBodyLiterals(),
                    [&](const AstVariable& var) { bodyVariables.insert(var.getName()); });

            // Check if all body variables are in the head
            // Do this separately to the above so only one error is printed per variable
            for (const std::string& var : bodyVariables) {
                if (headVariables.find(var) == headVariables.end()) {
                    nonNegatableRelations.insert(rel);
                    foundNonNegatable = true;
                    break;
                }
            }

            if (foundNonNegatable) {
                break;
            }
        }
    }

    // Check that these relations never appear negated
    visitDepthFirst(program, [&](const AstNegation& neg) {
        AstRelation* associatedRelation = getRelation(program, neg.getAtom()->getQualifiedName());
        if (associatedRelation != nullptr &&
                nonNegatableRelations.find(associatedRelation) != nonNegatableRelations.end()) {
            report.addError(
                    "Cannot inline negated relation which may introduce new variables", neg.getSrcLoc());
        }
    });

    // Check 4:
    // Don't support inlining atoms within aggregators at this point.

    // Reasoning: Suppose we have an aggregator like `max X: a(X)`, where `a` is inlined to `a1` and `a2`.
    // Then, `max X: a(X)` will become `max( max X: a1(X),  max X: a2(X) )`. Suppose further that a(X) has
    // values X where it is true, while a2(X) does not. Then, the produced argument
    // `max( max X: a1(X),  max X: a2(X) )` will not return anything (as one of its arguments fails), while
    // `max X: a(X)` will.
    // Can work around this with emptiness checks (e.g. `!a1(_), ... ; !a2(_), ... ; ...`)

    // This corner case prevents generalising aggregator inlining with the current set up.

    visitDepthFirst(program, [&](const AstAggregator& aggr) {
        visitDepthFirst(aggr, [&](const AstAtom& subatom) {
            const AstRelation* rel = getRelation(program, subatom.getQualifiedName());
            if (rel != nullptr && isInline(rel)) {
                report.addError("Cannot inline relations that appear in aggregator", subatom.getSrcLoc());
            }
        });
    });

    // Check 5:
    // Suppose a relation `a` is inlined, appears negated in a clause, and contains a
    // (possibly nested) unnamed variable in its arguments. Then, the atom can't be
    // inlined, as unnamed variables are named during inlining (since they may appear
    // multiple times in an inlined-clause's body) => ungroundedness!

    // Exception: It's fine if the unnamed variable appears in a nested aggregator, as
    // the entire aggregator will automatically be grounded.

    // TODO (azreika): special case where all rules defined for `a` use the
    // underscored-argument exactly once: can workaround by remapping the variable
    // back to an underscore - involves changes to the actual inlining algo, though

    // Returns the pair (isValid, lastSrcLoc) where:
    //  - isValid is true if and only if the node contains an invalid underscore, and
    //  - lastSrcLoc is the source location of the last visited node
    std::function<std::pair<bool, SrcLocation>(const AstNode*)> checkInvalidUnderscore =
            [&](const AstNode* node) {
                if (dynamic_cast<const AstUnnamedVariable*>(node) != nullptr) {
                    // Found an invalid underscore
                    return std::make_pair(true, node->getSrcLoc());
                } else if (dynamic_cast<const AstAggregator*>(node) != nullptr) {
                    // Don't care about underscores within aggregators
                    return std::make_pair(false, node->getSrcLoc());
                }

                // Check if any children nodes use invalid underscores
                for (const AstNode* child : node->getChildNodes()) {
                    std::pair<bool, SrcLocation> childStatus = checkInvalidUnderscore(child);
                    if (childStatus.first) {
                        // Found an invalid underscore
                        return childStatus;
                    }
                }

                return std::make_pair(false, node->getSrcLoc());
            };

    // Perform the check
    visitDepthFirst(program, [&](const AstNegation& negation) {
        const AstAtom* associatedAtom = negation.getAtom();
        const AstRelation* associatedRelation = getRelation(program, associatedAtom->getQualifiedName());
        if (associatedRelation != nullptr && isInline(associatedRelation)) {
            std::pair<bool, SrcLocation> atomStatus = checkInvalidUnderscore(associatedAtom);
            if (atomStatus.first) {
                report.addError(
                        "Cannot inline negated atom containing an unnamed variable unless the variable is "
                        "within an aggregator",
                        atomStatus.second);
            }
        }
    });
}

// Check that type and relation names are disjoint sets.
void AstSemanticCheckerImpl::checkNamespaces() {
    std::map<std::string, SrcLocation> names;

    // Find all names and report redeclarations as we go.
    for (const auto& type : program.getTypes()) {
        const std::string name = toString(type->getQualifiedName());
        if (names.count(name) != 0u) {
            report.addError("Name clash on type " + name, type->getSrcLoc());
        } else {
            names[name] = type->getSrcLoc();
        }
    }

    for (const auto& rel : program.getRelations()) {
        const std::string name = toString(rel->getQualifiedName());
        if (names.count(name) != 0u) {
            report.addError("Name clash on relation " + name, rel->getSrcLoc());
        } else {
            names[name] = rel->getSrcLoc();
        }
    }
}

bool AstExecutionPlanChecker::transform(AstTranslationUnit& translationUnit) {
    auto* relationSchedule = translationUnit.getAnalysis<RelationSchedule>();
    auto* recursiveClauses = translationUnit.getAnalysis<RecursiveClauses>();

    for (const RelationScheduleStep& step : relationSchedule->schedule()) {
        const std::set<const AstRelation*>& scc = step.computed();
        for (const AstRelation* rel : scc) {
            for (const AstClause* clause : getClauses(*translationUnit.getProgram(), *rel)) {
                if (!recursiveClauses->recursive(clause)) {
                    continue;
                }
                if (clause->getExecutionPlan() == nullptr) {
                    continue;
                }
                int version = 0;
                for (const auto* atom : getBodyLiterals<AstAtom>(*clause)) {
                    if (scc.count(getAtomRelation(atom, translationUnit.getProgram())) != 0u) {
                        version++;
                    }
                }
                int maxVersion = -1;
                for (auto const& cur : clause->getExecutionPlan()->getOrders()) {
                    maxVersion = std::max(cur.first, maxVersion);
                }

                if (version <= maxVersion) {
                    for (const auto& cur : clause->getExecutionPlan()->getOrders()) {
                        if (cur.first >= version) {
                            translationUnit.getErrorReport().addDiagnostic(Diagnostic(Diagnostic::ERROR,
                                    DiagnosticMessage(
                                            "execution plan for version " + std::to_string(cur.first),
                                            cur.second->getSrcLoc()),
                                    {DiagnosticMessage("only versions 0.." + std::to_string(version - 1) +
                                                       " permitted")}));
                        }
                    }
                }
            }
        }
    }
    return false;
}

void TypeChecker::visitAtom(const AstAtom& atom) {
    auto relation = getAtomRelation(&atom, &program);
    if (relation == nullptr) {
        return;  // error unrelated to types.
    }

    auto attributes = relation->getAttributes();
    auto arguments = atom.getArguments();
    if (attributes.size() != arguments.size()) {
        return;  // error in input program
    }

    for (size_t i = 0; i < attributes.size(); ++i) {
        auto& typeName = attributes[i]->getTypeName();
        if (typeEnv.isType(typeName)) {
            auto argTypes = typeAnalysis.getTypes(arguments[i]);
            auto& attributeType = typeEnv.getType(typeName);

            if (argTypes.isAll() || argTypes.empty()) {
                continue;  // This will be reported later.
            }

            // Attribute and argument type agree if, argument type is a subtype of declared type
            // or is of the appropriate constant type or the (constant) record type.
            bool validAttribute = all_of(argTypes, [&attributeType](const Type& type) {
                if (isSubtypeOf(type, attributeType)) return true;
                if (!isSubtypeOf(attributeType, type)) return false;
                if (isA<ConstantType>(type)) return true;
                return isA<RecordType>(type) && !isA<SubsetType>(type);
            });
            if (!validAttribute && !Global::config().has("legacy")) {
                report.addError("Atoms argument type is not a subtype of its declared type",
                        arguments[i]->getSrcLoc());
            }
        }
    }
}

void TypeChecker::visitVariable(const AstVariable& var) {
    if (typeAnalysis.getTypes(&var).empty()) {
        report.addError("Unable to deduce type for variable " + var.getName(), var.getSrcLoc());
    }
}

void TypeChecker::visitStringConstant(const AstStringConstant& constant) {
    TypeSet types = typeAnalysis.getTypes(&constant);
    if (!isOfKind(types, TypeAttribute::Symbol)) {
        report.addError("Symbol constant (type mismatch)", constant.getSrcLoc());
    }
}

void TypeChecker::visitNumericConstant(const AstNumericConstant& constant) {
    TypeSet types = typeAnalysis.getTypes(&constant);

    // No type could be assigned.
    if (!constant.getType().has_value()) {
        report.addError("Ambiguous constant (unable to deduce type)", constant.getSrcLoc());
        return;
    }

    switch (*constant.getType()) {
        case AstNumericConstant::Type::Int:
            if (!isOfKind(types, TypeAttribute::Signed)) {
                report.addError("Number constant (type mismatch)", constant.getSrcLoc());
            }
            break;
        case AstNumericConstant::Type::Uint:
            if (!isOfKind(types, TypeAttribute::Unsigned)) {
                report.addError("Unsigned constant (type mismatch)", constant.getSrcLoc());
            }
            break;
        case AstNumericConstant::Type::Float:
            if (!isOfKind(types, TypeAttribute::Float)) {
                report.addError("Float constant (type mismatch)", constant.getSrcLoc());
            }
            break;
    }
}

void TypeChecker::visitNilConstant(const AstNilConstant& constant) {
    TypeSet types = typeAnalysis.getTypes(&constant);
    if (!isOfKind(types, TypeAttribute::Record)) {
        report.addError("Nil constant used as a non-record", constant.getSrcLoc());
        return;
    }
}

void TypeChecker::visitRecordInit(const AstRecordInit& rec) {
    TypeSet types = typeAnalysis.getTypes(&rec);

    if (!isOfKind(types, TypeAttribute::Record) || types.size() != 1) {
        report.addError("Ambiguous record", rec.getSrcLoc());
        return;
    }

    // At this point we know that there is exactly one type in set, so we can take it.
    auto& recordType = *as<RecordType>(*types.begin());

    if (recordType.getFields().size() != rec.getArguments().size()) {
        report.addError("Wrong number of arguments given to record", rec.getSrcLoc());
        return;
    }
}

void TypeChecker::visitTypeCast(const AstTypeCast& cast) {
    if (!typeEnv.isType(cast.getType())) {
        report.addError(
                tfm::format("Type cast to the undeclared type \"%s\"", cast.getType()), cast.getSrcLoc());
        return;
    }

    auto& castTypes = typeAnalysis.getTypes(&cast);
    auto& argTypes = typeAnalysis.getTypes(cast.getValue());

    if (castTypes.isAll() || castTypes.size() != 1) {
        report.addError("Unable to deduce type of the argument (cast)", cast.getSrcLoc());
        return;
    }

    // This should be reported elsewhere
    if (argTypes.isAll() || castTypes.size() != 1) {
        return;
    }

    // We know that both sets have size 1.
    auto& castTy = *castTypes.begin();
    auto& argTy = *argTypes.begin();

    if (!haveCommonSupertype(castTy, argTy)) {
        report.addError(
                tfm::format(R"(Type "%s" can't be converted to "%s")", argTy, castTy), cast.getSrcLoc());
        return;
    }
}

void TypeChecker::visitIntrinsicFunctor(const AstIntrinsicFunctor& fun) {
    if (!fun.getFunctionInfo()) {  // no info => no overload found during inference
        auto args = fun.getArguments();
        if (!isValidFunctorOpArity(fun.getFunction(), args.size())) {
            report.addError("invalid overload (arity mismatch)", fun.getSrcLoc());
            return;
        }

        assert(validOverloads(typeAnalysis, fun).empty() && "polymorphic transformation wasn't applied?");
        report.addError("no valid overloads", fun.getSrcLoc());
    }
}

void TypeChecker::visitUserDefinedFunctor(const AstUserDefinedFunctor& fun) {
    // check type of result
    const TypeSet& resultType = typeAnalysis.getTypes(&fun);

    if (!isOfKind(resultType, fun.getReturnType())) {
        switch (fun.getReturnType()) {
            case TypeAttribute::Signed:
                report.addError("Non-numeric use for numeric functor", fun.getSrcLoc());
                break;
            case TypeAttribute::Unsigned:
                report.addError("Non-unsigned use for unsigned functor", fun.getSrcLoc());
                break;
            case TypeAttribute::Float:
                report.addError("Non-float use for float functor", fun.getSrcLoc());
                break;
            case TypeAttribute::Symbol:
                report.addError("Non-symbolic use for symbolic functor", fun.getSrcLoc());
                break;
            case TypeAttribute::Record: fatal("Invalid return type");
        }
    }

    size_t i = 0;
    for (auto arg : fun.getArguments()) {
        if (!isOfKind(typeAnalysis.getTypes(arg), fun.getArgType(i))) {
            switch (fun.getArgType(i)) {
                case TypeAttribute::Signed:
                    report.addError("Non-numeric argument for functor", arg->getSrcLoc());
                    break;
                case TypeAttribute::Symbol:
                    report.addError("Non-symbolic argument for functor", arg->getSrcLoc());
                    break;
                case TypeAttribute::Unsigned:
                    report.addError("Non-unsigned argument for functor", arg->getSrcLoc());
                    break;
                case TypeAttribute::Float:
                    report.addError("Non-float argument for functor", arg->getSrcLoc());
                    break;
                case TypeAttribute::Record: fatal("Invalid argument type");
            }
        }
        ++i;
    }
}

void TypeChecker::visitBinaryConstraint(const AstBinaryConstraint& constraint) {
    auto op = constraint.getOperator();
    auto left = constraint.getLHS();
    auto right = constraint.getRHS();
    auto opRamTypes = getBinaryConstraintTypes(op);
    // Skip checks if either side is `Bottom` b/c it just adds noise.
    // The unable-to-deduce-type checker will point out the issue.
    if (typeAnalysis.getTypes(left).empty() || typeAnalysis.getTypes(right).empty()) return;

    // give them a slightly nicer error
    if (isOrderedBinaryConstraintOp(op) && typeAnalysis.getTypes(left) != typeAnalysis.getTypes(right)) {
        report.addError("Cannot compare different types", constraint.getSrcLoc());
    } else {
        auto checkTyAttr = [&](AstArgument const& side) {
            auto opMatchesType = any_of(opRamTypes,
                    [&](auto& ramType) { return isOfKind(typeAnalysis.getTypes(&side), ramType); });

            if (!opMatchesType) {
                std::stringstream ss;
                ss << "Constraint requires an operand of type "
                   << join(opRamTypes, " or ", [&](auto& out, auto& ramTy) {
                          switch (ramTy) {
                              case TypeAttribute::Signed: out << "`number`"; break;
                              case TypeAttribute::Symbol: out << "`symbol`"; break;
                              case TypeAttribute::Unsigned: out << "`unsigned`"; break;
                              case TypeAttribute::Float: out << "`float`"; break;
                              case TypeAttribute::Record: out << "a record"; break;
                          }
                      });
                report.addError(ss.str(), side.getSrcLoc());
            }
        };

        checkTyAttr(*left);
        checkTyAttr(*right);
    }
}

void TypeChecker::visitAggregator(const AstAggregator& aggregator) {
    auto op = aggregator.getOperator();

    auto aggregatorType = typeAnalysis.getTypes(&aggregator);

    TypeAttribute opType = getTypeAttributeAggregate(op);

    // Check if operation type and return type agree.
    if (!isOfKind(aggregatorType, opType)) {
        report.addError("Couldn't assign types to the aggregator", aggregator.getSrcLoc());
    }
}

void GroundedTermsChecker::verify(AstTranslationUnit& translationUnit) {
    auto&& program = *translationUnit.getProgram();
    auto&& report = translationUnit.getErrorReport();

    // -- check grounded variables and records --
    visitDepthFirst(program.getClauses(), [&](const AstClause& clause) {
        if (isFact(clause)) return;  // only interested in rules

        auto isGrounded = getGroundedTerms(translationUnit, clause);

        std::set<std::string> reportedVars;
        // all terms in head need to be grounded
        for (auto&& cur : getVariables(clause)) {
            if (!isGrounded[cur] && reportedVars.insert(cur->getName()).second) {
                report.addError("Ungrounded variable " + cur->getName(), cur->getSrcLoc());
            }
        }

        // all records need to be grounded
        for (auto&& cur : getRecords(clause)) {
            if (!isGrounded[cur]) {
                report.addError("Ungrounded record", cur->getSrcLoc());
            }
        }
    });
}

}  // end of namespace souffle
