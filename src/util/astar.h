#ifndef ASTAR_H
#define ASTAR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <easybasics.h>

typedef struct {
    size_t id;
    float g;
    float f;
    size_t parent;
    BOOL open;
    BOOL closed;
    size_t pqindex;
} AStarNode;

typedef float (*HeuristicFunction)(size_t a, size_t b, void* ctx);
typedef size_t (*NeighborsFunction)(size_t id, size_t* out, size_t max, void* ctx);
typedef float (*EdgeCostFunction)(size_t a, size_t b, void* ctx);

typedef struct {
    HeuristicFunction heuristic;
    NeighborsFunction neighbors;
    EdgeCostFunction edgecost;
    void* ctx;
    size_t max_nodes;
    size_t max_neighbors;
} AStarConfig;

size_t AStar(size_t start, size_t goal, const AStarConfig* cfg, ARRLIST_size_t* path_out);

#endif
