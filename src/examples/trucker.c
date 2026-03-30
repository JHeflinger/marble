#include "trucker.h"
#include "systems/defined/drawsystem.h"
#include "systems/defined/collisionsystem.h"
#include "game/application.h"
#include "ecs/components.h"
#include "ecs/entity.h"
#include <renderer/renderer.h>
#include <raymath.h>

#define UNIQUE_CARS 3
#define NUM_CARS 10

typedef struct {
    Entity e;
    Vector3 offset;
    Vector3 velocity;
    Vector3 force;
} Vehicle;

Entity g_timer;
Entity g_basecars[NUM_CARS];
float g_basescale[UNIQUE_CARS] = { 0.25f, 0.25f, 0.375f };
const char* g_basecarpaths[UNIQUE_CARS] = { "resources/models/cars/car_1.obj", "resources/models/cars/car_2.obj", "resources/models/cars/car_3.obj" };
Vehicle g_player;

void UpdateCar(Vehicle* v, float dt) {
    v->force.y = -9.8f;
    if (v->force.z == 0) v->velocity.z *= 0.99f;
    v->velocity = Vector3Add(v->velocity, Vector3Scale(v->force, dt));
    *EntityPosition(v->e) = Vector3Add(*EntityPosition(v->e), Vector3Scale(EntityOrient(v->e, v->velocity), dt));
    if (EntityPosition(v->e)->y <= v->offset.y) {
        if (IsKeyPressed(KEY_J)) {
            v->velocity.y = 10.0f;
        }
        EntityPosition(v->e)->y = v->offset.y;
        if (v->velocity.y < 0) v->velocity.y *= -0.5f;
    }
    if (IsKeyDown(KEY_W)) v->force.z = 1.0f;
    else if (IsKeyDown(KEY_S)) v->force.z = -1.0f;
    else v->force.z = 0;
    if (IsKeyDown(KEY_A)) EntityRotation(v->e)->y += 0.856f;
    else if (IsKeyDown(KEY_D)) EntityRotation(v->e)->y -= 0.856f;
    Vector3 behind = EntityOrient(v->e, (Vector3){ 0, 5, -10 });
    SimpleCamera c = GetCamera();
    c.look[0] = EntityPosition(v->e)->x;
    c.look[1] = EntityPosition(v->e)->y;
    c.look[2] = EntityPosition(v->e)->z;
    c.position[0] = c.look[0] + behind.x;
    c.position[1] = c.look[1] + behind.y;
    c.position[2] = c.look[2] + behind.z;
    //MoveCamera(c);
}

void UpdateCoreScene(World* world, float dt) {
    UpdateCar(&g_player, dt);
}

Scene* GenerateCoreScene() {
    Scene* scene = GenerateScene("Core");
    World* world = GenerateWorld(NULL, UpdateCoreScene, NULL, NULL, NULL, NULL);
    AddWorld(scene, world);
    AddSystem(world, GenerateDrawSystem());
    AddSystem(world, GenerateCollisionSystem());

    // player
    g_player.e = CreateEntity(world);
    AddComponent(g_player.e, MeshComponent, UploadGeometry("resources/models/truck/truck.obj"));
    AddComponent(g_player.e, DynamicCollisionComponent, 0, {0}, MESH_COLLIDER);
    g_player.offset = (Vector3){ 0, 0.0f, 0 };
    g_player.velocity = (Vector3){ 0 };
    g_player.force = (Vector3){ 0 };

    // base cars
    for (int i = 0; i < NUM_CARS; i++) {
        size_t j = i%UNIQUE_CARS;
        float radius = 10.0f;
        g_basecars[i] = CreateEntityP(world, sin(i + 1) * radius, sin(i + 1) * radius * (rand()%2 == 0 ? -1.0f : 1.0f), cos(i + 1) * radius);
        *(EntityScale(g_basecars[i])) = (Vector3){ g_basescale[j], g_basescale[j], g_basescale[j] };
        AddComponent(g_basecars[i], MeshComponent, UploadGeometry(g_basecarpaths[j]));
        AddComponent(g_basecars[i], StaticCollisionComponent, 0, MESH_COLLIDER);
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
