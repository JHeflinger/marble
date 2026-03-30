#include "entity.h"
#include "ecs/components.h"
#include "game/world.h"

void* EntityAddComponent(const Entity e, size_t type, void* component, size_t size) {
    return RegistryEmplaceComponent(e.context->registry, e.id, type, component, size);
}

void* EntityGetComponent(const Entity e, size_t type) {
    return RegistryGetComponent(e.context->registry, e.id, type);
}

BOOL EntityHasComponent(const Entity e, size_t type) {
    return RegistryHasComponent(e.context->registry, e.id, type);
}

void EntityRemoveComponent(const Entity e, size_t type) {
    RegistryRemoveComponent(e.context->registry, e.id, type);
}

Vector3* EntityPosition(const Entity e) {
    EZ_ASSERT(HasComponent(e, TransformComponent), "Entity does not have transform component!");
    TransformComponent* tc = GetComponent(e, TransformComponent);
    return &(tc->translation);
}

Vector3* EntityScale(const Entity e) {
    EZ_ASSERT(HasComponent(e, TransformComponent), "Entity does not have transform component!");
    TransformComponent* tc = GetComponent(e, TransformComponent);
    return &(tc->scale);
}

Vector3* EntityRotation(const Entity e) {
    EZ_ASSERT(HasComponent(e, TransformComponent), "Entity does not have transform component!");
    TransformComponent* tc = GetComponent(e, TransformComponent);
    return &(tc->rotation);
}
