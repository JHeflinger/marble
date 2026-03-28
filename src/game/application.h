#ifndef APPLICATION_H
#define APPLICATION_H

#include "game/scene.h"
#include <ui/ui.h>

DECLARE_HASHMAP(int, BOOL, KeyMap);

typedef struct {
    const char* name;
    const char* goodbye;
    UI* ui;
    ARRLIST_ScenePtr scenes;
    size_t current;
    HASHMAP_KeyMap keymap;
    ARRLIST_int keylist;

    #ifndef PROD_BUILD
    size_t memory;
    #endif
} Application;

void InitializeApplication(const char* name, const char* goodbye);

void DestroyApplication();

void RunApplication();

void AddScene(Scene* scene);

void SetScene(const char* name);

#endif
