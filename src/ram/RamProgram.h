/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamProgram.h
 *
 * Defines a Program of a relational algebra query
 *
 ***********************************************************************/

#pragma once

#include "ram/RamNode.h"
#include "ram/RamRelation.h"
#include "ram/RamStatement.h"
#include "utility/ContainerUtil.h"
#include <cassert>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 * @class RamProgram
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
class RamProgram : public RamNode {
private:
    RamProgram() = default;

public:
    RamProgram(std::vector<std::unique_ptr<RamRelation>> rels, std::unique_ptr<RamStatement> main,
            std::map<std::string, std::unique_ptr<RamStatement>> subs)
            : relations(std::move(rels)), main(std::move(main)), subroutines(std::move(subs)) {
        assert(this->main != nullptr && "Main program is a null-pointer");
        for (const auto& rel : relations) {
            assert(rel != nullptr && "Relation is a null-pointer");
        }
        for (const auto& sub : subroutines) {
            assert(sub.second != nullptr && "Subroutine is a null-pointer");
        }
    }

    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> children;
        children = main->getChildNodes();
        for (auto& rel : relations) {
            children.push_back(rel.get());
        }
        for (auto& sub : subroutines) {
            children.push_back(sub.second.get());
        }
        return children;
    }

    /** @brief Get main program */
    RamStatement& getMain() const {
        return *main;
    }

    /** @brief Get all relations of RAM program  */
    std::vector<RamRelation*> getRelations() const {
        return toPtrVector(relations);
    }

    /** @brief Get all subroutines of a RAM program */
    const std::map<std::string, RamStatement*> getSubroutines() const {
        std::map<std::string, RamStatement*> subroutineRefs;
        for (auto& sub : subroutines) {
            subroutineRefs.insert({sub.first, sub.second.get()});
        }
        return subroutineRefs;
    }

    /** @brief Get a specific subroutine */
    const RamStatement& getSubroutine(const std::string& name) const {
        return *subroutines.at(name);
    }

    RamProgram* clone() const override {
        auto* res = new RamProgram();
        res->main = souffle::clone(main);
        for (auto& rel : relations) {
            res->relations.push_back(souffle::clone(rel));
        }
        for (auto& sub : subroutines) {
            res->subroutines[sub.first] = souffle::clone(sub.second);
        }
        std::map<const RamRelation*, const RamRelation*> refMap;
        res->apply(makeLambdaRamMapper([&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            // rewire relation references to newly cloned relations
            if (const RamRelationReference* relRef = dynamic_cast<RamRelationReference*>(node.get())) {
                const RamRelation* rel = refMap[relRef->get()];
                assert(rel != nullptr && "dangling RAM relation reference");
                return std::make_unique<RamRelationReference>(rel);
            } else {
                return node;
            }
        }));
        return res;
    }

    void apply(const RamNodeMapper& map) override {
        main = map(std::move(main));
        for (auto& rel : relations) {
            rel = map(std::move(rel));
        }
        for (auto& sub : subroutines) {
            sub.second = map(std::move(sub.second));
        }
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

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamProgram&>(node);

        return equal_targets(relations, other.relations) && equal_ptr(main, other.main) &&
               equal_targets(subroutines, other.subroutines);
    }

protected:
    /** Relations of RAM program */
    std::vector<std::unique_ptr<RamRelation>> relations;

    /** Main program */
    std::unique_ptr<RamStatement> main;

    /** Subroutines for provenance system */
    std::map<std::string, std::unique_ptr<RamStatement>> subroutines;
};

}  // end of namespace souffle
