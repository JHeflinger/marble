#include "scene.h"

IMPL_ARRLIST_NAMED(ScenePtr, Scene*);

Scene* GenerateScene(const char* name) {
    Scene* scene = EZ_ALLOC(1, sizeof(Scene));
    scene->name = name;
    return scene;
}

void DestroyScene(Scene* scene) {
    for (size_t i = 0; i < scene->worlds.size; i++) DestroyWorld(scene->worlds.data[i]);
    ARRLIST_WorldPtr_clear(&scene->worlds);
    EZ_FREE(scene);
}

void AddWorld(Scene* scene, World* world) {
    ARRLIST_WorldPtr_add(&scene->worlds, world);
}
