/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file GraphUtils.h
 *
 * A simple utility graph for conducting simple, graph-based operations.
 *
 ***********************************************************************/

#pragma once

#include "souffle/utility/FileUtil.h"
#include <functional>
#include <map>
#include <ostream>
#include <set>
#include <utility>
#include <vector>

namespace souffle {

/**
 * A simple graph structure for graph-based operations.
 */
template <typename Vertex, typename Compare = std::less<Vertex>>
class Graph {
public:
    /**
     * Adds a new edge from the given vertex to the target vertex.
     */
    void insert(const Vertex& from, const Vertex& to) {
        insert(from);
        insert(to);
        _successors[from].insert(to);
        _predecessors[to].insert(from);
    }

    /**
     * Adds a vertex.
     */
    void insert(const Vertex& vertex) {
        auto iter = _vertices.insert(vertex);
        if (iter.second) {
            _successors.insert(std::make_pair(vertex, std::set<Vertex, Compare>()));
            _predecessors.insert(std::make_pair(vertex, std::set<Vertex, Compare>()));
        }
    }

    /** Obtains a reference to the set of all vertices */
    const std::set<Vertex, Compare>& vertices() const {
        return _vertices;
    }

    /** Returns the set of vertices the given vertex has edges to */
    const std::set<Vertex, Compare>& successors(const Vertex& from) const {
        return _successors.at(from);
    }

    /** Returns the set of vertices the given vertex has edges from */
    const std::set<Vertex, Compare>& predecessors(const Vertex& to) const {
        return _predecessors.at(to);
    }

    /** Determines whether the given vertex is present */
    bool contains(const Vertex& vertex) const {
        return _vertices.find(vertex) != _vertices.end();
    }

    /** Determines whether the given edge is present */
    bool contains(const Vertex& from, const Vertex& to) const {
        auto pos = _successors.find(from);
        if (pos == _successors.end()) {
            return false;
        }
        auto p2 = pos->second.find(to);
        return p2 != pos->second.end();
    }

    /** Determines whether there is a directed path between the two vertices */
    bool reaches(const Vertex& from, const Vertex& to) const {
        // quick check
        if (!contains(from) || !contains(to)) {
            return false;
        }

        // conduct a depth-first search starting at from
        bool found = false;
        bool first = true;
        visitDepthFirst(from, [&](const Vertex& cur) {
            found = !first && (found || cur == to);
            first = false;
        });
        return found;
    }

    /** Obtains the set of all vertices in the same clique than the given vertex */
    const std::set<Vertex, Compare> clique(const Vertex& vertex) const {
        std::set<Vertex, Compare> res;
        res.insert(vertex);
        for (const auto& cur : vertices()) {
            if (reaches(vertex, cur) && reaches(cur, vertex)) {
                res.insert(cur);
            }
        }
        return res;
    }

    /** A generic utility for depth-first visits */
    template <typename Lambda>
    void visitDepthFirst(const Vertex& vertex, const Lambda& lambda) const {
        std::set<Vertex, Compare> visited;
        visitDepthFirst(vertex, lambda, visited);
    }

    /** Enables graphs to be printed (e.g. for debugging) */
    void print(std::ostream& out) const {
        bool first = true;
        out << "{";
        for (const auto& cur : _successors) {
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
    // not a very efficient but simple graph representation
    std::set<Vertex, Compare> _vertices;                        // all the vertices in the graph
    std::map<Vertex, std::set<Vertex, Compare>> _successors;    // all edges forward directed
    std::map<Vertex, std::set<Vertex, Compare>> _predecessors;  // all edges backward

    /** The internal implementation of depth-first visits */
    template <typename Lambda>
    void visitDepthFirst(
            const Vertex& vertex, const Lambda& lambda, std::set<Vertex, Compare>& visited) const {
        lambda(vertex);
        auto pos = _successors.find(vertex);
        if (pos == _successors.end()) {
            return;
        }
        for (const auto& cur : pos->second) {
            if (visited.insert(cur).second) {
                visitDepthFirst(cur, lambda, visited);
            }
        }
    }
};

inline std::string toBase64(const std::string& data) {
    static const std::vector<char> table = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
            'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
            'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
            'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};
    std::string result;
    std::string tmp = data;
    unsigned int padding = 0;
    if (data.size() % 3 == 2) {
        padding = 1;
    } else if (data.size() % 3 == 1) {
        padding = 2;
    }

    for (unsigned int i = 0; i < padding; i++) {
        tmp.push_back(0);
    }
    for (unsigned int i = 0; i < tmp.size(); i += 3) {
        auto c1 = static_cast<unsigned char>(tmp[i]);
        auto c2 = static_cast<unsigned char>(tmp[i + 1]);
        auto c3 = static_cast<unsigned char>(tmp[i + 2]);
        unsigned char index1 = c1 >> 2;
        unsigned char index2 = ((c1 & 0x03) << 4) | (c2 >> 4);
        unsigned char index3 = ((c2 & 0x0F) << 2) | (c3 >> 6);
        unsigned char index4 = c3 & 0x3F;

        result.push_back(table[index1]);
        result.push_back(table[index2]);
        result.push_back(table[index3]);
        result.push_back(table[index4]);
    }
    if (padding == 1) {
        result[result.size() - 1] = '=';
    } else if (padding == 2) {
        result[result.size() - 1] = '=';
        result[result.size() - 2] = '=';
    }
    return result;
}

inline std::string convertDotToSVG(const std::string& dotSpec) {
    // Check if dot is present
    std::string cmd = which("dot");
    if (!isExecutable(cmd)) {
        return "";
    }

    TempFileStream dotFile;
    dotFile << dotSpec;
    dotFile.flush();
    return execStdOut("dot -Tsvg < " + dotFile.getFileName()).str();
}

inline void printHTMLGraph(std::ostream& out, const std::string& dotSpec, const std::string& id) {
    std::string data = convertDotToSVG(dotSpec);

    if (data.find("<svg") != std::string::npos) {
        out << "<img alt='graph image' src='data:image/svg+xml;base64," << toBase64(data) << "'><br/>\n";
    } else {
        out << "<div class='" << id << "-source"
            << "'>\n<pre>" << dotSpec << "</pre>\n";
        out << "</div>\n";
    }
}

}  // end of namespace souffle
