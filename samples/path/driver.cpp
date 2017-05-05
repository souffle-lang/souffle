#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <array>
#include <map>

#include "souffle/SouffleInterface.h"
#include "render_tree.h"

using namespace souffle;

int depth = 0;

typedef RamDomain plabel; 

std::map<plabel, std::pair<std::string, std::string>> path;
std::map<plabel, std::pair<std::string, std::string>> edge;
std::map<plabel, plabel> path_rule_0;
std::map<plabel, std::pair<plabel, plabel>> path_rule_1;

std::unique_ptr<tree_node> explain(plabel l, int depth) {
    if (path_rule_0.find(l) != path_rule_0.end()){
        plabel x = path_rule_0[l];
        auto edg = edge[x];
        std::string label = "edge(" + edg.first + "," + edg.second + ")";
        
        std::unique_ptr<tree_node> leaf(new leaf_node(label));
        return leaf;
    } else if (path_rule_1.find(l) != path_rule_1.end() && depth > 0){
        auto xy = path_rule_1[l];
        plabel x = xy.first;
        plabel y = xy.second;

        auto pat = path[l];
        std::string label = "path(" + pat.first + "," + pat.second + ")";
        std::unique_ptr<inner_node> inner(new inner_node(label, std::string("(R1)")));
        inner->add_child(explain(x,depth-1));
        inner->add_child(explain(y,depth-1));
        return std::move(inner);
    } else if (path_rule_1.find(l) != path_rule_1.end() && depth == 0){
        std::string label = "proof(" + std::to_string(l) + ")";
        std::unique_ptr<tree_node> leaf(new leaf_node(label));
        return leaf; 
    } else {
        std::cerr << "inconsistent data-structure\n";
        exit(1);
    }
}

int limit = 4;

void explain(std::string a, std::string b) {
    for (auto p : path) {
        plabel l = p.first;
        auto xy = p.second; 
        if (xy.first == a && xy.second == b) { 
            auto root = explain(l,limit);
            root->place(0, 0);
            screen_buffer *s = new screen_buffer(root->getWidth(), root->getHeight());
            root->render(*s);
            s->print(std::cout);
            delete s;
            break;
        }
    }
}

void load(SouffleProgram *prog) {
    if (Relation *prel = prog->getRelation("edge_output")) { 
        for (auto &tuple : *prel) {
            plabel l; 
            std::string x,y;
            tuple >> l >> x >> y;
            edge[l] = std::make_pair(x,y); 
        }
    } else {
        std::cerr << "Cannot open edge_output\n";
        exit(1);
    }
    if (Relation *prel = prog->getRelation("path_output")) { 
        for (auto &tuple : *prel) {
            plabel l; 
            std::string x,y;
            tuple >> l >> x >> y;
            path[l] = std::make_pair(x,y); 
        }
    } else {
        std::cerr << "Cannot open path_output\n";
        exit(1);
    }
    if (Relation *prel = prog->getRelation("path_new_0")) { 
        for (auto &tuple : *prel) {
            plabel l; 
            plabel x;
            tuple >> l >> x;
            path_rule_0[l] = x; 
        }
    } else {
        std::cerr << "Cannot open path_new_0\n";
        exit(1);
    }
    if (Relation *prel = prog->getRelation("path_new_1")) { 
        for (auto &tuple : *prel) {
            plabel l; 
            plabel x, y;
            tuple >> l >> x >> y;
            path_rule_1[l] = std::make_pair(x, y); 
        }
    } else {
        std::cerr << "Cannot open path_new_1\n";
        exit(1);
    }
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
            limit = atoi(command[1].c_str());
            std::cout << "Depth is now " << limit << std::endl;
        } else if (command[0] == "explain") {
            explain(command[1], command[2]);
        } else if (command[0] == "subproof") {
            std::pair<std::string, std::string> p = path[atoi(command[1].c_str())];
            explain(p.first, p.second);
        } else if (command[0] == "exit") {
            break;
        }
    }
}

int main(int argc, char **argv) {
    if (SouffleProgram *prog = ProgramFactory::newInstance("path")) {
        prog->loadAll(argv[1]);
        prog->run();

        load(prog); 

        // prog->printAll();

        command(prog);
    } else {
        std::cout << "no program path" << std::endl;
    }
}

