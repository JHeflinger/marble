#ifndef BVH_H
#define BVH_H

#include <easyobjects.h>
#include <cglm/cglm.h>

#define BVH_LEAF 0
#define BVH_LEFT_ONLY 1
#define BVH_RIGHT_ONLY 2
#define BVH_BOTH 3

typedef struct {
    vec3 min;
    vec3 max;
    uint32_t branch_config;
    uint32_t left;
    uint32_t right;
    // branches[0] describes: 0 = leaf, 1 = left tree, 2 = right tree, 3 = both
    // branches[1] is left tree ind
    // branches[2] is right tree ind
} NodeBVH;
DECLARE_ARRLIST(NodeBVH);

typedef struct {
    vec3 min;
    vec3 max;
    vec3 centroid;
    uint32_t meshid;
} BVHBox;
DECLARE_ARRLIST(BVHBox);

void ReconstructBVH(ARRLIST_NodeBVH* bvh);

#endif
