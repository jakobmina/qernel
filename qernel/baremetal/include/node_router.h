#ifndef NODE_ROUTER_H
#define NODE_ROUTER_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief NodeRouter: single-threaded key-range distributor + sorter.
 *
 * Ported from the userspace NodeSort_v2 benchmark's bucket-by-key-range
 * idea (distribuir_seq / sort_seq), stripped of pthreads and malloc/realloc
 * since this freestanding kernel has neither threads nor a heap yet.
 * Buckets are fixed-capacity static arrays; distribution and per-bucket
 * sorting both run sequentially on the single boot core.
 *
 * Scaling path: once the kernel brings up a real allocator and the other
 * CPUs (APIC/AP bootstrap), the distribute step can run one thread per
 * node (like sort_v2's thread_distribute) and each bucket can be sorted
 * on its own core, exactly as NodeSort_v2 does on the host.
 */

#define ROUTER_NUM_NODES 4
#define ROUTER_BUCKET_CAP 128 /* fixed capacity per node: no heap to grow into */

typedef struct {
    int32_t data[ROUTER_BUCKET_CAP];
    size_t  size;
} RouterBucket;

typedef struct {
    RouterBucket nodes[ROUTER_NUM_NODES];
    size_t       dropped;  /* keys that arrived once their node's bucket was full */
    int32_t      min_key;
    int32_t      max_key;
} NodeRouter;

void router_reset(NodeRouter *r);
void router_distribute_sort(NodeRouter *r, const int32_t *keys, size_t n);

#endif /* NODE_ROUTER_H */
