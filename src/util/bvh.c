#include "bvh.h"
#include "game/world.h"
#include <renderer/renderer.h>
#include <renderer/rmath.h>
#include <easylogger.h>

#define BVH_LIMIT 0.01f

IMPL_ARRLIST(NodeBVH);
IMPL_ARRLIST(BVHBox);

BOOL AABBOverlap(NodeBVH a, BVHBox b) {
    return
        (a.min[0] <= b.max[0] && a.max[0] >= b.min[0]) &&
        (a.min[1] <= b.max[1] && a.max[1] >= b.min[1]) &&
        (a.min[2] <= b.max[2] && a.max[2] >= b.min[2]);
}

void ResizeBVH(ARRLIST_NodeBVH* bvh, size_t index) {
    if (bvh->data[index].branch_config == BVH_BOTH) {
        ResizeBVH(bvh, bvh->data[index].left);
        ResizeBVH(bvh, bvh->data[index].right);
        glm_vec3_minv(
            bvh->data[index].min,
            bvh->data[bvh->data[index].left].min,
            bvh->data[index].min);
        glm_vec3_maxv(
            bvh->data[index].max,
            bvh->data[bvh->data[index].left].max,
            bvh->data[index].max);
        glm_vec3_minv(
            bvh->data[index].min,
            bvh->data[bvh->data[index].right].min,
            bvh->data[index].min);
        glm_vec3_maxv(
            bvh->data[index].max,
            bvh->data[bvh->data[index].right].max,
            bvh->data[index].max);
    } else if (bvh->data[index].branch_config == BVH_LEFT_ONLY) {
        ResizeBVH(bvh, bvh->data[index].left);
        glm_vec3_minv(
            bvh->data[index].min,
            bvh->data[bvh->data[index].left].min,
            bvh->data[index].min);
        glm_vec3_maxv(
            bvh->data[index].max,
            bvh->data[bvh->data[index].left].max,
            bvh->data[index].max);
    } else if (bvh->data[index].branch_config == BVH_RIGHT_ONLY) {
        ResizeBVH(bvh, bvh->data[index].right);
        glm_vec3_minv(
            bvh->data[index].min,
            bvh->data[bvh->data[index].right].min,
            bvh->data[index].min);
        glm_vec3_maxv(
            bvh->data[index].max,
            bvh->data[bvh->data[index].right].max,
            bvh->data[index].max);
    } else {
        return;
    }
}

void SplitBVH(ARRLIST_NodeBVH* bvh, size_t index, ARRLIST_BVHBox* geometry, ARRLIST_size_t* children) {
    #define CBVH bvh->data[index]
    #define BVHMIN bvh->data[index].min
    #define BVHMAX bvh->data[index].max
    #define COPYVEC(v1, v2) { v1[0] = v2[0]; v1[1] = v2[1]; v1[2] = v2[2]; }

    // set up split boxes
    float xyz_dist[3] = {
        BVHMAX[0] - BVHMIN[0],
        BVHMAX[1] - BVHMIN[1],
        BVHMAX[2] - BVHMIN[2],
    };
    int dist_ind = xyz_dist[0] > xyz_dist[1] ?
        (xyz_dist[0] > xyz_dist[2] ? 0 : 2) :
        (xyz_dist[1] > xyz_dist[2] ? 1 : 2);
    if (xyz_dist[dist_ind] < BVH_LIMIT) {
        size_t stream_index = index;
        for (size_t i = 0; i < children->size; i++) {
            NodeBVH child = {
                { geometry->data[children->data[i]].min[0],
                  geometry->data[children->data[i]].min[1],
                  geometry->data[children->data[i]].min[2] },
                { geometry->data[children->data[i]].max[0],
                  geometry->data[children->data[i]].max[1],
                  geometry->data[children->data[i]].max[2] },
                BVH_LEAF, geometry->data[children->data[i]].eid, 0
            };
            ARRLIST_NodeBVH_add(bvh, child);
            bvh->data[stream_index].left = bvh->size - 1;

            if (i + 1 >= children->size) {
                bvh->data[stream_index].branch_config = BVH_LEFT_ONLY;
            } else {
                bvh->data[stream_index].branch_config = BVH_BOTH;
                NodeBVH next = {
                    { BVHMIN[0], BVHMIN[1], BVHMIN[2] },
                    { BVHMAX[0], BVHMAX[1], BVHMAX[2] },
                    BVH_LEAF, 0, 0
                };
                ARRLIST_NodeBVH_add(bvh, next);
                bvh->data[stream_index].right = bvh->size - 1;
                stream_index = bvh->size - 1;
            }
        }
        ResizeBVH(bvh, index);
        return;
    }
    float mid_dist = xyz_dist[dist_ind] / 2.0f;
    NodeBVH left = {
        { BVHMIN[0], BVHMIN[1], BVHMIN[2] },
        { BVHMAX[0], BVHMAX[1], BVHMAX[2] },
        BVH_LEAF, 0, 0
    };
    NodeBVH right = {
        { BVHMIN[0], BVHMIN[1], BVHMIN[2] },
        { BVHMAX[0], BVHMAX[1], BVHMAX[2] },
        BVH_LEAF, 0, 0
    };
    left.max[dist_ind] -= mid_dist;
    right.min[dist_ind] += mid_dist;

    // set up subchildren
    ARRLIST_size_t left_children = { 0 };
    ARRLIST_size_t right_children = { 0 };
    for (size_t i = 0; i < children->size; i++) {
        if (geometry->data[children->data[i]].centroid[dist_ind] < left.max[dist_ind])
            ARRLIST_size_t_add(&left_children, children->data[i]);
        else ARRLIST_size_t_add(&right_children, children->data[i]);
    }
    if (left_children.size > 0 && right_children.size > 0) {
        CBVH.branch_config = BVH_BOTH;
    } else if (left_children.size > 0) {
        CBVH.branch_config = BVH_LEFT_ONLY;
    } else if (right_children.size > 0) {
        CBVH.branch_config = BVH_RIGHT_ONLY;
    } else {
        EZ_FATAL("This should never happen");
        CBVH.branch_config = BVH_LEAF;
    }
    if (left_children.size > 1) {
        ARRLIST_NodeBVH_add(bvh, left);
        CBVH.left = bvh->size - 1;
        SplitBVH(bvh, CBVH.left, geometry, &left_children);
    } else if (left_children.size == 1) {
        left.branch_config = BVH_LEAF;
        left.left = geometry->data[left_children.data[0]].eid;
        COPYVEC(left.min, geometry->data[left_children.data[0]].min);
        COPYVEC(left.max, geometry->data[left_children.data[0]].max);
        ARRLIST_NodeBVH_add(bvh, left);
        CBVH.left = bvh->size - 1;
    }
    if (right_children.size > 1) {
        ARRLIST_NodeBVH_add(bvh, right);
        CBVH.right = bvh->size - 1;
        SplitBVH(bvh, CBVH.right, geometry, &right_children);
    } else if (right_children.size == 1) {
        right.branch_config = BVH_LEAF;
        right.left = geometry->data[right_children.data[0]].eid;
        COPYVEC(right.min, geometry->data[right_children.data[0]].min);
        COPYVEC(right.max, geometry->data[right_children.data[0]].max);
        ARRLIST_NodeBVH_add(bvh, right);
        CBVH.right = bvh->size - 1;
    }
    ARRLIST_size_t_clear(&left_children);
    ARRLIST_size_t_clear(&right_children);

    // resize
    if (bvh->data[index].branch_config == BVH_BOTH || bvh->data[index].branch_config == BVH_LEFT_ONLY) {
        glm_vec3_minv(
            bvh->data[index].min,
            bvh->data[bvh->data[index].left].min,
            bvh->data[index].min);
        glm_vec3_maxv(
            bvh->data[index].max,
            bvh->data[bvh->data[index].left].max,
            bvh->data[index].max);
    }
    if (bvh->data[index].branch_config == BVH_BOTH || bvh->data[index].branch_config == BVH_RIGHT_ONLY) {
        glm_vec3_minv(
            bvh->data[index].min,
            bvh->data[bvh->data[index].right].min,
            bvh->data[index].min);
        glm_vec3_maxv(
            bvh->data[index].max,
            bvh->data[bvh->data[index].right].max,
            bvh->data[index].max);
    }
    #undef CBVH
    #undef BVHMIN
    #undef BVHMAX
    #undef COPYVEC
}

size_t CountBVH(ARRLIST_NodeBVH* bvh, size_t index) {
    if (bvh->data[index].branch_config == BVH_LEAF) return 1;
    if (bvh->data[index].branch_config == BVH_LEFT_ONLY) return CountBVH(bvh, bvh->data[index].left);
    if (bvh->data[index].branch_config == BVH_RIGHT_ONLY) return CountBVH(bvh, bvh->data[index].right);
    if (bvh->data[index].branch_config == BVH_BOTH) return CountBVH(bvh, bvh->data[index].left) + CountBVH(bvh, bvh->data[index].right);
    return 0;
}

BVHBox EntityBox(Entity e) {
    vec3 center = { 0 };
    vec3 extents = { 0.5f, 0.5f, 0.5f }; 
    vec3 worldcenter, worldextents, worldmin, worldmax;
    mat4 transform;
    mat3 m3;
    Collider collider = 0;
    if (HasComponent(e, StaticCollisionComponent)) {
        StaticCollisionComponent* sc = GetComponent(e, StaticCollisionComponent);
         collider = sc->collider;
    } else if (HasComponent(e, DynamicCollisionComponent)) {
        DynamicCollisionComponent* dc = GetComponent(e, DynamicCollisionComponent);
        collider = dc->collider;
    } else {
        EZ_ASSERT(FALSE, "Cannot construct a BVH for non-collidable entities");
    }
    if (collider == MESH_COLLIDER) {
        EZ_ASSERT(HasComponent(e, MeshComponent), "Mesh collider has no mesh component to reference");
        MeshComponent* mc = GetComponent(e, MeshComponent);
        MeshDescriptor* md = MeshReference(mc->id);
        memcpy(transform, md->transform, sizeof(mat4));
        memcpy(center, md->center, sizeof(vec3));
        memcpy(extents, md->extents, sizeof(vec3));
    } else {
        vec3 translate = { EntityPosition(e)->x, EntityPosition(e)->y ,EntityPosition(e)->z };
        vec3 rotate = { EntityRotation(e)->x, EntityRotation(e)->y ,EntityRotation(e)->z };
        vec3 scale = { EntityScale(e)->x, EntityScale(e)->y ,EntityScale(e)->z };
        mat4 T, R, S, TR;
        glm_mat4_identity(T);
        glm_mat4_identity(R);
        glm_mat4_identity(S);
        glm_translate(T, translate);
        glm_rotate_x(R, glm_rad(rotate[0]), R);
        glm_rotate_y(R, glm_rad(rotate[1]), R);
        glm_rotate_z(R, glm_rad(rotate[2]), R);
        glm_scale(S, scale);
        glm_mat4_mul(T, R, TR);
        glm_mat4_mul(TR, S, transform);
    }
    glm_mat4_mulv3(transform, center, 1.0f, worldcenter);
    glm_mat4_pick3(transform, m3);
    for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) m3[r][c] = fabsf(m3[r][c]);
    worldextents[0] =
        m3[0][0] * extents[0] +
        m3[0][1] * extents[1] +
        m3[0][2] * extents[2];
    worldextents[1] =
        m3[1][0] * extents[0] +
        m3[1][1] * extents[1] +
        m3[1][2] * extents[2];
    worldextents[2] =
        m3[2][0] * extents[0] +
        m3[2][1] * extents[1] +
        m3[2][2] * extents[2];
    glm_vec3_sub(worldcenter, worldextents, worldmin);
    glm_vec3_add(worldcenter, worldextents, worldmax);
    return (BVHBox){ INLINEV3(worldmin), INLINEV3(worldmax), INLINEV3(worldcenter), (uint32_t)e.id };
}

void ReconstructBVH(ARRLIST_NodeBVH* bvh, ARRLIST_EntityID entities, World* context) {
    ARRLIST_NodeBVH_clear(bvh);
    ARRLIST_BVHBox geometry = { 0 };
    for (size_t i = 0; i < entities.size; i++) {
        Entity e = (Entity){ entities.data[i], context };
        ARRLIST_BVHBox_add(&geometry, EntityBox(e));
    }
    NodeBVH root = {
        { FLT_MAX, FLT_MAX, FLT_MAX },
        { -FLT_MAX, -FLT_MAX, -FLT_MAX },
        BVH_LEAF, 0, 0
    };
    ARRLIST_size_t indices = { 0 };
    for (size_t i = 0; i < geometry.size; i++) {
        glm_vec3_minv(root.min, geometry.data[i].min, root.min);
        glm_vec3_maxv(root.max, geometry.data[i].max, root.max);
        ARRLIST_size_t_add(&indices, i);
    }
    ARRLIST_NodeBVH_add(bvh, root);
    SplitBVH(bvh, 0, &geometry, &indices);
    ARRLIST_size_t_clear(&indices);
    ARRLIST_BVHBox_clear(&geometry);
}

void QueryBVH(ARRLIST_NodeBVH* bvh, Entity e, DispatchBVHCollisionFunction dispatch) {
    BVHBox box = EntityBox(e);
    uint32_t stack[128];
    uint32_t sp = 0;
    stack[sp++] = 0;
    while (sp > 0) {
        uint32_t node_index = stack[--sp];
        if (sp >= 128 - 1) {
            EZ_WARN("BVH stack failure detected");
            break;
        }
        NodeBVH* node = &bvh->data[node_index];
        if (!AABBOverlap(*node, box)) continue;
        uint32_t config = node->branch_config;
        if (config == BVH_LEFT_ONLY || config == BVH_BOTH) stack[sp++] = node->left;
        if (config == BVH_RIGHT_ONLY || config == BVH_BOTH) stack[sp++] = node->right;
        if (config == BVH_LEAF) {
            Entity other = (Entity){ (EntityID)node->left, e.context };
            Collider c1 = 0;
            Collider c2 = 0;
            DynamicCollisionComponent* dc1 = NULL;
            DynamicCollisionComponent* dc2 = NULL;
            if (HasComponent(e, DynamicCollisionComponent)) {
                dc1 = GetComponent(e, DynamicCollisionComponent);
                c1 = dc1->collider;
            } else if (HasComponent(e, StaticCollisionComponent)) {
                StaticCollisionComponent* sc = GetComponent(e, StaticCollisionComponent);
                c1 = sc->collider;
            } else EZ_ASSERT(FALSE, "BVH query entity does not have a valid collision component");
            if (HasComponent(other, DynamicCollisionComponent)) {
                dc2 = GetComponent(other, DynamicCollisionComponent);
                c2 = dc2->collider;
            } else if (HasComponent(other, StaticCollisionComponent)) {
                StaticCollisionComponent* sc = GetComponent(other, StaticCollisionComponent);
                c2 = sc->collider;
            } else EZ_ASSERT(FALSE, "BVH query entity does not have a valid collision component");
            if (dispatch(e, c1, other, c2)) {
                if (dc1) dc1->collided = TRUE;
                if (dc2) dc2->collided = TRUE;
            }
        }
    }
}
