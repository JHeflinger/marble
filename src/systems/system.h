#ifndef SYSTEM_H
#define SYSTEM_H

#include "data/forward.h"
#include <raylib.h>
#include <easyobjects.h>

typedef void (*SystemDrawFunction)(System* system);
typedef void (*SystemUpdateFunction)(System* system, float dt);
typedef void (*SystemKeyEventFunction)(System* system, int key, InputAction action);
typedef void (*SystemMouseButtonEventFunction)(System* system, int key, InputAction action);
typedef void (*SystemMouseScrollEventFunction)(System* system, Vector2 offset);
typedef void (*SystemMouseMoveFunction)(System* system, Vector2 position);
typedef void (*SystemCleanFunction)(System* system);

struct System {
    SystemDrawFunction draw;
    SystemUpdateFunction update;
    SystemKeyEventFunction key;
    SystemMouseButtonEventFunction mousebutton;
    SystemMouseScrollEventFunction mousescroll;
    SystemMouseMoveFunction mousemove;
    SystemCleanFunction clean;
    World* context;
};
DECLARE_ARRLIST_NAMED(SystemPtr, System*);

System* GenerateSystem(
        SystemDrawFunction draw,
        SystemUpdateFunction update,
        SystemKeyEventFunction key,
        SystemMouseButtonEventFunction mousebutton,
        SystemMouseScrollEventFunction mousescroll,
        SystemMouseMoveFunction mousemove,
        SystemCleanFunction clean);

void DestroySystem(System* system);

#endif
