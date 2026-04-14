#include "ghosts.h"
#include "systems/defined/drawsystem.h"
#include "systems/defined/collisionsystem.h"
#include "game/application.h"
#include "util/navigation.h"
#include "ecs/components.h"
#include "ecs/entity.h"
#include <renderer/renderer.h>
#include <raymath.h>

Entity g_maze;
Entity g_floor;

void UpdateMazeScene(World* world, float dt) {
    NavigationComponent* nc = GetComponent(g_floor, NavigationComponent);
    FindPath(
        &(nc->mesh),
        (Vector3){ 38.5f, 4.0f, -0.5f },
        (Vector3){ -38.5f, 4.0f, -0.5f },
        &(nc->path));
    for (size_t i = 0; i < nc->path.size; i++) {
        Triangle* tref = TriangleReference(nc->mesh.start + nc->path.data[i]);
        tref->material = 2;
    }
}

Scene* GenerateMazeScene() {
    Scene* scene = GenerateScene("Maze");
    World* world = GenerateWorld(NULL, UpdateMazeScene, NULL, NULL, NULL, NULL, NULL);
    AddWorld(scene, world);
    AddSystem(world, GenerateDrawSystem());
    AddSystem(world, GenerateCollisionSystem());

    // maze
    g_maze = CreateEntity(world);
    AddComponent(g_maze, MeshComponent, UploadGeometry("resources/models/maze/maze.obj"));

    // lights
    LightID l1 = SubmitLight((SceneLight){{ 0 }, { 1, 1, 1 }, { 0, -1.0f, 0 }, 0, 0 });
    LightID l2 = SubmitLight((SceneLight){{ 0 }, { 0.8f, 0.8f, 0.8f }, { 0.0f, 0.0f, 1.0f }, 0, 0 });
    LightID l3 = SubmitLight((SceneLight){{ 0 }, { 0.6f, 0.6f, 0.6f }, { 0.0f, 0.0f, -1.0f }, 0, 0 });
    LightID l4 = SubmitLight((SceneLight){{ 0 }, { 0.4f, 0.4f, 0.4f }, { 1.0f, 0.0f, 0.0f }, 0, 0 });
    LightID l5 = SubmitLight((SceneLight){{ 0 }, { 0.2f, 0.2f, 0.2f }, { -1.0f, 0.0f, 0.0f }, 0, 0 });
    AddComponent(CreateEntity(world), LightComponent, l1);
    AddComponent(CreateEntity(world), LightComponent, l2);
    AddComponent(CreateEntity(world), LightComponent, l3);
    AddComponent(CreateEntity(world), LightComponent, l4);
    AddComponent(CreateEntity(world), LightComponent, l5);

    // navmesh
    g_floor = CreateEntity(world);
    AddComponent(g_floor, NavigationComponent, UploadNavigationMesh("resources/navigation/maze.obj"), { 0 });

    return scene;
}

void GhostsMain() {
    InitializeApplication("Ghosts Example", "See you, Space Cowboy");
    AddScene(GenerateMazeScene());
    SetScene("Maze");
    RunApplication();
    DestroyApplication();
}
