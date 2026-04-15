#include "geometry.h"
#include <renderer/loader.h>
#include <renderer/renderer.h>
#include <raymath.h>

size_t UploadGeometry(const char* filepath) {
    if (!LoadOBJ(filepath)) return (size_t)-1;
    return NumMeshes() - 1;
}

Vector3 TriangleCentroid(TriangleID tid) {
    float* a = VertexReference(TriangleReference(tid)->a);
    float* b = VertexReference(TriangleReference(tid)->b);
    float* c = VertexReference(TriangleReference(tid)->c);
    Vector3 centroid = (Vector3) {
        a[0] + b[0] + c[0],
        a[1] + b[1] + c[1],
        a[2] + b[2] + c[2]
    };
    return Vector3Scale(centroid, 1.0f / 3.0f);
}
