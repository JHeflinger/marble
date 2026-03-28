#include "geometry.h"
#include <renderer/loader.h>
#include <renderer/renderer.h>

MeshDescriptor UploadGeometry(const char* filepath) {
    VertexID start = NumVertices();
    if (!LoadOBJ(filepath)) return (MeshDescriptor){ 0, 0 };
    vec3 offsetv = { 0 };
    SubmitVertex(offsetv);
    return (MeshDescriptor){ start, NumVertices() - 1 };
}
