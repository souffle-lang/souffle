#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <cstdlib>

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
            return std::string("");
        }

        // maintain iterators for integers and strings
        std::vector<plabel>::iterator i_itr;
        std::vector<std::string>::iterator s_itr;
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
        

    bool operator==(const elements e) const {
        return e.order == order && e.integers == integers && e.strings == strings;
    }

    bool operator<(const elements e) const {
        return order.size() < e.order.size();
    }
};

SouffleProgram *prog;

std::map<std::pair<std::string, elements>, plabel> values;
std::map<std::pair<std::string, plabel>, elements> labels;
std::map<std::pair<std::string, plabel>, std::vector<plabel>> rules;
std::map<std::string, std::vector<std::string>> info;

int depthLimit = 4;

void load() {
    for (Relation *rel : prog->getAllRelations()) {
        if (rel->getName().find("_output") != std::string::npos) {
            for (auto &tuple : *rel) {
                plabel label;
                elements tuple_elements;

                tuple >> label;

                for (size_t i = 0; i < tuple.size(); i++) {
                    if (*(rel->getAttrType(i)) == 'i' || *(rel->getAttrType(i)) == 'r') {
                        std::cout << rel->getName() << " " << rel->getAttrType(i) << std::endl;
                        plabel n;
                        tuple >> n;
                        std::cout << "ASKJFHDLJKRHESR" << std::endl;
                        tuple_elements.insert(n);
                    } else if (*(rel->getAttrType(i)) == 's') {
                        std::string s;
                        tuple >> s;
                        tuple_elements.insert(s);
                    }
                }

                // values[std::make_pair(rel->getName(), e)] = l;
                values.insert({std::make_pair(rel->getName(), tuple_elements), label});
                labels.insert({std::make_pair(rel->getName(), label), tuple_elements});
            }
        } else if (rel->getName().find("_new_") != std::string::npos) {
            for (auto &tuple : *rel) {
                plabel label;
                std::vector<plabel> refs;

                tuple >> label;

                for (size_t i = 0; i < tuple.size(); i++) {
                    plabel l;
                    tuple >> l;
                    refs.push_back(l);
                }

                rules.insert({std::make_pair(rel->getName(), label), refs});
            }
        } else if (rel->getName().find("_info") != std::string::npos) {
            for (auto &tuple : *rel) {
                std::vector<std::string> rels;
                for (size_t i = 0; i < tuple.size(); i++) {
                    std::string s;
                    tuple >> s;
                    rels.push_back(s);
                }

                info.insert({rel->getName(), rels});
            }
        }
    }
}

std::unique_ptr<tree_node> explain(std::string relName, plabel label, int depth) {
    if (prog->getRelation(relName)->isInput()) {
        auto key = std::make_pair(relName + "_output", label);
        std::string lab = relName + labels[key].getRepresentation();
        std::unique_ptr<tree_node> leaf(new leaf_node(lab));
        return leaf;
    } else {
        std::string internalRelName;
        // find correct relation
        for (auto rel : prog->getAllRelations()) {
            if (rel->getName().find(relName + "_info_") != std::string::npos) {
                if (labels.find(std::make_pair(rel->getName(), label)) != labels.end()) {
                    // found the correct relation
                    internalRelName = rel->getName();
                    break;
                }
            }
        }

        auto key = std::make_pair(internalRelName, label);
        std::string lab = relName + labels[key].getRepresentation();
        std::unique_ptr<inner_node> inner(new inner_node(lab, std::string("")));

        for (size_t i = 0; i < info[internalRelName].size(); i++) {
            auto rel = info[internalRelName][i];
            auto label = rules[key][i];
            inner->add_child(explain(rel, label, depth - 1));
        }
    }
}

std::unique_ptr<tree_node> explain(std::string relName, elements tuple_elements) {
    auto key = std::make_pair(relName + "_output", tuple_elements);
    if (values.find(key) == values.end()) {
        std::cerr << "no tuple found " << relName << tuple_elements.getRepresentation() << std::endl;
    }

    return explain(relName, values[key], depthLimit);
}


void command(SouffleProgram *prog) {
    auto split = [](std::string s, char delim)->std::vector<std::string> {
        std::vector<std::string> v;
        std::stringstream ss(s);
        std::string item;

        while (std::getline(ss, item, delim)) {
            v.push_back(item);
        }

        return v;
    };


    std::string line;
    while (1) {
        std::cout << "Enter command > ";
        std::getline(std::cin, line);
        std::vector<std::string> command = split(line, ' ');

        if (command[0] == "setdepth") {
            depthLimit = atoi(command[1].c_str());
            std::cout << "Depth is now " << depthLimit << std::endl;
        } else if (command[0] == "explain") {
            elements tuple_elements(std::vector<std::string>(command.begin() + 2, command.end()));
            explain(command[1], tuple_elements);
        } else if (command[0] == "subproof") {
        } else if (command[0] == "exit") {
            break;
        }
    }
}

int main(int argc, char **argv) {
    if (prog = ProgramFactory::newInstance("path")) {
        prog->loadAll(argv[1]);
        prog->run();

        load(); 

        // prog->printAll();
        //
        command(prog);
    } else {
        std::cout << "no program" << std::endl;
    }
}
