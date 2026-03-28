#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "util/geometry.h"

#define EXPOSE_COMPONENT(name) enum { name##_TYPE = __COUNTER__ }; struct name; typedef struct name name; struct name

typedef enum {
    TEXT_ALIGN_CENTER,
    TEXT_ALIGN_LEFT,
    TEXT_ALIGN_RIGHT
} TextAlignment;

typedef enum {
    CENTER_ANCHOR,
    ML_ANCHOR,
    MR_ANCHOR,
    TL_ANCHOR,
    TR_ANCHOR,
    BR_ANCHOR,
    BL_ANCHOR,
    TM_ANCHOR,
    BM_ANCHOR,
    ABSOLUTE_ANCHOR
} ViewportAnchor;

EXPOSE_COMPONENT(TagComponent) {
    const char* tag;
};

EXPOSE_COMPONENT(TransformComponent) {
    Vector3 translation;
    Vector3 rotation;
    Vector3 scale;
};

EXPOSE_COMPONENT(TextComponent) {
    char* text;
    TextAlignment alignment;
    ViewportAnchor anchor;
    Color color;
    float size;
};

EXPOSE_COMPONENT(MeshComponent) {
    MeshDescriptor mesh;
};

#endif
