// tests/test_hash_map.c
#include "../include/hash_map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ============ 整数 key 的 hash 和 equal ============ */
static size_t int_hash(const void* key) {
    int k = *(const int*)key;
    return (size_t)(unsigned int)k;
}

static bool int_equal(const void* a, const void* b) {
    return *(const int*)a == *(const int*)b;
}

/* ============ test_basic ============ */
static void test_basic(void) {
    printf("=== test_basic ===\n");

    HashMap* map = hashmap_create(8, int_hash, int_equal);
    assert(map != NULL);

    assert(hashmap_size(map) == 0);
    assert(!hashmap_contains(map, &(int){1}));

    int k1 = 1, k2 = 2, k3 = 3;
    char* v1 = "one";
    char* v2 = "two";
    char* v3 = "three";

    assert(hashmap_put(map, &k1, v1));
    assert(hashmap_put(map, &k2, v2));
    assert(hashmap_put(map, &k3, v3));

    assert(hashmap_size(map) == 3);

    assert(hashmap_contains(map, &k1));
    assert(hashmap_contains(map, &k2));
    assert(hashmap_contains(map, &k3));

    assert(hashmap_get(map, &k1) == v1);
    assert(hashmap_get(map, &k2) == v2);
    assert(hashmap_get(map, &k3) == v3);

    int k99 = 99;
    assert(!hashmap_contains(map, &k99));
    assert(hashmap_get(map, &k99) == NULL);

    hashmap_destroy(map);
    printf("test_basic passed\n\n");
}

/* ============ test_update ============ */
static void test_update(void) {
    printf("=== test_update ===\n");

    HashMap* map = hashmap_create(8, int_hash, int_equal);
    assert(map != NULL);

    int k = 42;
    char* v1 = "first";
    char* v2 = "second";

    assert(hashmap_put(map, &k, v1));
    assert(hashmap_size(map) == 1);
    assert(hashmap_get(map, &k) == v1);

    /* 同一个 key 再插入：更新 value，size 不变 */
    assert(hashmap_put(map, &k, v2));
    assert(hashmap_size(map) == 1);
    assert(hashmap_get(map, &k) == v2);

    /* 再更新回 v1 */
    assert(hashmap_put(map, &k, v1));
    assert(hashmap_size(map) == 1);
    assert(hashmap_get(map, &k) == v1);

    hashmap_destroy(map);
    printf("test_update passed\n\n");
}

/* ============ test_remove ============ */
static void test_remove(void) {
    printf("=== test_remove ===\n");

    HashMap* map = hashmap_create(8, int_hash, int_equal);
    assert(map != NULL);

    int k1 = 1, k2 = 2, k3 = 3;
    char* v1 = "one";
    char* v2 = "two";
    char* v3 = "three";

    hashmap_put(map, &k1, v1);
    hashmap_put(map, &k2, v2);
    hashmap_put(map, &k3, v3);
    assert(hashmap_size(map) == 3);

    /* 删除存在的 key */
    assert(hashmap_remove(map, &k2));
    assert(hashmap_size(map) == 2);
    assert(!hashmap_contains(map, &k2));
    assert(hashmap_get(map, &k2) == NULL);

    /* 其他 key 不受影响 */
    assert(hashmap_contains(map, &k1));
    assert(hashmap_contains(map, &k3));
    assert(hashmap_get(map, &k1) == v1);
    assert(hashmap_get(map, &k3) == v3);

    /* 重复删除同一个 key：第二次应返回 false */
    assert(!hashmap_remove(map, &k2));

    /* 删除不存在的 key */
    int k99 = 99;
    assert(!hashmap_remove(map, &k99));
    assert(hashmap_size(map) == 2);

    /* 删完所有 */
    assert(hashmap_remove(map, &k1));
    assert(hashmap_remove(map, &k3));
    assert(hashmap_size(map) == 0);
    assert(!hashmap_contains(map, &k1));
    assert(!hashmap_contains(map, &k3));

    hashmap_destroy(map);
    printf("test_remove passed\n\n");
}

/* ============ test_resize ============ */
static void test_resize(void) {
    printf("=== test_resize ===\n");

    /* 容量 4，阈值 0.75，插入第 4 个时触发扩容 */
    HashMap* map = hashmap_create(4, int_hash, int_equal);
    assert(map != NULL);

    #define N 100
    int* keys = malloc(N * sizeof(int));
    char** vals = malloc(N * sizeof(char*));
    assert(keys && vals);

    /* 插入 100 个元素，会多次扩容 */
    for (int i = 0; i < N; i++) {
        keys[i] = i;
        vals[i] = malloc(32);
        snprintf(vals[i], 32, "val_%d", i);
        assert(hashmap_put(map, &keys[i], vals[i]));
    }

    assert(hashmap_size(map) == N);

    /* 验证所有元素都还在 */
    for (int i = 0; i < N; i++) {
        assert(hashmap_contains(map, &keys[i]));
        assert(hashmap_get(map, &keys[i]) == vals[i]);
    }

    /* 删除一半，再验证 */
    for (int i = 0; i < N; i += 2) {
        assert(hashmap_remove(map, &keys[i]));
    }
    assert(hashmap_size(map) == N / 2);

    for (int i = 0; i < N; i++) {
        if (i % 2 == 0) {
            assert(!hashmap_contains(map, &keys[i]));
        } else {
            assert(hashmap_contains(map, &keys[i]));
            assert(hashmap_get(map, &keys[i]) == vals[i]);
        }
    }

    /* 释放 value */
    for (int i = 0; i < N; i++) {
        free(vals[i]);
    }
    free(keys);
    free(vals);

    hashmap_destroy(map);
    printf("test_resize passed\n\n");
    #undef N
}

/* ============ test_edge ============ */
static void test_edge(void) {
    printf("=== test_edge ===\n");

    /* NULL 参数 */
    assert(hashmap_create(0, int_hash, int_equal) == NULL);
    assert(hashmap_create(8, NULL, int_equal) == NULL);
    assert(hashmap_create(8, int_hash, NULL) == NULL);

    assert(!hashmap_put(NULL, &(int){1}, "x"));

    HashMap* map = hashmap_create(8, int_hash, int_equal);
    assert(map != NULL);

    assert(!hashmap_put(map, NULL, "x"));

    /* 空 map 上 get / contains / remove */
    int k = 1;
    assert(hashmap_get(map, &k) == NULL);
    assert(!hashmap_contains(map, &k));
    assert(!hashmap_remove(map, &k));

    /* size(NULL) */
    assert(hashmap_size(NULL) == 0);

    /* destroy(NULL) 不应崩 */
    hashmap_destroy(NULL);

    hashmap_destroy(map);
    printf("test_edge passed\n\n");
}

/* ============ main ============ */
int main(void) {
    test_basic();
    test_update();
    test_remove();
    test_resize();
    test_edge();
    printf("All tests passed.\n");
    return 0;
}