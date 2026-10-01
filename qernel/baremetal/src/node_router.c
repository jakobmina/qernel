#include "node_router.h"

/* Small, fixed-size buckets: a plain insertion sort beats recursive
 * quicksort here (no call-stack risk, no pivot degenerate cases) and is
 * what sort_v1/sort_v2 effectively reduce to once a bucket is this small. */
static void bucket_insertion_sort(int32_t *data, size_t n) {
    for (size_t i = 1; i < n; i++) {
        int32_t key = data[i];
        size_t j = i;
        while (j > 0 && data[j - 1] > key) {
            data[j] = data[j - 1];
            j--;
        }
        data[j] = key;
    }
}

void router_reset(NodeRouter *r) {
    for (int i = 0; i < ROUTER_NUM_NODES; i++) r->nodes[i].size = 0;
    r->dropped = 0;
    r->min_key = 0;
    r->max_key = 0;
}

void router_distribute_sort(NodeRouter *r, const int32_t *keys, size_t n) {
    router_reset(r);
    if (n == 0) return;

    /* 1. Range scan (equivalent to sort_seq's min/max pass) */
    int32_t mn = keys[0], mx = keys[0];
    for (size_t i = 1; i < n; i++) {
        if (keys[i] < mn) mn = keys[i];
        if (keys[i] > mx) mx = keys[i];
    }
    r->min_key = mn;
    r->max_key = mx;

    /* +1 on the range (not a float step) sidesteps the div-by-zero that
     * distribuir_seq hits when every key is identical (mx == mn). */
    int32_t span = (mx - mn) + 1;

    /* 2. Distribute: single sequential pass over the key stream, one
     *    key-range per virtual node (distribuir_seq, no threads). */
    for (size_t i = 0; i < n; i++) {
        int node = (int)(((int64_t)(keys[i] - mn) * ROUTER_NUM_NODES) / span);
        if (node >= ROUTER_NUM_NODES) node = ROUTER_NUM_NODES - 1;
        if (node < 0) node = 0;

        RouterBucket *b = &r->nodes[node];
        if (b->size < ROUTER_BUCKET_CAP) {
            b->data[b->size++] = keys[i];
        } else {
            r->dropped++;
        }
    }

    /* 3. Sort each node's bucket in place, one node at a time
     *    (sort_seq: the sequential baseline, no thread_sort workers yet). */
    for (int node = 0; node < ROUTER_NUM_NODES; node++) {
        bucket_insertion_sort(r->nodes[node].data, r->nodes[node].size);
    }
}
