#include "trucker.h"
#include "systems/defined/drawsystem.h"
#include "game/application.h"
#include "ecs/components.h"
#include "ecs/entity.h"

Scene* GenerateCoreScene() {
    Scene* scene = GenerateScene("Core");
    World* world = GenerateWorld(NULL, NULL, NULL, NULL, NULL, NULL);
    AddWorld(scene, world);
    AddSystem(world, GenerateDrawSystem());
    AddComponent(CreateEntity(world), MeshComponent, UploadGeometry("resources/models/bridge/bridge.obj"));
    return scene;
}

void TitleSceneKeyEvent(World* world, int key, InputAction action) {
    if (action == INPUTPRESS && key == KEY_P) {
        AddScene(GenerateCoreScene());
        SetScene("Core");
    }
}

Scene* GenerateTitleScene() {
    Scene* scene = GenerateScene("Title");
    World* world = GenerateWorld(NULL, NULL, TitleSceneKeyEvent, NULL, NULL, NULL);
    AddWorld(scene, world);
    AddSystem(world, GenerateDrawSystem());
    AddComponent(CreateEntity(world), TextComponent, "Press \"P\" To Play!", TEXT_ALIGN_CENTER, CENTER_ANCHOR, WHITE, 20.0f);
    return scene;
}

void TruckerMain() {
    InitializeApplication("Basic Game", "See you, Space Cowboy");
    AddScene(GenerateTitleScene());
    AddScene(GenerateScene("Win"));
    SetScene("Title");
    RunApplication();
    DestroyApplication();
}
