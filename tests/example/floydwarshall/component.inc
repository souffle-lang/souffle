// Souffle - A Datalog Compiler
// Copyright (c) 2019, The Souffle Developers. All rights reserved
// Licensed under the Universal Permissive License v 1.0 as shown at:
// - https://opensource.org/licenses/UPL
// - <souffle root>/licenses/SOUFFLE-UPL.txt

/////////////////////////////////////////////////////////////////////
//
// Floyd Warshall Algorithm as a Component
//
// This component finds the shortest paths in a 
// directed graph. For all pairs of vertices, 
// the algorithm will find the shortest path
// that connects the vertices and provides a
// witness, i.e., the set of nodes on the 
// shortest path. Although there could be a 
// set of shortest path, an arbitrary shortest
// path is chosen. 
//
// For more information:
//  (1) https://en.wikipedia.org/wiki/Floyd–Warshall_algorithm
//  (2)  Cormen, Thomas H.; Leiserson, Charles E.; Rivest, 
//       Ronald L. (1990). Introduction to Algorithms (1st ed.). 
//       MIT Press and McGraw-Hill. ISBN 0-262-03141-8. 
//
/////////////////////////////////////////////////////////////////////


#ifndef FLOYD_WARSHALL
#define FLOYD_WARSHALL

#include "interface.inc"

.comp FloydWarshall : ShortestPathInterface {


  // Auxiliary relation to perform the dynamic program
  // for shortest path. 
  .decl ShortestKPath(u:Node, v:Node, k:Node, dist:number) // Shortest path from u to v in iteration k 

  // Shortest path tree's next relation  
  .decl Next(u:Node, v:Node, k:Node, succ:Node) // Succ is the next node from u to v in iteration k

  // Base cases 

  // set self-loops to zero distance 
  ShortestKPath(1, 1, 0, 0). 
  Next(1,1,0, 1).
  ShortestKPath(i+1, i+1, 0, 0),
  Next(i+1, i+1, 0, i+1) :-
    ShortestKPath(i,i,0,0),
    NumNodes(n),
    i < n.
  // set Edges in the graph to unit distance
  ShortestKPath(i, j, 0, 1),
  Next(i,j,0, j) :- 
   Edge(i, j).
  // set remaining connections to n+1 distance, 
  // (repsenting that they are too expensive 
  // to traverse). 
  ShortestKPath(i+1, j, 0, n+1),
  Next(i+1, j, 0, -1) :- 
    ShortestKPath(i, j, _, _),
    !Edge(i+1,j),  
    i+1 != j,
    NumNodes(n),
    i < n. 

  ShortestKPath(i, j+1, 0, n+1),
  Next(i, j+1, 0, -1) :- 
    ShortestKPath(i, j, _, _),  
    !Edge(i, j+1),
    i != j+1,
    NumNodes(n),
    j < n. 

  // inductive case 

  // Shortest path via k is longer
  ShortestKPath(i, j, k+1, d),
  Next(i,j, k+1, succ):- 
   ShortestKPath(i, j, k, d),
   ShortestKPath(i, k+1, k, d1),
   ShortestKPath(k+1, j, k, d2),
   Next(i, j, k, succ),
   d <= d1 + d2,
   NumNodes(n),
   k < n.

  // Shortest path via k is shorter
   ShortestKPath(i, j, k+1, d1+d2),
   Next(i, j, k+1, succ) :-
    ShortestKPath(i, j, k, d),
    ShortestKPath(i, k+1, k, d1),
    ShortestKPath(k+1, j, k, d2),
    Next(i, k+1, k, succ),
    d > d1 + d2,
    NumNodes(n),
    k < n.

  // Populate shortest-path relation 
  ShortestPath(i, j, d):-
    ShortestKPath(i, j, n, d),
    d != n + 1, 
    NumNodes(n).

  // Compute witness for a shortest-path

  // base case
  ShortestPathWitness(i, j, i) :-
   Next(i, j, n, _),
   ShortestKPath(i, j, n, d),
   d != n + 1, 
   NumNodes(n). 

  // inductive case
  ShortestPathWitness(i, j, succ) :-
   ShortestPathWitness(i, j, u),
   Next(u, j, n, succ),
   NumNodes(n).     
}

#endif 

