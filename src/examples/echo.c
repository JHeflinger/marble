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
#define NUM_GHOSTS 3

static NavigationMesh g_navigation;
static ShaderBuffer* g_shaderbuffer = NULL;
static Entity g_terrain;
static Entity g_player;
static BOOL g_start = TRUE;
static Texture2D g_title_texture = { 0 };
static Texture2D g_retry_texture = { 0 };
static Texture2D g_end_texture = { 0 };

typedef struct {
    alignas(16) vec3 position;
    alignas(16) vec3 direction;
    alignas(4) uint32_t author;
    alignas(4) uint32_t color; // 0 white, 1 red
    alignas(4) float fade;
} AuthoredRay;

static Vector3 g_player_start = { -30.0f, 5.5f, 0.0f };

static Entity g_enemies[NUM_GHOSTS];
static Vector3 g_enemy_starts[NUM_GHOSTS] = {
    { -37.0f, 5.8f,   0.0f },
    {   0.0f, 5.8f, -37.0f },
    {   0.0f, 5.8f,  37.0f }
};

void ResetGame() {
    *(EntityPosition(g_player)) = g_player_start;
    *(EntityRotation(g_player)) = (Vector3){ 0.0f, 90.0f * DEG2RAD, 0.0f };

    for (int i = 0; i < NUM_GHOSTS; i++) {
        *(EntityPosition(g_enemies[i])) = g_enemy_starts[i];
        *(EntityRotation(g_enemies[i])) = (Vector3){ 0 };

        AIComponent* ai = GetComponent(g_enemies[i], AIComponent);
        BlackBoardSetBool(&ai->blackboard, "Reset", TRUE);
        BlackBoardSetInt(&ai->blackboard, "Index", 0);
        BlackBoardSetInt(&ai->blackboard, "Count", 0);
    }
}

AIStatus UpdateEnemyPath(void* ctx, BlackBoard* blackboard) {
    Entity ghost = *(Entity*)ctx;
    if (!g_start) return AI_FAILURE;
    BlackBoardSetInt(blackboard, "Count", BlackBoardGetInt(blackboard, "Count") + 1);
    if (g_start && BlackBoardGetInt(blackboard, "Count")%30 == 0) BlackBoardSetBool(blackboard, "Reset", TRUE);
    BOOL reset = BlackBoardGetBool(blackboard, "Reset");
    if (reset) {
        BlackBoardSetInt(blackboard, "Index", 0);
        BlackBoardSetBool(blackboard, "Reset", FALSE);
        Navigate(ghost, *(EntityPosition(g_player)));
        UpdateTriangles();
    }
    return AI_SUCCESS;
}

AIStatus FollowEnemyPath(void* ctx, BlackBoard* blackboard) {
    Entity ghost = *(Entity*)ctx;
    float ghost_speed = 2.0f;
    int index = BlackBoardGetInt(blackboard, "Index");
    NavigationComponent* nc = GetComponent(ghost, NavigationComponent);
    int path_size = (int)nc->path.size;
    Vector3 toplayer = Vector3Subtract(*(EntityPosition(g_player)), *(EntityPosition(ghost)));
    if (path_size == 1 || Vector3Length(toplayer) < 5.0f) {
        ghost_speed = 8.0f;
        toplayer.y = 0;
        toplayer = Vector3Normalize(toplayer);
        EntityRotation(ghost)->y = (atan2(toplayer.x, toplayer.z) * RAD2DEG) - 90.0f;
        toplayer = Vector3Scale(toplayer, ghost_speed * GetFrameTime());
        *(EntityPosition(ghost)) = Vector3Add(*(EntityPosition(ghost)), toplayer);
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
    Vector3* ghost_pos = EntityPosition(ghost);
    float dx = steering_target.x - ghost_pos->x;
    float dz = steering_target.z - ghost_pos->z;
    float len = sqrtf(dx * dx + dz * dz);
    if (len > 0.0001f) {
        Vector2 dir = { dx / len, dz / len };
        ghost_pos->x += dir.x * ghost_speed * GetFrameTime();
        ghost_pos->z += dir.y * ghost_speed * GetFrameTime();
        float newrot = (atan2(dir.x, dir.y) * RAD2DEG) - 90.0f;
        EntityRotation(ghost)->y += (newrot - EntityRotation(ghost)->y) / 10.0f;
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
    for (int i = 0; i < NUM_GHOSTS; i++) {
        Vector3 to_ghost = Vector3Subtract(*(EntityPosition(g_enemies[i])), *(EntityPosition(g_player)));
        if (Vector3Length(to_ghost) < 1.5f) {
            SetScene("GameOver");
            return;
        }
    }

    Vector3 goal = { -39.0f, 5.8f, 0.0f }; // end of maze
    Vector3 to_goal = Vector3Subtract(goal, *(EntityPosition(g_player)));
    if (Vector3Length(to_goal) < 1.5f) {
        SetScene("End");
        return;
    }

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

    for (int i = 0; i < NUM_GHOSTS; i++) {
        AudioSourceComponent* asc = GetComponent(g_enemies[i], AudioSourceComponent);
        if (asc) UpdateMusicStream(asc->music);
    }

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
        inline void fibbyrays(AuthoredRay* rays, size_t count, Vector3 p, float level, uint32_t color, size_t mid) {
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

        size_t ghost_rpr = RPR / NUM_GHOSTS;
        for (int g = 0; g < NUM_GHOSTS; g++) {
            size_t mid = GetComponent(g_enemies[g], MeshComponent)->id;
            fibbyrays(g_shaderbuffer->data + (rotation * RPR * sizeof(AuthoredRay)) + (g * ghost_rpr * sizeof(AuthoredRay)),
                      ghost_rpr, *(EntityPosition(g_enemies[g])), 1.0f, 1, mid);
        }
        size_t player_mid = GetComponent(g_player, MeshComponent)->id;
        fibbyrays(g_shaderbuffer->data + ((NUMRAYS/2) * sizeof(AuthoredRay)) + (rotation * RPR * sizeof(AuthoredRay)), RPR, *(EntityPosition(g_player)), fmin(pow(MicVolume(), 3.0f) * 40.0f, 1.0f), 0, player_mid);
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
    g_player = CreateEntityP(world, 37.0f, 5.5f, 0.0f);
    AddComponent(g_player, MeshComponent, UploadGeometry("resources/models/pacman/pacman.obj"));
    AddComponent(g_player, CameraComponent, TRUE, {0,0,0}, {0,0,0});
    AddComponent(g_player, AudioListenerComponent, 20);
    AddComponent(g_player, DynamicCollisionComponent, FALSE, {0,0,0}, BOX_COLLIDER);
    *(EntityScale(g_player)) = (Vector3){ 0.75f, 0.75f, 0.75f };
    *(EntityRotation(g_player)) = (Vector3){ 0.0f, 90.0f * DEG2RAD, 0.0f };

    SpatialInit(ambient.stream.sampleRate, 1);
    extern unsigned int g_audio_channels;
    g_audio_channels = ambient.stream.channels;
    extern DSPState* g_audio_dsps[];
    extern SpatialSource* g_audio_spatials[];
    extern void DSPProcessorCallback0(void*, unsigned int);
    extern void DSPProcessorCallback1(void*, unsigned int);
    extern void DSPProcessorCallback2(void*, unsigned int);
    void (*callbacks[])(void*, unsigned int) = {
        DSPProcessorCallback0, DSPProcessorCallback1, DSPProcessorCallback2
    };

    // ghosts
    for (int i = 0; i < NUM_GHOSTS; i++) {
        g_enemies[i] = CreateEntityP(world, g_enemy_starts[i].x, g_enemy_starts[i].y, g_enemy_starts[i].z);
        AddComponent(g_enemies[i], MeshComponent, UploadGeometry("resources/models/ghost/ghost.obj"));
        AddComponent(g_enemies[i], DynamicCollisionComponent, FALSE, {0,0,0}, BOX_COLLIDER);
        AddComponent(g_enemies[i], NavigationComponent, DuplicateNavigationMesh(g_navigation), { 0 });

        // each ghost gets its own audio stream + dsp + spatial
        Music ghost_music = LoadMusicStream("resources/sounds/ghost.mp3");
        ghost_music.looping = true;
        DSPState* dsp = DSPInit(ghost_music.stream.sampleRate, 2.0f);
        SpatialSource* spatial = SpatialCreateSource();
        AddComponent(g_enemies[i], AudioSourceComponent, ghost_music, dsp, spatial);

        g_audio_dsps[i] = dsp;
        g_audio_spatials[i] = spatial;
        AttachAudioStreamProcessor(ghost_music.stream, callbacks[i]);
        PlayMusicStream(ghost_music);
        SeekMusicStream(ghost_music, i * 1.7f); // offset so they don't sync up

        BehaviorNode* follow_sequence[] = {
            BehaviorAction(UpdateEnemyPath, &g_enemies[i]),
            BehaviorAction(FollowEnemyPath, &g_enemies[i])
        };
        BehaviorNode* root = BehaviorSequence(follow_sequence, 2);
        BlackBoard emptybb = { 0 };
        BlackBoardSetBool(&emptybb, "Reset", TRUE);
        BlackBoardSetInt(&emptybb, "Index", 0);
        BlackBoardSetInt(&emptybb, "Count", 0);
        AddComponent(g_enemies[i], AIComponent, emptybb, root);
    }

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
void CleanGameOverScene(World* world) {
    UnloadTexture(g_retry_texture);
}
void CleanEndScene(World* world) {
    UnloadTexture(g_end_texture);
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

void UpdateGameOverScene(World* world, float dt) {
    if (IsKeyPressed(KEY_P)) {
        ResetGame();
        SetScene("Main");
    }
}

Scene* GenerateGameOverScene() {
    Scene* scene = GenerateScene("GameOver");
    World* world = GenerateWorld(NULL, UpdateGameOverScene, NULL, NULL, NULL, NULL, CleanGameOverScene);
    AddWorld(scene, world);
    AddSystem(world, GenerateDrawSystem());
    g_retry_texture = LoadTexture("resources/images/death.png");
    Entity e = CreateEntityP(world, 800, 450, 0);
    *(EntityScale(e)) = (Vector3){ 1600, 900, 0 };
    AddComponent(e, ImageComponent, g_retry_texture);
    return scene;
}

void UpdateEndScene(World* world, float dt) {
    if (IsKeyPressed(KEY_P)) {
        ResetGame();
        SetScene("Main");
    }
}

Scene* GenerateEndScene() {
    Scene* scene = GenerateScene("End");
    World* world = GenerateWorld(NULL, UpdateGameOverScene, NULL, NULL, NULL, NULL, CleanEndScene);
    AddWorld(scene, world);
    AddSystem(world, GenerateDrawSystem());
    g_end_texture = LoadTexture("resources/images/end.png");
    Entity e = CreateEntityP(world, 800, 450, 0);
    *(EntityScale(e)) = (Vector3){ 1600, 900, 0 };
    AddComponent(e, ImageComponent, g_end_texture);
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
    AddScene(GenerateGameOverScene());
    AddScene(GenerateEndScene());
    SetScene("Title");
    RunApplication();
    DestroyApplication();
}
