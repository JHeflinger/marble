#include "boxloader.h"
#include "ecs/entity.h"
#include "ecs/components.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <raylib.h>
#include <easylogger.h>
#include <easymemory.h>

typedef struct JsonObject {
    Vector3 center;
    Vector3 scale;
} JsonObject;

void LoadBoxColliders(World* world, const char* filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        EZ_ERROR("Failed to open file: %s\n", filename);
        return;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char *buffer = EZ_ALLOC(size + 1, sizeof(char));
    if (!buffer) {
        fclose(file);
        return;
    }
    fread(buffer, 1, size, file);
    buffer[size] = '\0';
    fclose(file);
    char *ptr = buffer;
    while ((ptr = strstr(ptr, "\"center\"")) != NULL) {
        JsonObject obj;
        sscanf(
            ptr,
            "\"center\" : [ %f , %f , %f ]",
            &obj.center.x,
            &obj.center.y,
            &obj.center.z
        );
        ptr = strstr(ptr, "\"scale\"");
        if (!ptr) break;
        sscanf(
            ptr,
            "\"scale\" : [ %f , %f , %f ]",
            &obj.scale.x,
            &obj.scale.y,
            &obj.scale.z
        );
        Entity e = CreateEntityP(world, obj.center.x, obj.center.y, obj.center.z);
        *(EntityScale(e)) = obj.scale;
        AddComponent(e, StaticCollisionComponent, FALSE, BOX_COLLIDER);
        ptr++;
    }
    EZ_FREE(buffer);
}
