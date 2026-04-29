#include "examples/echo.h"
#include <renderer/renderer.h>

int main(int argc, char** argv) {
    #ifdef TEST_AVIZ
    typedef struct {
        alignas(16) vec3 position;
        alignas(16) vec3 direction;
    } DummyRay;
    SubmitExternalShader("build/expanded/audioviz.comp", "build/shaders/audioviz.comp.spv", 10000);
    SubmitExternalShader("build/expanded/switch.comp", "build/shaders/switch.comp.spv", 1600*900);
    ShaderBuffer* viz = CreateExternalBuffer("AudioRaySSBOIn");
    DummyRay* rays = EZ_ALLOC(10000, sizeof(DummyRay));
    inline void fibbyrays(DummyRay* rays, size_t count, Vector3 p) {
        const float golden_angle = GLM_PI * (sqrtf(5.0f) - 1.0f);
        vec3 origin = { p.x, p.y, p.z };
        for (size_t i = 0; i < count; i++) {
            float y = 1.0f - ((float)i / (float)(count - 1)) * 2.0f;
            float r = sqrtf(1.0f - y * y);
            float theta = golden_angle * (float)i;
            vec3 dir = { r * cosf(theta), y, r * sinf(theta) };
            glm_vec3_normalize(dir);
            glm_vec3_copy(origin, rays[i].position);
            glm_vec3_copy(dir, rays[i].direction);
        }
    }
    fibbyrays(rays, 10000, (Vector3){ 0, 10.0f, 0 });
    viz->data = rays;
    viz->size = 10000 * sizeof(DummyRay);
    #endif

    EchoMain();

    #ifdef TEST_AVIZ
    EZ_FREE(rays);
    #endif

    return 0;
}
