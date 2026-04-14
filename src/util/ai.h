#ifndef AI_H
#define AI_H

#include "data/forward.h"
#include <stddef.h>
#include <easyobjects.h>

typedef enum {
    AI_RUNNING = 0,
    AI_SUCCESS,
    AI_FAILURE
} AIStatus;

typedef enum {
    BB_INT,
    BB_FLOAT,
    BB_BOOL,
    BB_POINTER
} BlackBoardType;

typedef struct {
    BlackBoardType type;
    union {
        int _int;
        float _float;
        BOOL _bool;
        void* _ptr;
    } value;
} BlackBoardValue;

DECLARE_HASHMAP(const char*, BlackBoardValue, BlackBoard);

typedef struct {
    HASHMAP_BlackBoard map;
} BlackBoard;

typedef void (*BehaviorDestroy)(BehaviorNode* node);
typedef AIStatus (*BehaviorUpdate)(BehaviorNode* node, BlackBoard* blackboard);
typedef AIStatus (*BehaviorActionUpdate)(void* ctx, BlackBoard* blackboard);
typedef BOOL (*BehaviorConditionCheck)(void* ctx, BlackBoard* blackboard);

struct BehaviorNode {
    BehaviorUpdate update;
    BehaviorDestroy destroy;
};

BOOL BlackBoardHas(BlackBoard* board, const char* key);

void BlackBoardSet(BlackBoard* board, const char* key, BlackBoardValue val);

void BlackBoardSetInt(BlackBoard* board, const char* key, int v);

void BlackBoardSetFloat(BlackBoard* board, const char* key, float v);

void BlackBoardSetBool(BlackBoard* board, const char* key, BOOL v);

void BlackBoardSetPointer(BlackBoard* board, const char* key, void* v);

BlackBoardValue BlackBoardGet(BlackBoard* board, const char* key);

int BlackBoardGetInt(BlackBoard* board, const char* key);

float BlackBoardGetFloat(BlackBoard* board, const char* key);

BOOL BlackBoardGetBOOL(BlackBoard* board, const char* key);

void* BlackBoardGetPointer(BlackBoard* board, const char* key);

AIStatus BehaviorDefaultUpdate(BehaviorNode* node, BlackBoard* blackboard);

BehaviorNode* BehaviorSequence(BehaviorNode** children, size_t count);

BehaviorNode* BehaviorSelector(BehaviorNode** children, size_t count);

BehaviorNode* BehaviorInverter(BehaviorNode* child);

BehaviorNode* BehaviorAction(BehaviorActionUpdate update, void* ctx);

BehaviorNode* BehaviorCondition(BehaviorConditionCheck check, void* ctx);

void DestroyBehavior(BehaviorNode* node);

#endif
