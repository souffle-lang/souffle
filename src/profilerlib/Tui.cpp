/*
* Souffle - A Datalog Compiler
* Copyright (c) 2016, The Souffle Developers. All rights reserved
* Licensed under the Universal Permissive License v 1.0 as shown at:
* - https://opensource.org/licenses/UPL
* - <souffle root>/licenses/SOUFFLE-UPL.txt
*/

#include "Tui.hpp"

Tui::Tui(std::string filename, bool live, bool gui) : out() {
    this->f_name = filename;

    // out = OutputProcessor();
    std::shared_ptr<ProgramRun>& run = out.getProgramRun();

    this->reader = std::make_shared<Reader>(filename, run, false, live);
    reader->readFile();

    this->loaded = reader->isLoaded();
    this->alive = live;
    rul_table_state = out.getRulTable();
    rel_table_state = out.getRelTable();
}

void Tui::runCommand(std::vector<std::string> c) {
    if (!loaded) {
        std::cout << "Error: File cannot be loaded\n";
        return;
    }

    if (alive) {
        // remake tables to get new data
        rul_table_state = out.getRulTable();
        rel_table_state = out.getRelTable();

        setupTabCompletion();
    }

    if (c[0].compare("top") == 0) {
        top();
    } else if (c[0].compare("rel") == 0) {
        if (c.size() == 2) {
            relRul(c[1]);
        } else if (c.size() == 1) {
            rel(c[0]);
        } else {
            std::cout << "Invalid parameters to rel command.\n";
        }
    } else if (c[0].compare("rul") == 0) {
        if (c.size() > 1) {
            if (c.size() == 3 && c[1].compare("id") == 0) {
                std::printf("%7s%2s%-25s\n\n", "ID", "", "NAME");
                id(c[2]);
            } else if (c.size() == 2 && c[1].compare("id") == 0) {
                id("0");
            } else if (c.size() == 2) {
                verRul(c[1]);
            } else {
                std::cout << "Invalid parameters to rul command.\n";
            }
        } else {
            rul(c[0]);
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
    } else if (c[0].compare("help") == 0) {
        help();
    } else {
        std::cout << "Unknown command. Use \"help\" for a list of commands.\n";
    }
}

void Tui::runProf() {
    if (!loaded && !f_name.empty()) {
        std::cout << "Error: File cannot be loaded\n";
        return;
    }
    if (loaded) {
        std::cout << "SouffleProf v3.0.1\n";
        top();
    }

    linereader = InputReader();
    linereader.setPrompt("\n> ");
    setupTabCompletion();

    while (true) {
        if (!loaded) {
            loadMenu();
            if (!f_name.empty()) {
                std::cout << "Error loading file.\n";
            }
        }
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
        } else if (c[0] == "load" || c[0] == "open") {
            if (c.size() == 2) {
                load(c[0], c[1]);
            } else {
                loadMenu();
            }
        } else if (c[0] == "save") {
            if (c.size() == 1) {
                std::cout << "Enter file name to save.\n";
            } else if (c.size() == 2) {
                save(c[1]);
            }
        } else if (c[0] == "sort") {
            if (c.size() == 2 && std::stoi(c[1]) < 7) {
                sort_col = std::stoi(c[1]);
            } else {
                std::cout << "Invalid column, please select a number between 0 and 6.\n";
            }
        } else {
            runCommand(c);
        }
    }
}

void Tui::outputJson() {
    std::cout << "SouffleProf v3.0.1\n";
    std::cout << "Generating JSON files...\n";

    std::string workingdir = Tools::getworkingdir();
    if (workingdir.size() == 0) {
        std::cerr << "Error getting working directory.\nTry run the profiler using an absolute path."
                  << std::endl;
        throw 1;
    }
    DIR* dir;
    struct dirent* ent;
    bool exists = false;

    if ((dir = opendir((workingdir + std::string("/profiler_html")).c_str())) != NULL) {
        exists = true;
        closedir(dir);
    }
    if (!exists) {
        std::string sPath = workingdir + std::string("/profiler_html");
        mode_t nMode = 0733;  // UNIX style permissions
        int nError = 0;
#if defined(_WIN32) || defined(_WIN64) || defined(WINDOWS)
        nError = _mkdir(sPath.c_str());  // can be used on Windows
#else
        nError = mkdir(sPath.c_str(), nMode);  // can be used on non-Windows
#endif
        if (nError != 0) {
            std::cerr << "directory ./profiler_html/ failed to be created. Please create it and try again.";
            exit(2);
        }
    }

    std::string new_file = workingdir + std::string("/profiler_html/");
    if (Tools::file_exists(new_file)) {
        int i = 1;
        while (Tools::file_exists(new_file + std::to_string(i) + ".html")) {
            i++;
        }

        new_file = new_file + std::to_string(i) + ".html";
    }

    FILE* outfile;
    outfile = std::fopen(new_file.c_str(), "w");

    html_string html;

    std::fprintf(outfile, "%s", html.get_first_half().c_str());

    std::shared_ptr<ProgramRun>& run = out.getProgramRun();
    std::string source_loc;
    std::fprintf(
            outfile, "data={'top':[%f,%lu],\n'rel':{\n", run->getDoubleRuntime(), run->getTotNumTuples());
    for (auto& _row : rel_table_state.getRows()) {
        Row row = *_row;
        std::fprintf(outfile, "'%s':['%s','%s',%s,%s,%s,%s,%lu,'%s',[", row[6]->getStringVal().c_str(),
                Tools::cleanJsonOut(row[5]->getStringVal()).c_str(), row[6]->getStringVal().c_str(),
                Tools::cleanJsonOut(row[0]->getDoubVal()).c_str(),
                Tools::cleanJsonOut(row[1]->getDoubVal()).c_str(),
                Tools::cleanJsonOut(row[2]->getDoubVal()).c_str(),
                Tools::cleanJsonOut(row[3]->getDoubVal()).c_str(), row[4]->getLongVal(),
                row[7]->getStringVal().c_str());
        source_loc = row[7]->getStringVal();  // if (source_loc.empty()) {...} faster?
        for (auto& _rel_row : rul_table_state.getRows()) {
            Row rel_row = *_rel_row;
            if (rel_row[7]->getStringVal().compare(row[5]->getStringVal()) == 0) {
                std::fprintf(outfile, "'%s',", rel_row[6]->getStringVal().c_str());
            }
        }
        std::fprintf(outfile, "],{\"tot_t\":[");
        std::vector<std::shared_ptr<Iteration>> iter =
                run->getRelation_map()[row[5]->getStringVal()]->getIterations();
        for (auto& i : iter) {
            std::fprintf(outfile, "%s,", Tools::cleanJsonOut(i->getRuntime()).c_str());
        }
        std::fprintf(outfile, "],\"copy_t\":[");
        for (auto& i : iter) {
            std::fprintf(outfile, "%s,", Tools::cleanJsonOut(i->getCopy_time()).c_str());
        }
        std::fprintf(outfile, "],\"tuples\":[");
        for (auto& i : iter) {
            std::fprintf(outfile, "%lu,", i->getNum_tuples());
        }
        std::fprintf(outfile, "]}],\n");
    }
    std::fprintf(outfile, "},'rul':{\n");

    for (auto& _row : rul_table_state.getRows()) {
        Row row = *_row;

        std::vector<std::string> part = Tools::split(row[6]->getStringVal(), ".");
        std::string strRel = "R" + part[0].substr(1);
        Table ver_table = out.getVersions(strRel, row[6]->getStringVal());

        std::string src;
        if (ver_table.rows.size() > 0) {
            if (ver_table.rows[0]->cells[9] != nullptr) {
                src = (*ver_table.rows[0])[9]->getStringVal();
            } else {
                src = "-";
            }
        } else {
            src = row[10]->toString(-1);
        }

        std::fprintf(outfile, "\"%s\":[\"%s\",\"%s\",%s,%s,%s,%s,%lu,\"%s\",[",
                row[6]->getStringVal().c_str(), Tools::cleanJsonOut(row[5]->getStringVal()).c_str(),
                row[6]->getStringVal().c_str(), Tools::cleanJsonOut(row[0]->getDoubVal()).c_str(),
                Tools::cleanJsonOut(row[1]->getDoubVal()).c_str(),
                Tools::cleanJsonOut(row[2]->getDoubVal()).c_str(),
                Tools::cleanJsonOut(row[3]->getDoubVal()).c_str(), row[4]->getLongVal(), src.c_str());

        bool has_ver = false;
        for (auto& _ver_row : ver_table.getRows()) {
            has_ver = true;
            Row ver_row = *_ver_row;
            std::fprintf(outfile, "[\"%s\",\"%s\",%s,%s,%s,%s,%lu,\"%s\",%lu],",

                    Tools::cleanJsonOut(ver_row[5]->getStringVal()).c_str(),
                    ver_row[6]->getStringVal().c_str(), Tools::cleanJsonOut(ver_row[0]->getDoubVal()).c_str(),
                    Tools::cleanJsonOut(ver_row[1]->getDoubVal()).c_str(),
                    Tools::cleanJsonOut(ver_row[2]->getDoubVal()).c_str(),
                    Tools::cleanJsonOut(ver_row[3]->getDoubVal()).c_str(), ver_row[4]->getLongVal(),
                    src.c_str(), ver_row[8]->getLongVal());
        }
        if (row[6]->getStringVal().at(0) == 'C') {
            std::fprintf(outfile, "],{\"tot_t\":[");

            std::vector<long> iteration_tuples;
            for (auto& i : run->getRelation_map()[row[7]->getStringVal()]->getIterations()) {
                bool add = false;
                double tot_time = 0.0;
                long tot_num = 0.0;
                for (auto& rul : i->getRul_rec()) {
                    if (rul.second->getId().compare(row[6]->getStringVal()) == 0) {
                        tot_time += rul.second->getRuntime();

                        tot_num += rul.second->getNum_tuples();
                        add = true;
                    }
                }
                if (add) {
                    std::fprintf(outfile, "%s,", Tools::cleanJsonOut(tot_time).c_str());
                    iteration_tuples.push_back(tot_num);
                }
            }
            std::fprintf(outfile, "], \"tuples\":[");
            for (auto& i : iteration_tuples) {
                std::fprintf(outfile, "%lu,", i);
            }

            std::fprintf(outfile, "]},{");

            if (has_ver) {
                std::fprintf(outfile, "\"tot_t\":[\n");

                for (auto& row : ver_table.rows) {
                    std::fprintf(outfile, "%s,", Tools::cleanJsonOut((*row)[0]->getDoubVal()).c_str());
                }
                std::fprintf(outfile, "],\n\"copy_t\":[");
                for (auto& row : ver_table.rows) {
                    std::fprintf(outfile, "%s,", Tools::cleanJsonOut((*row)[3]->getDoubVal()).c_str());
                }
                std::fprintf(outfile, "],\n\"tuples\":[");
                for (auto& row : ver_table.rows) {
                    std::fprintf(outfile, "%ld,", (*row)[4]->getLongVal());
                }
                std::fprintf(outfile, "]}],\n");
            } else {
                std::fprintf(outfile, "}],\n");
            }
        } else {
            std::fprintf(outfile, "],{},{}],\n");
        }
    }
    std::fprintf(outfile, "},");

    std::string source_file_loc = Tools::split(source_loc, " ").at(0);  // add error check?
    std::ifstream source_file(source_file_loc);
    if (!source_file.is_open()) {
        std::cout << "Error opening \"" << source_file_loc << "\", creating GUI without source locator."
                  << std::endl;
    } else {
        std::string str;
        std::fprintf(outfile, "code:[");
        while (getline(source_file, str)) {
            std::fprintf(outfile, "\"%s\",", Tools::escapeQuotes(str).c_str());
        }
        std::fprintf(outfile, "]");
        source_file.close();
    }

    std::fprintf(outfile, "};");
    std::fprintf(outfile, "%s", html.get_second_half().c_str());

    fclose(outfile);

    std::cout << "file output to: " << new_file << std::endl;
}

void Tui::loadMenu() {
    std::cout << "Please 'load' a file or 'open' from Previous Runs.\n";
    std::cout << "Previous Runs:\n";

    // http://stackoverflow.com/a/612176
    // TODO: check cross-platform capability
    DIR* dir;
    struct dirent* ent;
    if ((dir = opendir("./old_runs")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            // if the file doesnt exist in the working directory, it is in old_runs (to remove . and ..)
            if (!Tools::file_exists(ent->d_name)) {
                printf("- %s\n", ent->d_name);
            }
        }
        closedir(dir);
    }
    //    else {
    //        do nothing
    //    }
}

void Tui::quit() {
    if (alive && loaded) {
        // std::cerr << "Liver reader not implemented\n";
        // live_reader.stopRead();
    }
}

void Tui::save(std::string save_name) {
    if (loaded) {
        std::shared_ptr<ProgramRun>& run = out.getProgramRun();
        Reader saver(this->f_name, run, false, false);
        saver.save(save_name);
        std::cout << "Save success.\n";
    } else {
        std::cout << "Save failed.\n";
    }
}

void Tui::load(std::string method, std::string load_file) {
    std::shared_ptr<ProgramRun> new_run = std::make_shared<ProgramRun>(ProgramRun());
    std::string f_name = load_file;
    // if load, should be a valid filepath
    if (method.compare("open") == 0) {
        f_name = Tools::getworkingdir() + "/old_runs/" + load_file;
    }
    Reader loader(f_name, new_run, false, false);
    loader.readFile();
    if (loader.isLoaded()) {
        std::cout << "Load success\n";
        this->loaded = true;
        this->f_name = f_name;
        //        if (alive) {
        //            std::cout << "Live reader not implemented\n" << std::endl;
        //            throw;
        //        }
        top();
    } else {
        std::cout << "Error: File not found\n";
    }
}

void Tui::setupTabCompletion() {
    linereader.clearTabCompletion();

    linereader.appendTabCompletion("rel");
    linereader.appendTabCompletion("rul");
    linereader.appendTabCompletion("rul id");
    linereader.appendTabCompletion("graph ");
    linereader.appendTabCompletion("top");
    linereader.appendTabCompletion("help");

    // add rel tab completes after the rest so users can see all commands first
    for (auto& row : out.formatTable(rel_table_state, precision)) {
        linereader.appendTabCompletion("rel " + row[5]);
        linereader.appendTabCompletion("graph " + row[5] + " tot_t");
        linereader.appendTabCompletion("graph " + row[5] + " copy_t");
        linereader.appendTabCompletion("graph " + row[5] + " tuples");
    }
}

void Tui::help() {
    std::cout << "\nAvailable profiling commands:" << std::endl;
    std::printf("  %-30s%-5s %-10s\n", "rel", "-", "display relation table.");
    std::printf("  %-30s%-5s %-10s\n", "rel <relation id>", "-", "display all rules of a given relation.");
    std::printf("  %-30s%-5s %-10s\n", "rul", "-", "display rule table");
    std::printf("  %-30s%-5s %-10s\n", "rul <rule id>", "-", "display all version of given rule.");
    std::printf("  %-30s%-5s %-10s\n", "rul id", "-", "display all rules names and ids.");
    std::printf(
            "  %-30s%-5s %-10s\n", "rul id <rule id>", "-", "display the rule name for the given rule id.");
    std::printf("  %-30s%-5s %-10s\n", "graph <relation id> <type>", "-",
            "graph a relation by type: (tot_t/copy_t/tuples).");
    std::printf("  %-30s%-5s %-10s\n", "graph <rule id> <type>", "-",
            "graph recursive(C) rule by type(tot_t/tuples).");
    std::printf("  %-30s%-5s %-10s\n", "graph ver <rule id> <type>", "-",
            "graph recursive(C) rule versions by type(tot_t/copy_t/tuples).");
    std::printf("  %-30s%-5s %-10s\n", "top", "-", "display top-level summary of program run.");
    std::printf("  %-30s%-5s %-10s\n", "help", "-", "print this.");

    std::cout << "\nInteractive mode only commands:" << std::endl;
    std::printf("  %-30s%-5s %-10s\n", "load <filename>", "-", "load the given profiler log file.");
    std::printf("  %-30s%-5s %-10s\n", "open", "-", "list stored souffle log files.");
    std::printf("  %-30s%-5s %-10s\n", "open <filename>", "-", "open the given stored log file.");
    std::printf("  %-30s%-5s %-10s\n", "save <filename>", "-", "store a copy of the souffle log file.");
    //    if (alive) std::printf("  %-30s%-5s %-10s\n", "stop", "-",
    //                "stop the current live run.");
    std::printf("  %-30s%-5s %-10s\n", "sort <col number>", "-", "sort tables by given column number.");
    std::printf("  %-30s%-5s %-10s\n", "q", "-", "exit program.");
}

void Tui::top() {
    std::shared_ptr<ProgramRun>& run = out.getProgramRun();
    if (alive) run->update();
    std::string runtime = run->getRuntime();
    std::cout << "\n Total runtime: " << runtime << "\n";

    std::cout << "\n Total number of new tuples: " << run->formatNum(precision, run->getTotNumTuples())
              << std::endl;
}

void Tui::rel(std::string c) {
    rel_table_state.sort(sort_col);
    std::cout << " ----- Relation Table -----\n";
    std::printf("%8s%8s%8s%8s%15s%6s%1s%-25s\n\n", "TOT_T", "NREC_T", "REC_T", "COPY_T", "TUPLES", "ID", "",
            "NAME");
    for (auto& row : out.formatTable(rel_table_state, precision)) {
        std::printf("%8s%8s%8s%8s%15s%6s%1s%-5s\n", row[0].c_str(), row[1].c_str(), row[2].c_str(),
                row[3].c_str(), row[4].c_str(), row[6].c_str(), "", row[5].c_str());
    }
}

void Tui::rul(std::string c) {
    rul_table_state.sort(sort_col);
    std::cout << "  ----- Rule Table -----\n";
    std::printf(
            "%8s%8s%8s%8s%15s    %-5s\n\n", "TOT_T", "NREC_T", "REC_T", "COPY_T", "TUPLES", "ID RELATION");
    for (auto& row : out.formatTable(rul_table_state, precision)) {
        std::printf("%8s%8s%8s%8s%15s%8s %-25s\n", row[0].c_str(), row[1].c_str(), row[2].c_str(),
                row[3].c_str(), row[4].c_str(), row[6].c_str(), row[7].c_str());
    }
}

void Tui::id(std::string col) {
    rul_table_state.sort(6);
    std::vector<std::vector<std::string>> table = out.formatTable(rul_table_state, precision);

    if (col.compare("0") == 0) {
        std::printf("%7s%2s%-25s\n\n", "ID", "", "NAME");
        for (auto& row : table) {
            std::printf("%7s%2s%-25s\n", row[6].c_str(), "", row[5].c_str());
        }
    } else {
        for (auto& row : table) {
            if (row[6].compare(col) == 0) {
                std::printf("%7s%2s%-25s\n", row[6].c_str(), "", row[5].c_str());
            }
        }
    }
}

void Tui::relRul(std::string str) {
    rul_table_state.sort(sort_col);

    std::vector<std::vector<std::string>> rul_table = out.formatTable(rul_table_state, precision);
    std::vector<std::vector<std::string>> rel_table = out.formatTable(rel_table_state, precision);

    std::cout << "  ----- Rules of a Relation -----\n";
    std::printf(
            "%8s%8s%8s%8s%10s%8s %-25s\n\n", "TOT_T", "NREC_T", "REC_T", "COPY_T", "TUPLES", "ID", "NAME");
    std::string name = "";
    bool found = false;  // workaround to make it the same as java (row[5] seems to have priority)
    for (auto& row : rel_table) {
        if (row[5].compare(str) == 0) {
            std::printf("%8s%8s%8s%8s%10s%8s %-25s\n", row[0].c_str(), row[1].c_str(), row[2].c_str(),
                    row[3].c_str(), row[4].c_str(), row[6].c_str(), row[5].c_str());
            name = row[5];
            found = true;
            break;
        }
    }
    if (!found) {
        for (auto& row : rel_table) {
            if (row[6].compare(str) == 0) {
                std::printf("%8s%8s%8s%8s%10s%8s %-25s\n", row[0].c_str(), row[1].c_str(), row[2].c_str(),
                        row[3].c_str(), row[4].c_str(), row[6].c_str(), row[5].c_str());
                name = row[5];
                break;
            }
        }
    }
    std::cout << " ---------------------------------------------------------\n";
    for (auto& row : rul_table) {
        if (row[7].compare(name) == 0) {
            std::printf("%8s%8s%8s%8s%10s%8s %-25s\n", row[0].c_str(), row[1].c_str(), row[2].c_str(),
                    row[3].c_str(), row[4].c_str(), row[6].c_str(), row[7].c_str());
        }
    }
    std::string src = "";
    std::shared_ptr<ProgramRun>& run = out.getProgramRun();
    if (run->getRelation(name) != nullptr) {
        src = run->getRelation(name)->getLocator();
    }
    std::cout << "\nSrc locator: " << src << "\n\n";
    for (auto& row : rul_table) {
        if (row[7].compare(name) == 0) {
            std::printf("%7s%2s%-25s\n", row[6].c_str(), "", row[5].c_str());
        }
    }
}

void Tui::verRul(std::string str) {
    if (str.find(".") == std::string::npos) {
        std::cout << "Rule does not exist\n";
        return;
    }
    std::vector<std::string> part = Tools::split(str, ".");
    std::string strRel = "R" + part[0].substr(1);

    Table ver_table = out.getVersions(strRel, str);
    ver_table.sort(sort_col);

    rul_table_state.sort(sort_col);  // why isnt it sorted in the original java?!?

    std::vector<std::vector<std::string>> rul_table = out.formatTable(rul_table_state, precision);

    std::cout << "  ----- Rule Versions Table -----\n";
    std::printf("%8s%8s%8s%8s%10s%6s   %-5s\n\n", "TOT_T", "NREC_T", "REC_T", "COPY_T", "TUPLES", "VER",
            "ID RELATION");
    bool found = false;
    for (auto& row : rul_table) {
        if (row[6].compare(str) == 0) {
            std::printf("%8s%8s%8s%8s%10s%6s%7s %-25s\n", row[0].c_str(), row[1].c_str(), row[2].c_str(),
                    row[3].c_str(), row[4].c_str(), "", row[6].c_str(), row[7].c_str());
            found = true;
        }
    }
    std::cout << " ---------------------------------------------------------\n";
    for (auto& _row : ver_table.rows) {
        Row row = *_row;

        std::printf("%8s%8s%8s%8s%10s%6s%7s %-25s\n", row[0]->toString(precision).c_str(),
                row[1]->toString(precision).c_str(), row[2]->toString(precision).c_str(),
                row[3]->toString(precision).c_str(), row[4]->toString(precision).c_str(),
                row[8]->toString(precision).c_str(), row[6]->toString(precision).c_str(),
                row[7]->toString(precision).c_str());
    }
    if (found) {
        if (ver_table.rows.size() > 0) {
            if (ver_table.rows[0]->cells[9] != nullptr) {
                std::cout << "\nSrc locator: " << (*ver_table.rows[0])[9]->getStringVal() << "\n\n";
            } else {
                std::cout << "\nSrc locator: -\n\n";
            }
        } else if (rul_table.size() > 0) {
            std::cout << "\nSrc locator-: " << rul_table[0][10] << "\n\n";
        }
    }

    for (auto& row : rul_table) {
        if (row[6].compare(str) == 0) {
            std::printf("%7s%2s%-25s\n", row[6].c_str(), "", row[5].c_str());
        }
    }
}

void Tui::iterRel(std::string c, std::string col) {
    std::vector<std::vector<std::string>> table = out.formatTable(rel_table_state, -1);
    std::vector<std::shared_ptr<Iteration>> iter;
    for (auto& row : table) {
        if (row[6].compare(c) == 0) {
            std::printf("%4s%2s%-25s\n\n", row[6].c_str(), "", row[5].c_str());
            std::shared_ptr<ProgramRun>& run = out.getProgramRun();
            iter = run->getRelation_map()[row[5]]->getIterations();
            if (col.compare("tot_t") == 0) {
                std::vector<double> list;
                for (auto& i : iter) {
                    list.emplace_back(i->getRuntime());
                }
                std::printf("%4s   %-6s\n\n", "NO", "RUNTIME");
                graphD(list);
            } else if (col.compare("copy_t") == 0) {
                std::vector<double> list;
                for (auto& i : iter) {
                    list.emplace_back(i->getCopy_time());
                }
                std::printf("%4s   %-6s\n\n", "NO", "COPYTIME");
                graphD(list);
            } else if (col.compare("tuples") == 0) {
                std::vector<long> list;
                for (auto& i : iter) {
                    list.emplace_back(i->getNum_tuples());
                }
                std::printf("%4s   %-6s\n\n", "NO", "TUPLES");
                graphL(list);
            }
            return;
        }
    }
    for (auto& row : table) {
        if (row[5].compare(c) == 0) {
            std::printf("%4s%2s%-25s\n\n", row[6].c_str(), "", row[5].c_str());
            std::shared_ptr<ProgramRun>& run = out.getProgramRun();
            iter = run->getRelation_map()[row[5]]->getIterations();
            if (col.compare("tot_t") == 0) {
                std::vector<double> list;
                for (auto& i : iter) {
                    list.emplace_back(i->getRuntime());
                }
                std::printf("%4s   %-6s\n\n", "NO", "RUNTIME");
                graphD(list);
            } else if (col.compare("copy_t") == 0) {
                std::vector<double> list;
                for (auto& i : iter) {
                    list.emplace_back(i->getCopy_time());
                }
                std::printf("%4s   %-6s\n\n", "NO", "COPYTIME");
                graphD(list);
            } else if (col.compare("tuples") == 0) {
                std::vector<long> list;
                for (auto& i : iter) {
                    list.emplace_back(i->getNum_tuples());
                }
                std::printf("%4s   %-6s\n\n", "NO", "TUPLES");
                graphL(list);
            }
            return;
        }
    }
}

void Tui::iterRul(std::string c, std::string col) {
    std::vector<std::vector<std::string>> table = out.formatTable(rul_table_state, precision);
    std::vector<std::shared_ptr<Iteration>> iter;
    for (auto& row : table) {
        if (row[6].compare(c) == 0) {
            std::printf("%6s%2s%-25s\n\n", row[6].c_str(), "", row[5].c_str());
            std::shared_ptr<ProgramRun>& run = out.getProgramRun();
            iter = run->getRelation_map()[row[7]]->getIterations();
            if (col.compare("tot_t") == 0) {
                std::vector<double> list;
                for (auto& i : iter) {
                    bool add = false;
                    double tot_time = 0.0;
                    for (auto& rul : i->getRul_rec()) {
                        if (rul.second->getId().compare(c) == 0) {
                            tot_time += rul.second->getRuntime();
                            add = true;
                        }
                    }
                    if (add) {
                        list.emplace_back(tot_time);
                    }
                }
                std::printf("%4s   %-6s\n\n", "NO", "RUNTIME");
                graphD(list);
            } else if (col.compare("tuples") == 0) {
                std::vector<long> list;
                for (auto& i : iter) {
                    bool add = false;
                    long tot_num = 0L;
                    for (auto& rul : i->getRul_rec()) {
                        if (rul.second->getId().compare(c) == 0) {
                            tot_num += rul.second->getNum_tuples();
                            add = true;
                        }
                    }
                    if (add) {
                        list.emplace_back(tot_num);
                    }
                }
                std::printf("%4s   %-6s\n\n", "NO", "TUPLES");
                graphL(list);
            }
            break;
        }
    }
}

void Tui::verGraph(std::string c, std::string col) {
    if (c.find('.') == std::string::npos) {
        std::cout << "Rule does not exist";
        return;
    }

    std::vector<std::string> part = Tools::split(c, ".");
    std::string strRel = "R" + part[0].substr(1);

    Table ver_table = out.getVersions(strRel, c);
    std::printf("%6s%2s%-25s\n\n", (*ver_table.rows[0])[6]->getStringVal().c_str(), "",
            (*ver_table.rows[0])[5]->getStringVal().c_str());
    if (col.compare("tot_t") == 0) {
        std::vector<double> list;
        for (auto& row : ver_table.rows) {
            list.emplace_back((*row)[0]->getDoubVal());
        }
        std::printf("%4s   %-6s\n\n", "NO", "RUNTIME");
        graphD(list);
    } else if (col.compare("copy_t") == 0) {
        std::vector<double> list;
        for (auto& row : ver_table.rows) {
            list.emplace_back((*row)[3]->getDoubVal());
        }
        std::printf("%4s   %-6s\n\n", "NO", "COPYTIME");
        graphD(list);
    } else if (col.compare("tuples") == 0) {
        std::vector<long> list;
        for (auto& row : ver_table.rows) {
            list.emplace_back((*row)[4]->getLongVal());
        }
        std::printf("%4s   %-6s\n\n", "NO", "TUPLES");
        graphL(list);
    }
}

void Tui::graphD(std::vector<double> list) {
    double max = 0;
    for (auto& d : list) {
        if (d > max) {
            max = d;
        }
    }

    std::sort(list.begin(), list.end());
    std::reverse(list.begin(), list.end());
    int i = 0;
    for (auto& d : list) {
        int len = (int)(67 * (d / max));
        // TODO: %4d %10.8f
        std::string bar = "";
        for (int j = 0; j < len; j++) {
            bar += "*";
        }

        if (std::isnan(d)) {
            std::printf("%4d        NaN | %s\n", i++, bar.c_str());
        } else {
            std::printf("%4d %10.8f | %s\n", i++, d, bar.c_str());
        }
    }
}

void Tui::graphL(std::vector<long> list) {
    long max = 0;
    for (auto& l : list) {
        if (l > max) {
            max = l;
        }
    }
    std::sort(list.begin(), list.end());
    std::reverse(list.begin(), list.end());
    int i = 0;
    for (auto& l : list) {
        int len = (int)(64 * ((double)l / (double)max));
        std::string bar = "";
        for (int j = 0; j < len; j++) {
            bar += "*";
        }

        std::printf("%4d %8s | %s\n", i++, out.formatNum(precision, l).c_str(), bar.c_str());
    }
}

bool Tui::string_sort(std::vector<std::string> a, std::vector<std::string> b) {
    // std::cerr << a->getCells()[0]->getDoubVal() << "\n";
    return a[0] > b[0];
}
