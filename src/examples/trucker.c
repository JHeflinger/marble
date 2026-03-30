#include "trucker.h"
#include "systems/defined/drawsystem.h"
#include "game/application.h"
#include "ecs/components.h"
#include "ecs/entity.h"
#include <renderer/renderer.h>

Entity g_player;
Entity g_timer;
Entity g_basecars[3];
Entity g_bridge[10];
float g_basescale[3] = { 0.25f, 0.25f, 0.375f };
float g_maxpos = 9.0f * 27.5f;
const char* g_basecarpaths[3] = { "resources/models/cars/car_1.obj", "resources/models/cars/car_2.obj", "resources/models/cars/car_3.obj" };
float g_gforce = 0.0f;
float g_sidespeed = 1.0f;
float g_drivespeed = 10.0f;
float g_gain = 0.1f;
float g_crashforce = 0.0f;

void UpdatePlayer(float dt) {
    if (EntityPosition(g_player)->z == 0) {
        g_gforce = 0.0f;
        g_sidespeed = 1.0f;
        g_drivespeed = 10.0f;
        g_gain = 0.1f;
        g_crashforce = 0.0f;
    }
    g_gforce += -9.8f * dt;
    g_drivespeed += g_gain * dt;
    g_sidespeed += g_gain * dt * 0.9f;
    EntityPosition(g_player)->y += g_gforce * dt;
    EntityPosition(g_player)->x += g_crashforce * dt;
    if (EntityPosition(g_player)->y <= EntityScale(g_player)->y / 2.0f && g_crashforce == 0.0f) {
        g_gforce = 0.0f;
        EntityPosition(g_player)->y = 0.0f;
    }
    vec3 move = { 0, 0, 1 };
    if (IsKeyDown(KEY_A) && g_crashforce == 0.0f) move[0] = 1.0f;
    if (IsKeyDown(KEY_D) && g_crashforce == 0.0f) move[0] = -1.0f;
    if (g_crashforce == 0.0f && (EntityPosition(g_player)->x < -5.5f || EntityPosition(g_player)->x > 6.0f))
        g_crashforce = g_sidespeed * move[0] * 0.5f;
    glm_vec3_normalize(move);
    glm_vec3_scale(move, 3.0f, move);
    move[2] = g_drivespeed;
    EntityPosition(g_player)->x += move[0] * dt * g_sidespeed;
    EntityPosition(g_player)->z += move[2] * dt * g_sidespeed;
    if (IsKeyDown(KEY_SPACE) && EntityPosition(g_player)->y <= 0.05f) g_gforce = 0.75f * g_drivespeed;
    if (g_crashforce != 0) {
        EntityRotation(g_player)->y += dt * g_crashforce * 0.3f;
        EntityRotation(g_player)->z -= dt * g_crashforce;
    }
    SimpleCamera c = GetCamera();
    c.look[0] = EntityPosition(g_player)->x;
    c.look[1] = EntityPosition(g_player)->y;
    c.look[2] = EntityPosition(g_player)->z;
    c.position[0] += move[0] * dt * g_sidespeed;
    c.position[2] += move[2] * dt * g_sidespeed;
    MoveCamera(c);
}

void UpdateCoreScene(World* world, float dt) {
    //UpdatePlayer(dt);
}

Scene* GenerateCoreScene() {
    Scene* scene = GenerateScene("Core");
    World* world = GenerateWorld(NULL, UpdateCoreScene, NULL, NULL, NULL, NULL);
    AddWorld(scene, world);
    AddSystem(world, GenerateDrawSystem());

    // player
    g_player = CreateEntity(world);
    AddComponent(g_player, MeshComponent, UploadGeometry("resources/models/truck/truck.obj"));

    // base cars
    for (int i = 0; i < 3; i++) {
        g_basecars[i] = CreateEntityP(world, 0, (i + 1)*2.0f, 0);
        *(EntityScale(g_basecars[i])) = (Vector3){ g_basescale[i], g_basescale[i], g_basescale[i] };
        AddComponent(g_basecars[i], MeshComponent, UploadGeometry(g_basecarpaths[i]));
    }

    // bridge
    for (int i = 0; i < 10; i++) {
        g_bridge[i] = CreateEntityP(world, 0, -4.3f, i*27.5f);
        *(EntityScale(g_bridge[i])) = (Vector3){ 1.75f, 1.75f, 1.75f };
        AddComponent(g_bridge[i], MeshComponent, UploadGeometry("resources/models/bridge/bridge.obj"));
    }

    // timer
    g_timer = CreateEntityP(world, 5, 20, 0);
    AddComponent(g_timer, TextComponent, "SCORE: 0", TEXT_ALIGN_LEFT, TL_ANCHOR, WHITE, 20.0f);

    return scene;
}

void TitleSceneKeyEvent(World* world, int key, InputAction action) {
    if (action == INPUTPRESS && key == KEY_P) {
        AddScene(GenerateCoreScene());
        SetScene("Core");
    }
}

Scene* GenerateTitleScene() {
    Scene* scene = GenerateScene("Title");
    World* world = GenerateWorld(NULL, NULL, TitleSceneKeyEvent, NULL, NULL, NULL);
    AddWorld(scene, world);
    AddSystem(world, GenerateDrawSystem());
    AddComponent(CreateEntity(world), TextComponent, "Press \"P\" To Play!", TEXT_ALIGN_CENTER, CENTER_ANCHOR, WHITE, 20.0f);
    return scene;
}

void TruckerMain() {
    InitializeApplication("Basic Game", "See you, Space Cowboy");
    AddScene(GenerateTitleScene());
    AddScene(GenerateScene("Win"));
    SetScene("Title");
    RunApplication();
    DestroyApplication();
}
