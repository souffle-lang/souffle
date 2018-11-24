/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2016, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#pragma once

#include "../ProfileDatabase.h"
#include "../ProfileEvent.h"
#include "Iteration.h"
#include "ProgramRun.h"
#include "Relation.h"
#include "Rule.h"
#include "StringUtils.h"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>

namespace souffle {
namespace profile {

namespace {
template <typename T>
class DSNVisitor : public Visitor {
public:
    DSNVisitor(T& base) : base(base) {}
    void visit(TextEntry& text) override {
        if (text.getKey() == "source-locator") {
            base.setLocator(text.getText());
        }
    }
    void visit(DurationEntry& duration) override {
        if (duration.getKey() == "runtime") {
            base.setStarttime(duration.getStart());
            base.setEndtime(duration.getEnd());
        }
    }
    void visit(SizeEntry& size) override {
        if (size.getKey() == "num-tuples") {
            base.setNumTuples(size.getSize());
        }
    }
    void visit(DirectoryEntry& ruleEntry) override {}

protected:
    T& base;
};

/**
 * Visit ProfileDB atom frequencies.
 * atomrule : {atom: {num-tuples: num}}
 */
class AtomFrequenciesVisitor : public Visitor {
public:
    AtomFrequenciesVisitor(Rule& rule) : rule(rule) {}
    void visit(DirectoryEntry& directory) override {
        const std::string& clause = directory.getKey();

        for (auto& key : directory.getKeys()) {
            auto* level = dynamic_cast<SizeEntry*>(directory.readDirectoryEntry(key)->readEntry("level"));
            auto* frequency =
                    dynamic_cast<SizeEntry*>(directory.readDirectoryEntry(key)->readEntry("num-tuples"));
            // Handle older logs
            size_t intFreq = frequency == nullptr ? 0 : frequency->getSize();
            size_t intLevel = level == nullptr ? 0 : level->getSize();
            rule.addAtomFrequency(clause, key, intLevel, intFreq);
        }
    }

private:
    Rule& rule;
};

/**
 * Visit ProfileDB recursive rule.
 * ruleversion: {DSN}
 */
class RecursiveRuleVisitor : public DSNVisitor<Rule> {
public:
    RecursiveRuleVisitor(Rule& rule) : DSNVisitor(rule) {}
    void visit(DirectoryEntry& directory) override {
        if (directory.getKey() == "atom-frequency") {
            AtomFrequenciesVisitor atomFrequenciesVisitor(base);
            for (auto& key : directory.getKeys()) {
                directory.readDirectoryEntry(key)->accept(atomFrequenciesVisitor);
            }
        }
    }
};

/**
 * Visit ProfileDB non-recursive rules.
 * rule: {versionNum : {DSN}, versionNum+1: {DSN}}
 */
class RecursiveRulesVisitor : public Visitor {
public:
    RecursiveRulesVisitor(Iteration& iteration, Relation& relation)
            : iteration(iteration), relation(relation) {}
    void visit(DirectoryEntry& ruleEntry) override {
        for (const auto& key : ruleEntry.getKeys()) {
            auto& versions = *ruleEntry.readDirectoryEntry(key);
            auto rule = std::make_shared<Rule>(
                    ruleEntry.getKey(), std::stoi(key), relation.createRecID(ruleEntry.getKey()));
            RecursiveRuleVisitor visitor(*rule);
            for (const auto& versionKey : versions.getKeys()) {
                versions.readEntry(versionKey)->accept(visitor);
            }
            // To match map keys defined in Iteration::addRule()
            std::string ruleKey = key + rule->getLocator() + key;
            iteration.addRule(ruleKey, rule);
        }
    }

protected:
    Iteration& iteration;
    Relation& relation;
};

/**
 * Visit ProfileDB non-recursive rule.
 * rule: {DSN}
 */
class NonRecursiveRuleVisitor : public DSNVisitor<Rule> {
public:
    NonRecursiveRuleVisitor(Rule& rule) : DSNVisitor(rule) {}
    void visit(DirectoryEntry& directory) override {
        if (directory.getKey() == "atom-frequency") {
            AtomFrequenciesVisitor atomFrequenciesVisitor(base);
            for (auto& key : directory.getKeys()) {
                directory.readDirectoryEntry(key)->accept(atomFrequenciesVisitor);
            }
        }
    }
};

/**
 * Visit ProfileDB non-recursive rules.
 * non-recursive-rule: {rule1: {DSN}, ...}
 */
class NonRecursiveRulesVisitor : public Visitor {
public:
    NonRecursiveRulesVisitor(Relation& relation) : relation(relation) {}
    void visit(DirectoryEntry& ruleEntry) override {
        auto rule = std::make_shared<Rule>(ruleEntry.getKey(), relation.createID());
        NonRecursiveRuleVisitor visitor(*rule);
        for (const auto& key : ruleEntry.getKeys()) {
            ruleEntry.readEntry(key)->accept(visitor);
        }
        relation.addRule(rule);
    }

protected:
    Relation& relation;
};

/**
 * Visit a ProfileDB relation iteration.
 * iterationNumber: {DSN, recursive-rule: {}}
 */
class IterationVisitor : public DSNVisitor<Iteration> {
public:
    IterationVisitor(Iteration& iteration, Relation& relation) : DSNVisitor(iteration), relation(relation) {}
    void visit(DurationEntry& duration) override {
        if (duration.getKey() == "copytime") {
            auto copytime = (duration.getEnd() - duration.getStart());
            base.setCopytime(copytime);
        }
        DSNVisitor::visit(duration);
    }
    void visit(DirectoryEntry& directory) override {
        if (directory.getKey() == "recursive-rule") {
            RecursiveRulesVisitor rulesVisitor(base, relation);
            for (const auto& key : directory.getKeys()) {
                directory.readEntry(key)->accept(rulesVisitor);
            }
        }
        if (directory.getKey() == "maxRSS") {
            auto* preMaxRSS = dynamic_cast<SizeEntry*>(directory.readEntry("pre"));
            auto* postMaxRSS = dynamic_cast<SizeEntry*>(directory.readEntry("post"));
            relation.setPreMaxRSS(preMaxRSS->getSize());
            relation.setPostMaxRSS(postMaxRSS->getSize());
        }
    }

protected:
    Relation& relation;
};

/**
 * Visit ProfileDB iterations.
 * iteration: {num: {}, num2: {}, ...}
 */
class IterationsVisitor : public Visitor {
public:
    IterationsVisitor(Relation& relation) : relation(relation) {}
    void visit(DirectoryEntry& ruleEntry) override {
        auto iteration = std::make_shared<Iteration>();
        relation.addIteration(iteration);
        IterationVisitor visitor(*iteration, relation);
        for (const auto& key : ruleEntry.getKeys()) {
            ruleEntry.readEntry(key)->accept(visitor);
        }
    }

protected:
    Relation& relation;
};

/**
 * Visit ProfileDB relations.
 * relname: {DSN, non-recursive-rule: {}, iteration: {...}}
 */
class RelationVisitor : public DSNVisitor<Relation> {
public:
    RelationVisitor(Relation& relation) : DSNVisitor(relation) {}
    void visit(DurationEntry& duration) override {
        if (duration.getKey() == "loadtime") {
            auto loadtime = (duration.getEnd() - duration.getStart());
            base.setLoadtime(loadtime);
        } else if (duration.getKey() == "savetime") {
            auto savetime = (duration.getEnd() - duration.getStart());
            base.setSavetime(savetime);
        }
        DSNVisitor::visit(duration);
    }
    void visit(DirectoryEntry& directory) override {
        if (directory.getKey() == "iteration") {
            IterationsVisitor iterationsVisitor(base);
            for (const auto& key : directory.getKeys()) {
                directory.readEntry(key)->accept(iterationsVisitor);
            }
        } else if (directory.getKey() == "non-recursive-rule") {
            NonRecursiveRulesVisitor rulesVisitor(base);
            for (const auto& key : directory.getKeys()) {
                directory.readEntry(key)->accept(rulesVisitor);
            }
        } else if (directory.getKey() == "maxRSS") {
            auto* preMaxRSS = dynamic_cast<SizeEntry*>(directory.readEntry("pre"));
            auto* postMaxRSS = dynamic_cast<SizeEntry*>(directory.readEntry("post"));
            base.setPreMaxRSS(preMaxRSS->getSize());
            base.setPostMaxRSS(postMaxRSS->getSize());
        }
    }
    void visit(SizeEntry& size) override {
        if (size.getKey() == "reads") {
            base.addReads(size.getSize());
        } else {
            DSNVisitor::visit(size);
        }
    }
};
}  // namespace

/*
 * Input reader and processor for log files
 */
class Reader {
private:
    std::string file_loc;
    std::streampos gpos;
    const ProfileDatabase& db = ProfileEventSingleton::instance().getDB();
    bool loaded = false;
    bool online{true};

    std::unordered_map<std::string, std::shared_ptr<Relation>> relationMap{};
    int rel_id{0};

public:
    std::shared_ptr<ProgramRun> run;

    Reader(std::string filename, std::shared_ptr<ProgramRun> run)
            : file_loc(std::move(filename)), run(std::move(run)) {
        try {
            ProfileEventSingleton::instance().setDBFromFile(file_loc);
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            exit(1);
        }
    }

    Reader(std::shared_ptr<ProgramRun> run) : run(std::move(run)) {}
    /**
     * Read the contents from file into the class
     */
    void processFile() {
        rel_id = 0;
        relationMap.clear();
        auto programDuration = dynamic_cast<DurationEntry*>(db.lookupEntry({"program", "runtime"}));
        if (programDuration == nullptr) {
            auto startTimeEntry = dynamic_cast<TimeEntry*>(db.lookupEntry({"program", "starttime"}));
            if (startTimeEntry != nullptr) {
                run->setStarttime(startTimeEntry->getTime());
                run->setEndtime(std::chrono::duration_cast<microseconds>(now().time_since_epoch()));
            }
        } else {
            run->setStarttime(programDuration->getStart());
            run->setEndtime(programDuration->getEnd());
            online = false;
        }

        auto relations = dynamic_cast<DirectoryEntry*>(db.lookupEntry({"program", "relation"}));
        if (relations == nullptr) {
            // Souffle hasn't generated any profiling information yet.
            return;
        }
        for (const auto& cur : relations->getKeys()) {
            auto relation = dynamic_cast<DirectoryEntry*>(db.lookupEntry({"program", "relation", cur}));
            if (relation != nullptr) {
                addRelation(*relation);
            }
        }
        for (const auto& relation : relationMap) {
            for (const auto& rule : relation.second->getRuleMap()) {
                for (const auto& atom : rule.second->getAtoms()) {
                    std::string relationName = extractRelationNameFromAtom(atom);
                    relationMap[relationName]->addReads(atom.frequency);
                }
            }
            for (const auto& iteration : relation.second->getIterations()) {
                for (const auto& rule : iteration->getRules()) {
                    for (const auto& atom : rule.second->getAtoms()) {
                        std::string relationName = extractRelationNameFromAtom(atom);
                        if (relationName.substr(0, 6) == "@delta") {
                            relationName = relationName.substr(7);
                        }
                        assert(relationMap.count(relationName) > 0 || "Relation name for atom not found");
                        relationMap[relationName]->addReads(atom.frequency);
                    }
                }
            }
        }
        run->setRelationMap(this->relationMap);
        loaded = true;
    }

    void save(std::string f_name);

    inline bool isLive() {
        return online;
    }

    void addRelation(const DirectoryEntry& relation) {
        const std::string& name = cleanRelationName(relation.getKey());

        relationMap.emplace(name, std::make_shared<Relation>(name, createId()));
        auto& rel = *relationMap[name];
        RelationVisitor relationVisitor(rel);

        for (const auto& key : relation.getKeys()) {
            relation.readEntry(key)->accept(relationVisitor);
        }
    }

    inline bool isLoaded() {
        return loaded;
    }

    std::string RelationcreateId() {
        return "R" + std::to_string(++rel_id);
    }

    std::string createId() {
        return "R" + std::to_string(++rel_id);
    }

protected:
    std::string cleanRelationName(const std::string& relationName) {
        std::string cleanName = relationName;
        for (auto& cur : cleanName) {
            if (cur == '-') {
                cur = '.';
            }
        }
        return cleanName;
    }
    std::string extractRelationNameFromAtom(const Atom& atom) {
        return cleanRelationName(atom.identifier.substr(0, atom.identifier.find('(')));
    }
};

}  // namespace profile
}  // namespace souffle
