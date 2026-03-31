#include "spacegrid.h"
#include "util/bvh.h"

uint64_t HashCell(CellCoord coord) {
    uint64_t h = 1469598103934665603ULL;
    h ^= (uint64_t)coord.x; h *= 1099511628211ULL;
    h ^= (uint64_t)coord.y; h *= 1099511628211ULL;
    h ^= (uint64_t)coord.z; h *= 1099511628211ULL;
    return h;
}

IMPL_HASHMAP(CellCoord, SpaceCell, GridMap, HashCell);

void QuerySpaceGrid(HASHMAP_GridMap* map, size_t iteration, Entity e, DispatchSpaceGridCollisionFunction dispatch) {
    BVHBox bounding_box = EntityBox(e);
    CellCoord basecoord = (CellCoord) {
        (int64_t)(bounding_box.min[0]/CELL_SIZE),
        (int64_t)(bounding_box.min[1]/CELL_SIZE),
        (int64_t)(bounding_box.min[2]/CELL_SIZE)
    };
    CellCoord endcoord = (CellCoord) {
        (int64_t)(bounding_box.max[0]/CELL_SIZE),
        (int64_t)(bounding_box.max[1]/CELL_SIZE),
        (int64_t)(bounding_box.max[2]/CELL_SIZE)
    };
    for (int64_t x = basecoord.x; x <= endcoord.x; x++) {
        for (int64_t y = basecoord.y; y <= endcoord.y; y++) {
            for (int64_t z = basecoord.z; z <= endcoord.z; z++) {
                CellCoord curr = (CellCoord){ x, y, z };
                if (HASHMAP_GridMap_has(map, curr)) {
                    SpaceCell sc = HASHMAP_GridMap_get(map, curr);
                    if (sc.iteration == iteration) {
                        for (size_t i = 0; i < sc.count; i++) {
                            if (sc.entities[i] != e.id) {
                                Entity other = (Entity){ sc.entities[i], e.context };
                                Collider c1 = 0;
                                Collider c2 = 0;
                                DynamicCollisionComponent* dc1 = NULL;
                                DynamicCollisionComponent* dc2 = NULL;
                                if (HasComponent(e, DynamicCollisionComponent)) {
                                    dc1 = GetComponent(e, DynamicCollisionComponent);
                                    c1 = dc1->collider;
                                } else if (HasComponent(e, StaticCollisionComponent)) {
                                    StaticCollisionComponent* sc = GetComponent(e, StaticCollisionComponent);
                                    c1 = sc->collider;
                                } else EZ_ASSERT(FALSE, "Spacegrid query entity does not have a valid collision component");
                                if (HasComponent(other, DynamicCollisionComponent)) {
                                    dc2 = GetComponent(other, DynamicCollisionComponent);
                                    c2 = dc2->collider;
                                } else if (HasComponent(other, StaticCollisionComponent)) {
                                    StaticCollisionComponent* sc = GetComponent(other, StaticCollisionComponent);
                                    c2 = sc->collider;
                                } else EZ_ASSERT(FALSE, "Spacegrid query entity does not have a valid collision component");
                                if (dispatch(e, c1, other, c2)) {
                                    if (dc1) dc1->collided = TRUE;
                                    if (dc2) dc2->collided = TRUE;
                                }
                            }
                        }
                        if (sc.count == CELL_DENSITY) {
                            EZ_WARN("Spacegrid cell density limit reached");
                        } else {
                            sc.entities[sc.count] = e.id;
                            sc.count++;
                            HASHMAP_GridMap_set(map, curr, sc);
                        }
                    } else {
                        sc.iteration = iteration;
                        sc.entities[0] = e.id;
                        sc.count = 1;
                        HASHMAP_GridMap_set(map, curr, sc);
                    }
                } else {
                    SpaceCell sc = { 0 };
                    sc.iteration = iteration;
                    sc.entities[0] = e.id;
                    sc.count = 1;
                    HASHMAP_GridMap_set(map, curr, sc);
                }
            }
        }
    }
}
