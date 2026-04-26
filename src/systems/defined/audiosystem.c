#include "audiosystem.h"
#include "ecs/components.h"
#include "ecs/entity.h"
#include "game/world.h"
#include "util/bvh.h"
#include <raymath.h>

#define MAX_BOUNCES 10

DECLARE_ARRLIST(TransformComponent);
IMPL_ARRLIST(TransformComponent);

ARRLIST_NodeBVH g_audio_bvh = { 0 };
ARRLIST_MetaTriangle g_audio_triangles = { 0 };
ARRLIST_EntityID g_verify_audio_barrier_entities = { 0 };
ARRLIST_size_t g_verify_audio_mesh_ids = { 0 };
ARRLIST_TransformComponent g_verify_audio_transforms = { 0 };
BOOL g_audio_init = FALSE;

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

void PathTrace(AudioRay ray, ARRLIST_EntityID* sources, ARRLIST_float* pools, World* context) {
    float total_dist = 0.0f;
    for (size_t i = 0; i < MAX_BOUNCES; i++) {
        AudioHit hit = TraceAudioRay(ray, &g_audio_bvh, &g_audio_triangles);
        if (hit.distance <= 0) break;
        total_dist += hit.distance;
        for (size_t j = 0; j < sources->size; j++) {
            if (pools->data[j] != 0) continue;
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
            if (shit.distance <= 0.0f || shit.distance < dist) continue;
            pools->data[j] = total_dist + dist;
        }
        ReflectDirection(ray.direction, hit.normal, ray.direction); // TODO: just straight reflection for now, decide how to treat audio materals later
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
            AudioRay* rays = EZ_ALLOC(alc->fidelity, sizeof(AudioRay));
            FibbonaciRays(rays, alc->fidelity, tc->translation);
            ARRLIST_float totaldists = { 0 };
            ARRLIST_float_zero(&totaldists, sources->size);
            ARRLIST_float currdists = { 0 };
            ARRLIST_float_zero(&currdists, sources->size);
            ARRLIST_int counts = { 0 };
            ARRLIST_int_zero(&counts, sources->size);
            for (size_t j = 0; j < alc->fidelity; j++) {
                for (size_t k = 0; k < sources->size; k++) currdists.data[k] = 0.0f;
                PathTrace(rays[j], sources, &currdists, system->context);
                for (size_t k = 0; k < sources->size; k++) {
                    if (currdists.data[k] != 0) counts.data[k]++;
                    totaldists.data[k] += currdists.data[k];
                }
            }
            // TODO: currently just scales with visibility, should also scale with distance
            for (size_t j = 0; j < sources->size; j++) {
                Entity es = (Entity){ sources->data[j], system->context };
                float visibility = ((float)counts.data[j]) / ((float)alc->fidelity);
                AudioSourceComponent* asc = GetComponent(es, AudioSourceComponent);
                SetSoundVolume(asc->sound, visibility);
            }
            ARRLIST_int_clear(&counts);
            ARRLIST_float_clear(&totaldists);
            ARRLIST_float_clear(&currdists);
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
