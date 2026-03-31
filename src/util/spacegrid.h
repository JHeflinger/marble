#ifndef SPACEGRID_H
#define SPACEGRID_H

#include "ecs/entity.h"
#include "ecs/components.h"

#define CELL_SIZE 1.0f
#define CELL_DENSITY 64

typedef BOOL (*DispatchSpaceGridCollisionFunction)(Entity e1, Collider c1, Entity e2, Collider c2);

typedef struct {
    int64_t x;
    int64_t y;
    int64_t z;
} CellCoord;

typedef struct {
    uint64_t iteration;
    EntityID entities[CELL_DENSITY];
    size_t count;
} SpaceCell;

DECLARE_HASHMAP(CellCoord, SpaceCell, GridMap);

void QuerySpaceGrid(HASHMAP_GridMap* map, size_t iteration, Entity e, DispatchSpaceGridCollisionFunction dispatch);

#endif
