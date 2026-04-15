#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <renderer/rstructs.h>

size_t UploadGeometry(const char* filepath);

Vector3 TriangleCentroid(TriangleID tid);

#endif
