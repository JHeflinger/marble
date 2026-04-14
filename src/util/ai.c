#include "ai.h"

uint64_t hash_string(const char* key) {
    uint64_t hash = 1469598103934665603ULL;
    while (*key) {
        hash ^= (uint8_t)*key;
        hash *= 1099511628211ULL;
        key++;
    }
    return hash;
}

typedef struct {
    BehaviorNode base;
    BehaviorNode** children;
    size_t count;
} CompositeNode;

typedef struct {
    BehaviorNode base;
    BehaviorNode* child;
} InverterNode;

typedef struct {
    BehaviorNode base;
    BehaviorActionUpdate update;
    void* ctx;
} ActionNode;

typedef struct {
    BehaviorNode base;
    BehaviorConditionCheck check;
    void* ctx;
} ConditionNode;

IMPL_HASHMAP(const char*, BlackBoardValue, BlackBoard, hash_string);

BOOL BlackBoardHas(BlackBoard* board, const char* key) {
    return HASHMAP_BlackBoard_has(&(board->map), key);
}

void BlackBoardSet(BlackBoard* board, const char* key, BlackBoardValue val) {
    HASHMAP_BlackBoard_set(&(board->map), key, val);
}

void BlackBoardSetInt(BlackBoard* board, const char* key, int v) {
    BlackBoardValue val = { 0 };
    val.type = BB_INT;
    val.value._int = v;
    BlackBoardSet(board, key, val);
}

void BlackBoardSetFloat(BlackBoard* board, const char* key, float v) {
    BlackBoardValue val = { 0 };
    val.type = BB_FLOAT;
    val.value._float = v;
    BlackBoardSet(board, key, val);
}

void BlackBoardSetBool(BlackBoard* board, const char* key, BOOL v) {
    BlackBoardValue val = { 0 };
    val.type = BB_BOOL;
    val.value._bool = v;
    BlackBoardSet(board, key, val);
}

void BlackBoardSetPointer(BlackBoard* board, const char* key, void* v) {
    BlackBoardValue val = { 0 };
    val.type = BB_POINTER;
    val.value._ptr = v;
    BlackBoardSet(board, key, val);
}

BlackBoardValue BlackBoardGet(BlackBoard* board, const char* key) {
    return HASHMAP_BlackBoard_get(&(board->map), key);
}

int BlackBoardGetInt(BlackBoard* board, const char* key) {
    BlackBoardValue val = BlackBoardGet(board, key);
    EZ_ASSERT(val.type == BB_INT, "BlackBoard value was requested to be an int but was not of that type");
    return val.value._int;
}

float BlackBoardGetFloat(BlackBoard* board, const char* key) {
    BlackBoardValue val = BlackBoardGet(board, key);
    EZ_ASSERT(val.type == BB_FLOAT, "BlackBoard value was requested to be a float but was not of that type");
    return val.value._float;
}

BOOL BlackBoardGetBOOL(BlackBoard* board, const char* key) {
    BlackBoardValue val = BlackBoardGet(board, key);
    EZ_ASSERT(val.type == BB_BOOL, "BlackBoard value was requested to be a bool but was not of that type");
    return val.value._bool;
}

void* BlackBoardGetPointer(BlackBoard* board, const char* key) {
    BlackBoardValue val = BlackBoardGet(board, key);
    EZ_ASSERT(val.type == BB_POINTER, "BlackBoard value was requested to be a pointer but was not of that type");
    return val.value._ptr;
}

AIStatus BehaviorDefaultUpdate(BehaviorNode* node, BlackBoard* blackboard) {
    return node ? node->update(node, blackboard) : AI_FAILURE;
}

void DestroyComposite(BehaviorNode* node) {
    CompositeNode* c = (CompositeNode*)node;
    for (size_t i = 0; i < c->count; i++) DestroyBehavior(c->children[i]);
    EZ_FREE(c->children);
}

CompositeNode* CreateComposite(BehaviorNode** children, size_t count, BehaviorUpdate update) {
    CompositeNode* c = EZ_ALLOC(1, sizeof(CompositeNode));
    c->base.update = update;
    c->base.destroy = DestroyComposite;
    c->children = EZ_ALLOC(count, sizeof(BehaviorNode*));
    memcpy(c->children, children, count * sizeof(BehaviorNode*));
    c->count = count;
    return c;
}

AIStatus BehaviorSequenceUpdate(BehaviorNode* node, BlackBoard* blackboard) {
    CompositeNode* c = (CompositeNode*)node;
    for (size_t i = 0; i < c->count; i++) {
        AIStatus s = BehaviorDefaultUpdate(c->children[i], blackboard);
        if (s != AI_SUCCESS) return s;
    }
    return AI_SUCCESS;
}

AIStatus BehaviorSelectorUpdate(BehaviorNode* node, BlackBoard* blackboard) {
    CompositeNode* c = (CompositeNode*)node;
    for (size_t i = 0; i < c->count; i++) {
        AIStatus s = BehaviorDefaultUpdate(c->children[i], blackboard);
        if (s != AI_FAILURE) return s;
    }
    return AI_FAILURE;
}

AIStatus BehaviorInverterUpdate(BehaviorNode* node, BlackBoard* blackboard) {
    InverterNode* i = (InverterNode*)node;
    AIStatus s = BehaviorDefaultUpdate(i->child, blackboard);
    if (s == AI_SUCCESS) return AI_FAILURE;
    if (s == AI_FAILURE) return AI_SUCCESS;
    return AI_RUNNING;
}

void DestroyInverter(BehaviorNode* node) {
    InverterNode* i = (InverterNode*)node;
    DestroyBehavior(i->child);
}

AIStatus BehaviorActionDefaultUpdate(BehaviorNode* node, BlackBoard* blackboard) {
    ActionNode* a = (ActionNode*)node;
    return a->update(a->ctx, blackboard);
}

AIStatus BehaviorConditionDefaultUpdate(BehaviorNode* node, BlackBoard* blackboard) {
    ConditionNode* c = (ConditionNode*)node;
    return c->check(c->ctx, blackboard) ? AI_SUCCESS : AI_FAILURE;
}

BehaviorNode* BehaviorSequence(BehaviorNode** children, size_t count) {
    return &(CreateComposite(children, count, BehaviorSequenceUpdate)->base);
}

BehaviorNode* BehaviorSelector(BehaviorNode** children, size_t count) {
    return &(CreateComposite(children, count, BehaviorSelectorUpdate)->base);
}

BehaviorNode* BehaviorInverter(BehaviorNode* child) {
    InverterNode* i = EZ_ALLOC(1, sizeof(InverterNode));
    i->base.update = BehaviorInverterUpdate;
    i->base.destroy = DestroyInverter;
    i->child = child;
    return &(i->base);
}

BehaviorNode* BehaviorAction(BehaviorActionUpdate update, void* ctx) {
    ActionNode* a = EZ_ALLOC(1, sizeof(ActionNode));
    a->base.update = BehaviorActionDefaultUpdate;
    a->update = update;
    a->ctx = ctx;
    return &(a->base);
}

BehaviorNode* BehaviorCondition(BehaviorConditionCheck check, void* ctx) {
    ConditionNode* c = EZ_ALLOC(1, sizeof(ConditionNode));
    c->base.update = BehaviorConditionDefaultUpdate;
    c->check = check;
    c->ctx = ctx;
    return &(c->base);
}

void DestroyBehavior(BehaviorNode* node) {
    if (!node) return;
    if (node->destroy) node->destroy(node);
    EZ_FREE(node);
}
