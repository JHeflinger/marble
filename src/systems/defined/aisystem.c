#include "aisystem.h"
#include "ecs/components.h"
#include "ecs/entity.h"
#include "game/world.h"

void UpdateAISystem(System* system, float dt) {
    ARRLIST_EntityID* ai_entities = GetEntities(system->context, AIComponent);
    if (ai_entities) {
        for (size_t i = 0 ; i < ai_entities->size; i++) {
            Entity e = (Entity){ ai_entities->data[i], system->context };
            AIComponent* ac = GetComponent(e, AIComponent);
            BehaviorDefaultUpdate(ac->root, &(ac->blackboard));
        }
    }
}

System* GenerateAISystem() {
    return GenerateSystem(NULL, UpdateAISystem, NULL, NULL, NULL, NULL, NULL);
}
