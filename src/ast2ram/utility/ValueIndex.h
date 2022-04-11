/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ValueIndex.h
 *
 * Defines the ValueIndex class, which indexes the location of variables
 * and record references within a loop nest during rule conversion.
 *
 ***********************************************************************/

#pragma once

#include "souffle/utility/ContainerUtil.h"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace souffle::ast {
class Argument;
class BranchInit;
class RecordInit;
class Variable;
}  // namespace souffle::ast

namespace souffle::ast2ram {

struct Location;

class ValueIndex {
public:
    ValueIndex();
    ~ValueIndex();

    // -- variables --
    const std::map<std::string, std::set<Location>>& getVariableReferences() const {
        return varReferencePoints;
    }
    const std::set<Location>& getVariableReferences(std::string var) const;

    void addVarReference(std::string varName, const Location& l);
    void addVarReference(std::string varName, std::size_t ident, std::size_t pos);
    bool isDefined(const std::string& varName) const;
    const Location& getDefinitionPoint(const std::string& varName) const;

    // -- records --
    void setRecordDefinition(const ast::RecordInit& init, std::size_t ident, std::size_t pos);
    const Location& getDefinitionPoint(const ast::RecordInit& init) const;

    // -- adts --
    void setAdtDefinition(const ast::BranchInit& adt, std::size_t ident, std::size_t pos);
    const Location& getDefinitionPoint(const ast::BranchInit& adt) const;

    // -- generators (aggregates & some functors) --
    void setGeneratorLoc(const ast::Argument& arg, const Location& loc);
    const Location& getGeneratorLoc(const ast::Argument& arg) const;
    bool isGenerator(const std::size_t level) const;

    // -- others --
    bool isSomethingDefinedOn(std::size_t level) const;
    void print(std::ostream& out) const;

private:
    // Map from variable name to use-points
    std::map<std::string, std::set<Location>> varReferencePoints;

    // Map from record inits to definition point (i.e. bounding point)
    std::map<const ast::RecordInit*, Location> recordDefinitionPoints;

    // Map from ADTs to definition point
    std::map<const ast::BranchInit*, Location> adtDefinitionPoints;

    // Map from generative arguments to definition point
    std::map<const ast::Argument*, Location> generatorDefinitionPoints;
};

}  // namespace souffle::ast2ram
