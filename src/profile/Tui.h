/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2016, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#pragma once

#include "../ProfileEvent.h"
#include "HtmlGenerator.h"
#include "OutputProcessor.h"
#include "Reader.h"
#include "Table.h"
#include "UserInputReader.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

namespace souffle {
namespace profile {

/*
 * Text User interface for SouffleProf
 * OutputProcessor creates a ProgramRun object
 * ProgramRun -> Reader.h ProgramRun stores all the data
 * OutputProcessor grabs the data and makes tables
 * Tui displays the data
 */
class Tui {
private:
    OutputProcessor out;
    bool loaded;
    std::string f_name;
    bool alive = false;
    std::thread updater;
    int sortColumn = 0;
    int precision = 3;
    Table relationTable;
    Table ruleTable;
    std::shared_ptr<Reader> reader;
    InputReader linereader;
    /// Limit results shown. Default value chosen to approximate unlimited
    size_t resultLimit = 20000;

    struct Usage {
        std::chrono::microseconds time;
        uint64_t maxRSS;
        std::chrono::microseconds systemtime;
        std::chrono::microseconds usertime;
        bool operator<(const Usage& other) const {
            return time < other.time;
        }
    };

public:
    Tui(std::string filename, bool live, bool gui) {
        // Set a friendlier output size if we're being interacted with directly.
        if (live) {
            resultLimit = 20;
        }
        this->f_name = filename;

        const std::shared_ptr<ProgramRun>& run = out.getProgramRun();

        this->reader = std::make_shared<Reader>(filename, run);

        this->alive = false;
        updateDB();
        this->loaded = reader->isLoaded();
    }

    Tui() {
        const std::shared_ptr<ProgramRun>& run = out.getProgramRun();
        this->reader = std::make_shared<Reader>(run);
        this->loaded = true;
        this->alive = true;
        updateDB();
        updater = std::thread([this]() {
            // Update the display every 30s. Check for input every 0.5s
            std::chrono::milliseconds interval(30000);
            auto nextUpdateTime = std::chrono::high_resolution_clock::now();
            do {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                if (nextUpdateTime < std::chrono::high_resolution_clock::now()) {
                    runCommand({});
                    nextUpdateTime = std::chrono::high_resolution_clock::now() + interval;
                }
            } while (reader->isLive() && !linereader.hasReceivedInput());
        });
    }

    ~Tui() {
        if (updater.joinable()) {
            updater.join();
        }
    }

    void runCommand(std::vector<std::string> c) {
        if (linereader.hasReceivedInput() && c.empty()) {
            return;
        }
        if (!loaded) {
            std::cout << "Error: File cannot be loaded\n";
            return;
        }

        if (alive) {
            updateDB();
            // remake tables to get new data
            ruleTable = out.getRulTable();
            relationTable = out.getRelTable();

            setupTabCompletion();
        }

        // If we have not received any input yet in live mode then run top.
        if ((!linereader.hasReceivedInput() && c.empty())) {
            // Move up n lines and overwrite the previous top output.
            std::cout << "\x1b[3D";
            std::cout << "\x1b[27A";
            top();
            std::cout << "\x1b[B> ";
        } else if (c[0].compare("top") == 0) {
            top();
        } else if (c[0].compare("rel") == 0) {
            if (c.size() == 2) {
                relRul(c[1]);
            } else if (c.size() == 1) {
                rel(resultLimit);
            } else {
                std::cout << "Invalid parameters to rel command.\n";
            }
        } else if (c[0].compare("rul") == 0) {
            if (c.size() > 1) {
                if (c.size() == 3 && c[1].compare("id") == 0) {
                    std::printf("%7s%2s%s\n\n", "ID", "", "NAME");
                    id(c[2]);
                } else if (c.size() == 2 && c[1].compare("id") == 0) {
                    id("0");
                } else if (c.size() == 2) {
                    verRul(c[1]);
                } else {
                    std::cout << "Invalid parameters to rul command.\n";
                }
            } else {
                rul(resultLimit);
            }
        } else if (c[0].compare("graph") == 0) {
            if (c.size() == 3 && c[1].find(".") == std::string::npos) {
                iterRel(c[1], c[2]);
            } else if (c.size() == 3 && c[1].at(0) == 'C') {
                iterRul(c[1], c[2]);
            } else if (c.size() == 4 && c[1].compare("ver") == 0 && c[2].at(0) == 'C') {
                verGraph(c[2], c[3]);
            } else {
                std::cout << "Invalid parameters to graph command.\n";
            }
        } else if (c[0].compare("memory") == 0) {
            memoryUsage();
        } else if (c[0].compare("usage") == 0) {
            if (c.size() > 1) {
                if (c[1][0] == 'R') {
                    usageRelation(c[1]);
                } else {
                    usageRule(c[1]);
                }
            } else {
                usage();
            }
        } else if (c[0].compare("help") == 0) {
            help();
        } else if (c[0].compare("limit") == 0) {
            if (c.size() == 1) {
                setResultLimit(20000);
            } else {
                try {
                    setResultLimit(std::stoul(c[1]));
                } catch (...) {
                    std::cout << "Invalid parameters to limit command.\n";
                }
            }
        } else if (c[0].compare("configuration") == 0) {
            configuration();
        } else {
            std::cout << "Unknown command. Use \"help\" for a list of commands.\n";
        }
    }

    void runProf() {
        if (!loaded && !f_name.empty()) {
            std::cout << "Error: File cannot be floaded\n";
            return;
        }
        if (loaded) {
            std::cout << "SouffleProf\n";
            top();
        }

        linereader.setPrompt("\n> ");
        setupTabCompletion();

        while (true) {
            std::string untrimmedInput = linereader.getInput();
            std::string input = Tools::trimWhitespace(untrimmedInput);

            std::cout << std::endl;
            if (input.empty()) {
                std::cout << "Unknown command. Type help for a list of commands.\n";
                continue;
            }

            linereader.addHistory(input.c_str());

            std::vector<std::string> c = Tools::split(input, " ");

            if (c[0] == "q" || c[0] == "quit") {
                quit();
                break;
            } else if (c[0] == "sort") {
                if (c.size() == 2 && std::stoi(c[1]) < 7) {
                    sortColumn = std::stoi(c[1]);
                } else {
                    std::cout << "Invalid column, please select a number between 0 and 6.\n";
                }
            } else {
                runCommand(c);
            }
        }
    }

    std::stringstream& genJsonTop(std::stringstream& ss) {
        const std::shared_ptr<ProgramRun>& run = out.getProgramRun();

        auto beginTime = run->getStarttime();
        auto endTime = run->getEndtime();
        ss << R"_({"top":[)_" << (endTime - beginTime).count() / 1000000.0 << "," << run->getTotalSize()
           << "," << run->getTotalLoadtime().count() / 1000000.0 << ","
           << run->getTotalSavetime().count() / 1000000.0 << "]";
        return ss;
    }

    std::stringstream& genJsonRelations(std::stringstream& ss, const std::string& name, size_t maxRows) {
        const std::shared_ptr<ProgramRun>& run = out.getProgramRun();

        auto comma = [&ss](bool& first, const std::string& delimiter = ", ") {
            if (!first) {
                ss << delimiter;
            } else {
                first = false;
            }
        };

        ss << '"' << name << R"_(":{)_";
        bool firstRow = true;
        auto rows = relationTable.getRows();
        std::stable_sort(rows.begin(), rows.end(), [](std::shared_ptr<Row> left, std::shared_ptr<Row> right) {
            return (*left)[0]->getDoubleVal() > (*right)[0]->getDoubleVal();
        });
        maxRows = std::min(rows.size(), maxRows);

        for (size_t i = 0; i < maxRows; ++i) {
            comma(firstRow, ",\n");

            Row& row = *rows[i];
            ss << '"' << row[6]->toString(0) << R"_(": [)_";
            ss << '"' << Tools::cleanJsonOut(row[5]->toString(0)) << R"_(", )_";
            ss << '"' << Tools::cleanJsonOut(row[6]->toString(0)) << R"_(", )_";
            ss << row[0]->getDoubleVal() << ", ";
            ss << row[1]->getDoubleVal() << ", ";
            ss << row[2]->getDoubleVal() << ", ";
            ss << row[3]->getDoubleVal() << ", ";
            ss << row[4]->getLongVal() << ", ";
            ss << row[12]->getLongVal() << ", ";
            ss << '"' << Tools::cleanJsonOut(row[7]->toString(0)) << R"_(", [)_";

            bool firstCol = true;
            for (auto& _rel_row : ruleTable.getRows()) {
                Row rel_row = *_rel_row;
                if (rel_row[7]->toString(0) == row[5]->toString(0)) {
                    comma(firstCol);
                    ss << '"' << rel_row[6]->toString(0) << '"';
                }
            }
            ss << "], ";
            std::vector<std::shared_ptr<Iteration>> iter =
                    run->getRelation(row[5]->toString(0))->getIterations();
            ss << R"_({"tot_t": [)_";
            firstCol = true;
            for (auto& i : iter) {
                comma(firstCol);
                ss << i->getRuntime().count();
            }
            ss << R"_(], "copy_t": [)_";
            firstCol = true;
            for (auto& i : iter) {
                comma(firstCol);
                ss << i->getCopytime().count();
            }
            ss << R"_(], "tuples": [)_";
            firstCol = true;
            for (auto& i : iter) {
                comma(firstCol);
                ss << i->size();
            }
            ss << "]}]";
        }
        ss << "}";

        return ss;
    }

    std::stringstream& genJsonRules(std::stringstream& ss, const std::string& name, size_t maxRows) {
        const std::shared_ptr<ProgramRun>& run = out.getProgramRun();

        auto comma = [&ss](bool& first, const std::string& delimiter = ", ") {
            if (!first) {
                ss << delimiter;
            } else {
                first = false;
            }
        };

        ss << '"' << name << R"_(":{)_";

        bool firstRow = true;
        auto rows = ruleTable.getRows();
        std::stable_sort(rows.begin(), rows.end(), [](std::shared_ptr<Row> left, std::shared_ptr<Row> right) {
            return (*left)[0]->getDoubleVal() > (*right)[0]->getDoubleVal();
        });
        maxRows = std::min(rows.size(), maxRows);

        for (size_t i = 0; i < maxRows; ++i) {
            Row& row = *rows[i];

            std::vector<std::string> part = Tools::split(row[6]->toString(0), ".");
            std::string strRel = "R" + part[0].substr(1);
            Table versionTable = out.getVersions(strRel, row[6]->toString(0));

            std::string src;
            if (versionTable.rows.size() > 0) {
                if (versionTable.rows[0]->cells[9] != nullptr) {
                    src = (*versionTable.rows[0])[9]->toString(0);
                } else {
                    src = "-";
                }
            } else {
                src = row[10]->toString(-1);
            }
            comma(firstRow);
            ss << "\n ";

            ss << '"' << row[6]->toString(0) << R"_(": [)_";
            ss << '"' << Tools::cleanJsonOut(row[5]->toString(0)) << R"_(", )_";
            ss << '"' << Tools::cleanJsonOut(row[6]->toString(0)) << R"_(", )_";
            ss << row[0]->getDoubleVal() << ", ";
            ss << row[1]->getDoubleVal() << ", ";
            ss << row[2]->getDoubleVal() << ", ";
            ss << row[4]->getLongVal() << ", ";

            ss << '"' << src << R"_(", )_";
            ss << "[";

            bool has_ver = false;
            bool firstCol = true;
            for (auto& _ver_row : versionTable.getRows()) {
                comma(firstCol);
                has_ver = true;
                Row ver_row = *_ver_row;
                ss << '[';
                ss << '"' << Tools::cleanJsonOut(ver_row[5]->toString(0)) << R"_(", )_";
                ss << '"' << Tools::cleanJsonOut(ver_row[6]->toString(0)) << R"_(", )_";
                ss << ver_row[0]->getDoubleVal() << ", ";
                ss << ver_row[1]->getDoubleVal() << ", ";
                ss << ver_row[2]->getDoubleVal() << ", ";
                ss << ver_row[4]->getLongVal() << ", ";
                ss << '"' << src << R"_(", )_";
                ss << ver_row[8]->getLongVal();
                ss << ']';
            }

            ss << "], ";

            if (row[6]->toString(0).at(0) != 'C') {
                ss << "{}, {}]";
            } else {
                ss << R"_({"tot_t": [)_";

                std::vector<uint64_t> iteration_tuples;
                bool firstCol = true;
                for (auto& i : run->getRelation(row[7]->toString(0))->getIterations()) {
                    bool add = false;
                    std::chrono::microseconds totalTime{};
                    uint64_t totalSize = 0L;
                    for (auto& rul : i->getRules()) {
                        if (rul.second->getId() == row[6]->toString(0)) {
                            totalTime += rul.second->getRuntime();

                            totalSize += rul.second->size();
                            add = true;
                        }
                    }
                    if (add) {
                        comma(firstCol);
                        ss << totalTime.count();
                        iteration_tuples.push_back(totalSize);
                    }
                }
                ss << R"_(], "tuples": [)_";
                firstCol = true;
                for (auto& i : iteration_tuples) {
                    comma(firstCol);
                    ss << i;
                }

                ss << "]}, {";

                if (has_ver) {
                    ss << R"_("tot_t": [)_";

                    firstCol = true;
                    for (auto& row : versionTable.rows) {
                        comma(firstCol);
                        ss << (*row)[0]->getDoubleVal();
                    }
                    ss << R"_(], "tuples": [)_";

                    firstCol = true;
                    for (auto& row : versionTable.rows) {
                        comma(firstCol);
                        ss << (*row)[4]->getLongVal();
                    }
                    ss << ']';
                }
                ss << "}]";
            }
        }
        ss << "\n}";
        return ss;
    }

    std::stringstream& genJsonUsage(std::stringstream& ss) {
        const std::shared_ptr<ProgramRun>& run = out.getProgramRun();

        auto comma = [&ss](bool& first, const std::string& delimiter = ", ") {
            if (!first) {
                ss << delimiter;
            } else {
                first = false;
            }
        };

        std::string source_loc = (*relationTable.getRows()[0])[7]->getStringVal();
        std::string source_file_loc = Tools::split(source_loc, " ").at(0);
        std::ifstream source_file(source_file_loc);
        if (!source_file.is_open()) {
            std::cout << "Error opening \"" << source_file_loc << "\", creating GUI without source locator."
                      << std::endl;
        } else {
            std::string str;
            ss << R"_("code": [)_";
            bool firstCol = true;
            while (getline(source_file, str)) {
                comma(firstCol, ",\n");
                ss << '"' << Tools::cleanJsonOut(str) << '"';
            }
            ss << "],\n";
            source_file.close();
        }

        // Add usage statistics
        auto usages = getUsageStats(100);
        auto beginTime = run->getStarttime();

        ss << R"_("usage": [)_";
        bool firstRow = true;
        Usage previousUsage = *usages.begin();
        previousUsage.time = beginTime;
        for (auto usage : usages) {
            comma(firstRow);
            ss << '[';
            ss << (usage.time - beginTime).count() / 1000000.0 << ", ";
            ss << 100.0 * (usage.usertime - previousUsage.usertime) / (usage.time - previousUsage.time)
               << ", ";
            ss << 100.0 * (usage.systemtime - previousUsage.systemtime) / (usage.time - previousUsage.time)
               << ", ";
            ss << usage.maxRSS * 1024 << ", ";
            ss << '"';
            bool firstCol = true;
            for (auto& cur : out.getProgramRun()->getRelationsAtTime(previousUsage.time, usage.time)) {
                comma(firstCol);
                ss << cur->getName();
            }
            ss << '"';
            ss << ']';
            previousUsage = usage;
        }
        ss << ']';
        return ss;
    }

    std::stringstream& genJsonConfiguration(std::stringstream& ss) {
        auto comma = [&ss](bool& first, const std::string& delimiter = ", ") {
            if (!first) {
                ss << delimiter;
            } else {
                first = false;
            }
        };

        // Add configuration key-value pairs
        ss << R"_("configuration": {)_";
        bool firstRow = true;
        for (auto& kvp :
                ProfileEventSingleton::instance().getDB().getStringMap({"program", "configuration"})) {
            comma(firstRow);
            ss << '"' << kvp.first << R"_(": ")_" << Tools::cleanJsonOut(kvp.second) << '"';
        }
        ss << '}';
        return ss;
    }

    std::stringstream& genJsonAtoms(std::stringstream& ss) {
        const std::shared_ptr<ProgramRun>& run = out.getProgramRun();

        auto comma = [&ss](bool& first, const std::string& delimiter = ", ") {
            if (!first) {
                ss << delimiter;
            } else {
                first = false;
            }
        };

        ss << R"_("atoms": {)_";

        bool firstRow = true;
        for (auto& relation : run->getRelationMap()) {
            // Get atoms for non-recursive rules
            for (auto& rule : relation.second->getRuleMap()) {
                comma(firstRow, ", \n");
                ss << '"' << rule.second->getId() << R"_(": [)_";
                bool firstCol = true;
                for (auto& atom : rule.second->getAtoms()) {
                    comma(firstCol);
                    std::string relationName = atom.identifier;
                    relationName = relationName.substr(0, relationName.find('('));
                    auto* relation = out.getProgramRun()->getRelation(relationName);
                    std::string relationSize = relation == nullptr ? "" : std::to_string(relation->size());
                    ss << '[';
                    ss << '"' << Tools::cleanJsonOut(Tools::cleanString(atom.rule)) << R"_(", )_";
                    ss << '"' << Tools::cleanJsonOut(atom.identifier) << R"_(", )_";
                    ss << relationSize << ", ";
                    ss << atom.frequency << ']';
                }
                ss << "]";
            }
            // Get atoms for recursive rules
            for (auto& iteration : relation.second->getIterations()) {
                for (auto& rule : iteration->getRules()) {
                    comma(firstRow, ", \n");
                    ss << '"' << rule.second->getId() << R"_(": [)_";
                    bool firstCol = true;
                    for (auto& atom : rule.second->getAtoms()) {
                        comma(firstCol);
                        std::string relationName = atom.identifier;
                        relationName = relationName.substr(0, relationName.find('('));
                        auto* relation = out.getProgramRun()->getRelation(relationName);
                        std::string relationSize =
                                relation == nullptr ? "" : std::to_string(relation->size());
                        ss << '[';
                        ss << '"' << Tools::cleanJsonOut(Tools::cleanString(atom.rule)) << R"_(", )_";
                        ss << '"' << Tools::cleanJsonOut(atom.identifier) << R"_(", )_";
                        ss << relationSize << ", ";
                        ss << atom.frequency << ']';
                    }
                    ss << "]";
                }
            }
        }

        ss << '}';
        return ss;
    }

    std::string genJson() {
        std::stringstream ss;

        genJsonTop(ss);
        ss << ",\n";
        genJsonRelations(ss, "topRel", 3);
        ss << ",\n";
        genJsonRules(ss, "topRul", 3);
        ss << ",\n";
        genJsonRelations(ss, "rel", relationTable.rows.size());
        ss << ",\n";
        genJsonRules(ss, "rul", ruleTable.rows.size());
        ss << ",\n";
        genJsonUsage(ss);
        ss << ",\n";
        genJsonConfiguration(ss);
        ss << ",\n";
        genJsonAtoms(ss);
        ss << '\n';

        ss << "};\n";

        return ss.str();
    }

    void outputHtml(std::string filename = "profiler_html/") {
        std::cout << "SouffleProf\n";
        std::cout << "Generating HTML files...\n";

        DIR* dir;
        bool exists = false;

        if (filename.find('/') != std::string::npos) {
            std::string path = filename.substr(0, filename.find('/'));
            if ((dir = opendir(path.c_str())) != nullptr) {
                exists = true;
                closedir(dir);
            }
            if (!exists) {
                mode_t nMode = 0733;  // UNIX style permissions
                int nError = 0;
                nError = mkdir(path.c_str(), nMode);
                if (nError != 0) {
                    std::cerr << "directory " << path
                              << " could not be created. Please create it and try again.";
                    exit(2);
                }
            }
        }
        std::string filetype = ".html";
        std::string newFile = filename;

        if (filename.size() <= filetype.size() ||
                !std::equal(filetype.rbegin(), filetype.rend(), filename.rbegin())) {
            int i = 0;
            do {
                ++i;
                newFile = filename + std::to_string(i) + ".html";
            } while (Tools::file_exists(newFile));
        }

        std::ofstream outfile(newFile);

        outfile << HtmlGenerator::getHtml(genJson());

        std::cout << "file output to: " << newFile << std::endl;
    }

    void quit() {
        if (updater.joinable()) {
            updater.join();
        }
    }

    static void help() {
        std::cout << "\nAvailable profiling commands:" << std::endl;
        std::printf("  %-30s%-5s %s\n", "rel", "-", "display relation table.");
        std::printf("  %-30s%-5s %s\n", "rel <relation id>", "-", "display all rules of a given relation.");
        std::printf("  %-30s%-5s %s\n", "rul", "-", "display rule table");
        std::printf("  %-30s%-5s %s\n", "rul <rule id>", "-", "display all version of given rule.");
        std::printf("  %-30s%-5s %s\n", "rul id", "-", "display all rules names and ids.");
        std::printf(
                "  %-30s%-5s %s\n", "rul id <rule id>", "-", "display the rule name for the given rule id.");
        std::printf("  %-30s%-5s %s\n", "graph <relation id> <type>", "-",
                "graph a relation by type: (tot_t/copy_t/tuples).");
        std::printf("  %-30s%-5s %s\n", "graph <rule id> <type>", "-",
                "graph recursive(C) rule by type(tot_t/tuples).");
        std::printf("  %-30s%-5s %s\n", "graph ver <rule id> <type>", "-",
                "graph recursive(C) rule versions by type(tot_t/copy_t/tuples).");
        std::printf("  %-30s%-5s %s\n", "top", "-", "display top-level summary of program run.");
        std::printf("  %-30s%-5s %s\n", "configuration", "-", "display configuration settings for this run.");
        std::printf("  %-30s%-5s %s\n", "usage [relation id|rule id]", "-",
                "display CPU usage graphs for a relation or rule.");
        std::printf("  %-30s%-5s %s\n", "memory", "-", "display memory usage.");
        std::printf("  %-30s%-5s %s\n", "help", "-", "print this.");

        std::cout << "\nInteractive mode only commands:" << std::endl;
        //    if (alive) std::printf("  %-30s%-5s %s\n", "stop", "-",
        //                "stop the current live run.");
        std::printf("  %-30s%-5s %s\n", "limit <row count>", "-", "limit number of results shown.");
        std::printf("  %-30s%-5s %s\n", "sort <col number>", "-", "sort tables by given column number.");
        std::printf("  %-30s%-5s %s\n", "q", "-", "exit program.");
    }

    void usageRelation(std::string id) {
        std::vector<std::vector<std::string>> formattedRelationTable =
                Tools::formatTable(relationTable, precision);
        std::string name = "";
        bool found = false;
        for (auto& row : formattedRelationTable) {
            if (row[5] == id || row[6] == id) {
                name = row[5];
                found = true;
                break;
            }
        }
        if (!found) {
            std::cout << "Relation does not exist.\n";
            return;
        }

        const Relation* rel = out.getProgramRun()->getRelation(name);
        usage(rel->getEndtime(), rel->getStarttime());
    }

    void usageRule(std::string id) {
        std::vector<std::vector<std::string>> formattedRuleTable = Tools::formatTable(ruleTable, precision);
        std::string relName = "";
        std::string srcLocator = "";
        bool found = false;
        for (auto& row : formattedRuleTable) {
            if (row[5] == id || row[6] == id) {
                relName = row[7];
                srcLocator = row[10];
                found = true;
                break;
            }
        }
        if (!found) {
            std::cout << "Rule does not exist.\n";
            return;
        }

        auto* rel = out.getProgramRun()->getRelation(relName);
        if (rel == nullptr) {
            std::cout << "Relation ceased to exist. Odd." << std::endl;
            return;
        }
        if (rel->getRuleMap().count(srcLocator) == 0) {
            std::cout << "Rule ceased to exist. Odd." << std::endl;
            return;
        }

        auto& rul = rel->getRuleMap().at(srcLocator);
        usage(rul->getEndtime(), rul->getStarttime());
    }

    std::set<Usage> getUsageStats(size_t width = size_t(-1)) {
        std::set<Usage> usages;
        DirectoryEntry* usageStats = dynamic_cast<DirectoryEntry*>(
                ProfileEventSingleton::instance().getDB().lookupEntry({"program", "usage", "timepoint"}));
        if (usageStats == nullptr || usageStats->getKeys().size() < 2) {
            return usages;
        }
        std::chrono::microseconds endTime{};
        std::chrono::microseconds startTime{};
        std::chrono::microseconds timeStep{};
        // Translate the string ordered text usage stats to a time ordered binary form.
        std::set<Usage> allUsages;
        for (auto& currentKey : usageStats->getKeys()) {
            Usage currentUsage{};
            uint64_t cur = std::stoul(currentKey);
            currentUsage.time = std::chrono::duration<uint64_t, std::micro>(cur);
            cur = dynamic_cast<SizeEntry*>(
                    usageStats->readDirectoryEntry(currentKey)->readEntry("systemtime"))
                          ->getSize();
            currentUsage.systemtime = std::chrono::duration<uint64_t, std::micro>(cur);
            cur = dynamic_cast<SizeEntry*>(usageStats->readDirectoryEntry(currentKey)->readEntry("usertime"))
                          ->getSize();
            currentUsage.usertime = std::chrono::duration<uint64_t, std::micro>(cur);
            currentUsage.maxRSS =
                    dynamic_cast<SizeEntry*>(usageStats->readDirectoryEntry(currentKey)->readEntry("maxRSS"))
                            ->getSize();

            // Duplicate times are possible
            if (allUsages.find(currentUsage) != allUsages.end()) {
                auto& existing = *allUsages.find(currentUsage);
                currentUsage.systemtime = std::max(existing.systemtime, currentUsage.systemtime);
                currentUsage.usertime = std::max(existing.usertime, currentUsage.usertime);
                currentUsage.maxRSS = std::max(existing.maxRSS, currentUsage.maxRSS);
                allUsages.erase(currentUsage);
            }
            allUsages.insert(currentUsage);
        }

        // cpu times aren't quite recorded in a monotonic way, so skip the invalid ones.
        for (auto it = ++allUsages.begin(); it != allUsages.end(); ++it) {
            auto previous = std::prev(it);
            if (it->usertime < previous->usertime || it->systemtime < previous->systemtime ||
                    it->time == previous->time) {
                it = allUsages.erase(it);
                --it;
            }
        }

        // Extract our overall stats
        startTime = allUsages.begin()->time;
        endTime = allUsages.rbegin()->time;

        // If we don't have enough records, just return what we can
        if (allUsages.size() < width) {
            return allUsages;
        }

        timeStep = (endTime - startTime) / width;

        // Store the timepoints we need for the graph
        for (uint32_t i = 1; i <= width; ++i) {
            auto it = allUsages.upper_bound(Usage{startTime + timeStep * i});
            if (it != allUsages.begin()) {
                --it;
            }
            usages.insert(*it);
        }

        return usages;
    }

    void usage(uint32_t height = 20) {
        usage({}, {}, height);
    }

    void usage(std::chrono::microseconds endTime, std::chrono::microseconds startTime, uint32_t height = 20) {
        uint32_t width = getTermWidth() - 8;

        std::set<Usage> usages = getUsageStats(width);

        if (usages.size() < 2) {
            for (uint8_t i = 0; i < height + 2; ++i) {
                std::cout << std::endl;
            }
            std::cout << "Insufficient data for usage statistics." << std::endl;
            return;
        }

        double maxIntervalUsage = 0;

        // Extract our overall stats
        if (startTime.count() == 0) {
            startTime = usages.begin()->time;
        }
        if (endTime.count() == 0) {
            endTime = usages.rbegin()->time;
        }

        if (usages.size() < width) {
            width = usages.size();
        }

        // Find maximum so we can normalise the graph
        Usage previousUsage{{}, 0, {}, {}};
        for (auto& currentUsage : usages) {
            double usageDiff = (currentUsage.systemtime - previousUsage.systemtime + currentUsage.usertime -
                                previousUsage.usertime)
                                       .count();
            usageDiff /= (currentUsage.time - previousUsage.time).count();
            if (usageDiff > maxIntervalUsage) {
                maxIntervalUsage = usageDiff;
            }

            previousUsage = currentUsage;
        }

        double intervalUsagePercent = 100.0 * maxIntervalUsage;
        std::printf("%11s\n", "cpu total");
        std::printf("%11s\n", Tools::formatTime(usages.rbegin()->usertime).c_str());

        // Add columns to the graph
        char grid[height][width];
        for (uint32_t i = 0; i < height; ++i) {
            for (uint32_t j = 0; j < width; ++j) {
                grid[i][j] = ' ';
            }
        }

        previousUsage = {{}, 0, {}, {}};
        uint32_t col = 0;
        for (const Usage& currentUsage : usages) {
            uint64_t curHeight = 0;
            uint64_t curSystemHeight = 0;
            // Usage may be 0
            if (maxIntervalUsage != 0) {
                curHeight = (currentUsage.systemtime - previousUsage.systemtime + currentUsage.usertime -
                             previousUsage.usertime)
                                    .count();
                curHeight /= (currentUsage.time - previousUsage.time).count();
                curHeight *= height / maxIntervalUsage;

                curSystemHeight = (currentUsage.systemtime - previousUsage.systemtime).count();
                curSystemHeight /= (currentUsage.time - previousUsage.time).count();
                curSystemHeight *= height / maxIntervalUsage;
            }
            for (uint32_t row = 0; row < curHeight; ++row) {
                grid[row][col] = '*';
            }
            for (uint32_t row = curHeight - curSystemHeight; row < curHeight; ++row) {
                grid[row][col] = '+';
            }
            previousUsage = currentUsage;
            ++col;
        }

        // Print array
        for (int32_t row = height - 1; row >= 0; --row) {
            printf("%6d%% ", uint32_t(intervalUsagePercent * (row + 1) / height));
            for (uint32_t col = 0; col < width; ++col) {
                std::cout << grid[row][col];
            }
            std::cout << std::endl;
        }
        for (uint32_t col = 0; col < 8; ++col) {
            std::cout << ' ';
        }
        for (uint32_t col = 0; col < width; ++col) {
            std::cout << '-';
        }
        std::cout << std::endl;
    }

    void memoryUsage(uint32_t height = 20) {
        memoryUsage({}, {}, height);
    }

    void memoryUsage(
            std::chrono::microseconds endTime, std::chrono::microseconds startTime, uint32_t height = 20) {
        uint32_t width = getTermWidth() - 8;
        uint64_t maxMaxRSS = 0;

        std::set<Usage> usages = getUsageStats(width);
        char grid[height][width];
        for (uint32_t i = 0; i < height; ++i) {
            for (uint32_t j = 0; j < width; ++j) {
                grid[i][j] = ' ';
            }
        }

        for (auto& usage : usages) {
            maxMaxRSS = std::max(maxMaxRSS, usage.maxRSS);
        }
        size_t col = 0;
        for (const Usage& currentUsage : usages) {
            uint64_t curHeight = height * currentUsage.maxRSS / maxMaxRSS;
            for (uint32_t row = 0; row < curHeight; ++row) {
                grid[row][col] = '*';
            }
            ++col;
        }

        // Print array
        for (int32_t row = height - 1; row >= 0; --row) {
            printf("%6s ", Tools::formatMemory(maxMaxRSS * (row + 1) / height).c_str());
            for (uint32_t col = 0; col < width; ++col) {
                std::cout << grid[row][col];
            }
            std::cout << std::endl;
        }
        for (uint32_t col = 0; col < 8; ++col) {
            std::cout << ' ';
        }
        for (uint32_t col = 0; col < width; ++col) {
            std::cout << '-';
        }
        std::cout << std::endl;
    }
    void setupTabCompletion() {
        linereader.clearTabCompletion();

        linereader.appendTabCompletion("rel");
        linereader.appendTabCompletion("rul");
        linereader.appendTabCompletion("rul id");
        linereader.appendTabCompletion("graph ");
        linereader.appendTabCompletion("top");
        linereader.appendTabCompletion("help");
        linereader.appendTabCompletion("usage");
        linereader.appendTabCompletion("limit ");
        linereader.appendTabCompletion("memory");
        linereader.appendTabCompletion("configuration");

        // add rel tab completes after the rest so users can see all commands first
        for (auto& row : Tools::formatTable(relationTable, precision)) {
            linereader.appendTabCompletion("rel " + row[5]);
            linereader.appendTabCompletion("graph " + row[5] + " tot_t");
            linereader.appendTabCompletion("graph " + row[5] + " copy_t");
            linereader.appendTabCompletion("graph " + row[5] + " tuples");
            linereader.appendTabCompletion("usage " + row[5]);
        }
    }

    void configuration() {
        std::cout << "Configuration" << '\n';
        printf("%30s      %s", "Key", "Value\n\n");
        for (auto& kvp :
                ProfileEventSingleton::instance().getDB().getStringMap({"program", "configuration"})) {
            if (kvp.first == "") {
                printf("%30s      %s\n", "Datalog input file", kvp.second.c_str());
                continue;
            }
            printf("%30s      %s\n", kvp.first.c_str(), kvp.second.c_str());
        }
        std::cout << std::endl;
    }

    void top() {
        const std::shared_ptr<ProgramRun>& run = out.getProgramRun();
        auto* totalRelationsEntry =
                dynamic_cast<TextEntry*>(ProfileEventSingleton::instance().getDB().lookupEntry(
                        {"program", "configuration", "relationCount"}));
        auto* totalRulesEntry =
                dynamic_cast<TextEntry*>(ProfileEventSingleton::instance().getDB().lookupEntry(
                        {"program", "configuration", "ruleCount"}));
        size_t totalRelations = 0;
        if (totalRelationsEntry != nullptr) {
            totalRelations = std::stoul(totalRelationsEntry->getText());
        } else {
            totalRelations = run->getRelationMap().size();
        }
        size_t totalRules = 0;
        if (totalRulesEntry != nullptr) {
            totalRules = std::stoul(totalRulesEntry->getText());
        } else {
            totalRules = ruleTable.getRows().size();
        }
        std::printf("%11s%10s%10s%10s%10s%20s\n", "runtime", "loadtime", "savetime", "relations", "rules",
                "tuples generated");

        std::printf("%11s%10s%10s%10s%10s%14s\n", run->getRuntime().c_str(),
                run->formatTime(run->getTotalLoadtime()).c_str(),
                run->formatTime(run->getTotalSavetime()).c_str(), run->formatNum(0, totalRelations).c_str(),
                run->formatNum(0, totalRules).c_str(),
                run->formatNum(precision, run->getTotalSize()).c_str());

        // Progress bar
        // Determine number of relations processed
        size_t processedRelations = run->getRelationMap().size();
        size_t screenWidth = getTermWidth() - 10;
        if (alive && totalRelationsEntry != nullptr) {
            std::cout << "Progress ";
            for (size_t i = 0; i < screenWidth; ++i) {
                if (screenWidth * processedRelations / totalRelations > i) {
                    std::cout << '#';
                } else {
                    std::cout << '_';
                }
            }
        }
        std::cout << std::endl;

        std::cout << "Slowest relations to fully evaluate\n";
        rel(3, false);
        for (size_t i = relationTable.getRows().size(); i < 3; ++i) {
            std::cout << "\n";
        }
        std::cout << "Slowest rules to fully evaluate\n";
        rul(3, false);
        for (size_t i = ruleTable.getRows().size(); i < 3; ++i) {
            std::cout << "\n";
        }

        usage(10);
    }

    void setResultLimit(size_t limit) {
        resultLimit = limit;
    }

    void rel(size_t limit, bool showLimit = true) {
        relationTable.sort(sortColumn);
        std::cout << " ----- Relation Table -----\n";
        std::printf("%8s%8s%8s%8s%8s%8s%8s%8s%8s%6s %s\n\n", "TOT_T", "NREC_T", "REC_T", "COPY_T", "LOAD_T",
                "SAVE_T", "TUPLES", "READS", "TUP/s", "ID", "NAME");
        size_t count = 0;
        for (auto& row : Tools::formatTable(relationTable, precision)) {
            if (++count > limit) {
                if (showLimit) {
                    std::cout << (relationTable.getRows().size() - resultLimit) << " rows not shown"
                              << std::endl;
                }
                break;
            }
            std::printf("%8s%8s%8s%8s%8s%8s%8s%8s%8s%6s %s\n", row[0].c_str(), row[1].c_str(), row[2].c_str(),
                    row[3].c_str(), row[9].c_str(), row[10].c_str(), row[4].c_str(), row[12].c_str(),
                    row[8].c_str(), row[6].c_str(), row[5].c_str());
        }
    }

    void rul(size_t limit, bool showLimit = true) {
        ruleTable.sort(sortColumn);
        std::cout << "  ----- Rule Table -----\n";
        std::printf(
                "%8s%8s%8s%8s%8s%8s %s\n\n", "TOT_T", "NREC_T", "REC_T", "TUPLES", "TUP/s", "ID", "RELATION");
        size_t count = 0;
        for (auto& row : Tools::formatTable(ruleTable, precision)) {
            if (++count > limit) {
                if (showLimit) {
                    std::cout << (ruleTable.getRows().size() - resultLimit) << " rows not shown" << std::endl;
                }
                break;
            }
            std::printf("%8s%8s%8s%8s%8s%8s %s\n", row[0].c_str(), row[1].c_str(), row[2].c_str(),
                    row[4].c_str(), row[9].c_str(), row[6].c_str(), row[7].c_str());
        }
    }

    void id(std::string col) {
        ruleTable.sort(6);
        std::vector<std::vector<std::string>> table = Tools::formatTable(ruleTable, precision);

        if (col.compare("0") == 0) {
            std::printf("%7s%2s%s\n\n", "ID", "", "NAME");
            for (auto& row : table) {
                std::printf("%7s%2s%s\n", row[6].c_str(), "", row[5].c_str());
            }
        } else {
            for (auto& row : table) {
                if (row[6].compare(col) == 0) {
                    std::printf("%7s%2s%s\n", row[6].c_str(), "", row[5].c_str());
                }
            }
        }
    }

    void relRul(std::string str) {
        ruleTable.sort(sortColumn);

        std::vector<std::vector<std::string>> formattedRuleTable = Tools::formatTable(ruleTable, precision);
        std::vector<std::vector<std::string>> formattedRelationTable =
                Tools::formatTable(relationTable, precision);

        std::cout << "  ----- Rules of a Relation -----\n";
        std::printf("%8s%8s%8s%8s%8s %s\n\n", "TOT_T", "NREC_T", "REC_T", "TUPLES", "ID", "NAME");
        std::string name = "";
        for (auto& row : formattedRelationTable) {
            // Test for relation name or relation id
            if (row[5].compare(str) == 0 || row[6].compare(str) == 0) {
                std::printf("%8s%8s%8s%8s%8s %s\n", row[0].c_str(), row[1].c_str(), row[2].c_str(),
                        row[4].c_str(), row[6].c_str(), row[5].c_str());
                name = row[5];
                break;
            }
        }
        std::cout << " ---------------------------------------------------------\n";
        for (auto& row : formattedRuleTable) {
            if (row[7].compare(name) == 0) {
                std::printf("%8s%8s%8s%8s%8s %s\n", row[0].c_str(), row[1].c_str(), row[2].c_str(),
                        row[4].c_str(), row[6].c_str(), row[7].c_str());
            }
        }
        std::string src = "";
        const std::shared_ptr<ProgramRun>& run = out.getProgramRun();
        if (run->getRelation(name) != nullptr) {
            src = run->getRelation(name)->getLocator();
        }
        std::cout << "\nSrc locator: " << src << "\n\n";
        for (auto& row : formattedRuleTable) {
            if (row[7].compare(name) == 0) {
                std::printf("%7s%2s%s\n", row[6].c_str(), "", row[5].c_str());
            }
        }
    }

    void verRul(std::string str) {
        if (str.find(".") == std::string::npos) {
            std::cout << "Rule does not exist\n";
            return;
        }
        std::vector<std::string> part = Tools::split(str, ".");
        std::string strRel = "R" + part[0].substr(1);

        Table versionTable = out.getVersions(strRel, str);
        versionTable.sort(sortColumn);

        ruleTable.sort(sortColumn);  // why isnt it sorted in the original java?!?

        std::vector<std::vector<std::string>> formattedRuleTable = Tools::formatTable(ruleTable, precision);

        bool found = false;
        std::string ruleName;
        std::string srcLocator;
        // Check that the rule exists, and print it out if so.
        for (auto& row : formattedRuleTable) {
            if (row[6].compare(str) == 0) {
                std::cout << row[5] << std::endl;
                found = true;
                ruleName = row[5];
                srcLocator = row[10];
            }
        }

        // If the rule exists, print out the source locator.
        if (found) {
            if (versionTable.rows.size() > 0) {
                if (versionTable.rows[0]->cells[9] != nullptr) {
                    std::cout << "Src locator-: " << (*versionTable.rows[0])[9]->getStringVal() << "\n\n";
                } else {
                    std::cout << "Src locator-: -\n\n";
                }
            } else if (formattedRuleTable.size() > 0) {
                std::cout << "Src locator-: " << formattedRuleTable[0][10] << "\n\n";
            }
        }

        // Print out the versions of this rule.
        std::cout << "  ----- Rule Versions Table -----\n";
        std::printf("%8s%8s%8s%16s%6s\n\n", "TOT_T", "NREC_T", "REC_T", "TUPLES", "VER");
        for (auto& row : formattedRuleTable) {
            if (row[6].compare(str) == 0) {
                std::printf("%8s%8s%8s%16s%6s\n", row[0].c_str(), row[1].c_str(), row[2].c_str(),
                        row[4].c_str(), "");
            }
        }
        std::cout << "   ---------------------------------------------\n";
        for (auto& _row : versionTable.rows) {
            Row row = *_row;

            std::printf("%8s%8s%8s%16s%6s\n", row[0]->toString(precision).c_str(),
                    row[1]->toString(precision).c_str(), row[2]->toString(precision).c_str(),
                    row[4]->toString(precision).c_str(), row[8]->toString(precision).c_str());
            Table atom_table = out.getVersionAtoms(strRel, srcLocator, row[8]->getLongVal());
            verAtoms(atom_table);
        }

        if (!versionTable.rows.empty()) {
            return;
        }

        Table atom_table = out.getAtomTable(strRel, str);
        verAtoms(atom_table, ruleName);
    }

    void iterRel(std::string c, std::string col) {
        const std::shared_ptr<ProgramRun>& run = out.getProgramRun();
        std::vector<std::vector<std::string>> table = Tools::formatTable(relationTable, -1);
        std::vector<std::shared_ptr<Iteration>> iter;
        for (auto& row : table) {
            if (row[6].compare(c) == 0) {
                std::printf("%4s%2s%s\n\n", row[6].c_str(), "", row[5].c_str());
                iter = run->getRelation(row[5])->getIterations();
                if (col.compare("tot_t") == 0) {
                    std::vector<std::chrono::microseconds> list;
                    for (auto& i : iter) {
                        list.emplace_back(i->getRuntime());
                    }
                    std::printf("%4s   %s\n\n", "NO", "RUNTIME");
                    graphByTime(list);
                } else if (col.compare("copy_t") == 0) {
                    std::vector<std::chrono::microseconds> list;
                    for (auto& i : iter) {
                        list.emplace_back(i->getCopytime());
                    }
                    std::printf("%4s   %s\n\n", "NO", "COPYTIME");
                    graphByTime(list);
                } else if (col.compare("tuples") == 0) {
                    std::vector<size_t> list;
                    for (auto& i : iter) {
                        list.emplace_back(i->size());
                    }
                    std::printf("%4s   %s\n\n", "NO", "TUPLES");
                    graphBySize(list);
                }
                return;
            }
        }
        for (auto& row : table) {
            if (row[5].compare(c) == 0) {
                std::printf("%4s%2s%s\n\n", row[6].c_str(), "", row[5].c_str());
                const std::shared_ptr<ProgramRun>& run = out.getProgramRun();
                iter = run->getRelation(row[5])->getIterations();
                if (col.compare("tot_t") == 0) {
                    std::vector<std::chrono::microseconds> list;
                    for (auto& i : iter) {
                        list.emplace_back(i->getRuntime());
                    }
                    std::printf("%4s   %s\n\n", "NO", "RUNTIME");
                    graphByTime(list);
                } else if (col.compare("copy_t") == 0) {
                    std::vector<std::chrono::microseconds> list;
                    for (auto& i : iter) {
                        list.emplace_back(i->getCopytime());
                    }
                    std::printf("%4s   %s\n\n", "NO", "COPYTIME");
                    graphByTime(list);
                } else if (col.compare("tuples") == 0) {
                    std::vector<size_t> list;
                    for (auto& i : iter) {
                        list.emplace_back(i->size());
                    }
                    std::printf("%4s   %s\n\n", "NO", "TUPLES");
                    graphBySize(list);
                }
                return;
            }
        }
    }

    void iterRul(std::string c, std::string col) {
        std::vector<std::vector<std::string>> table = Tools::formatTable(ruleTable, precision);
        std::vector<std::shared_ptr<Iteration>> iter;
        for (auto& row : table) {
            if (row[6].compare(c) == 0) {
                std::printf("%6s%2s%s\n\n", row[6].c_str(), "", row[5].c_str());
                const std::shared_ptr<ProgramRun>& run = out.getProgramRun();
                iter = run->getRelation(row[7])->getIterations();
                if (col.compare("tot_t") == 0) {
                    std::vector<std::chrono::microseconds> list;
                    for (auto& i : iter) {
                        bool add = false;
                        std::chrono::microseconds totalTime{};
                        for (auto& rul : i->getRules()) {
                            if (rul.second->getId().compare(c) == 0) {
                                totalTime += rul.second->getRuntime();
                                add = true;
                            }
                        }
                        if (add) {
                            list.emplace_back(totalTime);
                        }
                    }
                    std::printf("%4s   %s\n\n", "NO", "RUNTIME");
                    graphByTime(list);
                } else if (col.compare("tuples") == 0) {
                    std::vector<size_t> list;
                    for (auto& i : iter) {
                        bool add = false;
                        size_t totalSize = 0L;
                        for (auto& rul : i->getRules()) {
                            if (rul.second->getId().compare(c) == 0) {
                                totalSize += rul.second->size();
                                add = true;
                            }
                        }
                        if (add) {
                            list.emplace_back(totalSize);
                        }
                    }
                    std::printf("%4s   %s\n\n", "NO", "TUPLES");
                    graphBySize(list);
                }
                break;
            }
        }
    }

    void verGraph(std::string c, std::string col) {
        if (c.find('.') == std::string::npos) {
            std::cout << "Rule does not exist";
            return;
        }

        std::vector<std::string> part = Tools::split(c, ".");
        std::string strRel = "R" + part[0].substr(1);

        Table versionTable = out.getVersions(strRel, c);
        std::printf("%6s%2s%s\n\n", (*versionTable.rows[0])[6]->toString(0).c_str(), "",
                (*versionTable.rows[0])[5]->toString(0).c_str());
        if (col.compare("tot_t") == 0) {
            std::vector<std::chrono::microseconds> list;
            for (auto& row : versionTable.rows) {
                list.emplace_back((*row)[0]->getTimeVal());
            }
            std::printf("%4s   %s\n\n", "NO", "RUNTIME");
            graphByTime(list);
        } else if (col.compare("copy_t") == 0) {
            std::vector<std::chrono::microseconds> list;
            for (auto& row : versionTable.rows) {
                list.emplace_back((*row)[3]->getTimeVal());
            }
            std::printf("%4s   %s\n\n", "NO", "COPYTIME");
            graphByTime(list);
        } else if (col.compare("tuples") == 0) {
            std::vector<size_t> list;
            for (auto& row : versionTable.rows) {
                list.emplace_back((*row)[4]->getLongVal());
            }
            std::printf("%4s   %s\n\n", "NO", "TUPLES");
            graphBySize(list);
        }
    }

    void graphByTime(std::vector<std::chrono::microseconds> list) {
        std::chrono::microseconds max{};
        for (auto& d : list) {
            if (d > max) {
                max = d;
            }
        }

        std::sort(list.begin(), list.end());
        std::reverse(list.begin(), list.end());
        int i = 0;
        for (auto& d : list) {
            uint32_t len = 67.0 * d.count() / max.count();
            std::string bar = "";
            for (uint32_t j = 0; j < len; j++) {
                bar += "*";
            }

            std::printf("%4d %10.8f | %s\n", i++, (d.count() / 1000000.0), bar.c_str());
        }
    }

    void graphBySize(std::vector<size_t> list) {
        size_t max = 0;
        for (auto& l : list) {
            if (l > max) {
                max = l;
            }
        }
        std::sort(list.begin(), list.end());
        std::reverse(list.begin(), list.end());
        uint32_t i = 0;
        for (auto& l : list) {
            size_t len = max == 0 ? 0 : 64.0 * l / max;
            std::string bar = "";
            for (uint32_t j = 0; j < len; j++) {
                bar += "*";
            }

            std::printf("%4d %8s | %s\n", i++, Tools::formatNum(precision, l).c_str(), bar.c_str());
        }
    }

protected:
    void verAtoms(Table& atomTable, const std::string& ruleName = "") {
        // If there are no subrules then just print out any atoms found
        // If we do find subrules, then label atoms with their subrules.
        if (atomTable.rows.empty()) {
            return;
        }
        bool firstRun = true;
        std::string lastRule = ruleName;
        for (auto& _row : atomTable.rows) {
            Row& row = *_row;
            std::string rule = row[0]->toString(precision);
            if (rule != lastRule) {
                lastRule = rule;
                std::cout << "     " << row[0]->toString(precision) << std::endl;
                firstRun = true;
            }
            if (firstRun) {
                std::printf("      %-16s%-16s%s\n", "FREQ", "RELSIZE", "ATOM");
                firstRun = false;
            }
            std::string relationName = row[1]->getStringVal();
            relationName = relationName.substr(0, relationName.find('('));
            auto* relation = out.getProgramRun()->getRelation(relationName);
            std::string relationSize = relation == nullptr ? "--" : std::to_string(relation->size());
            std::printf("      %-16s%-16s%s\n", row[3]->toString(precision).c_str(), relationSize.c_str(),
                    row[1]->getStringVal().c_str());
        }
        std::cout << '\n';
    }
    void updateDB() {
        reader->processFile();
        ruleTable = out.getRulTable();
        relationTable = out.getRelTable();
    }

    uint32_t getTermWidth() {
        struct winsize w {};
        ioctl(0, TIOCGWINSZ, &w);
        uint32_t width = w.ws_col > 0 ? w.ws_col : 80;
        return width;
    }
};

}  // namespace profile
}  // namespace souffle
