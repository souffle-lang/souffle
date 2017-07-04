/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
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

#include <iostream>
#include <ncurses.h>

#include "SouffleInterface.h"

#define MAX_TREE_HEIGHT 500
#define MAX_TREE_WIDTH 500


namespace souffle {
typedef RamDomain plabel;
class screen_buffer {
private:
    int width;          // width of the screen buffer
    int height;         // height of the screen buffer
    char *buffer; // screen contents

public:
    // constructor
    screen_buffer(int w, int h) : width(w), height(h), buffer(nullptr) { 
        assert(width > 0 && height > 0 && "wrong dimensions"); 
        buffer = new char[width * height]; 
        memset(buffer, ' ', width * height);
    } 

    ~screen_buffer() { 
        delete [] buffer;
    }

    // write into screen buffer at a specific location
    void write(int x, int y, const std::string &s) {
        assert(x >= 0 && x < width && "wrong x dimension");
        assert(y >= 0 && y < height && "wrong y dimension"); 
        assert(x + s.length() <= width && "string too long"); 
        for(int i = 0;i < s.length(); i++) { 
            buffer[y * width + x + i ] = s[i]; 
        }
    }

    std::string getString() {
        std::stringstream ss;
        print(ss);
        return ss.str();
    }

    // print screen buffer
    void print(std::ostream &os) { 
        if (height > 0 && width > 0) {
            for(int i=height-1;i>=0;i--) { 
                for(int j=0;j<width;j++) {
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
class tree_node {
protected:
    std::string txt; // text of tree node
    int width;       // width of node (including sub-trees)
    int height;      // height of node (including sub-trees)
    int xpos;        // x-position of text 
    int ypos;        // y-position of text 

public:
    tree_node(const std::string &t="") : txt(t), width(0), height(0), xpos(0), ypos(0) { 
    }
    virtual ~tree_node() { }

    // get width
    int getWidth() {
        return width;
    }

    // get height
    int getHeight() {
        return height;
    } 

    // place the node
    virtual void place(int xpos, int ypos) = 0;

    // render node in screen buffer
    virtual void render(screen_buffer &s) = 0; 
};

/***
 * Concrete class
 */ 
class inner_node : public tree_node {
private: 
    std::vector<std::unique_ptr<tree_node>> children;
    std::string label;

public:
    inner_node(const std::string &t="", const std::string &l="") : tree_node(t), label(l) {
    }

    // add child to node
    void add_child(std::unique_ptr<tree_node> child) { 
        children.push_back(std::move(child));
    }

    // place node and its sub-trees
    void place(int x, int y) {
        // there must exist at least one kid 
        assert(children.size() > 0 && "no children"); 

        // set x/y pos
        xpos = x;
        ypos = y; 

        height = 0;
        // compute size of bounding box 
        //
        for(const std::unique_ptr<tree_node> &k: children) { 
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
    void render(screen_buffer &s) {
        s.write(xpos+(width - txt.length())/2, ypos, txt); 
        for(const std::unique_ptr<tree_node> &k: children) { 
            k->render(s);
        }
        std::string separator(width - label.length(),'-'); 
        separator += label;
        s.write(xpos,ypos+1,separator); 
    } 
};

/***
 * Concrete class for leafs 
 */ 

class leaf_node : public tree_node {
public:
    leaf_node(const std::string &t="") : tree_node(t) {
    }

    // place leaf node
    void place(int x, int y) { 
        xpos = x; 
        ypos = y; 
        width = txt.length(); 
        height = 1;  
    } 

    // render text of leaf node 
    void render(screen_buffer &s) {
        s.write(xpos,ypos,txt);  
    }
};

class Provenance {
private:
    typedef RamDomain plabel;


    // store elements of a tuple
    struct elements {
        // order of strings or ints in the tuple
        std::string order;
        std::vector<plabel> integers;
        std::vector<std::string> strings;

        elements() : order(std::string()), integers(std::vector<plabel>()), strings(std::vector<std::string>()){}

        elements(std::vector<std::string> tupleElements) {
            // check if string is integer
            auto isInteger = [](const std::string & s) {
                if (s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+')))
                    return false;

                char *p;
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

        // return a string representation of tuple
        std::string getRepresentation() {
            // construct vector of strings storing elements
            std::vector<std::string> elems;
            std::vector<plabel>::iterator i_itr = integers.begin();
            std::vector<std::string>::iterator s_itr = strings.begin();
            for (char &c : order) {
                if (c == 'i') {
                    elems.push_back(std::to_string(*(i_itr++)));
                } else {
                    elems.push_back(*(s_itr++));
                }
            }

            // construct string representation of elements
            std::stringstream repr;
            repr << "(";
            repr << join(elems, ", ");
            repr << ")";
            return repr.str();
        }

        // comparisons to use as keys for map
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

    SouffleProgram &prog;

    std::map<std::pair<std::string, elements>, plabel> valuesToLabel;
    std::map<std::pair<std::string, plabel>, elements> labelToValue;
    std::map<std::pair<std::string, plabel>, std::vector<plabel>> labelToProof;
    std::map<std::string, std::vector<std::string>> info;
    std::map<std::pair<std::string, int>, std::string> rule;

    int depthLimit;
    WINDOW *treePad;

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

public:
    Provenance(SouffleProgram &p) : prog(p), depthLimit(4) {}

    // construct maps storing provenance information
    void load() {
        for (Relation *rel : prog.getAllRelations()) {
            // add an output relation
            if (rel->getName().find(".output") != std::string::npos) {
                for (auto &tuple : *rel) {
                    plabel label;
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
                }
                
            // add a provenance relation
            } else if (rel->getName().find(".provenance.") != std::string::npos) {
                for (auto &tuple : *rel) {
                    plabel label;
                    std::vector<plabel> refs;

                    tuple >> label;

                    // construct vector of proof references
                    for (size_t i = 1; i < tuple.size(); i++) {
                        plabel l;
                        tuple >> l;
                        refs.push_back(l);
                    }

                    labelToProof.insert({std::make_pair(rel->getName(), label), refs});
                }
            
            // add an info relation
            } else if (rel->getName().find(".info.") != std::string::npos) {
                for (auto &tuple : *rel) {
                    // vector storing relations in body of rule
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
                    int ruleNum = atoi((*(split(rel->getName(), '.').rbegin() + 1)).c_str());

                    info.insert({rel->getName(), rels});
                    rule.insert({std::make_pair(relName, ruleNum), clauseRepr}); 
                }
            }
        }
    }

    // produce explanation for value given proof label
    std::unique_ptr<tree_node> explain(std::string relName, plabel label, int depth) {
        // if EDB relation, make a leaf node in the tree
        if (prog.getRelation(relName) != nullptr && prog.getRelation(relName)->isInput()) {
            auto key = std::make_pair(relName + ".output", label);
            std::string lab = relName + labelToValue[key].getRepresentation();
            std::unique_ptr<tree_node> leaf(new leaf_node(lab));
            return leaf;

        // if IDB relation, make internal node
        } else {
            if (depth > 0) {
                std::string internalRelName;

                // find correct relation
                for (auto rel : prog.getAllRelations()) {
                    if (rel->getName().find(relName + ".provenance.") != std::string::npos && rel->getName().find(".info") == std::string::npos) {
                        if (labelToProof.find(std::make_pair(rel->getName(), label)) != labelToProof.end()) {
                            // found the correct relation
                            internalRelName = rel->getName();
                            break;
                        }
                    }
                }

                // key for labelToValue map
                auto key = std::make_pair(relName + ".output", label);

                // key for labelToProof map
                auto subProofKey = std::make_pair(internalRelName, label);

                // label and rule number for node
                auto lab = relName + labelToValue[key].getRepresentation();
                auto ruleNum = split(internalRelName, '_').back();

                // internal node representing current value
                auto inner = std::unique_ptr<inner_node>(new inner_node(lab, std::string("(R" + ruleNum + ")")));

                // recursively add all provenance values for this value
                for (size_t i = 0; i < info[internalRelName + ".info"].size(); i++) {
                    auto rel = info[internalRelName + ".info"][i];
                    auto newLab = labelToProof[subProofKey][i];
                    inner->add_child(explain(rel, newLab, depth - 1));
                }
                return std::move(inner);

            // add subproof label if depth limit is exceeded
            } else {
                std::string lab = "subproof " + relName + "(" + std::to_string(label) + ")";
                return std::unique_ptr<tree_node>(new leaf_node(lab));
            }
        }
    }

    // produce explanation for value given elements
    std::unique_ptr<tree_node> explain(std::string relName, elements tuple_elements) {
        auto key = std::make_pair(relName + ".output", tuple_elements);
        if (valuesToLabel.find(key) == valuesToLabel.end()) {
            // std::cerr << "no tuple found " << relName << tuple_elements.getRepresentation() << std::endl;
            wprintw(treePad, "no tuple found %s%s\n", relName.c_str(), tuple_elements.getRepresentation().c_str());

            return nullptr;
        }

        return std::move(explain(relName, valuesToLabel[key], depthLimit));
    }

    // print provenance tree
    void printTree(std::unique_ptr<tree_node> t) {
        if (t) {
            t->place(0, 0);
            screen_buffer *s = new screen_buffer(t->getWidth(), t->getHeight());
            t->render(*s);
            // s->print(std::cout);
            wprintw(treePad, s->getString().c_str());
            // std::cout << s->getString() << std::endl;
        }
    }

    // parse relation, split into relation name and values
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

    // initialise ncurses window
    WINDOW *makeQueryWindow() {
        int y, x;
        getmaxyx(stdscr, y, x);

        WINDOW *w = newwin(3, x, y - 2, 0);

        wrefresh(w);

        return w;
    }

    // allow scrolling of provenance tree
    void scrollTree(int maxx, int maxy) {
        int x = 0;
        int y = 0;

        while (1) {
            int ch = wgetch(treePad);

            if (ch == KEY_LEFT) {
                if (x > 2)
                    x -= 3;
            } else if (ch == KEY_RIGHT) {
                if (x < MAX_TREE_WIDTH - 3)
                    x += 3;
            } else if (ch == KEY_UP) {
                if (y > 0)
                    y -= 1;
            } else if (ch == KEY_DOWN) {
                if (y < MAX_TREE_HEIGHT - 1)
                    y += 1;
            } else {
                break;
            }

            prefresh(treePad, y, x, 0, 0, maxy - 3, maxx - 1);
        }
    }

    // command line interface for provenance explanation
    void explain() {

        // Create ncurses window
        initscr();

        int maxx, maxy;
        getmaxyx(stdscr, maxy, maxx);

        // Create windows for query and tree display
        WINDOW *queryWindow = makeQueryWindow();
        treePad = newpad(MAX_TREE_HEIGHT, MAX_TREE_WIDTH);

        keypad(treePad, true);

        // process commands
        char buf[100];
        std::string line;
        while (1) {
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

            // std::cout << "Enter command > ";
            // std::getline(std::cin, line);
            std::vector<std::string> command = split(line, ' ', 1);

            if (command[0] == "setdepth") {
                depthLimit = atoi(command[1].c_str());
                wprintw(treePad, "Depth is now %d\n", depthLimit);
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
                    wprintw(treePad, "%s\n", rule[key].c_str());
                } else {
                    wprintw(treePad, "no rule found");
                }
            } else if (command[0] == "exit") {
                break;
            }
            prefresh(treePad, 0, 0, 0, 0, maxy - 3, maxx - 1);
            scrollTree(maxx, maxy);
            // wgetch(treePad);
        }
        endwin();
    }

};

inline void explain(SouffleProgram &prog) {
    std::cout << "Explain is invoked.\n";

    Provenance prov(prog);
    prov.load();
    prov.explain();
}

} // end of namespace souffle
