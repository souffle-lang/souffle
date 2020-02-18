/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstUtils.h
 *
 * A collection of utilities operating on AST constructs.
 *
 ***********************************************************************/

#pragma once

#include <cstddef>
#include <set>
#include <vector>

namespace souffle {

// some forward declarations
class AstAtom;
class AstClause;
class AstLiteral;
class AstNode;
class AstProgram;
class AstRelation;
class AstVariable;
class AstRecordInit;
class AstSumInit;

// ---------------------------------------------------------------
//                      General Utilities
// ---------------------------------------------------------------

/**
 * Obtains a list of all variables referenced within the AST rooted
 * by the given root node.
 *
 * @param root the root of the AST to be searched
 * @return a list of all variables referenced within
 */
std::vector<const AstVariable*> getVariables(const AstNode& root);

/**
 * Obtains a list of all records referenced within the AST rooted
 * by the given root node.
 *
 * @param root the root of the AST to be searched
 * @return a list of all records referenced within
 */
std::vector<const AstRecordInit*> getRecords(const AstNode& root);

/**
 * Obtains a list of all sum types referenced within the AST rooted
 * by the given root node.
 *
 * @param root the root of the AST to be searched
 * @return a list of all sum types referenced within
 */
std::vector<const AstSumInit*> getSums(const AstNode& root);

/**
 * Returns literals of a particular type in the body of a clause.
 *
 * @param the clause
 * @return vector of body literals of the specified type
 */
// TODO (azreika): add caching
template <typename T, typename C>
std::vector<T*> getBodyLiterals(const C& clause) {
    std::vector<T*> res;
    for (auto& lit : clause.getBodyLiterals()) {
        if (T* t = dynamic_cast<T*>(lit)) {
            res.push_back(t);
        }
    }
    return res;
}

/**
 * Returns the relation referenced by the given atom.
 * @param atom the atom
 * @param program the program containing the relations
 * @return relation referenced by the atom
 */
const AstRelation* getAtomRelation(const AstAtom* atom, const AstProgram* program);

/**
 * Returns the relation referenced by the head of the given clause.
 * @param clause the clause
 * @param program the program containing the relations
 * @return relation referenced by the clause head
 */
const AstRelation* getHeadRelation(const AstClause* clause, const AstProgram* program);

/**
 * Returns the relations referenced in the body of the given clause.
 * @param clause the clause
 * @param program the program containing the relations
 * @return relation referenced in the clause body
 */
std::set<const AstRelation*> getBodyRelations(const AstClause* clause, const AstProgram* program);

/**
 * Returns the index of a clause within its relation, ignoring facts.
 * Used in provenance as a unique ID for clauses within their relations.
 * @param program the program
 * @param clause the clause to get the index of
 * @return the index of the clause ignoring facts; 0 for facts
 */
size_t getClauseNum(const AstProgram* program, const AstClause* clause);

/**
 * Returns whether the given relation has any clauses which contain a negation of a specific relation.
 * @param relation the relation to search the clauses of
 * @param negRelation the relation to search for negations of in clause bodies
 * @param program the program containing the relations
 * @param foundLiteral set to the negation literal that was found
 */
bool hasClauseWithNegatedRelation(const AstRelation* relation, const AstRelation* negRelation,
        const AstProgram* program, const AstLiteral*& foundLiteral);

/**
 * Returns whether the given relation has any clauses which contain an aggregation over of a specific
 * relation.
 * @param relation the relation to search the clauses of
 * @param aggRelation the relation to search for in aggregations in clause bodies
 * @param program the program containing the relations
 * @param foundLiteral set to the literal found in an aggregation
 */
bool hasClauseWithAggregatedRelation(const AstRelation* relation, const AstRelation* aggRelation,
        const AstProgram* program, const AstLiteral*& foundLiteral);

/**
 * Returns whether the given clause is recursive.
 * @param clause the clause to check
 * @return true iff the clause is recursive
 */
bool isRecursiveClause(const AstClause& clause);

/**
 * Returns whether the given clause is a fact
 * @return true iff the clause is a fact
 */
bool isFact(const AstClause& clause);

/**
 * Returns whether the given clause is a rule
 * @return true iff the clause is a rule
 */
bool isRule(const AstClause& clause);

/**
 * Returns a clause which contains head of the given clause
 * @param clause the clause which head to be cloned
 * @return pointer to clause which has head cloned from given clause
 */
AstClause* cloneHead(const AstClause* clause);

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
AstClause* reorderAtoms(const AstClause* clause, const std::vector<unsigned int>& newOrder);
}  // end of namespace souffle
