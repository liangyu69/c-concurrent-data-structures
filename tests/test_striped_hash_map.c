#include "../include/striped_hash_map.h"
#include "../include/locked_hash_map.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>

#define NTHREADS       4
#define OPS_PER_THREAD 10000
#define TOTAL_KEYS     (NTHREADS * OPS_PER_THREAD)

/* int key 的 hash 和 equal */
static size_t int_hash(const void* key) {
    return (size_t)(unsigned int)(*(const int*)key);
}
static bool int_equal(const void* a, const void* b) {
    return *(const int*)a == *(const int*)b;
}

/* 预分配 key */
static int g_keys[TOTAL_KEYS];

/* 计时 */
static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* ============ 分段锁版 ============ */
typedef struct {
    StripedHashMap* map;
    int             id;
} StripedArg;

static void* striped_put_worker(void* arg) {
    StripedArg* ta = arg;
    int start = ta->id * OPS_PER_THREAD;
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int* k = &g_keys[start + i];
        striped_hashmap_put(ta->map, k, k);
    }
    return NULL;
}

static void* striped_get_worker(void* arg) {
    StripedArg* ta = arg;
    int start = ta->id * OPS_PER_THREAD;
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int* k = &g_keys[start + i];
        void* v = striped_hashmap_get(ta->map, k);
        assert(v == k);
    }
    return NULL;
}

/* ============ 全局锁版 ============ */
typedef struct {
    LockedHashMap* map;
    int            id;
} LockedArg;

static void* locked_put_worker(void* arg) {
    LockedArg* ta = arg;
    int start = ta->id * OPS_PER_THREAD;
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int* k = &g_keys[start + i];
        locked_hashmap_put(ta->map, k, k);
    }
    return NULL;
}

static void* locked_get_worker(void* arg) {
    LockedArg* ta = arg;
    int start = ta->id * OPS_PER_THREAD;
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int* k = &g_keys[start + i];
        void* v = locked_hashmap_get(ta->map, k);
        assert(v == k);
    }
    return NULL;
}

/* ============ 主函数 ============ */
int main(void) {
    for (int i = 0; i < TOTAL_KEYS; i++) g_keys[i] = i;

    pthread_t threads[NTHREADS];
    double t0, t1;

    /* --- 全局锁版：256 个桶 --- */
    printf("=== LockedHashMap (全局锁, 1048576 桶) ===\n");
    LockedHashMap* lmap = locked_hashmap_create(1048576, int_hash, int_equal);
    assert(lmap);
    LockedArg largs[NTHREADS];

    t0 = now_sec();
    for (int i = 0; i < NTHREADS; i++) {
        largs[i] = (LockedArg){ .map = lmap, .id = i };
        pthread_create(&threads[i], NULL, locked_put_worker, &largs[i]);
    }
    for (int i = 0; i < NTHREADS; i++) pthread_join(threads[i], NULL);
    t1 = now_sec();
    printf("put: %.3f s (%d ops)\n", t1 - t0, TOTAL_KEYS);
    assert(locked_hashmap_size(lmap) == TOTAL_KEYS);

    t0 = now_sec();
    for (int i = 0; i < NTHREADS; i++) {
        largs[i] = (LockedArg){ .map = lmap, .id = i };
        pthread_create(&threads[i], NULL, locked_get_worker, &largs[i]);
    }
    for (int i = 0; i < NTHREADS; i++) pthread_join(threads[i], NULL);
    t1 = now_sec();
    printf("get: %.3f s (%d ops)\n", t1 - t0, TOTAL_KEYS);

    /* --- 分段锁版：16 段 × 16 桶 = 256 桶 --- */
    printf("\n=== StripedHashMap (4 段 × 262144 桶 = 1048576 桶) ===\n");
    StripedHashMap* smap = striped_hashmap_create(4, 262144, int_hash, int_equal);
    assert(smap);
    StripedArg sargs[NTHREADS];

    t0 = now_sec();
    for (int i = 0; i < NTHREADS; i++) {
        sargs[i] = (StripedArg){ .map = smap, .id = i };
        pthread_create(&threads[i], NULL, striped_put_worker, &sargs[i]);
    }
    for (int i = 0; i < NTHREADS; i++) pthread_join(threads[i], NULL);
    t1 = now_sec();
    printf("put: %.3f s (%d ops)\n", t1 - t0, TOTAL_KEYS);
    assert(striped_hashmap_size(smap) == TOTAL_KEYS);

    t0 = now_sec();
    for (int i = 0; i < NTHREADS; i++) {
        sargs[i] = (StripedArg){ .map = smap, .id = i };
        pthread_create(&threads[i], NULL, striped_get_worker, &sargs[i]);
    }
    for (int i = 0; i < NTHREADS; i++) pthread_join(threads[i], NULL);
    t1 = now_sec();
    printf("get: %.3f s (%d ops)\n", t1 - t0, TOTAL_KEYS);

    /* --- 清理 --- */
    locked_hashmap_destroy(lmap);
    striped_hashmap_destroy(smap);

    printf("\nAll tests passed.\n");
    return 0;
}