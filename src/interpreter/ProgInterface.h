/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ProgInterface.h
 *
 * Defines classes that implement the SouffleInterface abstract class
 *
 ***********************************************************************/

#pragma once

#include "interpreter/Engine.h"
#include "interpreter/Index.h"
#include "interpreter/Relation.h"
#include "ram/IO.h"
#include "ram/Node.h"
#include "ram/Program.h"
#include "ram/Relation.h"
#include "ram/TranslationUnit.h"
#include "ram/utility/Visitor.h"
#include "souffle/RamTypes.h"
#include "souffle/SouffleInterface.h"
#include "souffle/SymbolTable.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <memory>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

namespace souffle::interpreter {

/**
 * Wrapper class for interpreter relations
 */
class RelInterface : public souffle::Relation {
public:
    RelInterface(RelationWrapper& r, SymbolTable& s, std::string n, std::vector<std::string> t,
            std::vector<std::string> an, std::size_t i)
            : relation(r), symTable(s), name(std::move(n)), types(std::move(t)), attrNames(std::move(an)),
              id(i) {}
    ~RelInterface() override = default;

    /** Insert tuple */
    void insert(const tuple& t) override {
        relation.insert(t.data);
    }

    /** Check whether tuple exists */
    bool contains(const tuple& t) const override {
        return relation.contains(t.data);
    }

    /** Iterator to first tuple */
    iterator begin() const override {
        return RelInterface::iterator(mk<RelInterface::iterator_base>(id, this, relation.begin()));
    }

    /** Iterator to last tuple */
    iterator end() const override {
        return RelInterface::iterator(mk<RelInterface::iterator_base>(id, this, relation.end()));
    }

    /** Get name */
    std::string getName() const override {
        return name;
    }

    /** Get arity */
    arity_type getArity() const override {
        return relation.getArity();
    }

    /** Get arity */
    arity_type getAuxiliaryArity() const override {
        return relation.getAuxiliaryArity();
    }

    /** Get symbol table */
    SymbolTable& getSymbolTable() const override {
        return symTable;
    }

    /** Get attribute type */
    const char* getAttrType(std::size_t idx) const override {
        assert(idx < getArity() && "exceeded tuple size");
        return types[idx].c_str();
    }

    /** Get attribute name */
    const char* getAttrName(std::size_t idx) const override {
        assert(idx < getArity() && "exceeded tuple size");
        return attrNames[idx].c_str();
    }

    /** Get number of tuples in relation */
    std::size_t size() const override {
        return relation.size();
    }

    /** Eliminate all the tuples in relation*/
    void purge() override {
        relation.purge();
    }

protected:
    /**
     * Iterator wrapper class
     */
    class iterator_base : public souffle::Relation::iterator_base {
    public:
        iterator_base(std::size_t arg_id, const RelInterface* r, RelationWrapper::Iterator i)
                : Relation::iterator_base(arg_id), ramRelationInterface(r), it(i), tup(r) {}
        ~iterator_base() override = default;

        /** Increment iterator */
        void operator++() override {
            ++it;
        }

        /** Get current tuple */
        tuple& operator*() override {
            // set tuple stream to first element
            tup.rewind();

            // construct the tuple to return
            for (std::size_t i = 0; i < ramRelationInterface->getArity(); i++) {
                switch (*(ramRelationInterface->getAttrType(i))) {
                    case 's': {
                        std::string s = ramRelationInterface->getSymbolTable().decode((*it)[i]);
                        tup << s;
                        break;
                    }
                    case 'f': {
                        tup << ramBitCast<RamFloat>((*it)[i]);
                        break;
                    }
                    case 'u': {
                        tup << ramBitCast<RamUnsigned>((*it)[i]);
                        break;
                    }
                    default: {
                        tup << (*it)[i];
                        break;
                    }
                }
            }
            tup.rewind();
            return tup;
        }

        /** Clone iterator */
        iterator_base* clone() const override {
            return new RelInterface::iterator_base(getId(), ramRelationInterface, it);
        }

    protected:
        /** Check equivalence */
        bool equal(const souffle::Relation::iterator_base& o) const override {
            auto& iter = asAssert<RelInterface::iterator_base>(o);
            return ramRelationInterface == iter.ramRelationInterface && it == iter.it;
        }

    private:
        const RelInterface* ramRelationInterface;
        RelationWrapper::Iterator it;
        tuple tup;
    };

private:
    /** Wrapped interpreter relation */
    RelationWrapper& relation;

    /** Symbol table */
    SymbolTable& symTable;

    /** Name of relation */
    std::string name;

    /** Attribute type */
    std::vector<std::string> types;

    /** Attribute Names */
    std::vector<std::string> attrNames;

    /** Unique id for wrapper */
    std::size_t id;
};

/**
 * Implementation of SouffleProgram interface for an interpreter instance
 */
class ProgInterface : public SouffleProgram {
public:
    explicit ProgInterface(Engine& interp)
            : prog(interp.getTranslationUnit().getProgram()), exec(interp), symTable(interp.getSymbolTable()),
              recordTable(interp.getRecordTable()) {
        std::size_t id = 0;

        // Retrieve AST Relations and store them in a map
        std::map<std::string, const ram::Relation*> map;
        visit(prog, [&](const ram::Relation& rel) { map[rel.getName()] = &rel; });

        // Build wrapper relations for Souffle's interface
        for (auto& relHandler : exec.getRelationMap()) {
            if (relHandler == nullptr) {
                // Skip droped relation.
                continue;
            }
            auto& interpreterRel = **relHandler;
            auto& name = interpreterRel.getName();
            assert(map[name]);
            const ram::Relation& rel = *map[name];

            // construct types and names vectors
            std::vector<std::string> types = rel.getAttributeTypes();
            std::vector<std::string> attrNames = rel.getAttributeNames();

            auto* interface = new RelInterface(interpreterRel, symTable, rel.getName(), types, attrNames, id);
            interfaces.push_back(interface);
            bool input = false;
            bool output = false;
            visit(prog, [&](const ram::IO& io) {
                if (map[io.getRelation()] == &rel) {
                    const std::string& op = io.get("operation");
                    if (op == "input") {
                        input = true;
                    } else if (op == "output" || op == "printsize") {
                        output = true;
                    } else {
                        assert("wrong i/o operation");
                    }
                }
            });
            addRelation(rel.getName(), *interface, input, output);
            id++;
        }
    }
    ~ProgInterface() override {
        for (auto* interface : interfaces) {
            delete interface;
        }
    }

    /** Run program instance: not implemented */
    void run() override {}

    /** Load data, run program instance, store data: not implemented */
    void runAll(std::string, std::string, bool, bool) override {}

    /** Load input data: not implemented */
    void loadAll(std::string) override {}

    /** Print output data: not implemented */
    void printAll(std::string) override {}

    /** Dump inputs: not implemented */
    void dumpInputs() override {}

    /** Dump outputs: not implemented */
    void dumpOutputs() override {}

    /** Run subroutine */
    void executeSubroutine(
            std::string name, const std::vector<RamDomain>& args, std::vector<RamDomain>& ret) override {
        exec.executeSubroutine(name, args, ret);
    }

    /** Get symbol table */
    SymbolTable& getSymbolTable() override {
        return symTable;
    }

    RecordTable& getRecordTable() override {
        return recordTable;
    }

private:
    const ram::Program& prog;
    Engine& exec;
    SymbolTable& symTable;
    RecordTable& recordTable;
    std::vector<RelInterface*> interfaces;
};

}  // namespace souffle::interpreter
