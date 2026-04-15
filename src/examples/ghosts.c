#include "ghosts.h"
#include "systems/defined/drawsystem.h"
#include "systems/defined/collisionsystem.h"
#include "systems/defined/aisystem.h"
#include "game/application.h"
#include "util/navigation.h"
#include "ecs/components.h"
#include "ecs/entity.h"
#include <renderer/renderer.h>
#include <raymath.h>

NavigationMesh g_navigation;
Entity g_maze;
Entity g_floor;
Entity g_pacman;
Entity g_ghost;
BOOL g_start = FALSE;

AIStatus UpdateGhostPath(void* ctx, BlackBoard* blackboard) {
    if (IsKeyPressed(KEY_S)) {
        g_start = TRUE;
        BlackBoardSetBool(blackboard, "Reset", TRUE);
    }
    if (!g_start) return AI_FAILURE;
    BOOL reset = BlackBoardGetBool(blackboard, "Reset");
    if (reset) {
        BlackBoardSetInt(blackboard, "Index", 0);
        BlackBoardSetBool(blackboard, "Reset", FALSE);
        for (TriangleID i = g_navigation.start; i < g_navigation.end; i++)
            TriangleReference(i)->material = 1;
        Navigate(g_ghost, *(EntityPosition(g_pacman)));
        NavigationComponent* nc = GetComponent(g_ghost, NavigationComponent);
        for (size_t i = 0; i < nc->path.size; i++)
            TriangleReference(nc->mesh.start + nc->path.data[i])->material = 2;
        UpdateTriangles();
    }
    return AI_SUCCESS;
}

AIStatus FollowGhostPath(void* ctx, BlackBoard* blackboard) {
    float ghost_speed = 10.0f;
    int index = BlackBoardGetInt(blackboard, "Index");
    NavigationComponent* nc = GetComponent(g_ghost, NavigationComponent);
    Vector3 centroid = TriangleCentroid(nc->mesh.start + nc->path.data[index]);
    Vector2 diffxz = (Vector2){ centroid.x - EntityPosition(g_ghost)->x, centroid.z - EntityPosition(g_ghost)->z };
    float len = Vector2Length(diffxz);
    diffxz = Vector2Normalize(diffxz);
    EntityPosition(g_ghost)->x += diffxz.x * ghost_speed * GetFrameTime();
    EntityPosition(g_ghost)->z += diffxz.y * ghost_speed * GetFrameTime();
    if (len < 0.01f * ghost_speed) {
        if ((size_t)index + 1 >= nc->path.size) index--;
        BlackBoardSetInt(blackboard, "Index", index + 1);
    }
    return AI_RUNNING;
}

void UpdateMazeScene(World* world, float dt) {
}

Scene* GenerateMazeScene() {
    Scene* scene = GenerateScene("Maze");
    World* world = GenerateWorld(NULL, UpdateMazeScene, NULL, NULL, NULL, NULL, NULL);
    AddWorld(scene, world);
    AddSystem(world, GenerateDrawSystem());
    AddSystem(world, GenerateCollisionSystem());
    AddSystem(world, GenerateAISystem());

    // maze
    g_maze = CreateEntity(world);
    AddComponent(g_maze, MeshComponent, UploadGeometry("resources/models/maze/maze.obj"));
    g_navigation = UploadNavigationMesh("resources/navigation/maze.obj");

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
    AddComponent(g_floor, NavigationComponent, g_navigation, { 0 });

    // pacman
    g_pacman = CreateEntityP(world, 37.0f, 5.5f, 0.0f);
    AddComponent(g_pacman, MeshComponent, UploadGeometry("resources/models/pacman/pacman.obj"));
    *(EntityScale(g_pacman)) = (Vector3){ 0.75f, 0.75f, 0.75f };

    // ghost
    g_ghost = CreateEntityP(world, -37.0f, 5.8f, 0.0f);
    AddComponent(g_ghost, MeshComponent, UploadGeometry("resources/models/ghost/ghost.obj"));
    AddComponent(g_ghost, NavigationComponent, DuplicateNavigationMesh(g_navigation), { 0 });

    // ghost ai
    BehaviorNode* follow_sequence[] = {
        BehaviorAction(UpdateGhostPath, NULL),
        BehaviorAction(FollowGhostPath, NULL)
    };
    BehaviorNode* root = BehaviorSequence(follow_sequence, 2);
    BlackBoard emptybb = { 0 };
    BlackBoardSetBool(&emptybb, "Reset", TRUE);
    BlackBoardSetInt(&emptybb, "Index", 0);
    AddComponent(g_ghost, AIComponent, emptybb, root);

    return scene;
}

void GhostsMain() {
    InitializeApplication("Ghosts Example", "See you, Space Cowboy");
    AddScene(GenerateMazeScene());
    SetScene("Maze");
    RunApplication();
    DestroyApplication();
}
