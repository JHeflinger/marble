#include "entity.h"
#include "ecs/components.h"
#include "game/world.h"
#include <raymath.h>

void* EntityAddComponent(const Entity e, size_t type, void* component, size_t size) {
    return RegistryEmplaceComponent(e.context->registry, e.id, type, component, size);
}

void* EntityGetComponent(const Entity e, size_t type) {
    return RegistryGetComponent(e.context->registry, e.id, type);
}

BOOL EntityHasComponent(const Entity e, size_t type) {
    return RegistryHasComponent(e.context->registry, e.id, type);
}

void EntityRemoveComponent(const Entity e, size_t type) {
    RegistryRemoveComponent(e.context->registry, e.id, type);
}

Vector3* EntityPosition(const Entity e) {
    EZ_ASSERT(HasComponent(e, TransformComponent), "Entity does not have transform component!");
    TransformComponent* tc = GetComponent(e, TransformComponent);
    return &(tc->translation);
}

Vector3* EntityScale(const Entity e) {
    EZ_ASSERT(HasComponent(e, TransformComponent), "Entity does not have transform component!");
    TransformComponent* tc = GetComponent(e, TransformComponent);
    return &(tc->scale);
}

Vector3* EntityRotation(const Entity e) {
    EZ_ASSERT(HasComponent(e, TransformComponent), "Entity does not have transform component!");
    TransformComponent* tc = GetComponent(e, TransformComponent);
    return &(tc->rotation);
}

Matrix EntityTransform(const Entity e) {
    vec3 translate = { EntityPosition(e)->x, EntityPosition(e)->y ,EntityPosition(e)->z };
    vec3 rotate = { EntityRotation(e)->x, EntityRotation(e)->y ,EntityRotation(e)->z };
    vec3 scale = { EntityScale(e)->x, EntityScale(e)->y ,EntityScale(e)->z };
    mat4 T, R, S, TR, m;
    glm_mat4_identity(T);
    glm_mat4_identity(R);
    glm_mat4_identity(S);
    glm_translate(T, translate);
    glm_rotate_x(R, glm_rad(rotate[0]), R);
    glm_rotate_y(R, glm_rad(rotate[1]), R);
    glm_rotate_z(R, glm_rad(rotate[2]), R);
    glm_scale(S, scale);
    glm_mat4_mul(T, R, TR);
    glm_mat4_mul(TR, S, m);
    Matrix result;
    result.m0  = m[0][0]; result.m1  = m[0][1]; result.m2  = m[0][2]; result.m3  = m[0][3];
    result.m4  = m[1][0]; result.m5  = m[1][1]; result.m6  = m[1][2]; result.m7  = m[1][3];
    result.m8  = m[2][0]; result.m9  = m[2][1]; result.m10 = m[2][2]; result.m11 = m[2][3];
    result.m12 = m[3][0]; result.m13 = m[3][1]; result.m14 = m[3][2]; result.m15 = m[3][3];
    return result;
}

Vector3 EntityOrient(const Entity e, Vector3 vector) {
    Matrix rotX = MatrixRotateX(DEG2RAD * EntityRotation(e)->x);
    Matrix rotY = MatrixRotateY(DEG2RAD * EntityRotation(e)->y);
    Matrix rotZ = MatrixRotateZ(DEG2RAD * EntityRotation(e)->z);
    Matrix rotation = MatrixMultiply(MatrixMultiply(rotZ, rotX), rotY);
    return Vector3Transform(vector, rotation);
}
