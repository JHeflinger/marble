#ifndef WORLD_H
#define WORLD_H

#include "systems/system.h"
#include "ecs/registry.h"

#define GetEntities(world, component) RegistryGetEntities(world->registry, (size_t)component##_TYPE)

typedef void (*DrawFunction)(World* world);
typedef void (*UpdateFunction)(World* world, float dt);
typedef void (*KeyEventFunction)(World* world, int key, InputAction action);
typedef void (*MouseButtonEventFunction)(World* world, int key, InputAction action);
typedef void (*MouseScrollEventFunction)(World* world, Vector2 offset);
typedef void (*MouseMoveFunction)(World* world, Vector2 position);

struct World {
    DrawFunction draw;
    UpdateFunction update;
    KeyEventFunction key;
    MouseButtonEventFunction mousebutton;
    MouseScrollEventFunction mousescroll;
    MouseMoveFunction mousemove;
    Registry* registry;
    ARRLIST_SystemPtr systems;
};
DECLARE_ARRLIST_NAMED(WorldPtr, World*);

World* GenerateWorld(
        DrawFunction draw,
        UpdateFunction update,
        KeyEventFunction key,
        MouseButtonEventFunction mousebutton,
        MouseScrollEventFunction mousescroll,
        MouseMoveFunction mousemove);

Entity CreateEntity(World* world);

void AddSystem(World* world, System* system);

void DestroyWorld(World* world);

#endif
