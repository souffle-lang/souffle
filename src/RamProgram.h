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

#include "RamStatement.h"

namespace souffle {

class RamProgram : public RamNode {
private:
    std::unique_ptr<RamStatement> main;
    std::map<std::string, std::unique_ptr<RamStatement>> subroutines;

public:
    RamProgram(std::unique_ptr<RamStatement> main) : RamNode(RN_Program), main(std::move(main)) {}

    /** Obtain child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> children;
        children.push_back(main.get());

        // add subroutines
        for (auto& s : subroutines) {
            children.push_back(s.second.get());
        }

        return children;
    }

    /** Print */
    void print(std::ostream& out) const override {
        out << "PROGRAM " << std::endl;
        main->print(out);
        out << "END PROGRAM" << std::endl;
        for (auto it = subroutines.begin(); it != subroutines.end(); it++) {
            out << std::endl << "SUBROUTINE " << it->first << std::endl;
            it->second->print(out);
            out << "END SUBROUTINE" << std::endl;
        }
    }

    /** Get main program */
    RamStatement* getMain() const {
        ASSERT(main);
        return main.get();
    }

    /** Add subroutine */
    void addSubroutine(std::string name, std::unique_ptr<RamStatement> subroutine) {
        subroutines.insert(std::make_pair(name, std::move(subroutine)));
    }

    /** Get subroutines */
    const std::map<std::string, RamStatement*> getSubroutines() const {
        std::map<std::string, RamStatement*> subroutineRefs;
        for (auto& s : subroutines) {
            subroutineRefs.insert({s.first, s.second.get()});
        }
        return subroutineRefs;
    }

    /** Get subroutine */
    const RamStatement& getSubroutine(std::string name) const {
        return *subroutines.at(name);
    }

    /** Create clone */
    RamProgram* clone() const override {
        RamProgram* res = new RamProgram(std::unique_ptr<RamStatement>(main->clone()));
        for (auto& cur : subroutines) {
            res->addSubroutine(cur.first, std::unique_ptr<RamStatement>(cur.second->clone()));
        }
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        main = map(std::move(main));
        for (auto& cur : subroutines) {
            subroutines[cur.first] = map(std::move(cur.second));
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(dynamic_cast<const RamProgram*>(&node));
        const RamProgram& other = static_cast<const RamProgram&>(node);
        bool areSubroutinesEqual = true;
        for (auto& cur : subroutines) {
            if (other.subroutines.count(cur.first) == 0) {
                areSubroutinesEqual = false;
                break;
            } else {
                if (other.getSubroutine(cur.first) != getSubroutine(cur.first)) {
                    areSubroutinesEqual = false;
                    break;
                }
            }
        }
        return getMain() == other.getMain() && areSubroutinesEqual;
    }
};

}  // end of namespace souffle
