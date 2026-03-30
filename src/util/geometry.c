#include "geometry.h"
#include <renderer/loader.h>
#include <renderer/renderer.h>

size_t UploadGeometry(const char* filepath) {
    if (!LoadOBJ(filepath)) return (size_t)-1;
    return NumMeshes() - 1;
}
