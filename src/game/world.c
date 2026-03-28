#include "world.h"
#include "ecs/entity.h"
#include "ecs/components.h"

IMPL_ARRLIST_NAMED(WorldPtr, World*);

World* GenerateWorld(
        DrawFunction draw,
        UpdateFunction update,
        KeyEventFunction key,
        MouseButtonEventFunction mousebutton,
        MouseScrollEventFunction mousescroll,
        MouseMoveFunction mousemove) {
    World* world = EZ_ALLOC(1, sizeof(World));
    world->draw = draw;
    world->update = update;
    world->key = key;
    world->mousebutton = mousebutton;
    world->mousescroll = mousescroll;
    world->mousemove = mousemove;
    world->registry = GenerateRegistry();
    return world;
}

Entity CreateEntity(World* world) {
    Entity e = (Entity){ RegistryCreateEntity(world->registry), world };
    AddComponent(e, TransformComponent);
    return e;
}

void AddSystem(World* world, System* system) {
    ARRLIST_SystemPtr_add(&world->systems, system);
    system->context = world;
}

void DestroyWorld(World* world) {
    for (size_t i = 0; i < world->systems.size; i++)
        DestroySystem(world->systems.data[i]);
    ARRLIST_SystemPtr_clear(&world->systems);
    DestroyRegistry(world->registry);
    EZ_FREE(world);
}
