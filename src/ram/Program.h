/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Program.h
 *
 * Defines a Program of a relational algebra query
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include "ram/Relation.h"
#include "ram/Statement.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class Program
 * @brief RAM program relation declaration and functions
 *
 * A typical example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * PROGRAM
 *   DECLARATION
 *     A(x:i:number)
 *   END DECLARATION
 *   BEGIN MAIN
 *     ...
 *   END MAIN
 * END PROGRAM
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class Program : public Node {
private:
    Program() : Node(NK_Program) {}

public:
    Program(VecOwn<Relation> rels, Own<Statement> main, std::map<std::string, Own<Statement>> subs)
            : Node(NK_Program), relations(std::move(rels)), main(std::move(main)),
              subroutines(std::move(subs)) {
        assert(this->main != nullptr && "Main program is a null-pointer");
        assert(allValidPtrs(relations));
        assert(allValidPtrs(makeTransformRange(subroutines, [](auto&& kv) { return kv.second.get(); })));
    }

    /** @brief Get main program */
    Statement& getMain() const {
        return *main;
    }

    /** @brief Get all relations of RAM program  */
    std::vector<Relation*> getRelations() const {
        return toPtrVector(relations);
    }

    /** @brief Get all subroutines of a RAM program */
    const std::map<std::string, Statement*> getSubroutines() const {
        std::map<std::string, Statement*> subroutineRefs;
        for (auto& sub : subroutines) {
            subroutineRefs.insert({sub.first, sub.second.get()});
        }
        return subroutineRefs;
    }

    /** @brief Get a specific subroutine */
    const Statement& getSubroutine(const std::string& name) const {
        return *subroutines.at(name);
    }

    Program* cloning() const override {
        auto* res = new Program();
        res->main = clone(main);
        for (auto& rel : relations) {
            res->relations.push_back(clone(rel));
        }
        for (auto& sub : subroutines) {
            res->subroutines[sub.first] = clone(sub.second);
        }
        return res;
    }

    void apply(const NodeMapper& map) override {
        main = map(std::move(main));
        for (auto& rel : relations) {
            rel = map(std::move(rel));
        }
        for (auto& sub : subroutines) {
            sub.second = map(std::move(sub.second));
        }
    }

    static bool classof(const Node* n) {
        return n->getKind() == NK_Program;
    }

protected:
    void print(std::ostream& out) const override {
        out << "PROGRAM" << std::endl;
        out << " DECLARATION" << std::endl;
        for (const auto& rel : relations) {
            out << "  " << *rel << std::endl;
        }
        out << " END DECLARATION" << std::endl;
        for (const auto& sub : subroutines) {
            out << " SUBROUTINE " << sub.first << std::endl;
            sub.second->print(out, 2);
            out << " END SUBROUTINE" << std::endl;
        }
        out << " BEGIN MAIN" << std::endl;
        main->print(out, 2);
        out << " END MAIN" << std::endl;
        out << "END PROGRAM" << std::endl;
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<Program>(node);
        return equal_targets(relations, other.relations) && equal_ptr(main, other.main) &&
               equal_targets(subroutines, other.subroutines);
    }

    NodeVec getChildren() const override {
        auto children = main->getChildren();
        for (auto& rel : relations) {
            children.push_back(rel.get());
        }
        for (auto& sub : subroutines) {
            children.push_back(sub.second.get());
        }
        return children;
    }

    /** Relations of RAM program */
    VecOwn<Relation> relations;

    /** Main program */
    Own<Statement> main;

    /** Subroutines for provenance system */
    std::map<std::string, Own<Statement>> subroutines;
};

}  // namespace souffle::ram
