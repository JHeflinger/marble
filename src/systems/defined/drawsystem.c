#include "drawsystem.h"
#include "extensions/eviewport.h"
#include "ecs/components.h"
#include "ecs/entity.h"
#include "game/world.h"
#include <renderer/renderer.h>
#include <raymath.h>

void DrawImageComponent(Entity e, Vector2 origin) {
    ImageComponent* ic = GetComponent(e, ImageComponent);
    TransformComponent* tc = GetComponent(e, TransformComponent);
    DrawTexturePro(
        ic->texture,
        (Rectangle){0, 0, ic->texture.width, ic->texture.height},
        (Rectangle){tc->translation.x - tc->scale.x/2.0f, tc->translation.y - tc->scale.y/2.0f, tc->scale.x, tc->scale.y},
        (Vector2){0, 0}, 0, WHITE);
}

void DrawTextComponent(Entity e, Vector2 origin) {
    TextComponent* tc = GetComponent(e, TextComponent);
    Font default_font = GetFontDefault();
    Vector2 text_size = MeasureTextEx(default_font, tc->text, tc->size, 2);
    Vector3* position = EntityPosition(e);
    Vector2 slice = GetViewportSlice();
    Vector2 tpos;
    switch (tc->anchor) {
        case CENTER_ANCHOR:
            tpos = (Vector2){ slice.x / 2.0f, slice.y / 2.0f };
            break;
        case ML_ANCHOR:
            tpos = (Vector2){ 0, slice.y / 2.0f };
            break;
        case MR_ANCHOR:
            tpos = (Vector2){ slice.x, slice.y / 2.0f };
            break;
        case TL_ANCHOR:
            tpos = (Vector2){ 0, 0 };
            break;
        case TR_ANCHOR:
            tpos = (Vector2){ slice.x, 0 };
            break;
        case BR_ANCHOR:
            tpos = slice;
            break;
        case BL_ANCHOR:
            tpos = (Vector2){ 0, slice.y };
            break;
        case TM_ANCHOR:
            tpos = (Vector2){ slice.x / 2.0f, 0 };
            break;
        case BM_ANCHOR:
            tpos = (Vector2){ slice.x / 2.0f, slice.y };
            break;
        case ABSOLUTE_ANCHOR:
            tpos = origin;
            break;
        default: break;
    }
    tpos = Vector2Add(tpos, (Vector2){ position->x, position->y });
    tpos.y -= text_size.y / 2.0f;
    switch (tc->alignment) {
        case TEXT_ALIGN_CENTER:
            tpos.x -= text_size.x / 2.0f;
            break;
        case TEXT_ALIGN_LEFT: break;
        case TEXT_ALIGN_RIGHT:
            tpos.x -= text_size.x;
            break;
        default: break;
    }
    DrawTextEx(default_font, tc->text, tpos, tc->size, 2, tc->color);
}

void DrawDrawSystem(System* system) {
    ARRLIST_EntityID* cameras = GetEntities(system->context, CameraComponent);
    SimpleCamera currcamera = GetCamera();
    if (cameras) {
        BOOL found = FALSE;
        for (size_t i = 0; i < cameras->size; i++) {
            Entity e = (Entity){ cameras->data[i], system->context };
            CameraComponent* cc = GetComponent(e, CameraComponent);
            TransformComponent* tc = GetComponent(e, TransformComponent);
            if (cc->enabled) {
                if (found) EZ_WARN("More than one camera component was detected to be enabled - any additional camers will override the previous one");
                found = TRUE;
                SimpleCamera sc = currcamera;
                vec3 translation = { tc->translation.x, tc->translation.y, tc->translation.z };
                vec3 rotation = { tc->rotation.x, tc->rotation.y, tc->rotation.z };
                vec3 ctranslation = { cc->offset.x, cc->offset.y, cc->offset.z };
                vec3 crotation = { cc->rotation.x, cc->rotation.y, cc->rotation.z };
                glm_vec3_add(translation, ctranslation, translation);
                glm_vec3_add(rotation, crotation, rotation);
                mat4 R;
                glm_euler_yxz(rotation, R);
                vec3 forward = {0.0f, 0.0f, -1.0f};
                vec3 up = {0.0f, 1.0f,  0.0f};
                vec3 look, cam_up;
                glm_mat4_mulv3(R, forward, 0.0f, look);
                glm_mat4_mulv3(R, up, 0.0f, cam_up);
                glm_vec3_copy(translation, sc.position);
                glm_vec3_add(translation, look, look);
                glm_vec3_copy(look, sc.look);
                glm_vec3_copy(cam_up, sc.up);
                MoveCamera(sc);
            }
        }
    }
    RenderConfig()->reset = TRUE;
    UpdateLights();
    Render();
    MoveCamera(currcamera);
    RenderTexture2D target = GetViewportTarget();
    Vector2 slice = GetViewportSlice();
    BeginTextureMode(target);
    Draw(0, 0, slice.x, slice.y);
    Vector2 image_origin = (Vector2){ slice.x / 2.0f - target.texture.width / 2.0f, slice.y / 2.0f - target.texture.height / 2.0f };
    ARRLIST_EntityID* image_entities = GetEntities(system->context, ImageComponent);
    if (image_entities) {
        for (size_t i = 0; i < image_entities->size; i++) {
            Entity e = (Entity){ image_entities->data[i], system->context };
            DrawImageComponent(e, image_origin);
        }
    }
    ARRLIST_EntityID* text_entities = GetEntities(system->context, TextComponent);
    if (text_entities) {
        for (size_t i = 0; i < text_entities->size; i++) {
            Entity e = (Entity){ text_entities->data[i], system->context };
            DrawTextComponent(e, image_origin);
        }
    }
    EndTextureMode();
}

void UpdateDrawSystem(System* system, float dt) {
    ARRLIST_EntityID* mesh_entities = GetEntities(system->context, MeshComponent);
    if (mesh_entities) {
        for (size_t i = 0 ; i < mesh_entities->size; i++) {
            Entity e = (Entity){ mesh_entities->data[i], system->context };
            TransformComponent* tc = GetComponent(e, TransformComponent);
            MeshComponent* mc = GetComponent(e, MeshComponent);
            MeshDescriptor* md = MeshReference(mc->id);
            md->translate[0] = tc->translation.x;
            md->translate[1] = tc->translation.y;
            md->translate[2] = tc->translation.z;
            md->rotate[0] = tc->rotation.x;
            md->rotate[1] = tc->rotation.y;
            md->rotate[2] = tc->rotation.z;
            md->scale[0] = tc->scale.x;
            md->scale[1] = tc->scale.y;
            md->scale[2] = tc->scale.z;
            UpdateObjectTransform(mc->id);
        }
    }
}

System* GenerateDrawSystem() {
    return GenerateSystem(DrawDrawSystem, UpdateDrawSystem, NULL, NULL, NULL, NULL, NULL);
}
