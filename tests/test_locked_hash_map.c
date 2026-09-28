#include "../include/locked_hash_map.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

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

/* 预分配 key，生命周期覆盖整个测试 */
static int g_keys[TOTAL_KEYS];

typedef struct {
    LockedHashMap* map;
    int            id;
} ThreadArg;

/* 每个线程 put 自己那段 key */
static void* put_worker(void* arg) {
    ThreadArg* ta = arg;
    int start = ta->id * OPS_PER_THREAD;
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int* k = &g_keys[start + i];
        locked_hashmap_put(ta->map, k, k);
    }
    return NULL;
}

/* 每个线程读自己那段 key */
static void* get_worker(void* arg) {
    ThreadArg* ta = arg;
    int start = ta->id * OPS_PER_THREAD;
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int* k = &g_keys[start + i];
        void* v = locked_hashmap_get(ta->map, k);
        assert(v == k);
    }
    return NULL;
}

int main(void) {
    for (int i = 0; i < TOTAL_KEYS; i++) g_keys[i] = i;

    printf("=== test_concurrent ===\n");
    LockedHashMap* map = locked_hashmap_create(16, int_hash, int_equal);
    assert(map);

    pthread_t threads[NTHREADS];
    ThreadArg args[NTHREADS];

    /* 并发 put */
    for (int i = 0; i < NTHREADS; i++) {
        args[i] = (ThreadArg){ .map = map, .id = i };
        pthread_create(&threads[i], NULL, put_worker, &args[i]);
    }
    for (int i = 0; i < NTHREADS; i++) pthread_join(threads[i], NULL);

    printf("after put: size = %zu (expected %d)\n",
           locked_hashmap_size(map), TOTAL_KEYS);
    assert(locked_hashmap_size(map) == TOTAL_KEYS);

    /* 并发 get */
    for (int i = 0; i < NTHREADS; i++) {
        pthread_create(&threads[i], NULL, get_worker, &args[i]);
    }
    for (int i = 0; i < NTHREADS; i++) pthread_join(threads[i], NULL);

    printf("all get passed\n");

    locked_hashmap_destroy(map);
    printf("test_concurrent passed\n");
    return 0;
}