#include "ghosts.h"
#include "systems/defined/drawsystem.h"
#include "systems/defined/collisionsystem.h"
#include "systems/defined/aisystem.h"
#include "game/application.h"
#include "util/navigation.h"
#include "ecs/components.h"
#include "ecs/entity.h"
#include "data/boxloader.h"
#include <renderer/renderer.h>
#include <raymath.h>

static NavigationMesh g_navigation;
static Entity g_maze;
static Entity g_floor;
static Entity g_pacman;
static Entity g_ghost;
static BOOL g_start = FALSE;

AIStatus UpdateGhostPath(void* ctx, BlackBoard* blackboard) {
    if (IsKeyPressed(KEY_S)) {
        g_start = TRUE;
        BlackBoardSetBool(blackboard, "Reset", TRUE);
    }
    if (!g_start) return AI_FAILURE;
    BlackBoardSetInt(blackboard, "Count", BlackBoardGetInt(blackboard, "Count") + 1);
    if (g_start && BlackBoardGetInt(blackboard, "Count")%120 == 0) BlackBoardSetBool(blackboard, "Reset", TRUE);
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
    int path_size = (int)nc->path.size;
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
    Vector3* ghost_pos = EntityPosition(g_ghost);
    float dx = steering_target.x - ghost_pos->x;
    float dz = steering_target.z - ghost_pos->z;
    float len = sqrtf(dx * dx + dz * dz);
    if (len > 0.001f) {
        ghost_pos->x += (dx / len) * ghost_speed * GetFrameTime();
        ghost_pos->z += (dz / len) * ghost_speed * GetFrameTime();
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

    // collision
    LoadBoxColliders(world, "resources/data/colliders.json");

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
    AddComponent(g_ghost, DynamicCollisionComponent, FALSE, {0,0,0}, CYLINDER_COLLIDER);
    *(EntityScale(g_ghost)) = (Vector3){ 0.5f, 0.5f, 0.5f };
    
    // ghost ai
    BehaviorNode* follow_sequence[] = {
        BehaviorAction(UpdateGhostPath, NULL),
        BehaviorAction(FollowGhostPath, NULL)
    };
    BehaviorNode* root = BehaviorSequence(follow_sequence, 2);
    BlackBoard emptybb = { 0 };
    BlackBoardSetBool(&emptybb, "Reset", TRUE);
    BlackBoardSetInt(&emptybb, "Index", 0);
    BlackBoardSetInt(&emptybb, "Count", 0);
    AddComponent(g_ghost, AIComponent, emptybb, root);

    return scene;
}

void GhostsMain() {
    InitializeApplication("Ghosts Example", "See you, Space Cowboy", TRUE);
    AddScene(GenerateMazeScene());
    SetScene("Maze");
    RunApplication();
    DestroyApplication();
}
