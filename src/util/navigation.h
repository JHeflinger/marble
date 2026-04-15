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

#endif
