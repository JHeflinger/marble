#ifndef REGISTRY_H
#define REGISTRY_H

#include <easyobjects.h>
#include <easybasics.h>

#define INVALID_ENTITY 0

typedef uint64_t EntityID;
DECLARE_ARRLIST(EntityID);
DECLARE_HASHMAP(EntityID, size_t, EntityMap);

typedef struct {
    ARRLIST_voidPtr dense;
    ARRLIST_EntityID entities;
    HASHMAP_EntityMap sparse;
} ComponentStorage;
DECLARE_HASHMAP(size_t, ComponentStorage*, StorageMap);
DECLARE_ARRLIST_NAMED(ComponentStoragePtr, ComponentStorage*);

typedef struct {
    EntityID next;
    HASHMAP_StorageMap storage;
    ARRLIST_ComponentStoragePtr dense;
    ARRLIST_size_t types;
} Registry;

Registry* GenerateRegistry();

EntityID RegistryCreateEntity(Registry* registry);

void RegistryEraseEntity(Registry* registry, EntityID entity);

void* RegistryEmplaceComponent(Registry* registry, EntityID entity, size_t type, void* component, size_t size);

void* RegistryGetComponent(Registry* registry, EntityID entity, size_t type);

BOOL RegistryHasComponent(Registry* registry, EntityID entity, size_t type);

void RegistryRemoveComponent(Registry* registry, EntityID entity, size_t type);

ARRLIST_EntityID* RegistryGetEntities(Registry* registry, size_t type);

void DestroyRegistry(Registry* registry);

#endif
