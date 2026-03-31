#ifndef WORLD_H
#define WORLD_H

#include "systems/system.h"
#include "ecs/registry.h"

#define GetEntities(world, component) RegistryGetEntities(world->registry, (size_t)component##_TYPE)

typedef void (*WorldDrawFunction)(World* world);
typedef void (*WorldUpdateFunction)(World* world, float dt);
typedef void (*WorldKeyEventFunction)(World* world, int key, InputAction action);
typedef void (*WorldMouseButtonEventFunction)(World* world, int key, InputAction action);
typedef void (*WorldMouseScrollEventFunction)(World* world, Vector2 offset);
typedef void (*WorldMouseMoveFunction)(World* world, Vector2 position);
typedef void (*WorldCleanFunction)(World* world);

struct World {
    WorldDrawFunction draw;
    WorldUpdateFunction update;
    WorldKeyEventFunction key;
    WorldMouseButtonEventFunction mousebutton;
    WorldMouseScrollEventFunction mousescroll;
    WorldMouseMoveFunction mousemove;
    WorldCleanFunction clean;
    Registry* registry;
    ARRLIST_SystemPtr systems;
};
DECLARE_ARRLIST_NAMED(WorldPtr, World*);

World* GenerateWorld(
        WorldDrawFunction draw,
        WorldUpdateFunction update,
        WorldKeyEventFunction key,
        WorldMouseButtonEventFunction mousebutton,
        WorldMouseScrollEventFunction mousescroll,
        WorldMouseMoveFunction mousemove,
        WorldCleanFunction clean);

Entity CreateEntity(World* world);

Entity CreateEntityP(World* world, float x, float y, float z);

void AddSystem(World* world, System* system);

void DestroyWorld(World* world);

#endif
