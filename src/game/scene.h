#ifndef SCENE_H
#define SCENE_H

#include "game/world.h"

typedef struct {
    const char* name;
    ARRLIST_WorldPtr worlds;
} Scene;
DECLARE_ARRLIST_NAMED(ScenePtr, Scene*);

Scene* GenerateScene(const char* name);

void DestroyScene(Scene* scene);

void AddWorld(Scene* scene, World* world);

#endif
