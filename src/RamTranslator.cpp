/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamTranslator.cpp
 *
 * Implementations of a translator from AST to RAM structures.
 *
 ***********************************************************************/

#include "RamTranslator.h"
#include "AstClause.h"
#include "AstIODirective.h"
#include "AstProgram.h"
#include "AstRelation.h"
#include "AstTypeAnalysis.h"
#include "AstUtils.h"
#include "AstVisitor.h"
#include "BinaryConstraintOps.h"
#include "Global.h"
#include "PrecedenceGraph.h"
#include "RamStatement.h"

namespace souffle {

namespace {

SymbolMask getSymbolMask(const AstRelation& rel, const TypeEnvironment& typeEnv) {
    auto arity = rel.getArity();
    SymbolMask res(arity);
    for (size_t i = 0; i < arity; i++) {
        res.setSymbol(i, isSymbolType(typeEnv.getType(rel.getAttribute(i)->getTypeName())));
    }
    return res;
}

/**
 * Converts the given relation identifier into a relation name.
 */
std::string getRelationName(const AstRelationIdentifier& id) {
    return toString(join(id.getNames(), "-"));
}

RamRelationIdentifier getRamRelationIdentifier(std::string name, unsigned arity, const AstRelation* rel,
        const TypeEnvironment* typeEnv, const bool istemp = false) {
    // avoid name conflicts for temporary identifiers
    if (istemp) {
        name.insert(0, "@");
    }

    if (!rel) {
        return RamRelationIdentifier(name, arity, istemp);
    }

    assert(arity == rel->getArity());
    std::vector<std::string> attributeNames;
    std::vector<std::string> attributeTypeQualifiers;
    for (unsigned int i = 0; i < arity; i++) {
        attributeNames.push_back(rel->getAttribute(i)->getAttributeName());
        if (typeEnv) {
            attributeTypeQualifiers.push_back(
                    getTypeQualifier(typeEnv->getType(rel->getAttribute(i)->getTypeName())));
        }
    }

    IODirectives inputDirectives;
    std::vector<IODirectives> outputDirectives;
    // If IO directives have been specified then set them up
    for (const auto& current : rel->getIODirectives()) {
        if (current->isInput()) {
            for (const auto& currentPair : current->getIODirectiveMap()) {
                inputDirectives.set(currentPair.first, currentPair.second);
            }
        } else if (current->isOutput()) {
            IODirectives ioDirectives;
            for (const auto& currentPair : current->getIODirectiveMap()) {
                ioDirectives.set(currentPair.first, currentPair.second);
            }
            outputDirectives.push_back(ioDirectives);
        }
    }
    // handle defaults
    if (rel->isInput()) {
        inputDirectives.setRelationName(getRelationName(rel->getName()));
        // Set a default IO type of file and a default filename if not supplied.
        if (!inputDirectives.has("IO")) {
            inputDirectives.setIOType("file");
        }
        if (inputDirectives.getIOType() == "file" && !inputDirectives.has("filename")) {
            inputDirectives.setFileName(inputDirectives.getRelationName() + ".facts");
        }

        // If filename is not an absolute path, concat with cmd line facts directory
        if (inputDirectives.getIOType() == "file" && inputDirectives.getFileName().front() != '/') {
            inputDirectives.setFileName(
                    Global::config().get("fact-dir") + "/" + inputDirectives.getFileName());
        }
    }
    if (rel->isOutput()) {
        if (Global::config().get("output-dir") == "-") {
            outputDirectives.clear();
            IODirectives ioDirectives;
            ioDirectives.setIOType("stdout");
            outputDirectives.push_back(ioDirectives);
        } else if (outputDirectives.empty()) {
            IODirectives ioDirectives;
            ioDirectives.setIOType("file");
            ioDirectives.setFileName(getRelationName(rel->getName()) + ".csv");
            outputDirectives.push_back(ioDirectives);
        }
        // If stdout is requested then remove all directives from the datalog file.
        for (auto& ioDirectives : outputDirectives) {
            ioDirectives.setRelationName(getRelationName(rel->getName()));
            if (!ioDirectives.has("IO")) {
                ioDirectives.setIOType("file");
            }
            if (ioDirectives.getIOType() == "file" && !ioDirectives.has("filename")) {
                ioDirectives.setFileName(ioDirectives.getRelationName() + ".csv");
            }
            if (ioDirectives.getIOType() == "file" && ioDirectives.getFileName().front() != '/') {
                ioDirectives.setFileName(
                        Global::config().get("output-dir") + "/" + ioDirectives.get("filename"));
            }
        }
    }
    return RamRelationIdentifier(name, arity, attributeNames, attributeTypeQualifiers,
            getSymbolMask(*rel, *typeEnv), rel->isInput(), rel->isComputed(), rel->isOutput(), rel->isBTree(),
            rel->isBrie(), rel->isEqRel(), rel->isData(), inputDirectives, outputDirectives, istemp);
}
}  // namespace

std::string RamTranslator::translateRelationName(const AstRelationIdentifier& id) {
    return getRelationName(id);
}

namespace {

/**
 * The location of some value in a loop nest.
 */
struct Location {
    int level;         // < the loop level
    int component;     // < the component within the tuple created in the given level
    std::string name;  // < name of the variable

    bool operator==(const Location& loc) const {
        return level == loc.level && component == loc.component;
    }

    bool operator!=(const Location& loc) const {
        return !(*this == loc);
    }

    bool operator<(const Location& loc) const {
        return level < loc.level || (level == loc.level && component < loc.component);
    }

    void print(std::ostream& out) const {
        out << "(" << level << "," << component << ")";
    }

    friend std::ostream& operator<<(std::ostream& out, const Location& loc) {
        loc.print(out);
        return out;
    }
};

/**
 * A class indexing the location of variables and record
 * references within a loop nest resulting from the conversion
 * of a rule.
 */
class ValueIndex {
    /**
     * The type mapping variables (referenced by their names) to the
     * locations where they are used.
     */
    typedef std::map<std::string, std::set<Location>> variable_reference_map;

    /**
     * The type mapping record init expressions to their definition points,
     * hence the point where they get grounded/bound.
     */
    typedef std::map<const AstRecordInit*, Location> record_definition_map;

    /**
     * The type mapping record init expressions to the loop level where
     * they get unpacked.
     */
    typedef std::map<const AstRecordInit*, int> record_unpack_map;

    /**
     * A map from AstAggregators to storage locations. Note, since in this case
     * AstAggregators are indexed by their values (not their address) no standard
     * map can be utilized.
     */
    typedef std::vector<std::pair<const AstAggregator*, Location>> aggregator_location_map;

    /** The index of variable accesses */
    variable_reference_map var_references;

    /** The index of record definition points */
    record_definition_map record_definitions;

    /** The index of record-unpack levels */
    record_unpack_map record_unpacks;

    /** The level of a nested ram operation that is handling a given aggregator operation */
    aggregator_location_map aggregator_locations;

public:
    // -- variables --

    void addVarReference(const AstVariable& var, const Location& l) {
        std::set<Location>& locs = var_references[var.getName()];
        locs.insert(l);
    }

    void addVarReference(const AstVariable& var, int level, int pos, const std::string& name = "") {
        addVarReference(var, Location({level, pos, name}));
    }

    bool isDefined(const AstVariable& var) const {
        return var_references.find(var.getName()) != var_references.end();
    }

    const Location& getDefinitionPoint(const AstVariable& var) const {
        auto pos = var_references.find(var.getName());
        assert(pos != var_references.end() && "Undefined variable referenced!");
        return *pos->second.begin();
    }

    const variable_reference_map& getVariableReferences() const {
        return var_references;
    }

    // -- records --

    // - definition -

    void setRecordDefinition(const AstRecordInit& init, const Location& l) {
        record_definitions[&init] = l;
    }

    void setRecordDefinition(const AstRecordInit& init, int level, int pos, std::string name = "") {
        setRecordDefinition(init, Location({level, pos, name}));
    }

    const Location& getDefinitionPoint(const AstRecordInit& init) const {
        auto pos = record_definitions.find(&init);
        if (pos != record_definitions.end()) {
            return pos->second;
        }
        assert(false && "Requested location for undefined record!");

        static Location fail;
        return fail;
    }

    // - unpacking -

    void setRecordUnpackLevel(const AstRecordInit& init, int level) {
        record_unpacks[&init] = level;
    }

    int getRecordUnpackLevel(const AstRecordInit& init) const {
        auto pos = record_unpacks.find(&init);
        if (pos != record_unpacks.end()) {
            return pos->second;
        }
        assert(false && "Requested record is not unpacked properly!");
        return 0;
    }

    // -- aggregates --

    void setAggregatorLocation(const AstAggregator& agg, const Location& loc) {
        aggregator_locations.push_back(std::make_pair(&agg, loc));
    }

    const Location& getAggregatorLocation(const AstAggregator& agg) const {
        // search list
        for (const auto& cur : aggregator_locations) {
            if (*cur.first == agg) {
                return cur.second;
            }
        }

        // fail
        std::cout << "Lookup of " << &agg << " = " << agg << " failed\n";
        assert(false && "Requested aggregation operation is not processed!");
        const static Location fail = Location();
        return fail;
    }

    // -- others --

    bool isSomethingDefinedOn(int level) const {
        // check for variable definitions
        for (const auto& cur : var_references) {
            if (cur.second.begin()->level == level) {
                return true;
            }
        }
        // check for record definitions
        for (const auto& cur : record_definitions) {
            if (cur.second.level == level) {
                return true;
            }
        }
        // nothing defined on this level
        return false;
    }

    void print(std::ostream& out) const {
        out << "Variables:\n\t";
        out << join(var_references, "\n\t");
    }

    friend std::ostream& operator<<(std::ostream& out, const ValueIndex& index) __attribute__((unused)) {
        index.print(out);
        return out;
    }
};

std::unique_ptr<RamValue> translateValue(const AstArgument* arg, const ValueIndex& index = ValueIndex()) {
    std::unique_ptr<RamValue> val;
    if (!arg) {
        return val;
    }

    if (const AstVariable* var = dynamic_cast<const AstVariable*>(arg)) {
        ASSERT(index.isDefined(*var) && "variable not grounded");
        const Location& loc = index.getDefinitionPoint(*var);
        val = std::unique_ptr<RamValue>(new RamElementAccess(loc.level, loc.component, loc.name));
    } else if (dynamic_cast<const AstUnnamedVariable*>(arg)) {
        return nullptr;  // utilized to identify _ values
    } else if (const AstConstant* c = dynamic_cast<const AstConstant*>(arg)) {
        val = std::unique_ptr<RamValue>(new RamNumber(c->getIndex()));
    } else if (const AstUnaryFunctor* uf = dynamic_cast<const AstUnaryFunctor*>(arg)) {
        val = std::unique_ptr<RamValue>(
                new RamUnaryOperator(uf->getFunction(), translateValue(uf->getOperand(), index)));
    } else if (const AstBinaryFunctor* bf = dynamic_cast<const AstBinaryFunctor*>(arg)) {
        val = std::unique_ptr<RamValue>(new RamBinaryOperator(
                bf->getFunction(), translateValue(bf->getLHS(), index), translateValue(bf->getRHS(), index)));
    } else if (const AstTernaryFunctor* tf = dynamic_cast<const AstTernaryFunctor*>(arg)) {
        val = std::unique_ptr<RamValue>(
                new RamTernaryOperator(tf->getFunction(), translateValue(tf->getArg(0), index),
                        translateValue(tf->getArg(1), index), translateValue(tf->getArg(2), index)));
    } else if (dynamic_cast<const AstCounter*>(arg)) {
        val = std::unique_ptr<RamValue>(new RamAutoIncrement());
    } else if (const AstRecordInit* init = dynamic_cast<const AstRecordInit*>(arg)) {
        std::vector<std::unique_ptr<RamValue>> values;
        for (const auto& cur : init->getArguments()) {
            values.push_back(translateValue(cur, index));
        }
        val = std::unique_ptr<RamValue>(new RamPack(std::move(values)));
    } else if (const AstAggregator* agg = dynamic_cast<const AstAggregator*>(arg)) {
        // here we look up the location the aggregation result gets bound
        auto loc = index.getAggregatorLocation(*agg);
        val = std::unique_ptr<RamValue>(new RamElementAccess(loc.level, loc.component, loc.name));
    } else {
        std::cout << "Unsupported node type of " << arg << ": " << typeid(*arg).name() << "\n";
        ASSERT(false && "unknown AST node type not permissible");
    }

    return val;
}

std::unique_ptr<RamValue> translateValue(const AstArgument& arg, const ValueIndex& index = ValueIndex()) {
    return translateValue(&arg, index);
}
}  // namespace

/** generate RAM code for a clause */
std::unique_ptr<RamStatement> RamTranslator::translateClause(
        const AstClause& clause, const AstProgram* program, const TypeEnvironment* typeEnv, int version) {
    // check whether there is an imposed order constraint
    if (clause.getExecutionPlan() && clause.getExecutionPlan()->hasOrderFor(version)) {
        // get the imposed order
        const AstExecutionOrder& order = clause.getExecutionPlan()->getOrderFor(version);

        // create a copy and fix order
        std::unique_ptr<AstClause> copy(clause.clone());

        // Change order to start at zero
        std::vector<unsigned int> newOrder(order.size());
        std::transform(order.begin(), order.end(), newOrder.begin(),
                [](unsigned int i) -> unsigned int { return i - 1; });

        // re-order atoms
        copy->reorderAtoms(newOrder);

        // clear other order and fix plan
        copy->clearExecutionPlan();
        copy->setFixedExecutionPlan();

        // translate reordered clause
        return translateClause(*copy, program, typeEnv, version);
    }

    // get extract some details
    const AstAtom& head = *clause.getHead();

    // a utility to translate atoms to relations
    auto getRelation = [&](const AstAtom* atom) {
        return getRamRelationIdentifier(getRelationName(atom->getName()), atom->getArity(),
                (program ? getAtomRelation(atom, program) : nullptr), typeEnv);
    };

    // handle facts
    if (clause.isFact()) {
        // translate arguments
        std::vector<std::unique_ptr<const RamValue>> values;
        for (auto& arg : clause.getHead()->getArguments()) {
            values.push_back(translateValue(*arg));
        }

        // create a fact statement
        return std::unique_ptr<RamStatement>(new RamFact(getRelation(&head), std::move(values)));
    }

    // the rest should be rules
    assert(clause.isRule());

    // -- index values in rule --

    // create value index
    ValueIndex valueIndex;

    // the order of processed operations
    std::vector<const AstNode*> op_nesting;

    int level = 0;
    for (AstAtom* atom : clause.getAtoms()) {
        // index nested variables and records
        typedef std::vector<AstArgument*> arg_list;
        // std::map<const arg_list*, int> arg_level;
        std::map<const AstNode*, std::unique_ptr<arg_list>> nodeArgs;

        std::function<arg_list*(const AstNode*)> getArgList = [&](const AstNode* curNode) {
            if (!nodeArgs.count(curNode)) {
                if (auto rec = dynamic_cast<const AstRecordInit*>(curNode)) {
                    nodeArgs.insert(std::make_pair(
                            curNode, std::unique_ptr<arg_list>(new arg_list(rec->getArguments()))));
                } else if (auto atom = dynamic_cast<const AstAtom*>(curNode)) {
                    nodeArgs.insert(std::make_pair(
                            curNode, std::unique_ptr<arg_list>(new arg_list(atom->getArguments()))));
                } else {
                    assert(false && "node type doesn't have arguments!");
                }
            }
            arg_list* cur = nodeArgs[curNode].get();
            return cur;
        };

        std::map<const arg_list*, int> arg_level;
        nodeArgs.insert(std::make_pair(atom, std::unique_ptr<arg_list>(new arg_list(atom->getArguments()))));
        // the atom is obtained at the current level
        arg_level[nodeArgs[atom].get()] = level;
        op_nesting.push_back(atom);

        // increment nesting level for the atom
        level++;

        // relation
        RamRelationIdentifier relation = getRelation(atom);

        std::function<void(const AstNode*)> indexValues = [&](const AstNode* curNode) {
            arg_list* cur = getArgList(curNode);
            for (size_t pos = 0; pos < cur->size(); pos++) {
                // get argument
                auto& arg = (*cur)[pos];

                // check for variable references
                if (auto var = dynamic_cast<const AstVariable*>(arg)) {
                    if (pos < relation.getArity()) {
                        valueIndex.addVarReference(*var, arg_level[cur], pos, relation.getArg(pos));
                    } else {
                        valueIndex.addVarReference(*var, arg_level[cur], pos);
                    }
                }

                // check for nested records
                if (auto rec = dynamic_cast<const AstRecordInit*>(arg)) {
                    // introduce new nesting level for unpack
                    int unpack_level = level++;
                    op_nesting.push_back(rec);
                    arg_level[getArgList(rec)] = unpack_level;
                    valueIndex.setRecordUnpackLevel(*rec, unpack_level);

                    // register location of record
                    valueIndex.setRecordDefinition(*rec, arg_level[cur], pos);

                    // resolve nested components
                    indexValues(rec);
                }
            }
        };

        indexValues(atom);
    }

    // add aggregation functions
    std::vector<const AstAggregator*> aggregators;
    visitDepthFirstPostOrder(clause, [&](const AstAggregator& cur) {

        // add each aggregator expression only once
        if (any_of(aggregators, [&](const AstAggregator* agg) { return *agg == cur; })) {
            return;
        }

        int aggLoc = level++;
        valueIndex.setAggregatorLocation(cur, Location({aggLoc, 0}));

        // bind aggregator variables to locations
        assert(dynamic_cast<const AstAtom*>(cur.getBodyLiterals()[0]));
        const AstAtom& atom = static_cast<const AstAtom&>(*cur.getBodyLiterals()[0]);
        for (size_t pos = 0; pos < atom.getArguments().size(); ++pos) {
            if (const AstVariable* var = dynamic_cast<const AstVariable*>(atom.getArgument(pos))) {
                valueIndex.addVarReference(*var, aggLoc, (int)pos, getRelation(&atom).getArg(pos));
            }
        };

        // and remember aggregator
        aggregators.push_back(&cur);
    });

    // -- create RAM statement --

    // begin with projection
    RamProject* project = new RamProject(getRelation(&head), level);

    for (AstArgument* arg : head.getArguments()) {
        project->addArg(translateValue(arg, valueIndex));
    }

    // build up insertion call
    std::unique_ptr<RamOperation> op(project);  // start with innermost

    // add aggregator levels
    for (auto it = aggregators.rbegin(); it != aggregators.rend(); ++it) {
        const AstAggregator* cur = *it;
        level--;

        // translate aggregation function
        RamAggregate::Function fun = RamAggregate::MIN;
        switch (cur->getOperator()) {
            case AstAggregator::min:
                fun = RamAggregate::MIN;
                break;
            case AstAggregator::max:
                fun = RamAggregate::MAX;
                break;
            case AstAggregator::count:
                fun = RamAggregate::COUNT;
                break;
            case AstAggregator::sum:
                fun = RamAggregate::SUM;
                break;
        }

        // translate target expression
        std::unique_ptr<RamValue> value = translateValue(cur->getTargetExpression(), valueIndex);

        // translate body literal
        assert(cur->getBodyLiterals().size() == 1 && "Unsupported complex aggregation body encountered!");
        const AstAtom* atom = dynamic_cast<const AstAtom*>(cur->getBodyLiterals()[0]);
        assert(atom && "Unsupported complex aggregation body encountered!");

        // add Ram-Aggregation layer
        op = std::unique_ptr<RamOperation>(
                new RamAggregate(std::move(op), fun, std::move(value), getRelation(atom)));

        // add constant constraints
        for (size_t pos = 0; pos < atom->argSize(); ++pos) {
            if (AstConstant* c = dynamic_cast<AstConstant*>(atom->getArgument(pos))) {
                op->addCondition(std::unique_ptr<RamCondition>(new RamBinaryRelation(BinaryConstraintOp::EQ,
                        std::unique_ptr<RamValue>(new RamElementAccess(
                                level, pos, getRelation(atom).getArg(pos))),
                        std::unique_ptr<RamValue>(new RamNumber(c->getIndex())))));
            }
        }
    }

    // build operation bottom-up
    while (!op_nesting.empty()) {
        // get next operator
        const AstNode* cur = op_nesting.back();
        op_nesting.pop_back();

        // get current nesting level
        auto level = op_nesting.size();

        if (const AstAtom* atom = dynamic_cast<const AstAtom*>(cur)) {
            // find out whether a "search" or "if" should be issued
            bool isExistCheck = !valueIndex.isSomethingDefinedOn(level);
            for (size_t pos = 0; pos < atom->argSize(); ++pos) {
                if (dynamic_cast<AstAggregator*>(atom->getArgument(pos))) {
                    isExistCheck = false;
                }
            }

            // add a scan level
            op = std::unique_ptr<RamOperation>(new RamScan(getRelation(atom), std::move(op), isExistCheck));

            // add constraints
            for (size_t pos = 0; pos < atom->argSize(); ++pos) {
                if (AstConstant* c = dynamic_cast<AstConstant*>(atom->getArgument(pos))) {
                    op->addCondition(std::unique_ptr<RamCondition>(new RamBinaryRelation(
                            BinaryConstraintOp::EQ, std::unique_ptr<RamValue>(new RamElementAccess(
                                                            level, pos, getRelation(atom).getArg(pos))),
                            std::unique_ptr<RamValue>(new RamNumber(c->getIndex())))));
                } else if (AstAggregator* agg = dynamic_cast<AstAggregator*>(atom->getArgument(pos))) {
                    auto loc = valueIndex.getAggregatorLocation(*agg);
                    op->addCondition(std::unique_ptr<RamCondition>(new RamBinaryRelation(
                            BinaryConstraintOp::EQ, std::unique_ptr<RamValue>(new RamElementAccess(
                                                            level, pos, getRelation(atom).getArg(pos))),
                            std::unique_ptr<RamValue>(
                                    new RamElementAccess(loc.level, loc.component, loc.name)))));
                }
            }

            // TODO: support constants in nested records!
        } else if (const AstRecordInit* rec = dynamic_cast<const AstRecordInit*>(cur)) {
            // add an unpack level
            const Location& loc = valueIndex.getDefinitionPoint(*rec);
            op = std::unique_ptr<RamOperation>(
                    new RamLookup(std::move(op), loc.level, loc.component, rec->getArguments().size()));

            // add constant constraints
            for (size_t pos = 0; pos < rec->getArguments().size(); ++pos) {
                if (AstConstant* c = dynamic_cast<AstConstant*>(rec->getArguments()[pos])) {
                    op->addCondition(
                            std::unique_ptr<RamCondition>(new RamBinaryRelation(BinaryConstraintOp::EQ,
                                    std::unique_ptr<RamValue>(new RamElementAccess(level, pos)),
                                    std::unique_ptr<RamValue>(new RamNumber(c->getIndex())))));
                }
            }
        } else {
            std::cout << "Unsupported AST node type: " << typeid(*cur).name() << "\n";
            assert(false && "Unsupported AST node for creation of scan-level!");
        }
    }

    /* add equivalence constraints imposed by variable binding */
    for (const auto& cur : valueIndex.getVariableReferences()) {
        // the first appearance
        const Location& first = *cur.second.begin();
        // all other appearances
        for (const Location& loc : cur.second) {
            if (first != loc) {
                op->addCondition(std::unique_ptr<RamCondition>(new RamBinaryRelation(
                        BinaryConstraintOp::EQ, std::unique_ptr<RamValue>(new RamElementAccess(
                                                        first.level, first.component, first.name)),
                        std::unique_ptr<RamValue>(
                                new RamElementAccess(loc.level, loc.component, loc.name)))));
            }
        }
    }

    /* add conditions caused by atoms, negations, and binary relations */
    for (const auto& lit : clause.getBodyLiterals()) {
        // for atoms
        if (dynamic_cast<const AstAtom*>(lit)) {
            // covered already within the scan/lookup generation step

            // for binary relations
        } else if (auto binRel = dynamic_cast<const AstConstraint*>(lit)) {
            std::unique_ptr<RamValue> valLHS = translateValue(binRel->getLHS(), valueIndex);
            std::unique_ptr<RamValue> valRHS = translateValue(binRel->getRHS(), valueIndex);
            op->addCondition(std::unique_ptr<RamCondition>(
                    new RamBinaryRelation(binRel->getOperator(), translateValue(binRel->getLHS(), valueIndex),
                            translateValue(binRel->getRHS(), valueIndex))));

            // for negations
        } else if (auto neg = dynamic_cast<const AstNegation*>(lit)) {
            // get contained atom
            const AstAtom* atom = neg->getAtom();

            // create constraint
            RamNotExists* notExists = new RamNotExists(getRelation(atom));

            for (const auto& arg : atom->getArguments()) {
                notExists->addArg(translateValue(*arg, valueIndex));
            }

            // add constraint
            op->addCondition(std::unique_ptr<RamCondition>(notExists));
        } else {
            std::cout << "Unsupported node type: " << typeid(*lit).name();
            assert(false && "Unsupported node type!");
        }
    }

    /* generate the final RAM Insert statement */
    return std::unique_ptr<RamStatement>(new RamInsert(clause, std::move(op)));
}

/* utility for appending statements */
static void appendStmt(std::unique_ptr<RamStatement>& stmtList, std::unique_ptr<RamStatement> stmt) {
    if (stmt) {
        if (stmtList) {
            stmtList = std::unique_ptr<RamStatement>(new RamSequence(std::move(stmtList), std::move(stmt)));
        } else {
            stmtList = std::move(stmt);
        }
    }
};

/** generate RAM code for a non-recursive relation */
std::unique_ptr<RamStatement> RamTranslator::translateNonRecursiveRelation(const AstRelation& rel,
        const AstProgram* program, const RecursiveClauses* recursiveClauses, const TypeEnvironment& typeEnv) {
    /* start with an empty sequence */
    std::unique_ptr<RamStatement> res;

    // the ram table reference
    RamRelationIdentifier rrel =
            getRamRelationIdentifier(getRelationName(rel.getName()), rel.getArity(), &rel, &typeEnv);

    /* iterate over all clauses that belong to the relation */
    for (AstClause* clause : rel.getClauses()) {
        // skip recursive rules
        if (recursiveClauses->isRecursive(clause)) {
            continue;
        }

        // translate clause
        std::unique_ptr<RamStatement> rule = translateClause(*clause, program, &typeEnv);

        // add logging
        if (logging) {
            std::string clauseText = stringify(toString(*clause));
            std::ostringstream line;
            line << "nonrecursive-rule;" << rel.getName() << ";";
            line << clause->getSrcLoc() << ";";
            line << clauseText << ";";
            std::string label = line.str();
            rule = std::unique_ptr<RamStatement>(new RamSequence(
                    std::unique_ptr<RamStatement>(new RamLogTimer(std::move(rule), "@t-" + label)),
                    std::unique_ptr<RamStatement>(new RamLogSize(rrel, "@n-" + label))));
        }

        // add debug info
        std::ostringstream ds;
        ds << toString(*clause) << "\nin file ";
        ds << clause->getSrcLoc();
        rule = std::unique_ptr<RamStatement>(new RamDebugInfo(std::move(rule), ds.str()));

        // add rule to result
        appendStmt(res, std::move(rule));
    }

    // if no clauses have been translated, we are done
    if (!res) {
        return res;
    }

    // add logging for entire relation
    if (logging) {
        // compute label
        std::ostringstream line;
        line << "nonrecursive-relation;" << rel.getName() << ";";
        line << rel.getSrcLoc() << ";";
        std::string label = line.str();

        // add timer
        res = std::unique_ptr<RamStatement>(new RamLogTimer(std::move(res), "@t-" + label));

        // add table size printer
        appendStmt(res, std::unique_ptr<RamStatement>(new RamLogSize(rrel, "@n-" + label)));
    }

    // done
    return res;
}

namespace {

/**
 * A utility function assigning names to unnamed variables such that enclosing
 * constructs may be cloned without losing the variable-identity.
 */
void nameUnnamedVariables(AstClause* clause) {
    // the node mapper conducting the actual renaming
    struct Instantiator : public AstNodeMapper {
        mutable int counter;

        Instantiator() : counter(0) {}

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            // apply recursive
            node->apply(*this);

            // replace unknown variables
            if (dynamic_cast<AstUnnamedVariable*>(node.get())) {
                auto name = " _unnamed_var" + toString(++counter);
                return std::unique_ptr<AstNode>(new AstVariable(name));
            }

            // otherwise nothing
            return node;
        }
    };

    // name all variables in the atoms
    Instantiator init;
    for (auto& atom : clause->getAtoms()) {
        atom->apply(init);
    }
}
}  // namespace

/** generate RAM code for recursive relations in a strongly-connected component */
std::unique_ptr<RamStatement> RamTranslator::translateRecursiveRelation(
        const std::set<const AstRelation*>& scc, const AstProgram* program,
        const RecursiveClauses* recursiveClauses, const TypeEnvironment& typeEnv) {
    // initialize sections
    std::unique_ptr<RamStatement> preamble;
    std::unique_ptr<RamSequence> updateTable(new RamSequence());
    std::unique_ptr<RamStatement> postamble;

    // --- create preamble ---

    // mappings for temporary relations
    std::map<const AstRelation*, RamRelationIdentifier> rrel;
    std::map<const AstRelation*, RamRelationIdentifier> relDelta;
    std::map<const AstRelation*, RamRelationIdentifier> relNew;

    /* Compute non-recursive clauses for relations in scc and push
       the results in their delta tables. */
    for (const AstRelation* rel : scc) {
        std::unique_ptr<RamStatement> updateRelTable;

        /* create two temporary tables for relaxed semi-naive evaluation */
        auto relName = getRelationName(rel->getName());
        rrel[rel] = getRamRelationIdentifier(relName, rel->getArity(), rel, &typeEnv);

        relDelta[rel] = getRamRelationIdentifier("delta_" + relName, rel->getArity(), rel, &typeEnv, true);
        relNew[rel] = getRamRelationIdentifier("new_" + relName, rel->getArity(), rel, &typeEnv, true);

        /* create update statements for fixpoint (even iteration) */
        appendStmt(updateRelTable,
                std::unique_ptr<RamStatement>(new RamSequence(
                        std::unique_ptr<RamStatement>(new RamMerge(rrel[rel], relNew[rel])),
                        std::unique_ptr<RamStatement>(new RamSwap(relDelta[rel], relNew[rel])),
                        std::unique_ptr<RamStatement>(new RamClear(relNew[rel])))));

        /* measure update time for each relation */
        if (logging) {
            std::ostringstream ost, osn;
            ost << "@c-recursive-relation;" << rel->getName() << ";" << rel->getSrcLoc() << ";";
            updateRelTable =
                    std::unique_ptr<RamStatement>(new RamLogTimer(std::move(updateRelTable), ost.str()));
        }

        /* drop temporary tables after recursion */
        appendStmt(postamble, std::unique_ptr<RamStatement>(new RamSequence(
                                      std::unique_ptr<RamStatement>(new RamDrop(relDelta[rel])),
                                      std::unique_ptr<RamStatement>(new RamDrop(relNew[rel])))));

        /* Generate code for non-recursive part of relation */
        appendStmt(preamble, translateNonRecursiveRelation(*rel, program, recursiveClauses, typeEnv));

        /* Generate merge operation for temp tables */
        appendStmt(preamble, std::unique_ptr<RamStatement>(new RamMerge(relDelta[rel], rrel[rel])));

        /* Add update operations of relations to parallel statements */
        updateTable->add(std::move(updateRelTable));
    }

    // --- build main loop ---

    std::unique_ptr<RamParallel> loopSeq(new RamParallel());

    // create a utility to check SCC membership
    auto isInSameSCC = [&](
            const AstRelation* rel) { return std::find(scc.begin(), scc.end(), rel) != scc.end(); };

    /* Compute temp for the current tables */
    for (const AstRelation* rel : scc) {
        std::unique_ptr<RamStatement> loopRelSeq;

        /* Find clauses for relation rel */
        for (size_t i = 0; i < rel->clauseSize(); i++) {
            AstClause* cl = rel->getClause(i);

            // skip non-recursive clauses
            if (!recursiveClauses->isRecursive(cl)) {
                continue;
            }

            // each recursive rule results in several operations
            int version = 0;
            const auto& atoms = cl->getAtoms();
            for (size_t j = 0; j < atoms.size(); ++j) {
                const AstAtom* atom = atoms[j];
                const AstRelation* atomRelation = getAtomRelation(atom, program);

                // only interested in atoms within the same SCC
                if (!isInSameSCC(atomRelation)) {
                    continue;
                }

                // modify the processed rule to use relDelta and write to relNew
                std::unique_ptr<AstClause> r1(cl->clone());
                r1->getHead()->setName(relNew[rel].getName());
                r1->getAtoms()[j]->setName(relDelta[atomRelation].getName());
                r1->addToBody(std::unique_ptr<AstLiteral>(
                        new AstNegation(std::unique_ptr<AstAtom>(cl->getHead()->clone()))));

                // replace wildcards with variables (reduces indices when wildcards are used in recursive
                // atoms)
                nameUnnamedVariables(r1.get());

                // reduce R to P ...
                for (size_t k = j + 1; k < atoms.size(); k++) {
                    if (isInSameSCC(getAtomRelation(atoms[k], program))) {
                        AstAtom* cur = r1->getAtoms()[k]->clone();
                        cur->setName(relDelta[getAtomRelation(atoms[k], program)].getName());
                        r1->addToBody(
                                std::unique_ptr<AstLiteral>(new AstNegation(std::unique_ptr<AstAtom>(cur))));
                    }
                }

                std::unique_ptr<RamStatement> rule = translateClause(*r1, program, &typeEnv, version);

                /* add logging */
                if (logging) {
                    std::string clauseText = stringify(toString(*cl));
                    std::ostringstream line;
                    line << "recursive-rule;" << rel->getName() << ";";
                    line << version << ";";
                    line << cl->getSrcLoc() << ";";
                    line << clauseText << ";";
                    std::string label = line.str();
                    rule = std::unique_ptr<RamStatement>(new RamSequence(
                            std::unique_ptr<RamStatement>(new RamLogTimer(std::move(rule), "@t-" + label)),
                            std::unique_ptr<RamStatement>(new RamLogSize(relNew[rel], "@n-" + label))));
                }

                // add debug info
                std::ostringstream ds;
                ds << toString(*cl) << "\nin file ";
                ds << cl->getSrcLoc();
                rule = std::unique_ptr<RamStatement>(new RamDebugInfo(std::move(rule), ds.str()));

                // add to loop body
                appendStmt(loopRelSeq, std::move(rule));

                // increment version counter
                version++;
            }
            assert(cl->getExecutionPlan() == nullptr || version > cl->getExecutionPlan()->getMaxVersion());
        }

        // if there was no rule, continue
        if (!loopRelSeq) {
            continue;
        }

        // label all versions
        if (logging) {
            std::ostringstream line;
            line << "recursive-relation;" << rel->getName() << ";" << rel->getSrcLoc() << ";";
            std::string label = line.str();
            loopRelSeq = std::unique_ptr<RamStatement>(new RamLogTimer(std::move(loopRelSeq), "@t-" + label));
            appendStmt(loopRelSeq, std::unique_ptr<RamStatement>(new RamLogSize(relNew[rel], "@n-" + label)));
        }

        /* add rule computations of a relation to parallel statement */
        loopSeq->add(std::move(loopRelSeq));
    }

    /* construct exit conditions for odd and even iteration */
    auto addCondition = [](std::unique_ptr<RamCondition>& cond, std::unique_ptr<RamCondition> clause) {
        cond = ((cond) ? std::unique_ptr<RamCondition>(new RamAnd(std::move(cond), std::move(clause)))
                       : std::move(clause));
    };

    std::unique_ptr<RamCondition> exitCond;
    for (const AstRelation* rel : scc) {
        addCondition(exitCond, std::unique_ptr<RamCondition>(new RamEmpty(relNew[rel])));
    }

    /* construct fixpoint loop  */
    return std::unique_ptr<RamStatement>(new RamSequence(std::move(preamble),
            std::unique_ptr<RamStatement>(new RamLoop(std::move(loopSeq),
                    std::unique_ptr<RamStatement>(new RamExit(std::move(exitCond))), std::move(updateTable))),
            std::move(postamble)));

    assert(false && "Not Implemented");
    return nullptr;
}

/** translates the given datalog program into an equivalent RAM program  */
std::unique_ptr<RamStatement> RamTranslator::translateProgram(const AstTranslationUnit& translationUnit) {
    const TypeEnvironment& typeEnv =
            translationUnit.getAnalysis<TypeEnvironmentAnalysis>()->getTypeEnvironment();

    RecursiveClauses* recursiveClauses = translationUnit.getAnalysis<RecursiveClauses>();

    /* start with an empty sequence */
    std::unique_ptr<RamStatement> res;

    /* Compute SCCs of program */
    RelationSchedule* relationSchedule = translationUnit.getAnalysis<RelationSchedule>();

    // --- initialization ---

    /* Get relations of the program */
    auto rels = translationUnit.getProgram()->getRelations();

    /* Initialize all relations */
    for (AstRelation* rel : rels) {
        // initialize relation
        RamRelationIdentifier rrel =
                getRamRelationIdentifier(getRelationName(rel->getName()), rel->getArity(), rel, &typeEnv);
        appendStmt(res, std::unique_ptr<RamStatement>(new RamCreate(rrel)));

        // optional: load inputs
        if (rel->isInput()) {
            appendStmt(res, std::unique_ptr<RamStatement>(new RamLoad(rrel)));
        }

        // create delta-relations if necessary
        if (relationSchedule->isRecursive(rel)) {
            appendStmt(res, std::unique_ptr<RamStatement>(new RamCreate(
                                    getRamRelationIdentifier("delta_" + getRelationName(rel->getName()),
                                            rel->getArity(), rel, &typeEnv, true))));
            appendStmt(res, std::unique_ptr<RamStatement>(new RamCreate(
                                    getRamRelationIdentifier("new_" + getRelationName(rel->getName()),
                                            rel->getArity(), rel, &typeEnv, true))));
        }
    }

    // --- computation ---

    std::unique_ptr<RamStatement> comp;

    for (const RelationScheduleStep& step : relationSchedule->getSchedule()) {
        const std::set<const AstRelation*>& scc = step.getComputedRelations();
        std::unique_ptr<RamStatement> stmt;
        if (!step.isRecursive()) {
            ASSERT(scc.size() == 1 && "SCC contains more than one relation");
            const AstRelation* rel = *scc.begin();
            /* Run non-recursive evaluation */
            stmt = translateNonRecursiveRelation(
                    *rel, translationUnit.getProgram(), recursiveClauses, typeEnv);
        } else {
            stmt = translateRecursiveRelation(scc, translationUnit.getProgram(), recursiveClauses, typeEnv);
        }
        appendStmt(comp, std::move(stmt));

        /* Drop the tables of all expired relations to save memory */
        if (!Global::config().has("provenance")) {
            for (const auto& rel : step.getExpiredRelations()) {
                appendStmt(comp, std::unique_ptr<RamStatement>(new RamDrop(getRamRelationIdentifier(
                                         getRelationName(rel->getName()), rel->getArity(), rel, &typeEnv))));
            }
        }
    }

    // add logging entry for pure computation time
    appendStmt(res, std::move(comp));

    // --- output ---
    /* add store operations for output relations */
    for (AstRelation* rel : rels) {
        RamRelationIdentifier rrel =
                getRamRelationIdentifier(getRelationName(rel->getName()), rel->getArity(), rel, &typeEnv);
        if (rel->isOutput()) {
            appendStmt(res, std::unique_ptr<RamStatement>(new RamStore(rrel)));
        }
        if (rel->isPrintSize()) {
            appendStmt(res, std::unique_ptr<RamStatement>(new RamPrintSize(rrel)));
        }
        if (rel->isOutput()) {
            appendStmt(res, std::unique_ptr<RamStatement>(new RamDrop(rrel)));
        }
    }

    if (res && logging) {
        res = std::unique_ptr<RamStatement>(new RamLogTimer(std::move(res), "@runtime;"));
    }

    // done
    return res;
}

}  // end of namespace souffle
