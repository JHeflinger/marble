#include "audiosystem.h"
#include "ecs/components.h"
#include "ecs/entity.h"
#include "game/world.h"
#include "util/bvh.h"
#include "audio/dsp.h"
#include <raymath.h>
#include <math.h>

#define MAX_BOUNCES 10
#define SPEED_OF_SOUND 343.0f
#define BOUNCE_ABSORPTION 0.8f
#define MAX_FRAMES_PER_CALLBACK 8192

DECLARE_ARRLIST(TransformComponent);
IMPL_ARRLIST(TransformComponent);

ARRLIST_NodeBVH g_audio_bvh = { 0 };
ARRLIST_MetaTriangle g_audio_triangles = { 0 };
ARRLIST_EntityID g_verify_audio_barrier_entities = { 0 };
ARRLIST_size_t g_verify_audio_mesh_ids = { 0 };
ARRLIST_TransformComponent g_verify_audio_transforms = { 0 };
BOOL g_audio_init = FALSE;

DECLARE_ARRLIST(Tap);
IMPL_ARRLIST(Tap);

DSPState* g_audio_dsp = NULL;
SpatialSource* g_audio_spatial = NULL;
unsigned int g_audio_channels = 1; // make 2 later

static float g_mono_scratch[MAX_FRAMES_PER_CALLBACK];
static float g_stereo_scratch[MAX_FRAMES_PER_CALLBACK * 2];

void DSPProcessorCallback(void* buffer, unsigned int frames) {
    if (!g_audio_dsp || frames > MAX_FRAMES_PER_CALLBACK) return;

    float* samples = (float*)buffer;

    float input_max = 0.0f;
    for (unsigned int f = 0; f < frames * g_audio_channels; f++) {
        if (fabsf(samples[f]) > input_max) input_max = fabsf(samples[f]);
    }

    for (unsigned int f = 0; f < frames; f++) {
        float sum = 0.0f;
        for (unsigned int c = 0; c < g_audio_channels; c++) {
            sum += samples[f * g_audio_channels + c];
        }
        g_mono_scratch[f] = sum / (float)g_audio_channels;
    }

    float pre_dsp_max = 0.0f;
    for (unsigned int f = 0; f < frames; f++) {
        if (fabsf(g_mono_scratch[f]) > pre_dsp_max) pre_dsp_max = fabsf(g_mono_scratch[f]);
    }

    DSPProcess(g_audio_dsp, g_mono_scratch, frames);

    float post_dsp_max = 0.0f;
    for (unsigned int f = 0; f < frames; f++) {
        if (fabsf(g_mono_scratch[f]) > post_dsp_max) post_dsp_max = fabsf(g_mono_scratch[f]);
    }

    if (g_audio_spatial) {
        float dirX, dirY, dirZ;
        DSPGetDirection(g_audio_dsp, &dirX, &dirY, &dirZ);

        SpatialApply(g_audio_spatial, g_mono_scratch, g_stereo_scratch,
                     dirX, dirY, dirZ, frames);

        float post_spatial_max = 0.0f;
        for (unsigned int f = 0; f < frames * 2; f++) {
            if (fabsf(g_stereo_scratch[f]) > post_spatial_max) post_spatial_max = fabsf(g_stereo_scratch[f]);
        }

        for (unsigned int f = 0; f < frames; f++) {
            for (unsigned int c = 0; c < g_audio_channels; c++) {
                samples[f * g_audio_channels + c] = g_stereo_scratch[f * 2 + (c % 2)];
            }
        }
    }
}

// amplitude function, but might change if we have different materials or source properties
float TapAmplitude(float pathLength, size_t bounces) {
    float distanceFalloff = 1.0f / (1.0f + pathLength);
    float bounceFalloff = powf(BOUNCE_ABSORPTION, (float)bounces);
    return distanceFalloff * bounceFalloff;
}

void FibbonaciRays(AudioRay* rays, size_t count, Vector3 p) {
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

void ReflectDirection(vec3 dir, vec3 normal, vec3 dest) {
    float dot = glm_vec3_dot(dir, normal);
    vec3 scaled;
    glm_vec3_scale(normal, 2.0f * dot, scaled);
    glm_vec3_sub(dir, scaled, dest);
    glm_vec3_normalize(dest);
}

void PathTrace(AudioRay ray, ARRLIST_EntityID* sources, ARRLIST_Tap* tapLists, World* context) {
    float total_dist = 0.0f;
    for (size_t i = 0; i < MAX_BOUNCES; i++) {
        AudioHit hit = TraceAudioRay(ray, &g_audio_bvh, &g_audio_triangles);
        if (hit.distance <= 0) break;
        total_dist += hit.distance;
        for (size_t j = 0; j < sources->size; j++) {
            Entity e = (Entity){ sources->data[j], context };
            TransformComponent* tc = GetComponent(e, TransformComponent);
            AudioRay sray;
            vec3 translate = { tc->translation.x, tc->translation.y, tc->translation.z };
            glm_vec3_sub(translate, hit.position, sray.direction);
            float dist = glm_vec3_norm(sray.direction);
            glm_vec3_normalize(sray.direction);
            vec3 iota;
            glm_vec3_scale(hit.normal, 1e-6, iota);
            glm_vec3_add(iota, hit.position, sray.position);
            AudioHit shit = TraceAudioRay(sray, &g_audio_bvh, &g_audio_triangles);
            if (shit.distance > 0.0f && shit.distance < dist) continue;
            float pathLength = total_dist + dist;

            float jitter = ((float)rand() / (float)RAND_MAX - 0.5f) * 0.0005f;
            Tap tap = {
                .delay_seconds = pathLength / SPEED_OF_SOUND + jitter,
                .amplitude = TapAmplitude(pathLength, i + 1)
            };
            ARRLIST_Tap_add(&tapLists[j], tap);
        }
        ReflectDirection(ray.direction, hit.normal, ray.direction);
        vec3 iota;
        glm_vec3_scale(hit.normal, 1e-6, iota);
        glm_vec3_add(hit.position, iota, ray.position);
    }
}

void UpdateAudioSystem(System* system, float dt) {
    BOOL reconstruct = FALSE;
    ARRLIST_EntityID* barriers = GetEntities(system->context, AudioBarrierComponent);
    if (barriers) {
        if (barriers->size != g_verify_audio_barrier_entities.size) {
            reconstruct = TRUE;
        }
        if (!reconstruct) {
            for (size_t i = 0; i < barriers->size; i++) {
                if (barriers->data[i] != g_verify_audio_barrier_entities.data[i]) {
                    reconstruct = TRUE;
                    break;
                }
            }
        }
        if (!reconstruct) {
            for (size_t i = 0; i < barriers->size; i++) {
                Entity e = (Entity){ barriers->data[i], system->context };
                AudioBarrierComponent* abc = GetComponent(e, AudioBarrierComponent);
                if (abc->meshid != g_verify_audio_mesh_ids.data[i]) {
                    reconstruct = TRUE;
                    break;
                }
            }
        }
        if (!reconstruct) {
            for (size_t i = 0; i < barriers->size; i++) {
                Entity e = (Entity){ barriers->data[i], system->context };
                TransformComponent* tc = GetComponent(e, TransformComponent);
                TransformComponent* tc2 = &(g_verify_audio_transforms.data[i]);
                if (!Vector3Equals(tc->translation, tc2->translation) ||
                    !Vector3Equals(tc->rotation, tc2->rotation) ||
                    !Vector3Equals(tc->scale, tc2->scale)) {
                    reconstruct = TRUE;
                    break;
                }
            }
        }
    } else if (g_verify_audio_barrier_entities.size != 0) reconstruct = TRUE;
    if (reconstruct) {
        ARRLIST_EntityID_wipe(&g_verify_audio_barrier_entities);
        ARRLIST_size_t_wipe(&g_verify_audio_mesh_ids);
        ARRLIST_TransformComponent_wipe(&g_verify_audio_transforms);
        for (size_t i = 0; i < barriers->size; i++) {
            Entity e = (Entity){ barriers->data[i], system->context };
            TransformComponent* tc = GetComponent(e, TransformComponent);
            AudioBarrierComponent* abc = GetComponent(e, AudioBarrierComponent);
            ARRLIST_EntityID_add(&g_verify_audio_barrier_entities, barriers->data[i]);
            ARRLIST_size_t_add(&g_verify_audio_mesh_ids, abc->meshid);
            ARRLIST_TransformComponent_add(&g_verify_audio_transforms, *tc);
        }
        ReconstructAudioBVH(&g_audio_triangles, &g_audio_bvh, *barriers, system->context);
    }
    ARRLIST_EntityID* listeners = GetEntities(system->context, AudioListenerComponent);
    ARRLIST_EntityID* sources = GetEntities(system->context, AudioSourceComponent);
    if (listeners && sources) {
        if (listeners->size > 1) EZ_WARN("More than one audio listener has been defined");
        for (size_t i = 0; i < listeners->size; i++) {
            Entity e = (Entity){ listeners->data[i], system->context };
            AudioListenerComponent* alc = GetComponent(e, AudioListenerComponent);
            TransformComponent* tc = GetComponent(e, TransformComponent);

            // Compute listener orientation from transform + camera (mirrors drawsystem.c).
            vec3 rotation = { tc->rotation.x, tc->rotation.y, tc->rotation.z };
            CameraComponent* cc = GetComponent(e, CameraComponent);
            if (cc) {
                vec3 crot = { cc->rotation.x, cc->rotation.y, cc->rotation.z };
                glm_vec3_add(rotation, crot, rotation);
            }
            mat4 R;
            glm_euler_yxz(rotation, R);
            vec3 base_forward = { 0.0f, 0.0f, -1.0f };
            vec3 base_up      = { 0.0f, 1.0f,  0.0f };
            vec3 forward_v, up_v;
            glm_mat4_mulv3(R, base_forward, 0.0f, forward_v);
            glm_mat4_mulv3(R, base_up,      0.0f, up_v);

            AudioRay* rays = EZ_ALLOC(alc->fidelity, sizeof(AudioRay));
            FibbonaciRays(rays, alc->fidelity, tc->translation);

            // one tap list per source
            ARRLIST_Tap* tapLists = EZ_ALLOC(sources->size, sizeof(ARRLIST_Tap));
            for (size_t j = 0; j < sources->size; j++) tapLists[j] = (ARRLIST_Tap){ 0 };

            // direct tap test to replace an explicit dry sample
            for (size_t j = 0; j < sources->size; j++) {
                Entity es = (Entity){ sources->data[j], system->context };
                TransformComponent* stc = GetComponent(es, TransformComponent);
                AudioRay direct;
                vec3 listenerPos = { tc->translation.x, tc->translation.y, tc->translation.z };
                vec3 sourcePos = { stc->translation.x, stc->translation.y, stc->translation.z };
                glm_vec3_sub(sourcePos, listenerPos, direct.direction);
                float dist = glm_vec3_norm(direct.direction);
                glm_vec3_normalize(direct.direction);
                glm_vec3_copy(listenerPos, direct.position);
                AudioHit hit = TraceAudioRay(direct, &g_audio_bvh, &g_audio_triangles);

                if (hit.distance <= 0.0f || hit.distance > dist) {
                    // jitter makes reflections sound naturally uneven
                    float jitter = ((float)rand() / (float)RAND_MAX - 0.5f) * 0.0005f;
                    Tap tap = {
                        .delay_seconds = dist / SPEED_OF_SOUND + jitter,
                        .amplitude = TapAmplitude(dist, 0)
                    };
                    ARRLIST_Tap_add(&tapLists[j], tap);
                }
            }

            for (size_t j = 0; j < alc->fidelity; j++) {
                PathTrace(rays[j], sources, tapLists, system->context);
            }

            for (size_t j = 0; j < sources->size; j++) {
                Entity es = (Entity){ sources->data[j], system->context };
                AudioSourceComponent* asc = GetComponent(es, AudioSourceComponent);
                TransformComponent* stc = GetComponent(es, TransformComponent);

                if (asc->dsp) {
                    size_t count = tapLists[j].size;
                    if (count > DSP_MAX_TAPS) count = DSP_MAX_TAPS;
                    DSPSubmitTaps(asc->dsp, tapLists[j].data, count);

                    // compute world-space dir from listener to src
                    vec3 worldDir;
                    vec3 listenerPos = { tc->translation.x, tc->translation.y, tc->translation.z };
                    vec3 sourcePos   = { stc->translation.x, stc->translation.y, stc->translation.z };
                    glm_vec3_sub(sourcePos, listenerPos, worldDir);

                    // project wrt listener
                    vec3 right;
                    glm_vec3_cross(forward_v, up_v, right);
                    glm_vec3_normalize(forward_v);
                    glm_vec3_normalize(up_v);
                    glm_vec3_normalize(right);

                    float localX = glm_vec3_dot(worldDir, right);
                    float localY = glm_vec3_dot(worldDir, up_v);
                    float localForward = glm_vec3_dot(worldDir, forward_v);

                    float saX = localX;
                    float saY = localY;
                    float saZ = -localForward;

                    DSPSubmitDirection(asc->dsp, saX, saY, saZ);
                }
                ARRLIST_Tap_clear(&tapLists[j]);
            }

            EZ_FREE(tapLists);
            EZ_FREE(rays);
        }
    }
}

void CleanAudioSystem(System* system) {
    ARRLIST_EntityID_clear(&g_verify_audio_barrier_entities);
    ARRLIST_size_t_clear(&g_verify_audio_mesh_ids);
    ARRLIST_TransformComponent_clear(&g_verify_audio_transforms);
    ARRLIST_NodeBVH_clear(&g_audio_bvh);
    ARRLIST_MetaTriangle_clear(&g_audio_triangles);
    CloseAudioDevice();
    g_audio_init = FALSE;
}

System* GenerateAudioSystem() {
    if (!g_audio_init) {
        g_audio_init = TRUE;
        InitAudioDevice();
    }
    return GenerateSystem(NULL, UpdateAudioSystem, NULL, NULL, NULL, NULL, CleanAudioSystem);
}
