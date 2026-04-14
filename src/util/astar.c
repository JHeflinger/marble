#include "astar.h"
#include <float.h>

size_t AStar(size_t start, size_t goal, const AStarConfig* cfg, ARRLIST_size_t* path_out) {
    size_t N = cfg->max_nodes;
    AStarNode* nodes = EZ_ALLOC(N, sizeof(AStarNode));
    for (size_t i = 0; i < N; i++) {
        nodes[i].id = i;
        nodes[i].g = FLT_MAX;
        nodes[i].f = FLT_MAX;
        nodes[i].parent = (size_t)-1;
        nodes[i].pqindex = (size_t)-1;
    }
    nodes[start].g = 0.0f;
    nodes[start].f = cfg->heuristic(start, goal, cfg->ctx);
    PQUEUE_size_t pq = { 0 };
    PQUEUE_size_t_insert(&pq, start, nodes[start].f);
    nodes[start].pqindex = pq.size - 1;
    nodes[start].open = TRUE;
    size_t* nbr_buf = EZ_ALLOC(cfg->max_neighbors, sizeof(size_t));
    size_t found = 0;
    while (pq.size > 0) {
        size_t cur = PQUEUE_size_t_pop(&pq);
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
                if (nodes[nbr].open) {
                    PQUEUE_size_t_update(&pq, nodes[nbr].pqindex, nodes[nbr].f);
                } else {
                    PQUEUE_size_t_insert(&pq, nbr, nodes[nbr].f);
                    nodes[nbr].pqindex = pq.size - 1;
                    nodes[nbr].open = TRUE;
                }
            }
        }
    }
    size_t path_len = 0;
    ARRLIST_size_t_wipe(path_out);
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
    PQUEUE_size_t_clear(&pq);
    EZ_FREE(nodes);
    return path_len;
}
