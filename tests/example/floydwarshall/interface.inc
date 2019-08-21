// Souffle - A Datalog Compiler
// Copyright (c) 2019, The Souffle Developers. All rights reserved
// Licensed under the Universal Permissive License v 1.0 as shown at:
// - https://opensource.org/licenses/UPL
// - <souffle root>/licenses/SOUFFLE-UPL.txt

#ifndef SHORTEST_PATH
#define SHORTEST_PATH

// Generic interface for shortest path
.comp ShortestPathInterface {

  // Node type for shortest path
  .number_type Node

  // input relations of this component
  .decl NumNodes(n:number)    // Number of nodes
  .decl Edge(u:Node, v:Node)  // Edge relation, needs to be input

  // output relations of this component
  .decl ShortestPath(i: Node, j: Node, d: number) 
  .decl ShortestPathWitness(s:Node, e:Node, witness:Node) // Reconstructing the trace s -> e with witness

}
#endif

