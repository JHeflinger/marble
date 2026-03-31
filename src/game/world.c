#include "world.h"
#include "ecs/entity.h"
#include "ecs/components.h"

IMPL_ARRLIST_NAMED(WorldPtr, World*);

World* GenerateWorld(
        WorldDrawFunction draw,
        WorldUpdateFunction update,
        WorldKeyEventFunction key,
        WorldMouseButtonEventFunction mousebutton,
        WorldMouseScrollEventFunction mousescroll,
        WorldMouseMoveFunction mousemove,
        WorldCleanFunction clean) {
    World* world = EZ_ALLOC(1, sizeof(World));
    world->draw = draw;
    world->update = update;
    world->key = key;
    world->mousebutton = mousebutton;
    world->mousescroll = mousescroll;
    world->mousemove = mousemove;
    world->clean = clean;
    world->registry = GenerateRegistry();
    return world;
}

Entity CreateEntity(World* world) {
    Entity e = (Entity){ RegistryCreateEntity(world->registry), world };
    Vector3 zero = (Vector3){ 0 };
    Vector3 one = (Vector3){ 1.0f, 1.0f, 1.0f };
    AddComponent(e, TransformComponent, zero, zero, one);
    return e;
}

Entity CreateEntityP(World* world, float x, float y, float z) {
    Entity e = CreateEntity(world);
    *(EntityPosition(e)) = (Vector3){ x, y, z };
    return e;
}

void AddSystem(World* world, System* system) {
    ARRLIST_SystemPtr_add(&world->systems, system);
    system->context = world;
}

void DestroyWorld(World* world) {
    if (world->clean) world->clean(world);
    for (size_t i = 0; i < world->systems.size; i++)
        DestroySystem(world->systems.data[i]);
    ARRLIST_SystemPtr_clear(&world->systems);
    DestroyRegistry(world->registry);
    EZ_FREE(world);
}
