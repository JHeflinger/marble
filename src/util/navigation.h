#ifndef NAVIGATION_H
#define NAVIGATION_H

#include <stddef.h>
#include <renderer/renderer.h>
#include <easybasics.h>

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

void FindPath(NavigationMesh* mesh, Vector3 start, Vector3 end, ARRLIST_size_t* path);

#endif
