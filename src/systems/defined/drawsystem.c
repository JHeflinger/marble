#include "drawsystem.h"
#include "extensions/eviewport.h"
#include "ecs/components.h"
#include "ecs/entity.h"
#include "game/world.h"
#include <renderer/renderer.h>
#include <raymath.h>

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
    Render();
    RenderTexture2D target = GetViewportTarget();
    Vector2 slice = GetViewportSlice();
    BeginTextureMode(target);
    Draw(0, 0, slice.x, slice.y);
    Vector2 image_origin = (Vector2){ slice.x / 2.0f - target.texture.width / 2.0f, slice.y / 2.0f - target.texture.height / 2.0f };
    ARRLIST_EntityID* text_entities = GetEntities(system->context, TextComponent);
    if (text_entities) {
        for (size_t i = 0; i < text_entities->size; i++) {
            Entity e = (Entity){ text_entities->data[i], system->context };
            DrawTextComponent(e, image_origin);
        }
    }
    EndTextureMode();
}

System* GenerateDrawSystem() {
    return GenerateSystem(DrawDrawSystem, NULL, NULL, NULL, NULL, NULL);
}
