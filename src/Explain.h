/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Explain.h
 *
 * Provenance interface for Souffle; works for compiler and interpreter
 *
 ***********************************************************************/

#pragma once

#include <csignal>
#include <iostream>
#include <regex>
#include <string>
#include <ncurses.h>

#include "SouffleInterface.h"

#define MAX_TREE_HEIGHT 500
#define MAX_TREE_WIDTH 500

namespace souffle {
typedef RamDomain plabel;

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
}

class ScreenBuffer {
private:
    uint32_t width;   // width of the screen buffer
    uint32_t height;  // height of the screen buffer
    char* buffer;     // screen contents

public:
    // constructor
    ScreenBuffer(uint32_t w, uint32_t h) : width(w), height(h), buffer(nullptr) {
        assert(width > 0 && height > 0 && "wrong dimensions");
        buffer = new char[width * height];
        memset(buffer, ' ', width * height);
    }

    ~ScreenBuffer() {
        delete[] buffer;
    }

    // write into screen buffer at a specific location
    void write(uint32_t x, uint32_t y, const std::string& s) {
        assert(x >= 0 && x < width && "wrong x dimension");
        assert(y >= 0 && y < height && "wrong y dimension");
        assert(x + s.length() <= width && "string too long");
        for (size_t i = 0; i < s.length(); i++) {
            buffer[y * width + x + i] = s[i];
        }
    }

    std::string getString() {
        std::stringstream ss;
        print(ss);
        return ss.str();
    }

    // print screen buffer
    void print(std::ostream& os) {
        if (height > 0 && width > 0) {
            for (int i = height - 1; i >= 0; i--) {
                for (size_t j = 0; j < width; j++) {
                    os << buffer[width * i + j];
                }
                os << std::endl;
            }
        }
    }
};

/***
 * Abstract Class for a Proof Tree Node
 *
 */
class TreeNode {
protected:
    std::string txt;  // text of tree node
    uint32_t width;   // width of node (including sub-trees)
    uint32_t height;  // height of node (including sub-trees)
    int xpos;         // x-position of text
    int ypos;         // y-position of text

public:
    TreeNode(const std::string& t = "") : txt(t), width(0), height(0), xpos(0), ypos(0) {}
    virtual ~TreeNode() {}

    // get width
    uint32_t getWidth() const {
        return width;
    }

    // get height
    uint32_t getHeight() const {
        return height;
    }

    // place the node
    virtual void place(uint32_t xpos, uint32_t ypos) = 0;

    // render node in screen buffer
    virtual void render(ScreenBuffer& s) = 0;
};

/***
 * Concrete class
 */
class InnerNode : public TreeNode {
private:
    std::vector<std::unique_ptr<TreeNode>> children;
    std::string label;

public:
    InnerNode(const std::string& t = "", const std::string& l = "") : TreeNode(t), label(l) {}

    // add child to node
    void add_child(std::unique_ptr<TreeNode> child) {
        children.push_back(std::move(child));
    }

    // place node and its sub-trees
    void place(uint32_t x, uint32_t y) {
        // there must exist at least one kid
        assert(children.size() > 0 && "no children");

        // set x/y pos
        xpos = x;
        ypos = y;

        height = 0;

        // compute size of bounding box
        for (const std::unique_ptr<TreeNode>& k : children) {
            k->place(x, y + 2);
            x += k->getWidth() + 1;
            width += k->getWidth() + 1;
            height = std::max(height, k->getHeight());
        }
        width += label.length();
        height += 2;

        // text of inner node is longer than all its sub-trees
        if (width < txt.length()) {
            width = txt.length();
        }
    };

    // render node text and separator line
    void render(ScreenBuffer& s) {
        s.write(xpos + (width - txt.length()) / 2, ypos, txt);
        for (const std::unique_ptr<TreeNode>& k : children) {
            k->render(s);
        }
        std::string separator(width - label.length(), '-');
        separator += label;
        s.write(xpos, ypos + 1, separator);
    }
};

/***
 * Concrete class for leafs
 */
class leaf_node : public TreeNode {
public:
    leaf_node(const std::string& t = "") : TreeNode(t) {}

    // place leaf node
    void place(uint32_t x, uint32_t y) {
        xpos = x;
        ypos = y;
        width = txt.length();
        height = 1;
    }

    // render text of leaf node
    void render(ScreenBuffer& s) {
        s.write(xpos, ypos, txt);
    }
};

// store elements of a tuple
struct elements {
    // order of strings or ints in the tuple
    std::string order;
    std::vector<plabel> integers;
    std::vector<std::string> strings;

    elements() : order(std::string()), integers(std::vector<plabel>()), strings(std::vector<std::string>()) {}

    elements(std::vector<std::string> tupleElements) {
        // check if string is integer
        auto isInteger = [](const std::string& s) {
            if (s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) return false;

            char* p;
            strtol(s.c_str(), &p, 10);

            return (*p == 0);
        };

        // insert each element
        for (auto s : tupleElements) {
            if (isInteger(s)) {
                insert(atoi(s.c_str()));
            } else {
                insert(s);
            }
        }
    }

    // insert a string element
    void insert(std::string s) {
        order.push_back('s');
        strings.push_back(s);
    }

    // insert a number element
    void insert(plabel i) {
        order.push_back('i');
        integers.push_back(i);
    }

    // print the elements of the tuple
    void print(std::ostream& os) {
        // construct vector of strings storing elements
        std::vector<std::string> elems;
        std::vector<plabel>::iterator i_itr = integers.begin();
        std::vector<std::string>::iterator s_itr = strings.begin();
        for (char& c : order) {
            if (c == 'i') {
                elems.push_back(std::to_string(*(i_itr++)));
            } else {
                elems.push_back(*(s_itr++));
            }
        }

        os << "(" << join(elems, ", ") << ")";
    }

    // return a string representation of tuple
    std::string getRepresentation() {
        std::stringstream repr;
        print(repr);
        return repr.str();
    }

    // comparisons to use as keys for map
    bool operator==(const elements& e) const {
        return e.order == order && e.integers == integers && e.strings == strings;
    }

    bool operator!=(const elements& e) const {
        return !(this->operator==(e));
    }

    bool operator<(const elements& e) const {
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
const elements defaultElement = elements(std::vector<std::string>({"DEFAULT"}));

// store provenance information as maps
class ProvenanceInfo {
private:
    SouffleProgram& prog;

    std::map<std::pair<std::string, elements>, plabel> valuesToLabel;
    std::map<std::pair<std::string, plabel>, elements> labelToValue;
    std::map<std::pair<std::string, plabel>, std::vector<plabel>> labelToProof;
    std::map<std::string, std::vector<std::string>> info;
    std::map<std::pair<std::string, int>, std::string> rule;

    void load() {
        for (Relation* rel : prog.getAllRelations()) {
            std::string relName = rel->getName();

            for (auto& tuple : *rel) {
                plabel label;

                if (relName.find("-output") != std::string::npos) {
                    elements tuple_elements;
                    tuple >> label;

                    // construct tuple elements
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

                    // insert into maps
                    valuesToLabel.insert({std::make_pair(rel->getName(), tuple_elements), label});
                    labelToValue.insert({std::make_pair(rel->getName(), label), tuple_elements});
                } else if (relName.find("-provenance-") != std::string::npos) {
                    std::vector<plabel> refs;
                    tuple >> label;

                    // construct vector of proof references
                    for (size_t i = 1; i < tuple.size(); i++) {
                        plabel l;
                        tuple >> l;
                        refs.push_back(l);
                    }

                    labelToProof.insert({std::make_pair(rel->getName(), label), refs});
                } else if (relName.find("-info") != std::string::npos) {
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
                    int ruleNum = atoi((*(split(rel->getName(), '-').rbegin() + 1)).c_str());

                    info.insert({rel->getName(), rels});
                    rule.insert({std::make_pair(relName, ruleNum), clauseRepr});
                }
            }
        }
    }

public:
    ProvenanceInfo(SouffleProgram& p) : prog(p) {
        load();
    }

    plabel getLabel(std::string relName, elements e) {
        auto key = std::make_pair(relName, e);

        if (valuesToLabel.find(key) != valuesToLabel.end()) {
            return valuesToLabel[key];
        } else {
            return -1;
        }
    }

    elements getTuple(std::string relName, plabel l) {
        auto key = std::make_pair(relName, l);

        if (labelToValue.find(key) != labelToValue.end()) {
            return labelToValue[key];
        } else {
            return defaultElement;
        }
    }

    std::vector<plabel> getSubproofs(std::string relName, plabel l) {
        auto key = std::make_pair(relName, l);

        if (labelToProof.find(key) != labelToProof.end()) {
            return labelToProof[key];
        } else {
            return std::vector<plabel>();
        }
    }

    std::vector<std::string> getInfo(std::string relName) {
        assert(info.find(relName) != info.end());
        return info[relName];
    }

    std::string getRule(std::string relName, int ruleNum) {
        auto key = std::make_pair(relName, ruleNum);

        if (rule.find(key) != rule.end()) {
            return rule[key];
        } else {
            return std::string("no rule found");
        }
    }
};

class ProvenanceTree {
private:
    SouffleProgram& prog;
    ProvenanceInfo provInfo;
    std::unique_ptr<TreeNode> root;
    int depthLimit;

    std::unique_ptr<TreeNode> explainLabel(std::string relName, plabel label, int depth) {
        bool isEDB = true;
        bool found = false;
        for (auto rel : prog.getAllRelations()) {
            std::regex provRelName(relName + "-provenance-[0-9]+");
            if (std::regex_match(rel->getName(), provRelName)) {
                isEDB = false;
                break;
            }
            if (rel->getName().find(relName) != std::string::npos) {
                found = true;
            }
        }

        // check that relation is in the program
        if (!found) {
            return std::unique_ptr<TreeNode>(new leaf_node("Relation " + relName + " not found"));
        }

        // if EDB relation, make a leaf node in the tree
        if (prog.getRelation(relName) != nullptr && isEDB) {
            std::string lab = relName + provInfo.getTuple(relName + "-output", label).getRepresentation();
            return std::unique_ptr<TreeNode>(new leaf_node(lab));
        } else {
            if (depth > 0) {
                std::string internalRelName;

                // find correct relation
                for (auto rel : prog.getAllRelations()) {
                    if (rel->getName().find(relName + "-provenance-") != std::string::npos) {
                        // if relation contains the correct tuple label
                        if (provInfo.getSubproofs(rel->getName(), label) != std::vector<plabel>()) {
                            // found the correct relation
                            internalRelName = rel->getName();
                            break;
                        }
                    }
                }

                if (internalRelName == "") {
                    return std::unique_ptr<TreeNode>(new leaf_node("Relation " + relName + " not found"));
                }

                // label and rule number for node
                auto lab = relName + provInfo.getTuple(relName + "-output", label).getRepresentation();
                auto ruleNum = split(internalRelName, '-').back();

                // internal node representing current value
                auto inner =
                        std::unique_ptr<InnerNode>(new InnerNode(lab, std::string("(R" + ruleNum + ")")));

                // key for info map
                std::string infoKey = relName + "-info-" + ruleNum;

                // recursively add all provenance values for this value
                for (size_t i = 0; i < provInfo.getInfo(infoKey).size(); i++) {
                    auto rel = provInfo.getInfo(infoKey)[i];
                    auto newLab = provInfo.getSubproofs(internalRelName, label)[i];
                    inner->add_child(explainLabel(rel, newLab, depth - 1));
                }
                return std::move(inner);

                // add subproof label if depth limit is exceeded
            } else {
                std::string lab = "subproof " + relName + "(" + std::to_string(label) + ")";
                return std::unique_ptr<TreeNode>(new leaf_node(lab));
            }
        }
    }

    std::unique_ptr<TreeNode> explain(std::string relName, elements tuple_elements) {
        auto lab = provInfo.getLabel(relName + "-output", tuple_elements);
        if (lab == -1) {
            return std::unique_ptr<TreeNode>(
                    new leaf_node("Tuple " + relName + tuple_elements.getRepresentation() + " not found"));
        }

        return explainLabel(relName, lab, depthLimit);
    }

public:
    ProvenanceTree(SouffleProgram& p, int d = 4) : prog(p), provInfo(p), depthLimit(d) {}

    std::unique_ptr<TreeNode> getTree(std::string relName, elements tuple_elements) {
        return explain(relName, tuple_elements);
    }

    std::unique_ptr<TreeNode> getTree(std::string relName, plabel label) {
        return explainLabel(relName, label, depthLimit);
    }

    std::string getRule(std::string relName, int ruleNum) {
        return provInfo.getRule(relName, ruleNum);
    }

    void setDepth(int d) {
        if (d > 0) {
            depthLimit = d;
        }
    }
};

class ProvenanceDisplay {
private:
    SouffleProgram& prog;

    bool ncurses;
    WINDOW* treePad;
    WINDOW* queryWindow;
    int maxx, maxy;

    int depthLimit;

    // parse relation, split into relation name and values
    std::pair<std::string, std::vector<std::string>> parseRel(std::string rel) {
        // remove spaces
        rel.erase(std::remove(rel.begin(), rel.end(), ' '), rel.end());

        // remove last closing parenthesis
        if (rel.back() == ')') {
            rel.pop_back();
        }

        auto splitRel = split(rel, '(');
        std::string relName;
        if (splitRel.size() > 0) {
            relName = splitRel[0];
        }
        std::vector<std::string> args;

        if (splitRel.size() > 1) {
            args = split(splitRel[1], ',');
        }

        return std::make_pair(relName, args);
    }

    // initialise ncurses window
    WINDOW* makeQueryWindow() {
        WINDOW* w = newwin(3, maxx, maxy - 2, 0);
        wrefresh(w);
        return w;
    }

    // print provenance tree
    void printTree(std::unique_ptr<TreeNode> t) {
        if (t) {
            t->place(0, 0);
            ScreenBuffer* s = new ScreenBuffer(t->getWidth(), t->getHeight());
            t->render(*s);
            if (ncurses) {
                wprintw(treePad, s->getString().c_str());
            } else {
                std::cout << s->getString();
            }
        }
    }

    // print string
    void printStr(std::string s) {
        if (ncurses) {
            wprintw(treePad, s.c_str());
        } else {
            std::cout << s;
        }
    }

    // allow scrolling of provenance tree
    void scrollTree(int maxx, int maxy) {
        int x = 0;
        int y = 0;

        while (1) {
            int ch = wgetch(treePad);

            if (ch == KEY_LEFT) {
                if (x > 2) x -= 3;
            } else if (ch == KEY_RIGHT) {
                if (x < MAX_TREE_WIDTH - 3) x += 3;
            } else if (ch == KEY_UP) {
                if (y > 0) y -= 1;
            } else if (ch == KEY_DOWN) {
                if (y < MAX_TREE_HEIGHT - 1) y += 1;
            } else {
                break;
            }

            prefresh(treePad, y, x, 0, 0, maxy - 3, maxx - 1);
        }
    }

    // initialise ncurses window
    void initialiseWindow() {
        initscr();

        // get size of window
        getmaxyx(stdscr, maxy, maxx);

        // create windows for query and tree display
        queryWindow = makeQueryWindow();
        treePad = newpad(MAX_TREE_HEIGHT, MAX_TREE_WIDTH);

        keypad(treePad, true);
    }

public:
    ProvenanceDisplay(SouffleProgram& p, bool ncurses, int d = 4)
            : prog(p), ncurses(ncurses), depthLimit(d) {}

    void explain() {
        if (ncurses) {
            initialiseWindow();
            std::signal(SIGWINCH, NULL);
        }

        // process commands
        char buf[100];
        std::string line;

        ProvenanceTree provTree(prog, depthLimit);

        while (1) {
            if (ncurses) {
                // reset command line on each loop
                werase(queryWindow);
                wrefresh(queryWindow);
                mvwprintw(queryWindow, 1, 0, "Enter command > ");
                curs_set(1);
                echo();

                // get next command
                wgetnstr(queryWindow, buf, 100);
                noecho();
                curs_set(0);
                line = buf;

                // reset tree display on each loop
                werase(treePad);
                prefresh(treePad, 0, 0, 0, 0, maxy - 3, maxx - 1);
            } else {
                std::cout << "Enter command > ";
                getline(std::cin, line);
            }

            std::vector<std::string> command = split(line, ' ', 1);

            if (command[0] == "setdepth") {
                try {
                    depthLimit = atoi(command[1].c_str());
                } catch (std::exception& e) {
                    printStr("Usage: setdepth <depth>\n");
                    continue;
                }
                printStr("Depth is now " + std::to_string(depthLimit) + "\n");
            } else if (command[0] == "explain") {
                std::pair<std::string, std::vector<std::string>> query;
                if (command.size() > 1) {
                    query = parseRel(command[1]);
                } else {
                    printStr("Usage: explain relation_name(<element1>, <element2>, ...)\n");
                    continue;
                }
                elements tuple_elements(query.second);
                std::unique_ptr<TreeNode> t = provTree.getTree(query.first, tuple_elements);
                printTree(std::move(t));
            } else if (command[0] == "subproof") {
                std::pair<std::string, std::vector<std::string>> query;
                int label = -1;
                if (command.size() > 1) {
                    query = parseRel(command[1]);
                    label = atoi(query.second[0].c_str());
                } else {
                    printStr("Usage: subproof relation_name(<label>)\n");
                    continue;
                }
                std::unique_ptr<TreeNode> t = provTree.getTree(query.first, label);
                printTree(std::move(t));
            } else if (command[0] == "rule") {
                try {
                    auto query = split(command[1], ' ');
                    printStr(provTree.getRule(query[0], atoi(query[1].c_str())));
                } catch (std::exception& e) {
                    printStr("Usage: rule <rule number>\n");
                    continue;
                }
            } else if (command[0] == "exit") {
                break;
            }

            // refresh treePad and allow scrolling
            if (ncurses) {
                prefresh(treePad, 0, 0, 0, 0, maxy - 3, maxx - 1);
                scrollTree(maxx, maxy);
            }
        }
        if (ncurses) {
            endwin();
        }
    }
};

inline void explain(SouffleProgram& prog, bool ncurses = true) {
    std::cout << "Explain is invoked.\n";

    ProvenanceDisplay prov(prog, ncurses);
    prov.explain();
}
}
