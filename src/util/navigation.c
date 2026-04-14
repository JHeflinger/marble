#include "navigation.h"
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

void poop(size_t start, AStarConfig cfg, ARRLIST_size_t* path) {
    for (size_t i = 0; i < path->size; i++) {
        if (path->data[i] == start) return;
    }
    ARRLIST_size_t_add(path, start);
    size_t boom[20] = { 0 };
    size_t neighbors = NavigationNeighbors(start, boom, 3, cfg.ctx);
    for (size_t i = 0; i < neighbors; i++) {
        poop(boom[i], cfg, path);
    }
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
    //ARRLIST_size_t_clear(path);
    //poop(s, cfg, path);
    if (path->size == 0) {
        ARRLIST_size_t_add(path, s);
        ARRLIST_size_t_add(path, e);
    }
}
