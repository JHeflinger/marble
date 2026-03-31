#ifndef BVH_H
#define BVH_H

#include "ecs/entity.h"
#include "ecs/components.h"
#include <cglm/cglm.h>

#define BVH_LEAF 0
#define BVH_LEFT_ONLY 1
#define BVH_RIGHT_ONLY 2
#define BVH_BOTH 3

typedef BOOL (*DispatchBVHCollisionFunction)(Entity e1, Collider c1, Entity e2, Collider c2);

typedef struct {
    alignas(16) vec3 min;
    alignas(16) vec3 max;
    alignas(4) uint32_t branch_config;
    alignas(4) uint32_t left;
    alignas(4) uint32_t right;
    // branches[0] describes: 0 = leaf, 1 = left tree, 2 = right tree, 3 = both
    // branches[1] is left tree ind
    // branches[2] is right tree ind
} NodeBVH;
DECLARE_ARRLIST(NodeBVH);

typedef struct {
    vec3 min;
    vec3 max;
    vec3 centroid;
    uint32_t eid;
} BVHBox;
DECLARE_ARRLIST(BVHBox);

BVHBox EntityBox(Entity e);

void ReconstructBVH(ARRLIST_NodeBVH* bvh, ARRLIST_EntityID entities, World* context);

void QueryBVH(ARRLIST_NodeBVH* bvh, Entity e, DispatchBVHCollisionFunction dispatch);

#endif
