#include "echo.h"
#include "systems/defined/drawsystem.h"
#include "systems/defined/audiosystem.h"
#include "game/application.h"
#include "ecs/components.h"
#include "ecs/entity.h"
#include "audio/dsp.h"
#include "audio/spatial.h"
#include <renderer/renderer.h>
#include <raymath.h>

Entity g_terrain;
Entity g_player;
Entity g_enemy;

void UpdateMainScene(World* scene, float dt) {
    CameraComponent* cc = GetComponent(g_player, CameraComponent);
    MeshDescriptor* md = MeshReference(GetComponent(g_player, MeshComponent)->id);
    if (IsKeyPressed(KEY_P)) cc->enabled = !cc->enabled;
    if (cc->enabled) {
        md->disabled = TRUE;
        Vector3 translate = { 0 };
        if (IsKeyDown(KEY_D)) translate.x += dt * 10.0f;
        if (IsKeyDown(KEY_A)) translate.x -= dt * 10.0f;
        if (IsKeyDown(KEY_W)) translate.z -= dt * 10.0f;
        if (IsKeyDown(KEY_S)) translate.z += dt * 10.0f;
        Vector2 md = GetMouseDelta();
        Vector3 rotate = { md.y * dt * -0.15f, md.x * dt * -0.15f, 0 };
        RotateEntityFPV(g_player, rotate);
        MoveFlatEntityFPV(g_player, translate);
        DisableCursor();
    } else {
        md->disabled = FALSE;
    }
    AudioSourceComponent* asc = GetComponent(g_enemy, AudioSourceComponent);
    if (asc) UpdateMusicStream(asc->music);

}

Scene* GenerateMainScene() {
    Scene* scene = GenerateScene("Main");
    World* world = GenerateWorld(NULL, UpdateMainScene, NULL, NULL, NULL, NULL, NULL);
    AddWorld(scene, world);
    AddSystem(world, GenerateDrawSystem());
    AddSystem(world, GenerateAudioSystem());

    Music ambient = LoadMusicStream("resources/sounds/click1.mp3");
    ambient.looping = true;

    if (!SpatialInit(ambient.stream.sampleRate, 1 )) {//4096)) {
        EZ_ERROR("Failed to initialize Steam Audio");
    }

    // maze
    g_terrain = CreateEntity(world);
    MeshComponent* mc = AddComponent(g_terrain, MeshComponent, UploadGeometry("resources/models/maze/maze.obj"));
    AddComponent(g_terrain, AudioBarrierComponent, mc->id);

    // lights
    LightID l1 = SubmitLight((SceneLight){{ 0 }, { 1, 1, 1 }, { 0, -1.0f, 0 }, 0, 0 });
    LightID l2 = SubmitLight((SceneLight){{ 0 }, { 0.8f, 0.8f, 0.8f }, { 0.0f, 0.0f, 1.0f }, 0, 0 });
    LightID l3 = SubmitLight((SceneLight){{ 0 }, { 0.6f, 0.6f, 0.6f }, { 0.0f, 0.0f, -1.0f }, 0, 0 });
    LightID l4 = SubmitLight((SceneLight){{ 0 }, { 0.4f, 0.4f, 0.4f }, { 1.0f, 0.0f, 0.0f }, 0, 0 });
    LightID l5 = SubmitLight((SceneLight){{ 0 }, { 0.2f, 0.2f, 0.2f }, { -1.0f, 0.0f, 0.0f }, 0, 0 });
    AddComponent(CreateEntity(world), LightComponent, l1);
    AddComponent(CreateEntity(world), LightComponent, l2);
    AddComponent(CreateEntity(world), LightComponent, l3);
    AddComponent(CreateEntity(world), LightComponent, l4);
    AddComponent(CreateEntity(world), LightComponent, l5);

    // pacman
    g_player = CreateEntityP(world, -30.0f, 5.5f, 0.0f);
    AddComponent(g_player, MeshComponent, UploadGeometry("resources/models/pacman/pacman.obj"));
    AddComponent(g_player, CameraComponent, FALSE, {0,0,0}, {0,0,0});
    AddComponent(g_player, AudioListenerComponent, 20);
    *(EntityScale(g_player)) = (Vector3){ 0.75f, 0.75f, 0.75f };

    // ghost
    g_enemy = CreateEntityP(world, -37.0f, 5.8f, 0.0f);
    DSPState* dsp = DSPInit(ambient.stream.sampleRate, 2.0f);
    SpatialSource* spatial = SpatialCreateSource();
    AddComponent(g_enemy, MeshComponent, UploadGeometry("resources/models/ghost/ghost.obj"));
    AddComponent(g_enemy, AudioSourceComponent, ambient, dsp, spatial);

    // todo: make less ugly with extern lines
    extern DSPState* g_audio_dsp;
    extern SpatialSource* g_audio_spatial;
    extern unsigned int g_audio_channels;
    extern void DSPProcessorCallback(void* buffer, unsigned int frames);
    g_audio_dsp = dsp;
    g_audio_spatial = spatial;
    g_audio_channels = ambient.stream.channels;
    AttachAudioStreamProcessor(ambient.stream, DSPProcessorCallback);
    PlayMusicStream(ambient);

    return scene;
}

void EchoMain() {
    InitializeApplication("Echo Example", "See you, Space Cowboy");
    AddScene(GenerateMainScene());
    SetScene("Main");
    RunApplication();
    DestroyApplication();
}
