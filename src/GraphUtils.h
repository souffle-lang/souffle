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
#include <stack>

#include "IndexUtils.h"

namespace souffle {

/** A generic graph class. */
template <typename Node, typename Compare = std::less<Node>>
class Graph {
protected:
    /** The set of vertices. */
    std::set<Node, Compare> nodes;

    /** The predecessor set of each vertex. */
    std::map<Node, std::set<Node, Compare>> predecessors;

    /** The successor set of each vertex. */
    std::map<Node, std::set<Node, Compare>> successors;

public:
    /** Check if a vertex is recursive, i.e. has an edge to itself. */
    const bool isRecursive(const Node& vertex) const {
        return successors.at(vertex).find(vertex) != successors.at(vertex).end();
    }

    /** Get the set of all vertices. */
    const std::set<Node, Compare>& allVertices() const {
        return nodes;
    }

    /** Get the total number of vertices. */
    const size_t vertexCount() const {
        return nodes.size();
    }

    /** Insert a new vertex into the graph. */
    virtual void insertVertex(const Node& vertex) {
        if (hasVertex(vertex)) return;
        if (nodes.insert(vertex).second) {
            successors.insert(std::make_pair(vertex, std::set<Node, Compare>()));
            predecessors.insert(std::make_pair(vertex, std::set<Node, Compare>()));
        }
    }

    /** Remove a vertex from the graph. */
    virtual void removeVertex(const Node& vertex) {
        if (!hasVertex(vertex)) return;
        for (const Node& in : predecessors.at(vertex)) removeEdge(in, vertex);
        for (const Node& out : successors.at(vertex)) removeEdge(vertex, out);
        nodes.erase(vertex);
        successors.erase(vertex);
        predecessors.erase(vertex);
    }

    /** Check if the graph has the given vertex. */
    const bool hasVertex(const Node& vertex) const {
        return nodes.find(vertex) != nodes.end();
    }

    /** Get the set of all edges. */
    const std::map<Node, std::set<Node, Compare>>& allEdges() const {
        return successors;
    }

    /** Insert a new edge into the graph (as well as new vertices if they do not already exist). */
    virtual void insertEdge(const Node& vertex1, const Node& vertex2) {
        if (hasEdge(vertex1, vertex2)) return;
        if (!hasVertex(vertex1)) insertVertex(vertex1);
        if (!hasVertex(vertex2)) insertVertex(vertex2);
        successors.at(vertex1).insert(vertex2);
        predecessors.at(vertex2).insert(vertex1);
    }

    /** Remove an edge from the graph. */
    virtual void removeEdge(const Node& vertex1, const Node& vertex2) {
        if (!hasEdge(vertex1, vertex2)) return;
        successors.at(vertex1).erase(vertex2);
        predecessors.at(vertex2).erase(vertex1);
    }

    /** Check if the graph has the given edge. */
    const bool hasEdge(const Node& vertex1, const Node& vertex2) const {
        return (hasVertex(vertex1) && hasVertex(vertex2))
                       ? successors.at(vertex1).find(vertex2) != successors.at(vertex1).end()
                       : false;
    }

    /** Check if there is a path from the first vertex to the second vertex. */
    const bool hasPath(const Node& fromVertex, const Node& toVertex) const {
        if (!hasVertex(fromVertex) || !hasVertex(toVertex)) return false;
        bool found = false;
        bool first = true;
        visitDepthFirst(fromVertex, [&](const Node& current) {
            found = !first && (found || current == toVertex);
            first = false;
        });
        return found;
    }

    /** Get the successor set (the outbound neighbours) of a vertex. */
    const std::set<Node, Compare>& getSuccessors(const Node& vertex) const {
        assert(hasVertex(vertex));
        return successors.at(vertex);
    }

    /** Insert edges from the given vertex to each in the set of vertices. */
    const void insertSuccessors(const Node& vertex, const std::set<Node, Compare>& vertices) {
        for (const auto& successor : vertices) insertEdge(vertex, successor);
    }

    /** Get the successor set (the inbound neighbours) of a vertex. */
    const std::set<Node, Compare>& getPredecessors(const Node& vertex) const {
        assert(hasVertex(vertex));
        return predecessors.at(vertex);
    }

    /** Insert edges to the given vertex from each in the set of vertices. */
    const void insertPredecessors(const Node& vertex, const std::set<Node, Compare>& vertices) {
        for (const auto& predecessor : vertices) insertEdge(predecessor, vertex);
    }

    /** Get the clique of the given vertex. */
    std::set<Node, Compare> getClique(const Node& fromVertex) const {
        assert(hasVertex(fromVertex));
        std::set<Node, Compare> clique;
        clique.insert(fromVertex);
        for (const auto& toVertex : allVertices())
            if (hasPath(fromVertex, toVertex) && hasPath(toVertex, fromVertex)) clique.insert(toVertex);
        return clique;
    }

    /** Join two vertices into one, merging their edges. */
    virtual void joinVertices(const Node& retainedVertex, const Node& removedVertex) {
        insertSuccessors(retainedVertex, getSuccessors(removedVertex));
        insertPredecessors(retainedVertex, getPredecessors(removedVertex));
        removeVertex(removedVertex);
    }

    /** Visit this graph starting at the given vertex in a depth first traversal, executing the given lambda
     * function for each newly encountered node. */
    template <typename Lambda>
    void visitDepthFirst(const Node& vertex, const Lambda lambda) const {
        std::set<Node, Compare> visited;
        visitDepthFirst(vertex, lambda, visited);
    }

    /** Calls 'print' on the given input stream. */
    friend std::ostream& operator<<(std::ostream& os, const Graph& graph) {
        graph.print(os);
        return os;
    }

    /** Prints the graph to the given output stream in Graphviz dot format. If the 'invert' argument is true,
     * edges will be drawn backward. */
    virtual void print(std::ostream& os, const bool invert = false) const {
        bool first = true;
        os << "digraph {\n";
        for (const auto& vertex : successors) {
            for (const auto& successor : vertex.second) {
                if (!first) os << ";" << std::endl;
                os << "\"" << ((!invert) ? vertex.first : successor) << "\" -> \""
                   << ((invert) ? vertex.first : successor) << "\"";
                first = false;
            }
        }
        os << std::endl << "}" << std::endl;
    }

private:
    /** Recursive component of visitDepthFirst. */
    template <typename Lambda>
    void visitDepthFirst(const Node& vertex, const Lambda lambda, std::set<Node, Compare>& visited) const {
        lambda(vertex);
        if (!hasVertex(vertex)) return;
        for (const auto& it : successors.at(vertex))
            if (visited.insert(it).second) visitDepthFirst(it, lambda, visited);
    }
};

/** A generic hyper-graph class. */
template <template <typename> class Table, typename Node>
class HyperGraph : public Graph<size_t> {
private:
    Table<Node> indexTable;

public:
    /** Get a constant reference to the table backing this hypergraph. */
    const Table<Node>& table() const {
        return this->indexTable;
    }

    /** Insert a new vertex into the graph and create an entry in the table for it. */
    void insertVertex(const size_t& vertex) {
        Graph<size_t>::insertVertex(vertex);
        this->indexTable.set(vertex);
    }

    /** Insert a new vertex into the graph and create an entry in the table for it with the given object. */
    void insertVertex(const size_t& vertex, const Node& object) {
        Graph<size_t>::insertVertex(vertex);
        this->indexTable.setIndex(object, vertex);
    }

    /** Insert a new vertex into the graph and create an entry in the table for it with the given collection
     * of objects. */
    template <template <typename...> class T>
    void insertVertex(const size_t& vertex, const T<Node>& objects) {
        Graph<size_t>::insertVertex(vertex);
        this->indexTable.set(vertex, objects);
    }

    /** Remove a vertex from the graph and its entry from the table. */
    void removeVertex(const size_t& vertex) {
        Graph<size_t>::removeVertex(vertex);
        this->indexTable.remove(vertex);
    }

    /** Append the object to the collection for the vertex in the table. */
    void appendToVertex(const size_t vertex, const Node& object) {
        if (!hasVertex(vertex)) Graph<size_t>::insertVertex(vertex);
        this->indexTable.append(vertex, object);
    }

    /** Append the objects to the collection for the vertex in the table. */
    template <template <typename...> class T>
    void appendToVertex(const size_t vertex, const T<Node>& objects) {
        if (!hasVertex(vertex)) Graph<size_t>::insertVertex(vertex);
        this->indexTable.append(vertex, objects);
    }

    /** Prepend the object to the collection for the vertex in the table. */
    void prependToVertex(const size_t vertex, const Node& object) {
        if (!hasVertex(vertex)) Graph<size_t>::insertVertex(vertex);
        this->indexTable.prepend(vertex, object);
    }

    /** Prepend the objects to the collection for the vertex in the table. */
    template <template <typename...> class T>
    void prependToVertex(const size_t vertex, const T<Node>& objects) {
        if (!hasVertex(vertex)) Graph<size_t>::insertVertex(vertex);
        this->indexTable.prepend(vertex, objects);
    }

    /** Join the vertices, merging their edges and their entries in the table. */
    void joinVertices(const size_t& retainedVertex, const size_t& removedVertex) {
        if (hasEdge(removedVertex, retainedVertex))
            this->indexTable.movePrepend(removedVertex, retainedVertex);
        else
            this->indexTable.moveAppend(removedVertex, retainedVertex);
        Graph<size_t>::joinVertices(retainedVertex, removedVertex);
    }

    /** Prints the graph to the given output stream in Graphviz dot format. If the 'invert' argument is true,
     * edges will be drawn backward. */
    virtual void print(std::ostream& os, const bool invert = false) const {
        bool first = true;
        os << "digraph {";
        for (const auto& iter : this->successors) {
            if (!first) os << ";\n";
            os << "\"" << iter.first << "\""
               << " [label=\"";
            first = true;
            for (const auto& inner : this->indexTable.get(iter.first)) {
                if (!first) os << ",";
                os << inner;
                first = false;
            }
            os << "\"]";
            for (const auto& successor : iter.second)
                os << ";\n\"" << ((!invert) ? iter.first : successor) << "\" -> \""
                   << ((invert) ? iter.first : successor) << "\"";
        }
        os << "}\n" << std::endl;
    }
};

/** A class for graph search algorithms. Every public member function is static and takes a constant reference
 * to a hyper-graph and a lambda only, which is executed for each newly encountered node in the search. Every
 * private member function forms the recursive component of its corresponding public member function, if any.
 */
class GraphSearch {
private:
    /** Recursive component of Khan's algorithm. */
    template <typename Lambda, template <typename> class Table, typename Node>
    static void khansAlgorithm(const HyperGraph<Table, Node>& graph, const size_t vertex,
            std::vector<bool>& visited, Lambda lambda) {
        bool foundValidVertex = false, foundVisitedPredecessor = false, foundVisitedSuccessor = false;
        for (const size_t successor : graph.getSuccessors(vertex)) {
            if (visited[successor] == false) {
                for (auto successorsPredecessor : graph.getPredecessors(successor)) {
                    if (visited[successorsPredecessor] == false) {
                        foundVisitedPredecessor = true;
                        break;
                    }
                }
                if (!foundVisitedPredecessor) {
                    visited[successor] = true;
                    lambda(successor);
                    khansAlgorithm(graph, successor, visited, lambda);
                    foundValidVertex = true;
                }
                foundVisitedPredecessor = false;
            }
        }
        if (!foundValidVertex) {
            return;
        }
        for (auto predecessor : graph.getPredecessors(vertex)) {
            if (visited[predecessor] == false) {
                foundVisitedPredecessor = true;
                break;
            }
        }
        for (auto successor : graph.getSuccessors(vertex)) {
            if (visited[successor] == false) {
                foundVisitedSuccessor = true;
                break;
            }
        }
        if (!foundVisitedPredecessor && foundVisitedSuccessor) khansAlgorithm(graph, vertex, visited, lambda);
    }

    /** Recursive component of reverse DFS algorithm. */
    template <typename Lambda, template <typename> class Table, typename Node>
    static void reverseDFS(const HyperGraph<Table, Node>& graph, const size_t vertex,
            std::vector<bool>& visited, Lambda lambda) {
        if (visited[vertex] == false) {
            visited[vertex] = true;
            for (size_t predecessor : graph.getPredecessors(vertex)) {
                reverseDFS(graph, predecessor, visited, lambda);
            }
            lambda(vertex);
        }
    }

public:
    /** Search the graph in the order of Khan's algorithm, executing the given lambda function for each
     * visited node. Note that this only works for acyclic graphs, otherwise behaviour is undefined. */
    template <typename Lambda, template <typename> class Table, typename Node>
    static void khansAlgorithm(const HyperGraph<Table, Node>& graph, Lambda lambda) {
        // TODO: is this actually Khan's algorithm?
        std::vector<bool> visited;
        visited.resize(graph.vertexCount());
        std::fill(visited.begin(), visited.end(), false);
        for (size_t vertex : graph.allVertices()) {
            if (graph.getPredecessors(vertex).empty()) {
                visited[vertex] = true;
                lambda(vertex);
                if (!graph.getSuccessors(vertex).empty()) khansAlgorithm(graph, vertex, visited, lambda);
            }
        }
    }

    /** Search the graph in the order of the reverse DFS algorithm, executing the given lambda function for
     * each visited node. Note that this only works for acyclic graphs, otherwise behaviour is undefined. */
    template <typename Lambda, template <typename> class Table, typename Node>
    static void reverseDFS(const HyperGraph<Table, Node>& graph, Lambda lambda) {
        std::vector<bool> visited;
        visited.resize(graph.vertexCount());
        std::fill(visited.begin(), visited.end(), false);
        for (size_t vertex : graph.allVertices()) {
            if (graph.getPredecessors(vertex).empty() && graph.getSuccessors(vertex).empty()) {
                visited[vertex] = true;
                lambda(vertex);
            } else {
                reverseDFS(graph, vertex, visited, lambda);
            }
        }
    }
};

/** A class to obtain node orderings for graph searches. */
class GraphOrder {
public:
    /** Appends the collection of objects in the table for each vertex to an order as the vertex is
     * encountered by the given search function, then returns that order. */
    template <template <typename> class Table, typename Node>
    static const std::vector<Node> innerOrder(const HyperGraph<Table, Node>& graph,
            void (*algorithm)(const HyperGraph<Table, Node>&, std::function<void(const size_t)>)) {
        std::vector<Node> order;
        for (const size_t vertex : outerOrder(graph, algorithm)) {
            const auto& objects = graph.table().get(vertex);
            order.insert(order.end(), objects.begin(), objects.end());
        }
        return order;
    }

    /** Appends each vertex to an order as the vertex is encountered by the given search function, then
     * returns that order. */
    template <template <typename> class Table, typename Node>
    static const std::vector<size_t> outerOrder(const HyperGraph<Table, Node>& graph,
            void (*algorithm)(const HyperGraph<Table, Node>&, std::function<void(const size_t)>)) {
        std::vector<size_t> order;
        algorithm(graph, [&order](const size_t vertex) { order.push_back(vertex); });
        return order;
    }
};

/** A class to execute queries over a graph. Every public member function is static and takes a constant
 * reference to a hyper-graph, as well as some form of collection of vertices and/or edges and performs a
 * 'query' with respect to that collection. Every private member function forms the recursive component of its
 * corresponding public member function, if any. */
class GraphQuery {
public:
    template <template <typename> class Table, typename Node>
    static const int topologicalOrderingCost(
            const HyperGraph<Table, Node>& graph, const std::vector<size_t>& order) {
        // create variables to hold the cost of the current SCC and the permutation as a whole
        int costOfSCC = 0;
        int costOfPermutation = -1;
        // for each of the scc's in the ordering, resetting the cost of the scc to zero on each loop
        for (auto it_i = order.begin(); it_i != order.end(); ++it_i, costOfSCC = 0) {
            // check that the index of all predecessor sccs of are before the index of the current scc
            for (auto scc : graph.getPredecessors(*it_i))
                if (std::find(order.begin(), it_i, scc) == it_i)
                    // if not, the sort is not a valid topological sort
                    return -1;
            // otherwise, calculate the cost of the current scc
            // as the number of sccs with an index before the current scc
            for (auto it_j = order.begin(); it_j != it_i; ++it_j)
                // having some successor scc with an index after the current scc
                for (auto scc : graph.getSuccessors(*it_j))
                    if (std::find(order.begin(), it_i, scc) == it_i) costOfSCC++;
            // and if this cost is greater than the maximum recorded cost for the whole permutation so far,
            // set the cost of the permutation to it
            if (costOfSCC > costOfPermutation) {
                costOfPermutation = costOfSCC;
            }
        }
        return costOfPermutation;
    }
};

/** A class for converting graphs (and other hyper-graphs) to hyper-graphs having certain properties. Every
 * public member function is static and takes a constant copy of either a graph or hyper-graph, and returns a
 * new hyper-graph bearing the properties specific to that function. Every private member function forms the
 * recursive component of its corresponding public member function, if any. */
class GraphConvert {
private:
    /** Recursive component of conversion to acyclic hyper-graph. */
    template <template <typename> class Table, typename Node, typename Compare = std::less<Node>>
    static void toAcyclicHyperGraph(const Graph<Node, Compare>& graph, HyperGraph<Table, Node>& sccGraph,
            const Node& w, std::map<Node, int>& preOrder, size_t& counter, std::stack<Node>& S,
            std::stack<Node>& P) {
        // Compute strongly connected components using Gabow's algorithm (cf. Algorithms in
        // Java by Robert Sedgewick / Part 5 / Graph *  algorithms). The algorithm has linear
        // runtime.

        preOrder[w] = counter++;

        S.push(w);
        P.push(w);

        for (const Node& t : graph.getPredecessors(w))
            if (preOrder[t] == -1)
                toAcyclicHyperGraph(graph, sccGraph, t, preOrder, counter, S, P);
            else if (!sccGraph.table().has(t))
                while (preOrder[P.top()] > preOrder[t]) P.pop();

        if (P.top() == w)
            P.pop();
        else
            return;

        Node v;
        size_t s = sccGraph.vertexCount();
        sccGraph.insertVertex(s);
        do {
            v = S.top();
            S.pop();
            sccGraph.appendToVertex(s, v);
        } while (v != w);
    }

public:
    /** Convert the given graph to a hyper-graph of the specified table type. */
    template <template <typename> class Table, typename Node, typename Compare = std::less<Node>>
    static HyperGraph<Table, Node> toHyperGraph(Graph<Node, Compare> oldGraph) {
        HyperGraph<Table, Node> newGraph = HyperGraph<Table, Node>();
        size_t index = 0;
        for (const size_t vertex : oldGraph.allVertices()) {
            newGraph.insertVertex(index, vertex);
            index++;
        }
        for (const size_t vertex : oldGraph.allVertices()) {
            index = newGraph.table().getIndex(vertex);
            for (const size_t successor : oldGraph.getSuccessors(vertex)) {
                newGraph.insertEdge(index, newGraph.table().getIndex(successor));
            }
        }
        return newGraph;
    }

    /** Convert the given graph to an acyclic hyper-graph of the specified table type. */
    template <template <typename> class Table, typename Node, typename Compare = std::less<Node>>
    static HyperGraph<Table, Node> toAcyclicHyperGraph(const Graph<Node, Compare> graph) {
        // TODO: add a function like this for hyper-graphs (taking them as an argument)
        size_t counter = 0;
        std::stack<Node> S, P;
        std::map<Node, int> preOrder;
        HyperGraph<Table, Node> sccGraph = HyperGraph<Table, Node>();
        for (const Node& vertex : graph.allVertices()) preOrder[vertex] = -1;
        for (const Node& vertex : graph.allVertices())
            if (preOrder[vertex] == -1) toAcyclicHyperGraph(graph, sccGraph, vertex, preOrder, counter, S, P);
        for (const Node& vertex : graph.allVertices())
            for (const Node& predecessor : graph.getPredecessors(vertex))
                if (vertex != predecessor &&
                        sccGraph.table().getIndex(vertex) != sccGraph.table().getIndex(predecessor))
                    sccGraph.insertEdge(
                            sccGraph.table().getIndex(vertex), sccGraph.table().getIndex(predecessor));
        return sccGraph;
    }

    /** Convert the given hyper-graph to another hyper-graph of the specified table type. */
    template <template <typename> class Table, template <typename> class OtherTable, typename OtherNode>
    static HyperGraph<Table, size_t> toHyperGraph(HyperGraph<OtherTable, OtherNode> oldGraph) {
        HyperGraph<Table, size_t> newGraph = HyperGraph<Table, size_t>();
        for (size_t index = 0; index < oldGraph.vertexCount(); ++index) newGraph.insertVertex(index, index);
        for (const size_t vertex : oldGraph.allVertices())
            for (const size_t successor : oldGraph.getSuccessors(vertex))
                newGraph.insertEdge(vertex, successor);
        return newGraph;
    }
};

}  // end of namespace souffle
