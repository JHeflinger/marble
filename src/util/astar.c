#include "astar.h"
#include <float.h>

typedef struct {
    int *data;
    int size;
    int capacity;
    float *f;
} MinHeap;

MinHeap* HeapCreate(int capacity, float *f_table) {
    MinHeap *h = EZ_ALLOC(1, sizeof(MinHeap));
    h->data = EZ_ALLOC(capacity, sizeof(int));
    h->size = 0;
    h->capacity = capacity;
    h->f = f_table;
    return h;
}

void DestroyHeap(MinHeap *h) {
    EZ_FREE(h->data);
    EZ_FREE(h);
}

void SwapHeap(MinHeap* h, int i, int j) {
    int tmp = h->data[i];
    h->data[i] = h->data[j];
    h->data[j] = tmp;
}

void HeapUp(MinHeap* h, int i) {
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (h->f[h->data[parent]] > h->f[h->data[i]]) {
            SwapHeap(h, i, parent);
            i = parent;
        } else break;
    }
}

void HeapDown(MinHeap* h, int i) {
    for (;;) {
        int smallest = i;
        int l = 2*i + 1, r = 2*i + 2;
        if (l < h->size && h->f[h->data[l]] < h->f[h->data[smallest]]) smallest = l;
        if (r < h->size && h->f[h->data[r]] < h->f[h->data[smallest]]) smallest = r;
        if (smallest == i) break;
        SwapHeap(h, i, smallest);
        i = smallest;
    }
}

void PushHeap(MinHeap* h, int id) {
    h->data[h->size++] = id;
    HeapUp(h, h->size - 1);
}

int PopHeap(MinHeap* h) {
    int top = h->data[0];
    h->data[0] = h->data[--h->size];
    if (h->size > 0) HeapDown(h, 0);
    return top;
}

size_t AStar(size_t start, size_t goal, const AStarConfig* cfg, ARRLIST_size_t* path_out) {
    size_t N = cfg->max_nodes;
    AStarNode* nodes = EZ_ALLOC(N, sizeof(AStarNode));
    float* f_table = EZ_ALLOC(N, sizeof(float));
    for (size_t i = 0; i < N; i++) {
        nodes[i].id = i;
        nodes[i].g = FLT_MAX;
        nodes[i].f = FLT_MAX;
        nodes[i].parent = (size_t)-1;
        f_table[i] = FLT_MAX;
    }
    nodes[start].g = 0.0f;
    nodes[start].f = cfg->heuristic(start, goal, cfg->ctx);
    f_table[start] = nodes[start].f;
    MinHeap* open = HeapCreate(N, f_table);
    PushHeap(open, start);
    nodes[start].open = TRUE;
    size_t* nbr_buf = EZ_ALLOC(cfg->max_neighbors, sizeof(size_t));
    size_t found = 0;
    while (open->size > 0) {
        size_t cur = PopHeap(open);
        nodes[cur].open = FALSE;
        if (cur == goal) { found = 1; break; }
        if (nodes[cur].closed) continue;
        nodes[cur].closed = TRUE;
        size_t n_nbr = cfg->neighbors(cur, nbr_buf, cfg->max_neighbors, cfg->ctx);
        for (size_t i = 0; i < n_nbr; i++) {
            size_t nbr = nbr_buf[i];
            if (nodes[nbr].closed) continue;
            float tentative_g = nodes[cur].g + cfg->edgecost(cur, nbr, cfg->ctx);
            if (tentative_g < nodes[nbr].g) {
                nodes[nbr].parent = cur;
                nodes[nbr].g = tentative_g;
                nodes[nbr].f = tentative_g + cfg->heuristic(nbr, goal, cfg->ctx);
                f_table[nbr] = nodes[nbr].f;
                nodes[nbr].open = TRUE;
                PushHeap(open, nbr);
            }
        }
    }
    size_t path_len = 0;
    ARRLIST_size_t_clear(path_out);
    if (found) {
        size_t* tmp = EZ_ALLOC(N, sizeof(size_t));
        size_t len = 0;
        size_t cur = goal;
        while (cur != (size_t)-1) { tmp[len++] = cur; cur = nodes[cur].parent; }
        for (size_t i = 0; i < len; i++) ARRLIST_size_t_add(path_out, tmp[len - 1 - i]);
        path_len = len;
        EZ_FREE(tmp);
    }
    EZ_FREE(nbr_buf);
    DestroyHeap(open);
    EZ_FREE(nodes);
    EZ_FREE(f_table);
    return path_len;
}
