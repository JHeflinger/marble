#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <renderer/rstructs.h>

typedef struct {
    VertexID start;
    VertexID end;
} MeshDescriptor;

MeshDescriptor UploadGeometry(const char* filepath);

#endif
