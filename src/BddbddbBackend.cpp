/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file BddbddbBackend.cpp
 *
 * Defines the functionality for bddbddb backend.
 *
 ***********************************************************************/

#include "BddbddbBackend.h"
#include "AstTranslationUnit.h"
#include "AstVisitor.h"
#include "RamTypes.h"

#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace souffle {

namespace detail {

class BddbddbConverter : private AstVisitor<void, std::ostream&> {
    // literals aggregated to be added to the end of a rule while converting
    std::vector<std::string> extra_literals;

    int varCounter;

public:
    BddbddbConverter() : varCounter(0) {}

    void convert(std::ostream& out, const AstProgram& program) {
        visit(program, out);
    }

private:
    /**
     * The entry point for the conversion of a program, converting the basic top-level structure.
     */
    void visitProgram(const AstProgram& program, std::ostream& out) override {
        // type definition
        out << "N " << std::numeric_limits<RamDomain>::max() << "\n\n";

        // variable order
        int max_attributes = 0;
        for (const auto& rel : program.getRelations()) {
            max_attributes = std::max<int>(max_attributes, rel->getAttributes().size());
        }
        out << ".bddvarorder ";
        for (int i = 0; i < max_attributes; i++) {
            out << "N" << i;
            if (i + 1 != max_attributes) {
                out << "_";
            }
        }
        out << "\n\n";

        // declarations
        for (const auto& rel : program.getRelations()) {
            // process the relation declaration
            visit(*rel, out);
        }
        out << "\n";

        // rules
        for (const auto& rel : program.getRelations()) {
            for (const auto& clause : rel->getClauses()) {
                visit(*clause, out);
            }
        }
        out << "\n";
    }

    /**
     * Converting a relation by creating its declaration.
     */
    void visitRelation(const AstRelation& rel, std::ostream& out) override {
        visitRelationIdentifier(rel.getName(), out);

        // make nullary relations single-element relations
        out << "(";
        if (rel.getAttributes().empty()) {
            out << "dummy:N0";
        }
        int i = 0;
        out << join(rel.getAttributes(), ",", [&](std::ostream& out, AstAttribute* cur) {
            out << cur->getAttributeName() << ":N" << (i++);
        });
        out << ")";

        if (rel.isInput()) {
            out << " inputtuples";
        }
        if (rel.isOutput()) {
            out << " outputtuples";
        }

        out << "\n";
    }

    /**
     * Converting a clause.
     */
    void visitClause(const AstClause& clause, std::ostream& out) override {
        visit(*clause.getHead(), out);

        // if it is a fact, that's it
        if (clause.isFact()) {
            // there must not be any additons
            if (!extra_literals.empty()) {
                throw UnsupportedConstructException("Unsupported fact: " + toString(clause));
            }
            out << ".\n";
            return;
        }

        // convert the body
        out << " :- ";
        out << join(
                clause.getBodyLiterals(), ",", [&](std::ostream& out, AstLiteral* cur) { visit(*cur, out); });

        // add extra_literals
        for (const auto& cur : extra_literals) {
            out << "," << cur;
        }
        extra_literals.clear();

        // done
        out << ".\n";
    }

    void visitAtom(const AstAtom& atom, std::ostream& out) override {
        visitRelationIdentifier(atom.getName(), out);

        // since no nullary-relations are allowed, we add a dummy value
        if (atom.getArguments().empty()) {
            out << "(0)";
            return;
        }

        out << "(";
        out << join(atom.getArguments(), ",", [&](std::ostream& out, AstArgument* cur) { visit(*cur, out); });
        out << ")";
    }

    void visitNegation(const AstNegation& neg, std::ostream& out) override {
        out << "!";
        visit(*neg.getAtom(), out);
    }

    void visitConstraint(const AstConstraint& cnstr, std::ostream& out) override {
        visit(*cnstr.getLHS(), out);
        out << toBinaryConstraintSymbol(cnstr.getOperator());
        visit(*cnstr.getRHS(), out);
    }

    void visitStringConstant(const AstStringConstant& str, std::ostream& out) override {
        // we dump the index to the output, not the string itself
        out << str.getIndex();
    }

    void visitNumberConstant(const AstNumberConstant& num, std::ostream& out) override {
        out << num;
    }

    void visitTypeCast(const AstTypeCast& cast, std::ostream& out) override {
        visit(*cast.getValue(), out);
    }

    void visitFunctor(const AstFunctor& fun, std::ostream& out) override {
        // create a new variable
        std::string var = "aux_var_" + toString(varCounter++);

        // write variable
        out << var;

        // assign variable in extra clause
        std::stringstream binding;
        binding << var << "=";

        // process unary operators
        if (dynamic_cast<const AstUnaryFunctor*>(&fun)) {
            // binary functors are not supported
            throw UnsupportedConstructException("Unsupported function: " + toString(fun));
        } else if (const AstBinaryFunctor* binary = dynamic_cast<const AstBinaryFunctor*>(&fun)) {
            visit(*binary->getLHS(), binding);
            binding << getSymbolForBinaryOp(binary->getFunction());
            visit(*binary->getRHS(), binding);
        } else {
            assert(false && "Unsupported functor!");
        }
        extra_literals.push_back(binding.str());
    }

    void visitVariable(const AstVariable& var, std::ostream& out) override {
        out << var;
    }

    void visitUnnamedVariable(const AstUnnamedVariable& var, std::ostream& out) override {
        out << "_";
    }

    void visitRelationIdentifier(const AstRelationIdentifier& id, std::ostream& out) {
        out << join(id.getNames(), "_");
    }

    void visitNode(const AstNode& node, std::ostream& /*unused*/) override {
        throw UnsupportedConstructException(
                "Unable to convert the following language construct into bddbddb format: " + toString(node));
    }
};
}  // namespace detail

void toBddbddb(std::ostream& out, const AstTranslationUnit& translationUnit) {
    // simply run the converter
    detail::BddbddbConverter().convert(out, *translationUnit.getProgram());
}

}  // end of namespace souffle
