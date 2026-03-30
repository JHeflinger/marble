#include "collisionsystem.h"
#include "ecs/components.h"
#include "ecs/entity.h"
#include "game/world.h"
#include <renderer/renderer.h>
#include <raymath.h>

#define MAX_POLYTOPE  128
#define MAX_FACES     384  // 3x polytope
#define MAX_NORMALS   128
#define MAX_EDGES     128

typedef struct {
    Vector3 points[4];
    int size;
} Simplex;

void GetFaceNormals(
    Vector3* polytope, int polytopeSize,
    size_t* faces, int facesSize,
    Vector4* normalsOut, int* normalsOutSize,
    size_t* minTriangleOut) {
    *normalsOutSize = 0;
    *minTriangleOut = 0;
    float minDistance = FLT_MAX;
    for (int i = 0; i < facesSize; i += 3) {
        Vector3 a = polytope[faces[i]];
        Vector3 b = polytope[faces[i + 1]];
        Vector3 c = polytope[faces[i + 2]];
        Vector3 normal = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a)));
        float distance = Vector3DotProduct(normal, a);
        if (distance < 0) { normal = Vector3Negate(normal); distance = -distance; }
        normalsOut[*normalsOutSize] = (Vector4){ normal.x, normal.y, normal.z, distance };
        if (distance < minDistance) { *minTriangleOut = i / 3; minDistance = distance; }
        (*normalsOutSize)++;
    }
}

void AddIfUniqueEdge(
    size_t (*edges)[2], int* edgeCount,
    size_t* faces, int a, int b) {
    for (int i = 0; i < *edgeCount; i++) {
        if (edges[i][0] == faces[b] && edges[i][1] == faces[a]) {
            edges[i][0] = edges[*edgeCount - 1][0];
            edges[i][1] = edges[*edgeCount - 1][1];
            (*edgeCount)--;
            return;
        }
    }
    edges[*edgeCount][0] = faces[a];
    edges[*edgeCount][1] = faces[b];
    (*edgeCount)++;
}

BOOL SameDirection(Vector3 direction, Vector3 ao) {
    return Vector3DotProduct(direction, ao) > 0;
}

BOOL HandleLine(Simplex* simplex, Vector3* direction) {
    Vector3 a = simplex->points[0];
    Vector3 b = simplex->points[1];
    Vector3 ab = Vector3Subtract(b, a);
    Vector3 ao = Vector3Negate(a);
    if (SameDirection(ab, ao)) {
        *direction = Vector3CrossProduct(Vector3CrossProduct(ab, ao), ab);
    } else {
        simplex->points[0] = a;
        simplex->size = 1;
        *direction = ao;
    }
    return FALSE;
}

BOOL HandleTriangle(Simplex* simplex, Vector3* direction) {
    Vector3 a = simplex->points[0];
    Vector3 b = simplex->points[1];
    Vector3 c = simplex->points[2];
    Vector3 ab = Vector3Subtract(b, a);
    Vector3 ac = Vector3Subtract(c, a);
    Vector3 ao = Vector3Negate(a);
    Vector3 abc = Vector3CrossProduct(ab, ac);
    if (SameDirection(Vector3CrossProduct(abc, ac), ao)) {
        if (SameDirection(ac, ao)) {
            simplex->points[0] = a; simplex->points[1] = c;
            simplex->size = 2;
            *direction = Vector3CrossProduct(Vector3CrossProduct(ac, ao), ac);
        } else {
            simplex->points[0] = a; simplex->points[1] = b;
            simplex->size = 2;
            return HandleLine(simplex, direction);
        }
    } else {
        if (SameDirection(Vector3CrossProduct(ab, abc), ao)) {
            simplex->points[0] = a; simplex->points[1] = b;
            simplex->size = 2;
            return HandleLine(simplex, direction);
        } else {
            if (SameDirection(abc, ao)) {
                *direction = abc;
            } else {
                simplex->points[0] = a; simplex->points[1] = c; simplex->points[2] = b;
                simplex->size = 3;
                *direction = Vector3Negate(abc);
            }
        }
    }
    return FALSE;
}

BOOL HandleTetrahedron(Simplex* simplex, Vector3* direction) {
    Vector3 a = simplex->points[0];
    Vector3 b = simplex->points[1];
    Vector3 c = simplex->points[2];
    Vector3 d = simplex->points[3];
    Vector3 ab = Vector3Subtract(b, a);
    Vector3 ac = Vector3Subtract(c, a);
    Vector3 ad = Vector3Subtract(d, a);
    Vector3 ao = Vector3Negate(a);
    Vector3 abc = Vector3CrossProduct(ab, ac);
    Vector3 acd = Vector3CrossProduct(ac, ad);
    Vector3 adb = Vector3CrossProduct(ad, ab);
    if (SameDirection(abc, ao)) {
        simplex->points[0] = a; simplex->points[1] = b; simplex->points[2] = c;
        simplex->size = 3;
        return HandleTriangle(simplex, direction);
    }
    if (SameDirection(acd, ao)) {
        simplex->points[0] = a; simplex->points[1] = c; simplex->points[2] = d;
        simplex->size = 3;
        return HandleTriangle(simplex, direction);
    }
    if (SameDirection(adb, ao)) {
        simplex->points[0] = a; simplex->points[1] = d; simplex->points[2] = b;
        simplex->size = 3;
        return HandleTriangle(simplex, direction);
    }
    return TRUE;
}

BOOL HandleSimplex(Simplex* simplex, Vector3* direction) {
    switch (simplex->size) {
        case 2: return HandleLine(simplex, direction);
        case 3: return HandleTriangle(simplex, direction);
        case 4: return HandleTetrahedron(simplex, direction);
    }
    return FALSE;
}

Vector3 BoxSupport(Entity e, Vector3 direction) {
    Matrix model = EntityTransform(e);
    Matrix invModel = MatrixInvert(model);
    Vector3 localDir = Vector3Transform(direction, invModel);
    Vector3 result = {
        localDir.x >= 0 ? 0.5f : -0.5f,
        localDir.y >= 0 ? 0.5f : -0.5f,
        localDir.z >= 0 ? 0.5f : -0.5f
    };
    return Vector3Transform(result, model);
}

Vector3 CylinderSupport(Entity e, Vector3 direction) {
    Matrix model = EntityTransform(e);
    Matrix invModel = MatrixInvert(model);
    Vector3 localDir = Vector3Transform(direction, invModel);
    Vector2 radial = { localDir.x, localDir.z };
    float len = Vector2Length(radial);
    if (len > 0) radial = Vector2Scale(radial, 0.5f / len);
    Vector3 result = { radial.x, localDir.y >= 0 ? 0.5f : -0.5f, radial.y };
    return Vector3Transform(result, model);
}

Vector3 SphereSupport(Entity e, Vector3 direction) {
    return Vector3Add(*EntityPosition(e), Vector3Scale(Vector3Normalize(direction), EntityScale(e)->x * 0.5f));
}

Vector3 MeshSupport(Entity e, Vector3 direction) {
    Matrix model = EntityTransform(e);
    Matrix RS = model;
    RS.m12 = 0; RS.m13 = 0; RS.m14 = 0;
    Vector3 localDir = Vector3Transform(direction, MatrixInvert(RS));
    MeshComponent* mc = GetComponent(e, MeshComponent);
    MeshDescriptor* md = MeshReference(mc->id);
    float bestDot = -FLT_MAX;
    Vector3 bestPoint = { 0 };
    for (VertexID i = md->start; i <= md->end; i++) {
        vec3 glmvec;
        GetVertex(i, glmvec);
        Vector3 vertex = (Vector3){ glmvec[0], glmvec[1], glmvec[2] };
        float d = Vector3DotProduct(vertex, localDir);
        if (d > bestDot) { bestDot = d; bestPoint = vertex; }
    }
    return Vector3Add(Vector3Transform(bestPoint, RS), *EntityPosition(e));
}

void ApplyMTV(Entity e1, Entity e2, Vector3 mtv) {
    if (HasComponent(e2, StaticCollisionComponent)) {
        *EntityPosition(e1) = Vector3Add(*EntityPosition(e1), Vector3Scale(mtv, 2.0f));
        GetComponent(e1, DynamicCollisionComponent)->mtv =
            Vector3Add(GetComponent(e1, DynamicCollisionComponent)->mtv, Vector3Scale(mtv, 2.0f));
    } else if (HasComponent(e1, StaticCollisionComponent)) {
        *EntityPosition(e2) = Vector3Add(*EntityPosition(e2), Vector3Scale(mtv, 2.0f));
        GetComponent(e2, DynamicCollisionComponent)->mtv =
            Vector3Add(GetComponent(e2, DynamicCollisionComponent)->mtv, Vector3Scale(mtv, 2.0f));
    } else {
        *EntityPosition(e1) = Vector3Add(*EntityPosition(e1), mtv);
        *EntityPosition(e2) = Vector3Subtract(*EntityPosition(e2), mtv);
        GetComponent(e1, DynamicCollisionComponent)->mtv =
            Vector3Add(GetComponent(e1, DynamicCollisionComponent)->mtv, mtv);
        GetComponent(e2, DynamicCollisionComponent)->mtv =
            Vector3Subtract(GetComponent(e2, DynamicCollisionComponent)->mtv, mtv);
    }
}

Vector3 DispatchSupport(Entity e, Collider ec, Vector3 direction) {
    switch (ec) {
        case BOX_COLLIDER: return BoxSupport(e, direction);
        case SPHERE_COLLIDER: return SphereSupport(e, direction);
        case CYLINDER_COLLIDER: return CylinderSupport(e, direction);
        case MESH_COLLIDER: return MeshSupport(e, direction);
        default: EZ_ASSERT(FALSE, "Unhandled collider type detected");
    }
    return (Vector3){ 0 };
}

BOOL GJK(Entity e1, Entity e2, Collider ec, Simplex* simplex) {
    simplex->size = 0;
    Vector3 direction = { 1, 0, 0 };
    Vector3 support = Vector3Subtract(MeshSupport(e1, direction), DispatchSupport(e2, ec, Vector3Negate(direction)));
    simplex->points[simplex->size++] = support;
    direction = Vector3Negate(support);
    while (TRUE) {
        support = Vector3Subtract(MeshSupport(e1, direction), DispatchSupport(e2, ec, Vector3Negate(direction)));
        if (Vector3DotProduct(support, direction) <= 0) return FALSE;
        simplex->points[3] = simplex->points[2];
        simplex->points[2] = simplex->points[1];
        simplex->points[1] = simplex->points[0];
        simplex->points[0] = support;
        simplex->size = (simplex->size < 4) ? simplex->size + 1 : 4;
        if (HandleSimplex(simplex, &direction)) return TRUE;
    }
}

BOOL CollideME(Entity e1, Entity e2, Collider ec) {
    Simplex simplex;
    if (!GJK(e1, e2, ec, &simplex)) return FALSE;
    Vector3 polytope[MAX_POLYTOPE];
    int polytopeSize = simplex.size;
    for (int i = 0; i < simplex.size; i++) polytope[i] = simplex.points[i];
    size_t faces[MAX_FACES] = { 0, 1, 2,  0, 3, 1,  0, 2, 3,  1, 3, 2 };
    int facesSize = 12;
    Vector4 normals[MAX_NORMALS];
    int normalsSize = 0;
    size_t minFace = 0;
    GetFaceNormals(polytope, polytopeSize, faces, facesSize, normals, &normalsSize, &minFace);
    Vector3 minNormal = { 0 };
    float minDistance = FLT_MAX;
    while (minDistance == FLT_MAX) {
        minNormal = (Vector3){ normals[minFace].x, normals[minFace].y, normals[minFace].z };
        minDistance = normals[minFace].w;
        Vector3 support = Vector3Subtract(MeshSupport(e1, minNormal), DispatchSupport(e2, ec, Vector3Negate(minNormal)));
        float sDistance = Vector3DotProduct(minNormal, support);
        if (fabsf(sDistance - minDistance) > 0.001f) {
            minDistance = FLT_MAX;
            size_t edges[MAX_EDGES][2];
            int edgeCount = 0;
            for (int i = 0; i < normalsSize; i++) {
                Vector3 n = { normals[i].x, normals[i].y, normals[i].z };
                if (Vector3DotProduct(n, support) > Vector3DotProduct(n, polytope[faces[i * 3]])) {
                    int f = i * 3;
                    AddIfUniqueEdge(edges, &edgeCount, faces, f,     f + 1);
                    AddIfUniqueEdge(edges, &edgeCount, faces, f + 1, f + 2);
                    AddIfUniqueEdge(edges, &edgeCount, faces, f + 2, f    );
                    faces[f]     = faces[facesSize - 1];
                    faces[f + 1] = faces[facesSize - 2];
                    faces[f + 2] = faces[facesSize - 3];
                    facesSize -= 3;
                    normals[i] = normals[normalsSize - 1];
                    normalsSize--;
                    i--;
                }
            }
            size_t newFaces[MAX_FACES];
            int newFacesSize = 0;
            for (int i = 0; i < edgeCount; i++) {
                newFaces[newFacesSize++] = edges[i][0];
                newFaces[newFacesSize++] = edges[i][1];
                newFaces[newFacesSize++] = (size_t)polytopeSize;
            }
            polytope[polytopeSize++] = support;
            Vector4 newNormals[MAX_NORMALS];
            int newNormalsSize = 0;
            size_t newMinFace = 0;
            GetFaceNormals(polytope, polytopeSize, newFaces, newFacesSize, newNormals, &newNormalsSize, &newMinFace);
            float oldMinDistance = FLT_MAX;
            for (int i = 0; i < normalsSize; i++) {
                if (normals[i].w < oldMinDistance) {
                    oldMinDistance = normals[i].w;
                    minFace = (size_t)i;
                }
            }
            if (newNormals[newMinFace].w < oldMinDistance) {
                minFace = newMinFace + (size_t)normalsSize;
            }
            for (int i = 0; i < newFacesSize; i++) faces[facesSize++] = newFaces[i];
            for (int i = 0; i < newNormalsSize; i++) normals[normalsSize++] = newNormals[i];
        }
    }
    ApplyMTV(e1, e2, Vector3Scale(Vector3Normalize(minNormal), minDistance * -1.0f));
    return TRUE;
}

BOOL CollideSS(Entity e1, Entity e2) {
    float radius_a = 0.5f * EntityScale(e1)->x;
    float radius_b = 0.5f * EntityScale(e2)->x;
    Vector3 direction = Vector3Subtract(*EntityPosition(e2), *EntityPosition(e1));
    float distance = Vector3Length(direction);
    float radiusSum = radius_a + radius_b;
    if (distance >= radiusSum) return FALSE;
    float depth = radiusSum - distance;
    Vector3 mtv = Vector3Scale((distance > 0.0001f) ? Vector3Scale(direction, 1.0f / distance) : (Vector3){ 0,1,0 }, depth);
    ApplyMTV(e1, e2, Vector3Scale(mtv, -0.5f));
    return TRUE;
}

BOOL CollideBB(Entity e1, Entity e2) {
    Vector3 a = *EntityPosition(e1);
    Vector3 b = *EntityPosition(e2);
    Vector3 amax = Vector3Add(a, Vector3Scale(*EntityScale(e1), 0.5f));
    Vector3 amin = Vector3Subtract(a, Vector3Scale(*EntityScale(e1), 0.5f));
    Vector3 bmax = Vector3Add(b, Vector3Scale(*EntityScale(e2), 0.5f));
    Vector3 bmin = Vector3Subtract(b, Vector3Scale(*EntityScale(e2), 0.5f));
    Vector3 overlap = (Vector3){
        fmin(amax.x, bmax.x) - fmax(amin.x, bmin.x),
        fmin(amax.y, bmax.y) - fmax(amin.y, bmin.y),
        fmin(amax.z, bmax.z) - fmax(amin.z, bmin.z)};
    if (overlap.x <= 0 || overlap.y <= 0 || overlap.z <= 0) return FALSE;
    Vector3 mtv = (Vector3){ 0 };
    if (overlap.x < overlap.y && overlap.x < overlap.z) mtv.x = a.x < b.x ? -overlap.x : overlap.x;
    else if (overlap.y < overlap.z) mtv.y = a.y < b.y ? -overlap.y : overlap.y;
    else mtv.z = a.z < b.z ? -overlap.z : overlap.z;
    ApplyMTV(e1, e2, Vector3Scale(mtv, 0.5f));
    return TRUE;
}

BOOL CollideCC(Entity e1, Entity e2) {
    Vector2 a = (Vector2){ EntityPosition(e1)->x, EntityPosition(e1)->z };
    Vector2 b = (Vector2){ EntityPosition(e2)->x, EntityPosition(e2)->z };
    Vector3 mtv = (Vector3){ 0 };
    Vector2 delta = Vector2Subtract(a, b);
    float dist2 = Vector2DotProduct(delta, delta);
    float rsum = EntityScale(e1)->x/2.0f + EntityScale(e2)->x/2.0f;
    if (dist2 > rsum * rsum) return false;
    float amin = EntityPosition(e1)->y - EntityScale(e1)->y/2.0f;
    float amax = EntityPosition(e1)->y + EntityScale(e1)->y/2.0f;
    float bmin = EntityPosition(e2)->y - EntityScale(e2)->y/2.0f;
    float bmax = EntityPosition(e2)->y + EntityScale(e2)->y/2.0f;
    if (!(amin < bmax && amax > bmin)) return false;
    float dist = sqrt(dist2);
    float penetration_xz = rsum - dist;
    float penetration_y = fmin(amax - bmin, bmax - amin);
    if (penetration_xz < penetration_y) {
        Vector2 normal = (dist > 0.0001f) ? Vector2Scale(delta, 1.0f / dist) : (Vector2){ 1, 0 };
        Vector2 correction = Vector2Scale(normal, penetration_xz * 0.5f);
        mtv.x += correction.x;
        mtv.z += correction.y;
    } else {
        float direction = (EntityPosition(e1)->y < EntityPosition(e2)->y) ? -1.0f : 1.0f;
        float correction = penetration_y * 0.5f * direction;
        mtv.y += correction;
    }
    ApplyMTV(e1, e2, mtv);
    return TRUE;
}

BOOL CollideCB(Entity e1, Entity e2) {
    Vector3 a = *EntityPosition(e1);
    Vector3 b = *EntityPosition(e2);
    Vector3 bs = *EntityScale(e2);
    Vector3 bmax = Vector3Add(b, Vector3Scale(bs, 0.5f));
    Vector3 bmin = Vector3Subtract(b, Vector3Scale(bs, 0.5f));
    float h = EntityScale(e1)->y;
    float ymin = a.y - h * 0.5f;
    float ymax = a.y + h * 0.5f;
    float overlapY = fmin(ymax, bmax.y) - fmax(ymin, bmin.y);
    if (overlapY <= 0) return FALSE;
    float r = EntityScale(e1)->x * 0.5f;
    Vector2 center = (Vector2){ a.x, a.z };
    Vector2 bcenter = (Vector2){ b.x, b.z };
    Vector2 bhalf = (Vector2){ (bmax.x - bmin.x) * 0.5f, (bmax.z - bmin.z) * 0.5f };
    Vector2 p = Vector2Subtract(center, bcenter);
    Vector2 d = (Vector2){ fabs(p.x) - bhalf.x, fabs(p.y) - bhalf.y };
    Vector2 outside = (Vector2){ fmax(d.x, 0.0f), fmax(d.y, 0.0f) };
    float dist = Vector2Length(outside);
    Vector2 n;
    float penxz;
    if (dist > 0.0f) {
        if (dist >= r) return FALSE;
        penxz = r - dist;
        n = Vector2Scale(outside, 1.0f / dist);
        n.x *= (p.x > 0) ? 1.0f : -1.0f;
        n.y *= (p.y > 0) ? 1.0f : -1.0f;
    } else {
        float dx = bhalf.x - fabs(p.x);
        float dz = bhalf.y - fabs(p.y);
        if (dx < dz) {
            float sign = (p.x > 0) ? 1.0f : -1.0f;
            n = (Vector2){ sign, 0 };
            penxz = r + dx;
        } else {
            float sign = (p.y > 0) ? 1.0f : -1.0f;
            n = (Vector2){ 0, sign };
            penxz = r + dz;
        }
    }
    Vector3 result;
    float signy = a.y > (bmin.y + bmax.y) * 0.5f ? 1.0f : -1.0f;
    if (penxz < overlapY) result = (Vector3){ n.x * penxz, 0, n.y * penxz };
    else result = (Vector3){ 0, signy * overlapY, 0 };
    ApplyMTV(e1, e2, Vector3Scale(result, -0.5f));
    return TRUE;
}

BOOL CollideSC(Entity e1, Entity e2) {
    Vector3 a = *EntityPosition(e1);
    Vector3 b = *EntityPosition(e2);
    float sradius = EntityScale(e1)->x * 0.5f;
    float cradius = EntityScale(e2)->x * 0.5f;
    float h = EntityScale(e2)->y;
    float ymin = b.y - h * 0.5f;
    float ymax = b.y + h * 0.5f;
    float dx = a.x - b.x;
    float dz = a.z - b.z;
    float distXZ = sqrt(dx*dx + dz*dz);
    float closestX = b.x + (distXZ > 0 ? (dx / distXZ) * fmin(distXZ, cradius) : 0);
    float closestZ = b.z + (distXZ > 0 ? (dz / distXZ) * fmin(distXZ, cradius) : 0);
    float closestY = Clamp(a.y, ymin, ymax);
    float ex = a.x - closestX;
    float ey = a.y - closestY;
    float ez = a.z - closestZ;
    float dist2 = ex*ex + ey*ey + ez*ez;
    if (dist2 >= sradius * sradius) return FALSE;
    float dist = sqrt(dist2);
    float pen  = sradius - dist;
    Vector3 n;
    if (dist > 1e-6f) n = Vector3Normalize((Vector3){ ex, ey, ez });
    else n = (Vector3){ 0, 1, 0 };
    ApplyMTV(e1, e2, Vector3Scale(n, pen * 0.5f));
    return TRUE;
}

BOOL CollideSB(Entity e1, Entity e2) {
    Vector3 a = *EntityPosition(e1);
    Vector3 b = *EntityPosition(e2);
    Vector3 bmax = Vector3Add(b, Vector3Scale(*EntityScale(e2), 0.5f));
    Vector3 bmin = Vector3Add(b, Vector3Scale(*EntityScale(e2), -0.5f));
    Vector3 closest = Vector3Clamp(a, bmin, bmax);
    Vector3 d = Vector3Subtract(a, closest);
    float radius = EntityScale(e1)->x * 0.5f;
    float d2 = d.x*d.x+d.y*d.y+d.z*d.z;
    float r2 = radius*radius;
    if (d2 > r2) return FALSE;
    float dist = sqrt(d2);
    Vector3 mtv = { 0 };
    if (dist > 0.000001f) {
        float penetration = radius - dist;
        Vector3 n = Vector3Scale(d, 1.0f / dist);
        mtv = Vector3Scale(n, penetration);
    } else {
        float dx = fmin(bmax.x - a.x, a.x - bmin.x);
        float dy = fmin(bmax.y - a.y, a.y - bmin.y);
        float dz = fmin(bmax.z - a.z, a.z - bmin.z);
        if (dx < dy && dx < dz) mtv = (Vector3){ a.x > (bmin.x + bmax.x)*0.5f ? radius + dx : -(radius+ dx), 0, 0 };
        else if (dy < dz) mtv = (Vector3){ 0, a.y > (bmin.y + bmax.y)*0.5f ? radius + dy : -(radius + dy), 0 };
        else mtv = (Vector3){ 0, 0, a.z > (bmin.z + bmax.z)*0.5f ? radius + dz : -(radius + dz) };
    }
    ApplyMTV(e1, e2, Vector3Scale(mtv, 0.5f));
    return TRUE;
}

BOOL DispatchBoxCollision(Entity box, Entity e, Collider c) {
    switch (c) {
        case BOX_COLLIDER: return CollideBB(box, e);
        case SPHERE_COLLIDER: return CollideSB(e, box);
        case CYLINDER_COLLIDER: return CollideCB(e, box);
        case MESH_COLLIDER: return CollideME(e, box, BOX_COLLIDER);
        default: EZ_ASSERT(FALSE, "Unhandled collision type detected");
    }
    return FALSE;
}

BOOL DispatchSphereCollision(Entity sphere, Entity e, Collider c) {
    switch (c) {
        case BOX_COLLIDER: return CollideSB(sphere, e);
        case SPHERE_COLLIDER: return CollideSS(sphere, e);
        case CYLINDER_COLLIDER: return CollideSC(sphere, e);
        case MESH_COLLIDER: return CollideME(e, sphere, SPHERE_COLLIDER);
        default: EZ_ASSERT(FALSE, "Unhandled collision type detected");
    }
    return FALSE;
}

BOOL DispatchCylinderCollision(Entity cylinder, Entity e, Collider c) {
    switch (c) {
        case BOX_COLLIDER: return CollideCB(cylinder, e);
        case SPHERE_COLLIDER: return CollideSC(e, cylinder);
        case CYLINDER_COLLIDER: return CollideCC(cylinder, e);
        case MESH_COLLIDER: return CollideME(e, cylinder, CYLINDER_COLLIDER);
        default: EZ_ASSERT(FALSE, "Unhandled collision type detected");
    }
    return FALSE;
}

BOOL DispatchCollision(Entity e1, Collider c1, Entity e2, Collider c2) {
    switch (c1) {
        case BOX_COLLIDER: return DispatchBoxCollision(e1, e2, c2);
        case SPHERE_COLLIDER: return DispatchSphereCollision(e1, e2, c2);
        case CYLINDER_COLLIDER: return DispatchCylinderCollision(e1, e2, c2);
        case MESH_COLLIDER: return CollideME(e1, e2, c2);
        default: EZ_ASSERT(FALSE, "Unhandled collision type detected");
    }
    return FALSE;
}

void UpdateCollisionSystem(System* system, float dt) {
    ARRLIST_EntityID* dynamics = GetEntities(system->context, DynamicCollisionComponent);
    ARRLIST_EntityID* statics = GetEntities(system->context, StaticCollisionComponent);
    if (dynamics) {
        for (size_t i = 0; i < dynamics->size; i++) {
            Entity e = (Entity){ dynamics->data[i], system->context };
            DynamicCollisionComponent* dc = GetComponent(e, DynamicCollisionComponent);
            dc->mtv = (Vector3){ 0 };
            dc->collided = FALSE;
        }
        for (size_t i = 0; i < dynamics->size; i++) {
            Entity ei = (Entity){ dynamics->data[i], system->context };
            DynamicCollisionComponent* dci = GetComponent(ei, DynamicCollisionComponent);
            for (size_t j = i + 1; j < dynamics->size; j++) {
                Entity ej = (Entity){ dynamics->data[j], system->context };
                DynamicCollisionComponent* dcj = GetComponent(ej, DynamicCollisionComponent);
                if (DispatchCollision(ei, dci->collider, ej, dcj->collider)) {
                    dci->collided = TRUE;
                    dcj->collided = TRUE;
                }
            }
            if (statics) {
                for (size_t j = 0; j < statics->size; j++) {
                    Entity ej = (Entity){ statics->data[j], system->context };
                    StaticCollisionComponent* scj = GetComponent(ej, StaticCollisionComponent);
                    if (DispatchCollision(ei, dci->collider, ej, scj->collider)) {
                        dci->collided = TRUE;
                    }
                }
            }
        }
    }
}

System* GenerateCollisionSystem() {
    return GenerateSystem(NULL, UpdateCollisionSystem, NULL, NULL, NULL, NULL);
}
