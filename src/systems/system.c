#include "system.h"
#include <easymemory.h>

IMPL_ARRLIST_NAMED(SystemPtr, System*);

System* GenerateSystem(
        SystemDrawFunction draw,
        SystemUpdateFunction update,
        SystemKeyEventFunction key,
        SystemMouseButtonEventFunction mousebutton,
        SystemMouseScrollEventFunction mousescroll,
        SystemMouseMoveFunction mousemove,
        SystemCleanFunction clean) {
    System* system = EZ_ALLOC(1, sizeof(System));
    system->draw = draw;
    system->update = update;
    system->key = key;
    system->mousebutton = mousebutton;
    system->mousescroll = mousescroll;
    system->mousemove = mousemove;
    system->clean = clean;
    return system;
}

void DestroySystem(System* system) {
    if (system->clean) system->clean(system);
    EZ_FREE(system);
}
