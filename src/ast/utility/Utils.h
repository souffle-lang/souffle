/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Utils.h
 *
 * A collection of utilities operating on AST
 *
 ***********************************************************************/

#pragma once

#include "FunctorOps.h"
#include "souffle/utility/DynamicCasting.h"
#include "souffle/utility/Types.h"
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace souffle::ast {

// some forward declarations
class Atom;
class Clause;
class Constraint;
class Directive;
class FunctorDeclaration;
class IntrinsicFunctor;
class Literal;
class Node;
class Program;
class QualifiedName;
class Relation;
class TranslationUnit;
class Type;
class Variable;
class RecordInit;

namespace analysis {
class TypeAnalysis;
}

// ---------------------------------------------------------------
//                      General Utilities
// ---------------------------------------------------------------

// Deliberately wraps `toString` in order to assure `pprint` works for
// all AST nodes during debugging. If `toString` were to be used, only
// the specific instanciations would be available at runtime.
std::string pprint(const Node& node);

/**
 * Obtains a list of all variables referenced within the AST rooted
 * by the given root node.
 *
 * @param root the root of the AST to be searched
 * @return a list of all variables referenced within
 */
std::vector<const Variable*> getVariables(const Node& root);

/**
 * Obtains a list of all records referenced within the AST rooted
 * by the given root node.
 *
 * @param root the root of the AST to be searched
 * @return a list of all records referenced within
 */
std::vector<const RecordInit*> getRecords(const Node& root);

/**
 * Returns literals of a particular type in the body of a clause.
 *
 * @param the clause
 * @return vector of body literals of the specified type
 */
template <typename T, typename C>
std::vector<T*> getBodyLiterals(const C& clause) {
    std::vector<T*> res;
    for (auto& lit : clause.getBodyLiterals()) {
        if (T* t = as<T>(lit)) {
            res.push_back(t);
        }
    }
    return res;
}

/**
 * Returns a vector of clauses in the program describing the relation with the given name.
 *
 * @param program the program
 * @param name the name of the relation to search for
 * @return vector of clauses describing the relation with the given name
 */
std::vector<Clause*> getClauses(const Program& program, const QualifiedName& relationName);

/**
 * Returns a vector of clauses in the program describing the given relation.
 *
 * @param program the program
 * @param rel the relation to search for
 * @return vector of clauses describing the given relation
 */
std::vector<Clause*> getClauses(const Program& program, const Relation& rel);

/**
 * Returns the relation with the given name in the program.
 *
 * @param program the program
 * @param name the name of the relation to search for
 * @return the relation if it exists; nullptr otherwise
 */
Relation* getRelation(const Program& program, const QualifiedName& name);

/**
 * Returns the functor declaration with the given name in the program.
 *
 * @param program the program
 * @param name the name of the functor to search for
 * @return the functor declaration if it exists; nullptr otherwise
 */
FunctorDeclaration* getFunctorDeclaration(const Program& program, const std::string& name);

/**
 * Returns the set of directives associated with a given relation in a program.
 *
 * @param program the program
 * @param name the name of the relation to search for
 * @return a vector of all associated directives
 */
std::vector<Directive*> getDirectives(const Program& program, const QualifiedName& relationName);

/**
 * Remove relation and all its clauses from the program.
 *
 * @param tu the translation unit
 * @param name the name of the relation to delete
 */
void removeRelation(TranslationUnit& tu, const QualifiedName& name);

/**
 * Removes the set of clauses with the given relation name.
 *
 * @param tu the translation unit
 * @param name the name of the relation to search for
 */
void removeRelationClauses(TranslationUnit& tu, const QualifiedName& name);

/**
 * Removes the set of IOs with the given relation name.
 *
 * @param tu the translation unit
 * @param name the name of the relation to search for
 */
void removeRelationIOs(TranslationUnit& tu, const QualifiedName& name);

/**
 * Returns the relation referenced by the given atom.
 * @param atom the atom
 * @param program the program containing the relations
 * @return relation referenced by the atom
 */
const Relation* getAtomRelation(const Atom* atom, const Program* program);

/**
 * Returns the relation referenced by the head of the given clause.
 * @param clause the clause
 * @param program the program containing the relations
 * @return relation referenced by the clause head
 */
const Relation* getHeadRelation(const Clause* clause, const Program* program);

/**
 * Returns the relations referenced in the body of the given clause.
 * @param clause the clause
 * @param program the program containing the relations
 * @return relation referenced in the clause body
 */
std::set<const Relation*> getBodyRelations(const Clause* clause, const Program* program);

/**
 * Returns whether the given relation has any clauses which contain a negation of a specific relation.
 * @param relation the relation to search the clauses of
 * @param negRelation the relation to search for negations of in clause bodies
 * @param program the program containing the relations
 * @param foundLiteral set to the negation literal that was found
 */
bool hasClauseWithNegatedRelation(const Relation* relation, const Relation* negRelation,
        const Program* program, const Literal*& foundLiteral);

/**
 * Returns whether the given relation has any clauses which contain an aggregation over of a specific
 * relation.
 * @param relation the relation to search the clauses of
 * @param aggRelation the relation to search for in aggregations in clause bodies
 * @param program the program containing the relations
 * @param foundLiteral set to the literal found in an aggregation
 */
bool hasClauseWithAggregatedRelation(const Relation* relation, const Relation* aggRelation,
        const Program* program, const Literal*& foundLiteral);

/**
 * Returns whether the given clause is recursive.
 * @param clause the clause to check
 * @return true iff the clause is recursive
 */
bool isRecursiveClause(const Clause& clause);

/**
 * Returns whether the given clause is a fact
 * @return true iff the clause is a fact
 */
bool isFact(const Clause& clause);

/**
 * Returns whether the given clause is a rule
 * @return true iff the clause is a rule
 */
bool isRule(const Clause& clause);

/**
 * Returns whether the given atom is a propositon
 * @return true iff the atom has no arguments
 */
bool isProposition(const Atom* atom);

/**
 * Returns whether the given atom is a delta relation
 * @return true iff the atom is a delta relation
 */
bool isDeltaRelation(const QualifiedName& name);

/**
 * Returns a clause which contains head of the given clause
 * @param clause the clause which head to be cloned
 * @return pointer to clause which has head cloned from given clause
 */
Own<Clause> cloneHead(const Clause& clause);

/**
 * Reorders the atoms of a clause to be in the given order.
 * Remaining body literals remain in the same order.
 *
 * E.g. if atoms are [a,b,c] and given order is [1,2,0], then
 * the final atom order will be [b,c,a].
 *
 * @param clause clause to reorder atoms in
 * @param newOrder new order of atoms; atoms[i] = atoms[newOrder[i]]
 */
Clause* reorderAtoms(const Clause* clause, const std::vector<unsigned int>& newOrder);

/**
 * Reorders a vector of atoms to be in the given order.
 *
 * @param atoms atoms to reorder
 * @param newOrder new order of atoms; atoms[i] = atoms[newOrder[i]]
 */
std::vector<Atom*> reorderAtoms(const std::vector<Atom*>& atoms, const std::vector<unsigned int>& newOrder);

/**
 * Negate an ast constraint
 *
 * @param constraint constraint that will be negated
 */
void negateConstraintInPlace(Constraint& constraint);

/**
 * Pick valid overloads for a functor, sorted by some measure of "preference".
 */
IntrinsicFunctors validOverloads(const analysis::TypeAnalysis&, const IntrinsicFunctor&);

/**
 * Rename all atoms hat appear in a node to a given name.
 * @param node node to alter the children of
 * @param oldToNew map from old atom names to new atom names
 * @return true if the node was changed
 */
bool renameAtoms(Node& node, const std::map<QualifiedName, QualifiedName>& oldToNew);

}  // namespace souffle::ast
