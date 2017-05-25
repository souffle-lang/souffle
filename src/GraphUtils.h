/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Graph.h
 *
 * A simple utility graph for conducting simple, graph-based operations.
 *
 ***********************************************************************/

#pragma once

#include <map>
#include <ostream>
#include <set>

namespace souffle {

/**
 * A simple graph structure for graph-based operations.
 */
template <typename Node, typename Compare = std::less<Node>>
class Graph {
    // not a very efficient but simple graph representation
    std::set<Node, Compare> nodes;                     // all the nodes in the graph
    std::map<Node, std::set<Node, Compare>> forward;   // all edges forward directed
    std::map<Node, std::set<Node, Compare>> backward;  // all edges backward

public:
    /**
     * Adds a new edge from the given node to the target node.
     */
    void addEdge(const Node& from, const Node& to) {
        addNode(from);
        addNode(to);
        forward[from].insert(to);
        backward[to].insert(from);
    }

    /**
     * Adds a node.
     */
    void addNode(const Node& node) {
        auto iter = nodes.insert(node);
        if (iter.second) {
            forward.insert(std::make_pair(node, std::set<Node, Compare>()));
            backward.insert(std::make_pair(node, std::set<Node, Compare>()));
        }
    }

    /** Obtains a reference to the set of all nodes */
    const std::set<Node, Compare>& getNodes() const {
        return nodes;
    }

    /** Returns the set of nodes the given node has edges to */
    const std::set<Node, Compare>& getEdges(const Node& from) const {
        assert(contains(from));
        return forward.find(from)->second;
    }

    /** Returns the set of nodes the given node has edges from */
    const std::set<Node, Compare>& getReverseEdges(const Node& to) const {
        assert(contains(to));
        return backward.find(to)->second;
    }

    /** Determines whether the given node is present */
    bool contains(const Node& node) const {
        return nodes.find(node) != nodes.end();
    }

    /** Determines whether the given edge is present */
    bool contains(const Node& from, const Node& to) const {
        auto pos = forward.find(from);
        if (pos == forward.end()) {
            return false;
        }
        auto p2 = pos->second.find(to);
        return p2 != pos->second.end();
    }

    /** Determines whether there is a directed path between the two nodes */
    bool reaches(const Node& from, const Node& to) const {
        // quick check
        if (!contains(from) || !contains(to)) {
            return false;
        }

        // conduct a depth-first search starting at from
        bool found = false;
        bool first = true;
        visitDepthFirst(from, [&](const Node& cur) {
            found = !first && (found || cur == to);
            first = false;
        });
        return found;
    }

    /** Obtains the set of all nodes in the same clique than the given node */
    const std::set<Node, Compare> getClique(const Node& node) const {
        std::set<Node, Compare> res;
        res.insert(node);
        for (const auto& cur : getNodes()) {
            if (reaches(node, cur) && reaches(cur, node)) {
                res.insert(cur);
            }
        }
        return res;
    }

    /** A generic utility for depth-first visits */
    template <typename Lambda>
    void visitDepthFirst(const Node& node, const Lambda& lambda) const {
        std::set<Node, Compare> visited;
        visitDepthFirst(node, lambda, visited);
    }

    /** Enables graphs to be printed (e.g. for debugging) */
    void print(std::ostream& out) const {
        bool first = true;
        out << "{";
        for (const auto& cur : forward) {
            for (const auto& trg : cur.second) {
                if (!first) {
                    out << ",";
                }
                out << cur.first << "->" << trg;
                first = false;
            }
        }
        out << "}";
    }

    friend std::ostream& operator<<(std::ostream& out, const Graph& g) {
        g.print(out);
        return out;
    }

private:
    /** The internal implementation of depth-first visits */
    template <typename Lambda>
    void visitDepthFirst(const Node& node, const Lambda& lambda, std::set<Node, Compare>& visited) const {
        lambda(node);
        auto pos = forward.find(node);
        if (pos == forward.end()) {
            return;
        }
        for (const auto& cur : pos->second) {
            if (visited.insert(cur).second) {
                visitDepthFirst(cur, lambda, visited);
            }
        }
    }
};

}  // end of namespace souffle
