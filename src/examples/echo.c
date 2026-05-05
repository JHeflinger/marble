#include "echo.h"
#include "systems/defined/drawsystem.h"
#include "systems/defined/audiosystem.h"
#include "systems/defined/collisionsystem.h"
#include "systems/defined/aisystem.h"
#include "game/application.h"
#include "ecs/components.h"
#include "ecs/entity.h"
#include "audio/dsp.h"
#include "audio/spatial.h"
#include "data/boxloader.h"
#include "audio/mic.h"
#include <renderer/renderer.h>
#include <raymath.h>

#define NUMRAYS 20000
#define RPR 1000

static NavigationMesh g_navigation;
static ShaderBuffer* g_shaderbuffer = NULL;
static Entity g_terrain;
static Entity g_player;
static Entity g_enemy;
static BOOL g_start = TRUE;
static Texture2D g_title_texture = { 0 };

typedef struct {
    alignas(16) vec3 position;
    alignas(16) vec3 direction;
    alignas(4) uint32_t author;
    alignas(4) uint32_t color; // 0 white, 1 red
    alignas(4) float fade;
} AuthoredRay;

AIStatus UpdateEnemyPath(void* ctx, BlackBoard* blackboard) {
    if (!g_start) return AI_FAILURE;
    BlackBoardSetInt(blackboard, "Count", BlackBoardGetInt(blackboard, "Count") + 1);
    if (g_start && BlackBoardGetInt(blackboard, "Count")%30 == 0) BlackBoardSetBool(blackboard, "Reset", TRUE);
    BOOL reset = BlackBoardGetBool(blackboard, "Reset");
    if (reset) {
        BlackBoardSetInt(blackboard, "Index", 0);
        BlackBoardSetBool(blackboard, "Reset", FALSE);
        //for (TriangleID i = g_navigation.start; i < g_navigation.end; i++) // INFO: uncomment these if you want to see the nav mesh pathing
        //    TriangleReference(i)->material = 1;
        Navigate(g_enemy, *(EntityPosition(g_player)));
        NavigationComponent* nc = GetComponent(g_enemy, NavigationComponent);
        //for (size_t i = 0; i < nc->path.size; i++)
        //    TriangleReference(nc->mesh.start + nc->path.data[i])->material = 2;
        UpdateTriangles();
    }
    return AI_SUCCESS;
}

AIStatus FollowEnemyPath(void* ctx, BlackBoard* blackboard) {
    float ghost_speed = 2.0f;
    int index = BlackBoardGetInt(blackboard, "Index");
    NavigationComponent* nc = GetComponent(g_enemy, NavigationComponent);
    int path_size = (int)nc->path.size;
    Vector3 toplayer = Vector3Subtract(*(EntityPosition(g_player)), *(EntityPosition(g_enemy)));
    if (path_size == 1 || Vector3Length(toplayer) < 5.0f) {
        ghost_speed = 8.0f;
        toplayer.y = 0;
        toplayer = Vector3Normalize(toplayer);
        EntityRotation(g_enemy)->y = (atan2(toplayer.x, toplayer.z) * RAD2DEG) - 90.0f;
        toplayer = Vector3Scale(toplayer, ghost_speed * GetFrameTime());
        *(EntityPosition(g_enemy)) = Vector3Add(*(EntityPosition(g_enemy)), toplayer);
        return AI_RUNNING;
    }
    if (index >= path_size) return AI_SUCCESS;
    BOOL at_last = (index + 1 >= path_size);
    Vector3 steering_target;
    if (at_last) {
        steering_target = nc->mesh.polys[nc->path.data[index]].centroid;
    } else {
        Vector3 ep1, ep2;
        if (!GetSharedEdge(nc, index, &ep1, &ep2)) {
            BlackBoardSetInt(blackboard, "Index", index + 1);
            return AI_RUNNING;
        }
        steering_target = (Vector3){
            (ep1.x + ep2.x) * 0.5f,
            (ep1.y + ep2.y) * 0.5f,
            (ep1.z + ep2.z) * 0.5f
        };
    }
    Vector3* ghost_pos = EntityPosition(g_enemy);
    float dx = steering_target.x - ghost_pos->x;
    float dz = steering_target.z - ghost_pos->z;
    float len = sqrtf(dx * dx + dz * dz);
    if (len > 0.0001f) {
        Vector2 dir = { dx / len, dz / len };
        ghost_pos->x += dir.x * ghost_speed * GetFrameTime();
        ghost_pos->z += dir.y * ghost_speed * GetFrameTime();
        float newrot = (atan2(dir.x, dir.y) * RAD2DEG) - 90.0f;
        EntityRotation(g_enemy)->y += (newrot - EntityRotation(g_enemy)->y) / 10.0f;
    }
    if (at_last) {
        if (len < 0.3f) {
            BlackBoardSetInt(blackboard, "Index", index + 1);
            return AI_SUCCESS;
        }
    } else {
        Vector3 ep1, ep2;
        GetSharedEdge(nc, index, &ep1, &ep2);
        Vector3 current_centroid = nc->mesh.polys[nc->path.data[index]].centroid;
        if (HasCrossedEdge(*ghost_pos, ep1, ep2, current_centroid)) {
            BlackBoardSetInt(blackboard, "Index", index + 1);
        }
    }
    return AI_RUNNING;
}

void UpdateMainScene(World* scene, float dt) {
    CameraComponent* cc = GetComponent(g_player, CameraComponent);
    MeshDescriptor* md = MeshReference(GetComponent(g_player, MeshComponent)->id);
    if (IsKeyPressed(KEY_P)) {
        if (cc->enabled) EnableCursor();
        cc->enabled = !cc->enabled;
    }
    if (cc->enabled) {
        md->disabled = TRUE;
        Vector3 translate = { 0 };
        if (IsKeyDown(KEY_D)) translate.x += dt * 10.0f;
        if (IsKeyDown(KEY_A)) translate.x -= dt * 10.0f;
        if (IsKeyDown(KEY_W)) translate.z -= dt * 10.0f;
        if (IsKeyDown(KEY_S)) translate.z += dt * 10.0f;
        Vector2 md = GetMouseDelta();
        Vector3 rotate = { md.y * dt * -0.15f, md.x * dt * -0.15f, 0 };
        RotateEntityFPV(g_player, rotate);
        MoveFlatEntityFPV(g_player, translate);
        DisableCursor();
    } else {
        md->disabled = FALSE;
    }
    AudioSourceComponent* asc = GetComponent(g_enemy, AudioSourceComponent);
    if (asc) UpdateMusicStream(asc->music);

    // update audio rays
    static float timer = 0.0f;
    static int rotation = 0;
    float fadeaway = 0.1f;
    timer += GetFrameTime();
    for (size_t i = 0; i < NUMRAYS; i++) {
        AuthoredRay* rays = (AuthoredRay*)g_shaderbuffer->data;
        int rotations = NUMRAYS/(2 * RPR);
        rays[i].fade -= GetFrameTime() / ((float)rotations * fadeaway);
        if (rays[i].fade < 0.0f) rays[i].fade = 0.0f;
    }
    if (timer > fadeaway) {
        timer = 0.0f;
        rotation = (rotation + 1)%(NUMRAYS/(2 * RPR));
        size_t mid = GetComponent(g_enemy, MeshComponent)->id;
        inline void fibbyrays(AuthoredRay* rays, size_t count, Vector3 p, float level, uint32_t color) {
            const float golden_angle = GLM_PI * (sqrtf(5.0f) - 1.0f);
            vec3 origin = { p.x, p.y, p.z };
            for (size_t i = 0; i < count; i++) {
                float ran = ((float)(rand()%1000)) / 1000.0f;
                float y = 1.0f - ((float)i / ((float)(count - 1) * level)) * 2.0f;
                y = 1.0f - ran * 2.0f;
                float r = sqrtf(1.0f - y * y);
                float theta = golden_angle * (float)i;
                vec3 dir = { r * cosf(theta), y, r * sinf(theta) };
                glm_vec3_normalize(dir);
                glm_vec3_copy(origin, rays[i].position);
                glm_vec3_copy(dir, rays[i].direction);
                if ((float)i / (float)count > level) {
                    rays[i].direction[0] = 0;
                    rays[i].direction[1] = 0;
                    rays[i].direction[2] = 0;
                }
                rays[i].author = (uint32_t)mid;
                rays[i].color = color;
                rays[i].fade = 1.0f;
            }
        }
        fibbyrays(g_shaderbuffer->data + (rotation * RPR * sizeof(AuthoredRay)), RPR, *(EntityPosition(g_enemy)), 1.0f, 1);
        fibbyrays(g_shaderbuffer->data + ((NUMRAYS/2) * sizeof(AuthoredRay)) + (rotation * RPR * sizeof(AuthoredRay)), RPR, *(EntityPosition(g_player)), fmin(pow(MicVolume(), 3.0f) * 40.0f, 1.0f), 0);
    }
    UpdateShaderBuffer(g_shaderbuffer);
}

Scene* GenerateMainScene() {
    RenderConfig()->maxbounces = 3;
    RenderConfig()->scenelightingonly = FALSE;
    RenderConfig()->scenelightshadows = FALSE;
    RenderConfig()->directonly = TRUE;
    Scene* scene = GenerateScene("Main");
    World* world = GenerateWorld(NULL, UpdateMainScene, NULL, NULL, NULL, NULL, NULL);
    AddWorld(scene, world);
    AddSystem(world, GenerateDrawSystem());
    AddSystem(world, GenerateAudioSystem());
    AddSystem(world, GenerateCollisionSystem());
    AddSystem(world, GenerateAISystem());

    Music ambient = LoadMusicStream("resources/sounds/ghost.mp3");
    ambient.looping = true;

    if (!SpatialInit(ambient.stream.sampleRate, 1 )) {//4096)) {
        EZ_ERROR("Failed to initialize Steam Audio");
    }

    // maze
    g_terrain = CreateEntity(world);
    MeshComponent* mc = AddComponent(g_terrain, MeshComponent, UploadGeometry("resources/models/maze/maze.obj"));
    AddComponent(g_terrain, AudioBarrierComponent, mc->id);

    // maze collision
    LoadBoxColliders(world, "resources/data/colliders.json");

    // navmesh
    g_navigation = UploadNavigationMesh("resources/navigation/maze.obj");
    AddComponent(CreateEntity(world), NavigationComponent, g_navigation, { 0 });

    // lights
    float lval = 0.05f;
    LightID l1 = SubmitLight((SceneLight){{ 0 }, { lval, lval, lval }, { 0, -1.0f, 0 }, 0, 0 });
    LightID l2 = SubmitLight((SceneLight){{ 0 }, { lval*0.8f, lval*0.8f, lval*0.8f }, { 0.0f, 0.0f, 1.0f }, 0, 0 });
    LightID l3 = SubmitLight((SceneLight){{ 0 }, { lval*0.6f, lval*0.6f, lval*0.6f }, { 0.0f, 0.0f, -1.0f }, 0, 0 });
    LightID l4 = SubmitLight((SceneLight){{ 0 }, { lval*0.4f, lval*0.4f, lval*0.4f }, { 1.0f, 0.0f, 0.0f }, 0, 0 });
    LightID l5 = SubmitLight((SceneLight){{ 0 }, { lval*0.2f, lval*0.2f, lval*0.2f }, { -1.0f, 0.0f, 0.0f }, 0, 0 });
    AddComponent(CreateEntity(world), LightComponent, l1);
    AddComponent(CreateEntity(world), LightComponent, l2);
    AddComponent(CreateEntity(world), LightComponent, l3);
    AddComponent(CreateEntity(world), LightComponent, l4);
    AddComponent(CreateEntity(world), LightComponent, l5);

    // pacman
    g_player = CreateEntityP(world, -30.0f, 5.5f, 0.0f);
    AddComponent(g_player, MeshComponent, UploadGeometry("resources/models/pacman/pacman.obj"));
    AddComponent(g_player, CameraComponent, TRUE, {0,0,0}, {0,0,0});
    AddComponent(g_player, AudioListenerComponent, 20);
    AddComponent(g_player, DynamicCollisionComponent, FALSE, {0,0,0}, BOX_COLLIDER);
    *(EntityScale(g_player)) = (Vector3){ 0.75f, 0.75f, 0.75f };
    *(EntityRotation(g_player)) = (Vector3){ 0.0f, 90.0f * DEG2RAD, 0.0f };

    // ghost
    g_enemy = CreateEntityP(world, -37.0f, 5.8f, 0.0f);
    DSPState* dsp = DSPInit(ambient.stream.sampleRate, 2.0f);
    SpatialSource* spatial = SpatialCreateSource();
    AddComponent(g_enemy, MeshComponent, UploadGeometry("resources/models/ghost/ghost.obj"));
    AddComponent(g_enemy, AudioSourceComponent, ambient, dsp, spatial);
    AddComponent(g_enemy, DynamicCollisionComponent, FALSE, {0,0,0}, BOX_COLLIDER);
    AddComponent(g_enemy, NavigationComponent, DuplicateNavigationMesh(g_navigation), { 0 });

    // ghost ai
    BehaviorNode* follow_sequence[] = {
        BehaviorAction(UpdateEnemyPath, NULL),
        BehaviorAction(FollowEnemyPath, NULL)
    };
    BehaviorNode* root = BehaviorSequence(follow_sequence, 2);
    BlackBoard emptybb = { 0 };
    BlackBoardSetBool(&emptybb, "Reset", TRUE);
    BlackBoardSetInt(&emptybb, "Index", 0);
    BlackBoardSetInt(&emptybb, "Count", 0);
    AddComponent(g_enemy, AIComponent, emptybb, root);

    // todo: make less ugly with extern lines
    extern DSPState* g_audio_dsp;
    extern SpatialSource* g_audio_spatial;
    extern unsigned int g_audio_channels;
    extern void DSPProcessorCallback(void* buffer, unsigned int frames);
    g_audio_dsp = dsp;
    g_audio_spatial = spatial;
    g_audio_channels = ambient.stream.channels;
    AttachAudioStreamProcessor(ambient.stream, DSPProcessorCallback);
    PlayMusicStream(ambient);

    return scene;
}

void UpdateTitleScene(World* world, float dt) {
    if (IsKeyPressed(KEY_P)) {
        SetScene("Main");
    }
}

void CleanTitleScene(World* world) {
    UnloadTexture(g_title_texture);
}

Scene* GenerateTitleScene() {
    Scene* scene = GenerateScene("Title");
    World* world = GenerateWorld(NULL, UpdateTitleScene, NULL, NULL, NULL, NULL, CleanTitleScene);
    AddWorld(scene, world);
    AddSystem(world, GenerateDrawSystem());
    g_title_texture = LoadTexture("resources/images/titlescreen.png");
    Entity e = CreateEntityP(world, 800, 450, 0);
    *(EntityScale(e)) = (Vector3){ 1600, 900, 0 };
    AddComponent(e, ImageComponent, g_title_texture);
    return scene;
}

void EchoMain() {
    SubmitExternalShader("build/expanded/audioviz.comp", "build/shaders/audioviz.comp.spv", NUMRAYS);
    SubmitExternalShader("build/expanded/vizfilter.comp", "build/shaders/vizfilter.comp.spv", OVERRIDE_W*OVERRIDE_H);
    SubmitExternalShader("build/expanded/switch.comp", "build/shaders/switch.comp.spv", OVERRIDE_W*OVERRIDE_H);
    g_shaderbuffer = CreateExternalBuffer("AudioRaySSBOIn", NUMRAYS*sizeof(AuthoredRay));
    InitializeApplication("Echo Example", "See you, Space Cowboy", FALSE);
    AddScene(GenerateMainScene());
    AddScene(GenerateTitleScene());
    SetScene("Title");
    RunApplication();
    DestroyApplication();
}
