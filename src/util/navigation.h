#ifndef NAVIGATION_H
#define NAVIGATION_H

#include "ecs/entity.h"
#include <stddef.h>
#include <renderer/renderer.h>

typedef struct {
    VertexID verts[3];
    TriangleID neighbors[3];
    Vector3 centroid;
} NavPoly;

typedef struct {
    size_t mesh;
    TriangleID start;
    TriangleID end;
    NavPoly* polys;
    size_t polycount;
} NavigationMesh;

NavigationMesh UploadNavigationMesh(const char* filepath);

NavigationMesh DuplicateNavigationMesh(NavigationMesh mesh);

void FindPath(NavigationMesh* mesh, Vector3 start, Vector3 end, ARRLIST_size_t* path);

void Navigate(Entity e, Vector3 target);

BOOL GetSharedEdge(NavigationComponent* nc, int index, Vector3* out_p1, Vector3* out_p2);

BOOL HasCrossedEdge(Vector3 pos, Vector3 ep1, Vector3 ep2, Vector3 from_centroid);

#endif
