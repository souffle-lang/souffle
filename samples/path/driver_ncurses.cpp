#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <cstdlib>
#include <algorithm>
#include <ncurses.h>

#include "souffle/SouffleInterface.h"
#include "render_tree.h"

using namespace souffle;

typedef RamDomain plabel;

// store elements of a tuple
struct elements {
    std::string order;
    std::vector<plabel> integers;
    std::vector<std::string> strings;

    elements() : order(std::string()), integers(std::vector<plabel>()), strings(std::vector<std::string>()){}

    elements(std::vector<std::string> ss) {
        auto isInteger = [](const std::string & s) {
            if (s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+')))
                return false;

            char *p;
            strtol(s.c_str(), &p, 10);

            return (*p == 0);
        };

        for (auto s : ss) {
            if (isInteger(s)) {
                insert(atoi(s.c_str()));
            } else {
                insert(s);
            }
        }
    }

    void insert(std::string s) {
        order.push_back('s');
        strings.push_back(s);
    }

    void insert(plabel i) {
        order.push_back('i');
        integers.push_back(i);
    }

    std::string getRepresentation() {
        if (order.size() == 0) {
            return std::string("()");
        }

        // maintain iterators for integers and strings
        std::vector<plabel>::iterator i_itr = integers.begin();
        std::vector<std::string>::iterator s_itr = strings.begin();
        std::string s = "(";

        for (char &c : order) {
            if (c == 'i') {
                s += std::to_string(*(i_itr++));
            } else {
                s += *(s_itr++);
            }

            s += ", ";
        }

        s.pop_back();
        s.pop_back();

        s += ")";

        return s;
    }


    bool operator==(const elements &e) const {
        return e.order == order && e.integers == integers && e.strings == strings;
    }

    bool operator<(const elements &e) const {
        if (order.size() < e.order.size()) {
            return true;
        } else if (order.size() > e.order.size()) {
            return false;
        }

        if (integers < e.integers) {
            return true;
        }

        if (strings < e.strings) {
            return true;
        }

        return false;
    }
};

SouffleProgram *prog;

std::map<std::pair<std::string, elements>, plabel> valuesToLabel;
std::map<std::pair<std::string, plabel>, elements> labelToValue;
std::map<std::pair<std::string, plabel>, std::vector<plabel>> labelToProof;
std::map<std::string, std::vector<std::string>> info;
std::map<std::pair<std::string, int>, std::string> rule;

int depthLimit = 4;

inline std::vector<std::string> split(std::string s, char delim, int times = -1) {
    std::vector<std::string> v;
    std::stringstream ss(s);
    std::string item;

    while ((times > 0 || times <= -1) && std::getline(ss, item, delim)) {
        v.push_back(item);
        times--;
    }

    if (ss.peek() != EOF) {
        std::string remainder;
        std::getline(ss, remainder);
        v.push_back(remainder);
    }

    return v;
};


void load() {
    for (Relation *rel : prog->getAllRelations()) {
        if (rel->getName().find("_output") != std::string::npos) {
            for (auto &tuple : *rel) {
                plabel label;
                elements tuple_elements;

                tuple >> label;

                for (size_t i = 1; i < tuple.size(); i++) {
                    if (*(rel->getAttrType(i)) == 'i' || *(rel->getAttrType(i)) == 'r') {
                        plabel n;
                        tuple >> n;
                        tuple_elements.insert(n);
                    } else if (*(rel->getAttrType(i)) == 's') {
                        std::string s;
                        tuple >> s;
                        tuple_elements.insert(s);
                    }
                }

                // valuesToLabel[std::make_pair(rel->getName(), e)] = l;
                valuesToLabel.insert({std::make_pair(rel->getName(), tuple_elements), label});
                labelToValue.insert({std::make_pair(rel->getName(), label), tuple_elements});
            }
        } else if (rel->getName().find("_new_") != std::string::npos && rel->getName().find("_info") == std::string::npos) {
            for (auto &tuple : *rel) {
                plabel label;
                std::vector<plabel> refs;

                tuple >> label;

                for (size_t i = 1; i < tuple.size(); i++) {
                    plabel l;
                    tuple >> l;
                    refs.push_back(l);
                }

                labelToProof.insert({std::make_pair(rel->getName(), label), refs});
            }
        } else if (rel->getName().find("_info") != std::string::npos) {
            for (auto &tuple : *rel) {
                std::vector<std::string> rels;
                for (size_t i = 0; i < tuple.size() - 2; i++) {
                    std::string s;
                    tuple >> s;
                    rels.push_back(s);
                }

                // second last argument is original relation name
                std::string relName;
                tuple >> relName;

                // last argument is representation of clause
                std::string clauseRepr;
                tuple >> clauseRepr;

                // extract rule number from relation name
                int ruleNum = atoi((*(split(rel->getName(), '_').rbegin() + 1)).c_str());

                info.insert({rel->getName(), rels});
                rule.insert({std::make_pair(relName, ruleNum), clauseRepr}); 
            }
        }
    }
}

std::unique_ptr<tree_node> explain(std::string relName, plabel label, int depth) {
    if (prog->getRelation(relName) != nullptr && prog->getRelation(relName)->isInput()) {
        auto key = std::make_pair(relName + "_output", label);
        std::string lab = relName + labelToValue[key].getRepresentation();
        std::unique_ptr<tree_node> leaf(new leaf_node(lab));
        return leaf;
    } else {
        if (depth > 0) {
            std::string internalRelName;
            // find correct relation
            for (auto rel : prog->getAllRelations()) {
                if (rel->getName().find(relName + "_new_") != std::string::npos && rel->getName().find("_info") == std::string::npos) {
                    if (labelToProof.find(std::make_pair(rel->getName(), label)) != labelToProof.end()) {
                        // found the correct relation
                        internalRelName = rel->getName();
                        break;
                    }
                }
            }

            auto key = std::make_pair(relName + "_output", label);
            auto subProofKey = std::make_pair(internalRelName, label);

            auto lab = relName + labelToValue[key].getRepresentation();
            auto ruleNum = split(internalRelName, '_').back();
            auto inner = std::unique_ptr<inner_node>(new inner_node(lab, std::string("(R" + ruleNum + ")")));

            for (size_t i = 0; i < info[internalRelName + "_info"].size(); i++) {
                auto rel = info[internalRelName + "_info"][i];
                auto newLab = labelToProof[subProofKey][i];
                inner->add_child(explain(rel, newLab, depth - 1));
            }
            return std::move(inner);
        } else {
            std::string lab = "subproof " + relName + "(" + std::to_string(label) + ")";
            return std::unique_ptr<tree_node>(new leaf_node(lab));
        }
    }
}

std::unique_ptr<tree_node> explain(std::string relName, elements tuple_elements) {
    auto key = std::make_pair(relName + "_output", tuple_elements);
    if (valuesToLabel.find(key) == valuesToLabel.end()) {
        // std::cerr << "no tuple found " << relName << tuple_elements.getRepresentation() << std::endl;
        wprintw(stdscr, "no tuple found %s%s\n", relName.c_str(), tuple_elements.getRepresentation().c_str());

        return nullptr;
    }

    return std::move(explain(relName, valuesToLabel[key], depthLimit));
}

void printTree(std::unique_ptr<tree_node> t) {
    refresh();
    if (t) {
        t->place(0, 0);
        screen_buffer *s = new screen_buffer(t->getWidth(), t->getHeight());
        t->render(*s);
        // s->print(std::cout);
        wprintw(stdscr, s->getString());
        // std::cout << s->getString() << std::endl;
    }
}

std::pair<std::string, std::vector<std::string>> parseRel(std::string rel) {
    // remove spaces
    rel.erase(std::remove(rel.begin(), rel.end(), ' '), rel.end());

    // remove last closing parenthesis
    if (rel.back() == ')') {
        rel.pop_back();
    }

    auto splitRel = split(rel, '(');
    std::string relName = splitRel[0];
    std::vector<std::string> args = split(splitRel[1], ',');

    return std::make_pair(relName, args);
}

WINDOW *makeQueryWindow() {
    int y, x;
    getmaxyx(stdscr, y, x);

    WINDOW *w = newwin(3, x, y - 2, 0);

    wrefresh(w);

    return w;
}

void commandLine(SouffleProgram *prog) {

    // Create ncurses window
    initscr();

    // Create new section at bottom of screen for query
    WINDOW *queryWindow = makeQueryWindow();

    char buf[100];
    std::string line;
    while (1) {
        werase(stdscr);
        werase(queryWindow);
        wrefresh(queryWindow);
        mvwprintw(queryWindow, 1, 0, "Enter command > ");
        echo();
        wgetnstr(queryWindow, buf, 100);
        noecho();
        line = buf;

        // std::cout << "Enter command > ";
        // std::getline(std::cin, line);
        std::vector<std::string> command = split(line, ' ', 1);

        if (command[0] == "setdepth") {
            depthLimit = atoi(command[1].c_str());
            wprintw(stdscr, "Depth is now %d\n", depthLimit);
            // std::cout << "Depth is now " << depthLimit << std::endl;
        } else if (command[0] == "explain") {
            auto query = parseRel(command[1]);
            elements tuple_elements(query.second);
            std::unique_ptr<tree_node> t = explain(query.first, tuple_elements);
            printTree(std::move(t));
        } else if (command[0] == "subproof") {
            auto query = parseRel(command[1]);
            std::unique_ptr<tree_node> t = explain(query.first, atoi(query.second[0].c_str()), depthLimit);
            printTree(std::move(t));
        } else if (command[0] == "rule") {
            auto query = split(command[1], ' ');
            auto key = make_pair(query[0], atoi(query[1].c_str()));
            if (rule.find(key) != rule.end()) {
                wprintw(stdscr, "%s\n", rule[key].c_str());
            } else {
                wprintw(stdscr, "no rule found");
            }
        } else if (command[0] == "exit") {
            break;
        }
        getch();
    }
    endwin();
}

int main(int argc, char **argv) {
    if (prog = ProgramFactory::newInstance("path")) {
        prog->loadAll(argv[1]);
        prog->run();

        load();

        // for (auto p : labelToValue) {
        //     std::cout << p.first.first << " " << p.first.second << p.second.getRepresentation() << std::endl;
        // }


        // for (auto p : valuesToLabel) {
        //     std::cout << p.first.first << p.second << std::endl;
        // }

        // prog->printAll();
        //
        commandLine(prog);
    } else {
        std::cout << "no program" << std::endl;
    }
}
