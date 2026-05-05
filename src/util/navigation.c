#include "navigation.h"
#include "ecs/components.h"
#include "util/geometry.h"
#include "util/astar.h"
#include <easyhash.h>
#include <raymath.h>

typedef struct {
    TriangleID t1;
    TriangleID t2;
} TrianglePair;

uint64_t HashEdge(Edge edge) {
    return ez_hash_uint64_t(((uint64_t)edge.a << 32) | edge.b);
}

DECLARE_HASHMAP(Edge, TrianglePair, TriangleGlue);
IMPL_HASHMAP(Edge, TrianglePair, TriangleGlue, HashEdge);

float NavigationHeuristic(size_t a, size_t b, void* ctx) {
    const NavigationMesh* m = (NavigationMesh*)ctx;
    return Vector3Length(Vector3Subtract(m->polys[a].centroid, m->polys[b].centroid));
}

size_t NavigationNeighbors(size_t id, size_t* out, size_t max_out, void* ctx) {
    const NavigationMesh* m = (NavigationMesh*)ctx;
    size_t n = 0;
    for (size_t i = 0; i < 3 && n < max_out; i++) {
        if (m->polys[id].neighbors[i] != (TriangleID)-1)
            out[n++] = m->polys[id].neighbors[i] - m->start;
    }
    return n;
}

float NavigationCost(size_t a, size_t b, void* ctx) {
    return NavigationHeuristic(a, b, ctx);
}

NavigationMesh UploadNavigationMesh(const char* filepath) {
    NavigationMesh nm = { 0 };
    nm.start = NumTriangles();
    nm.mesh = UploadGeometry(filepath);
    nm.end = NumTriangles();
    if (nm.mesh != (size_t)-1) {
        nm.polycount = nm.end - nm.start;
        nm.polys = EZ_ALLOC(nm.polycount, sizeof(NavPoly));
        HASHMAP_TriangleGlue map = { 0 };
        for (size_t i = nm.start; i < nm.end; i++) {
            Triangle* tref = TriangleReference(i);
            VertexID vs[] = { tref->a, tref->b, tref->c };
            for (size_t j = 0; j < 3; j++) {
                VertexID a = vs[j];
                VertexID b = vs[(j + 1)%3];
                Edge e = { a, b };
                Edge alternate = { b, a };
                Edge primed = HASHMAP_TriangleGlue_has(&map, e) ? e : alternate;
                if (!HASHMAP_TriangleGlue_has(&map, primed)) {
                    HASHMAP_TriangleGlue_set(&map, primed, (TrianglePair){ i, (TriangleID)-1 });
                } else {
                    TrianglePair tp = HASHMAP_TriangleGlue_get(&map, primed);
                    if (tp.t2 != (TriangleID)-1) EZ_WARN("Navigation mesh was detected to be non-manifold");
                    tp.t2 = i;
                    HASHMAP_TriangleGlue_set(&map, primed, tp);
                }
            }
        }
        for (size_t i = nm.start; i < nm.end; i++) {
            Triangle* tref = TriangleReference(i);
            tref->material = 1;
            VertexID vs[] = { tref->a, tref->b, tref->c };
            Vector3 centroid = (Vector3){ 0 };
            for (size_t j = 0; j < 3; j++) {
                VertexID a = vs[j];
                VertexID b = vs[(j + 1)%3];
                Edge e = { a, b };
                Edge alternate = { b, a };
                Edge primed = HASHMAP_TriangleGlue_has(&map, e) ? e : alternate;
                nm.polys[i - nm.start].neighbors[j] = (TriangleID)-1;
                centroid.x += VertexReference(a)[0];
                centroid.y += VertexReference(a)[1];
                centroid.z += VertexReference(a)[2];
                if (HASHMAP_TriangleGlue_has(&map, primed)) {
                    TrianglePair tp = HASHMAP_TriangleGlue_get(&map, primed);
                    TriangleID tid = tp.t1 == i ? tp.t2 : tp.t1;
                    nm.polys[i - nm.start].neighbors[j] = tid;
                }
            }
            nm.polys[i - nm.start].centroid = Vector3Scale(centroid, 1.0f / 3.0f);
        }
        MeshDescriptor* md = MeshReference(nm.mesh);
        for (size_t i = md->start; i < md->end; i++) VertexReference(i)[1] += 1e-4;
        UpdateVertices();
        UpdateTriangles();
        HASHMAP_TriangleGlue_clear(&map);
    }
    return nm;
}

NavigationMesh DuplicateNavigationMesh(NavigationMesh mesh) {
    NavigationMesh nm = mesh;
    nm.polys = EZ_ALLOC(nm.polycount, sizeof(NavPoly));
    memcpy(nm.polys, mesh.polys, nm.polycount * sizeof(NavPoly));
    return nm;
}

void FindPath(NavigationMesh* mesh, Vector3 start, Vector3 end, ARRLIST_size_t* path) {
    AStarConfig cfg = (AStarConfig) {
        NavigationHeuristic,
        NavigationNeighbors,
        NavigationCost,
        (void*)mesh,
        mesh->polycount,
        3
    };
    size_t s = 0;
    size_t e = 0;
    float sdist = FLT_MAX;
    float edist = FLT_MAX;
    for (size_t i = 0; i < mesh->polycount; i++) {
        Triangle* tref = TriangleReference(mesh->start + i);
        VertexID vs[] = { tref->a, tref->b, tref->c };
        vec3 centroidv;
        glm_vec3_add(VertexReference(tref->a), VertexReference(tref->b), centroidv);
        glm_vec3_add(centroidv, VertexReference(tref->c), centroidv);
        glm_vec3_scale(centroidv, 1.0f / 3.0f, centroidv);
        Vector3 centroid = (Vector3){ centroidv[0], centroidv[1], centroidv[2] };
        float csdist = Vector3Length(Vector3Subtract(start, centroid));
        float cedist = Vector3Length(Vector3Subtract(end, centroid));
        if (csdist < sdist) {
            sdist = csdist;
            s = i;
        }
        if (cedist < edist) {
            edist = cedist;
            e = i;
        }
        for (size_t j = 0; j < 3; j++) {
            Vector3 v = (Vector3){ VertexReference(vs[j])[0], VertexReference(vs[j])[1], VertexReference(vs[j])[2] };
            float newsdist = Vector3Length(Vector3Subtract(start, v));
            float newedist = Vector3Length(Vector3Subtract(end, v));
            if (newsdist < sdist) {
                sdist = newsdist;
                s = i;
            }
            if (newedist < edist) {
                edist = newedist;
                e = i;
            }
        }
    }
    AStar(s, e, &cfg, path);
    if (path->size == 0) {
        ARRLIST_size_t_add(path, s);
        ARRLIST_size_t_add(path, e);
    }
}

void Navigate(Entity e, Vector3 target) {
    EZ_ASSERT(HasComponent(e, NavigationComponent), "Cannot perform navigation on an entity without a navigation component");
    NavigationComponent* nc = GetComponent(e, NavigationComponent);
    FindPath(
        &(nc->mesh),
        *(EntityPosition(e)),
        target,
        &(nc->path));
}

BOOL GetSharedEdge(NavigationComponent* nc, int index, Vector3* out_p1, Vector3* out_p2) {
    TriangleID tid_a = nc->mesh.start + nc->path.data[index];
    TriangleID tid_b = nc->mesh.start + nc->path.data[index + 1];
    Triangle* ta = TriangleReference(tid_a);
    Triangle* tb = TriangleReference(tid_b);
    uint32_t a_verts[3] = { ta->a, ta->b, ta->c };
    uint32_t b_verts[3] = { tb->a, tb->b, tb->c };
    Vector3 shared[2];
    int found = 0;
    for (int i = 0; i < 3 && found < 2; i++) {
        float* pa = VertexReference(a_verts[i]);
        for (int j = 0; j < 3; j++) {
            float* pb = VertexReference(b_verts[j]);
            if (fabsf(pa[0] - pb[0]) < 0.001f &&
                fabsf(pa[2] - pb[2]) < 0.001f) {
                shared[found++] = (Vector3){ pa[0], pa[1], pa[2] };
                break;
            }
        }
    }
    if (found < 2) return FALSE;
    *out_p1 = shared[0];
    *out_p2 = shared[1];
    return TRUE;
}

BOOL HasCrossedEdge(Vector3 pos, Vector3 ep1, Vector3 ep2, Vector3 from_centroid) {
    float edge_dx = ep2.x - ep1.x;
    float edge_dz = ep2.z - ep1.z;
    float to_pos_x = pos.x - ep1.x;
    float to_pos_z = pos.z - ep1.z;
    float to_cent_x = from_centroid.x - ep1.x;
    float to_cent_z = from_centroid.z - ep1.z;
    float cross_pos = edge_dx * to_pos_z - edge_dz * to_pos_x;
    float cross_cent = edge_dx * to_cent_z - edge_dz * to_cent_x;
    return (cross_pos * cross_cent) <= 0.0f;
}
