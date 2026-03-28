#include "application.h"
#include "extensions/eviewport.h"
#include <data/config.h>
#include <data/input.h>
#include <data/colors.h>
#include <data/assets.h>
#include <ui/panels/diagnostics.h>
#include <ui/panels/overview.h>
#include <ui/panels/edit.h>
#include <ui/panels/graph.h>
#include <renderer/renderer.h>
#include <core/dev.h>
#include <core/binds.h>
#include <easymemory.h>
#include <easylogger.h>
#include <easyhash.h>

IMPL_HASHMAP(int, BOOL, KeyMap, ez_hash_int);

Application g_application = { 0 };

void UpdateApplication() {
    UpdateUI(g_application.ui);
    if (g_application.scenes.size > 0) {
        Scene* scene = g_application.scenes.data[g_application.current];
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            int mb = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ? MOUSE_BUTTON_LEFT : MOUSE_BUTTON_RIGHT;
            for (size_t i = 0; i < scene->worlds.size; i++) {
                if (scene->worlds.data[i]->mousebutton)
                    scene->worlds.data[i]->mousebutton(scene->worlds.data[i], mb, INPUTPRESS);
                for (size_t j = 0; j < scene->worlds.data[i]->systems.size; j++)
                    if (scene->worlds.data[i]->systems.data[j]->mousebutton)
                        scene->worlds.data[i]->systems.data[j]->mousebutton(scene->worlds.data[i]->systems.data[j], mb, INPUTPRESS);
            }
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
            int mb = IsMouseButtonReleased(MOUSE_BUTTON_LEFT) ? MOUSE_BUTTON_LEFT : MOUSE_BUTTON_RIGHT;
            for (size_t i = 0; i < scene->worlds.size; i++) {
                if (scene->worlds.data[i]->mousebutton)
                    scene->worlds.data[i]->mousebutton(scene->worlds.data[i], mb, INPUTRELEASE);
                for (size_t j = 0; j < scene->worlds.data[i]->systems.size; j++)
                    if (scene->worlds.data[i]->systems.data[j]->mousebutton)
                        scene->worlds.data[i]->systems.data[j]->mousebutton(scene->worlds.data[i]->systems.data[j], mb, INPUTRELEASE);
            }
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            int mb = IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? MOUSE_BUTTON_LEFT : MOUSE_BUTTON_RIGHT;
            for (size_t i = 0; i < scene->worlds.size; i++) {
                if (scene->worlds.data[i]->mousebutton)
                    scene->worlds.data[i]->mousebutton(scene->worlds.data[i], mb, INPUTDOWN);
                for (size_t j = 0; j < scene->worlds.data[i]->systems.size; j++)
                    if (scene->worlds.data[i]->systems.data[j]->mousebutton)
                        scene->worlds.data[i]->systems.data[j]->mousebutton(scene->worlds.data[i]->systems.data[j], mb, INPUTDOWN);
            }
        }
        Vector2 mdelt = GetMouseDelta();
        Vector2 mscroll = GetMouseWheelMoveV();
        if (mdelt.x != 0 || mdelt.y != 0) {
            for (size_t i = 0; i < scene->worlds.size; i++) {
                if (scene->worlds.data[i]->mousemove)
                    scene->worlds.data[i]->mousemove(scene->worlds.data[i], GetMousePosition());
                for (size_t j = 0; j < scene->worlds.data[i]->systems.size; j++)
                    if (scene->worlds.data[i]->systems.data[j]->mousemove)
                        scene->worlds.data[i]->systems.data[j]->mousemove(scene->worlds.data[i]->systems.data[j], GetMousePosition());
            }
        }
        if (mscroll.x != 0 || mscroll.y != 0) {
            for (size_t i = 0; i < scene->worlds.size; i++) {
                if (scene->worlds.data[i]->mousescroll)
                    scene->worlds.data[i]->mousescroll(scene->worlds.data[i], mscroll);
                for (size_t j = 0; j < scene->worlds.data[i]->systems.size; j++)
                    if (scene->worlds.data[i]->systems.data[j]->mousescroll)
                        scene->worlds.data[i]->systems.data[j]->mousescroll(scene->worlds.data[i]->systems.data[j], mscroll);
            }
        }
        int kcode = 0;
        while ((kcode = GetKeyPressed()) != 0) {
            if (!HASHMAP_KeyMap_has(&g_application.keymap, kcode))
                ARRLIST_int_add(&g_application.keylist, kcode);
            HASHMAP_KeyMap_set(&g_application.keymap, kcode, TRUE);
        }
        for (size_t i = 0; i < g_application.keylist.size; i++) {
            if (!IsKeyDown(g_application.keylist.data[i]))
                HASHMAP_KeyMap_set(&g_application.keymap, g_application.keylist.data[i], FALSE);
            if (HASHMAP_KeyMap_get(&g_application.keymap, g_application.keylist.data[i])) {
                for (size_t j = 0; j < scene->worlds.size; j++) {
                    if (scene->worlds.data[j]->key) {
                        scene->worlds.data[j]->key(scene->worlds.data[j], g_application.keylist.data[i], INPUTDOWN);
                        if (IsKeyPressed(g_application.keylist.data[i]))
                            scene->worlds.data[j]->key(scene->worlds.data[j], g_application.keylist.data[i], INPUTPRESS);
                    }
                    for (size_t k = 0; k < scene->worlds.data[j]->systems.size; k++)
                        if (scene->worlds.data[j]->systems.data[k]->key) {
                            scene->worlds.data[j]->systems.data[k]->key(scene->worlds.data[j]->systems.data[k], g_application.keylist.data[j], INPUTDOWN);
                            if (IsKeyPressed(g_application.keylist.data[i]))
                                scene->worlds.data[j]->systems.data[k]->key(scene->worlds.data[j]->systems.data[k], g_application.keylist.data[j], INPUTPRESS);
                        }
                }
            } else if (IsKeyReleased(g_application.keylist.data[i])) {
                for (size_t j = 0; j < scene->worlds.size; j++) {
                    if (scene->worlds.data[j]->key) {
                        scene->worlds.data[j]->key(scene->worlds.data[j], g_application.keylist.data[i], INPUTRELEASE);
                    }
                    for (size_t k = 0; k < scene->worlds.data[j]->systems.size; k++)
                        if (scene->worlds.data[j]->systems.data[k]->key)
                            scene->worlds.data[j]->systems.data[k]->key(scene->worlds.data[j]->systems.data[k], g_application.keylist.data[j], INPUTRELEASE);
                }
            }
        }
        for (size_t i = 0; i < scene->worlds.size; i++) {
            if (scene->worlds.data[i]->update)
                scene->worlds.data[i]->update(scene->worlds.data[i], GetFrameTime());
            for (size_t j = 0; j < scene->worlds.data[i]->systems.size; j++)
                if (scene->worlds.data[i]->systems.data[j]->update)
                    scene->worlds.data[i]->systems.data[j]->update(scene->worlds.data[i]->systems.data[j], GetFrameTime());
        }
    }
}

void PreRenderApplication() {
    if (g_application.scenes.size > 0) {
        Scene* scene = g_application.scenes.data[g_application.current];
        for (size_t i = 0; i < scene->worlds.size; i++) {
            if (scene->worlds.data[i]->draw)
                scene->worlds.data[i]->draw(scene->worlds.data[i]);
            for (size_t j = 0; j < scene->worlds.data[i]->systems.size; j++)
                if (scene->worlds.data[i]->systems.data[j]->draw)
                    scene->worlds.data[i]->systems.data[j]->draw(scene->worlds.data[i]->systems.data[j]);
        }
    }
    PreRenderUI(g_application.ui);
}

void DrawApplication() {
    ClearBackground(RAYWHITE);
    DrawUI(g_application.ui, 0, 0, GetScreenWidth(), GetScreenHeight());
}

void InitializeApplication(const char* name, const char* goodbye) {
    #ifndef PROD_BUILD
    g_application.memory = EZ_ALLOCATED();
    #endif
    g_application.name = name;
    g_application.goodbye = goodbye;
	SetTraceLogLevel(LOG_NONE);
    SetConfigFlags(FLAG_VSYNC_HINT /*| FLAG_WINDOW_RESIZABLE*/);
    InitWindow(EDITOR_DEFAULT_WIDTH, EDITOR_DEFAULT_HEIGHT, g_application.name);
    InitializeInput();
    InitializeColors();
    InitializeAssets();
    InitializeRenderer();
    g_application.ui = GenerateUI();
    g_application.ui->left = GenerateUI();
    g_application.ui->right = GenerateUI();
    ((UI*)g_application.ui->right)->right = GenerateUI();
    ((UI*)g_application.ui->right)->left = GenerateUI();
    ((UI*)g_application.ui->right)->divide = GetScreenHeight() - 560;
    ((UI*)g_application.ui->right)->vertical = TRUE;
    ((UI*)g_application.ui->left)->right = GenerateUI();
    ((UI*)g_application.ui->left)->left = GenerateUI();
    ((UI*)((UI*)g_application.ui->left)->left)->left = GenerateUI();
    ((UI*)((UI*)g_application.ui->left)->left)->right = GenerateUI();
    ((UI*)((UI*)g_application.ui->left)->left)->divide = GetScreenHeight() - 420;
    ((UI*)((UI*)g_application.ui->left)->left)->vertical = TRUE;
    ((UI*)g_application.ui->left)->divide = 350;
    ARRLIST_Panel_add(&(((UI*)(((UI*)g_application.ui->right)->right))->panels), GenerateDiagnosticsPanel());
    ARRLIST_Panel_add(&(((UI*)(((UI*)g_application.ui->right)->left))->panels), GenerateOverviewPanel());
    ARRLIST_Panel_add(&(((UI*)(((UI*)g_application.ui->left)->right))->panels), GenerateEViewportPanel());
    ARRLIST_Panel_add(&(GetLeftUI(GetLeftUI(GetLeftUI(g_application.ui)))->panels), GenerateEditPanel());
    ARRLIST_Panel_add(&(GetRightUI(GetLeftUI(GetLeftUI(g_application.ui)))->panels), GenerateGraphPanel());
    g_application.ui->divide = 1250;
    SetPrimaryUI(g_application.ui);
    DevInitialize();
}

void DestroyApplication() {
    CleanBinds();
    DestroyUI(g_application.ui);
    DestroyAssets();
    DestroyRenderer();
    CloseWindow();
    HASHMAP_KeyMap_clear(&g_application.keymap);
    ARRLIST_int_clear(&g_application.keylist);
    for (size_t i = 0; i < g_application.scenes.size; i++) DestroyScene(g_application.scenes.data[i]);
    ARRLIST_ScenePtr_clear(&g_application.scenes);
    #ifndef PROD_BUILD
    EZ_ASSERT(g_application.memory == EZ_ALLOCATED(), "Memory cleanup revealed a leak of %d bytes", (int)(EZ_ALLOCATED() - g_application.memory));
    #endif
    EZ_INFO("%s", g_application.goodbye);
}

void RunApplication() {
    while(!WindowShouldClose()) {
        DevUpdate();
        UpdateApplication();
        if (!UIRequestsBlockInput()) ListenBinds();
        PreRenderApplication();
        BeginDrawing();
        DrawApplication();
        EndDrawing();
    }
}

void AddScene(Scene* scene) {
    ARRLIST_ScenePtr_add(&g_application.scenes, scene);
}

void SetScene(const char* name) {
    for (size_t i = 0; i < g_application.scenes.size; i++) {
        if (strcmp(g_application.scenes.data[i]->name, name) == 0) {
            g_application.current = i;
            return;
        }
    }
    EZ_ASSERT(FALSE, "Unable to set the scene \"%s\" that does not exist", name);
}
