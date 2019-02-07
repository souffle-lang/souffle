/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstTranslator.h
 *
 * Translator from AST into RAM
 *
 ***********************************************************************/

#pragma once

#include "AstArgument.h"
#include "AstClause.h"
#include "AstRelationIdentifier.h"
#include "AstUtils.h"
#include "IODirectives.h"
#include "RamRelation.h"
#include "RamStatement.h"
#include "RamValue.h"
#include "SymbolMask.h"

#include <map>
#include <memory>
#include <set>
#include <string>

namespace souffle {

// forward declarations
class AstClause;
class AstProgram;
class AstRelation;
class AstTranslationUnit;
class RamProgram;
class RamStatement;
class RamTranslationUnit;
class RecursiveClauses;
class TypeEnvironment;

/**
 * Main class for AST Translator
 */
class AstTranslator {
private:
    /** AST program */
    const AstProgram* program = nullptr;

    /** Type environment */
    const TypeEnvironment* typeEnv = nullptr;

    /** RAM program */
    std::unique_ptr<RamProgram> ramProg;

    /**
     * Concrete attribute
     */
    struct Location {
        int level;         // loop level
        int element;       // element in a tuple
        std::string name;  // name of associated attribute

        bool operator==(const Location& loc) const {
            return level == loc.level && element == loc.element;
        }

        bool operator!=(const Location& loc) const {
            return !(*this == loc);
        }

        bool operator<(const Location& loc) const {
            return level < loc.level || (level == loc.level && element < loc.element);
        }

        void print(std::ostream& out) const {
            out << "(" << level << "," << element << ")";
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
        using variable_reference_map = std::map<std::string, std::set<Location>>;

        /**
         * The type mapping record init expressions to their definition points,
         * hence the point where they get grounded/bound.
         */
        using record_definition_map = std::map<const AstRecordInit*, Location>;

        /**
         * The type mapping record init expressions to the loop level where
         * they get unpacked.
         */
        using record_unpack_map = std::map<const AstRecordInit*, int>;

        /**
         * A map from AstAggregators to storage locations. Note, since in this case
         * AstAggregators are indexed by their values (not their address) no standard
         * map can be utilized.
         */
        using aggregator_location_map = std::vector<std::pair<const AstAggregator*, Location>>;

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
            setRecordDefinition(init, Location({level, pos, std::move(name)}));
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

        bool isAggregator(const int level) const {
            // check for aggregator definitions
            for (const auto& cur : aggregator_locations) {
                if (cur.second.level == level) {
                    return true;
                }
            }
            // nothing defined on this location
            return false;
        }

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

    /**
     * assigns names to unnamed variables such that enclosing
     * constructs may be cloned without losing the variable-identity
     */
    void nameUnnamedVariables(AstClause* clause);

    /** append statement to a list of statements */
    void appendStmt(std::unique_ptr<RamStatement>& stmtList, std::unique_ptr<RamStatement> stmt);

    /** get symbol mask of a relation describing type attributes */
    SymbolMask getSymbolMask(const AstRelation& rel);

    /** converts the given relation identifier into a relation name */
    std::string getRelationName(const AstRelationIdentifier& id) {
        return toString(join(id.getNames(), "."));
    }

    void makeIODirective(IODirectives& ioDirective, const AstRelation* rel, const std::string& filePath,
            const std::string& fileExt, const bool isIntermediate);

    std::vector<IODirectives> getInputIODirectives(const AstRelation* rel,
            std::string filePath = std::string(), const std::string& fileExt = std::string());

    std::vector<IODirectives> getOutputIODirectives(const AstRelation* rel,
            std::string filePath = std::string(), const std::string& fileExt = std::string());

    /** create a reference to a RAM relation */
    std::unique_ptr<RamRelationReference> createRelationReference(const std::string name, const size_t arity,
            const std::vector<std::string> attributeNames,
            const std::vector<std::string> attributeTypeQualifiers, const SymbolMask mask, const bool input,
            const bool computed, const bool output, const bool btree, const bool brie, const bool eqrel);

    /** create a reference to a RAM relation */
    std::unique_ptr<RamRelationReference> createRelationReference(const std::string name, const size_t arity);

    /** a utility to translate atoms to relations */
    std::unique_ptr<RamRelationReference> translateRelation(const AstAtom* atom);

    /** translate an AST relation to a RAM relation */
    std::unique_ptr<RamRelationReference> translateRelation(
            const AstRelation* rel, const std::string relationNamePrefix = "");

    /** translate a temporary `delta` relation to a RAM relation for semi-naive evaluation */
    std::unique_ptr<RamRelationReference> translateDeltaRelation(const AstRelation* rel);

    /** translate a temporary `new` relation to a RAM relation for semi-naive evaluation */
    std::unique_ptr<RamRelationReference> translateNewRelation(const AstRelation* rel);

    /** translate an AST argument to a RAM value */
    std::unique_ptr<RamValue> translateValue(const AstArgument* arg, const ValueIndex& index);

    /** translate an AST constraint to a RAM condition */
    std::unique_ptr<RamCondition> translateConstraint(const AstLiteral* arg, const ValueIndex& index);

    /** translate AST clause to RAM code */
    class ClauseTranslator {
        // index nested variables and records
        using arg_list = std::vector<AstArgument*>;

        std::vector<const AstAggregator*> aggregators;

        // the order of processed operations
        std::vector<const AstNode*> op_nesting;

        std::unique_ptr<AstClause> getReorderedClause(const AstClause& clause, const int version) const;

        arg_list* getArgList(
                const AstNode* curNode, std::map<const AstNode*, std::unique_ptr<arg_list>>& nodeArgs) const;

        void indexValues(const AstNode* curNode,
                std::map<const AstNode*, std::unique_ptr<arg_list>>& nodeArgs,
                std::map<const arg_list*, int>& arg_level, RamRelationReference* relation);

        void createValueIndex(const AstClause& clause);

    protected:
        AstTranslator& translator;

        // create value index
        ValueIndex valueIndex;

        // current nesting level
        int level = 0;

        virtual std::unique_ptr<RamOperation> createOperation(const AstClause& clause);
        virtual std::unique_ptr<RamCondition> createCondition(const AstClause& originalClause);

    public:
        ClauseTranslator(AstTranslator& translator) : translator(translator) {}

        std::unique_ptr<RamStatement> translateClause(
                const AstClause& clause, const AstClause& originalClause, const int version = 0);
    };

    class ProvenanceClauseTranslator : public ClauseTranslator {
    protected:
        std::unique_ptr<RamOperation> createOperation(const AstClause& clause) override;
        std::unique_ptr<RamCondition> createCondition(const AstClause& originalClause) override;

    public:
        ProvenanceClauseTranslator(AstTranslator& translator) : ClauseTranslator(translator) {}
    };

    /**
     * translate RAM code for the non-recursive clauses of the given relation.
     *
     * @return a corresponding statement or null if there are no non-recursive clauses.
     */
    std::unique_ptr<RamStatement> translateNonRecursiveRelation(
            const AstRelation& rel, const RecursiveClauses* recursiveClauses);

    /** translate RAM code for recursive relations in a strongly-connected component */
    std::unique_ptr<RamStatement> translateRecursiveRelation(
            const std::set<const AstRelation*>& scc, const RecursiveClauses* recursiveClauses);

    /** translate RAM code for subroutine to get subproofs */
    std::unique_ptr<RamStatement> makeSubproofSubroutine(const AstClause& clause);

    /** translate AST to RAM Program */
    void translateProgram(const AstTranslationUnit& translationUnit);

public:
    AstTranslator() = default;

    /** translates AST to translation unit  */
    std::unique_ptr<RamTranslationUnit> translateUnit(AstTranslationUnit& tu);
};

}  // end of namespace souffle
