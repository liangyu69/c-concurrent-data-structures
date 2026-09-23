#include "locked_hash_map.h"
#include <stdlib.h>
#include <pthread.h>

/* 负载因子阈值：元素数 / 桶数 > 0.75 时扩容 */
#define LOAD_FACTOR_THRESHOLD 0.75

/* 链表节点：存一个键值对 */
typedef struct HashNode {
    void*            key;
    void*            value;
    struct HashNode* next;
} HashNode;

/* 全局锁哈希表 */
struct LockedHashMap {
    HashNode**       buckets;    // 桶数组
    size_t           capacity;   // 桶数量
    size_t           size;       // 当前元素个数
    hash_fn_t        hash_fn;    // 哈希函数
    equal_fn_t       equal_fn;   // 比较函数
    pthread_mutex_t  mutex;      // ← 全局锁
};


LockedHashMap* locked_hashmap_create(size_t capacity,
                                     hash_fn_t hash_fn,
                                     equal_fn_t equal_fn) {
    if (capacity == 0 || !hash_fn || !equal_fn) return NULL;

    LockedHashMap* map = malloc(sizeof(*map));
    if (!map) return NULL;

    map->buckets = calloc(capacity, sizeof(*map->buckets));
    if (!map->buckets) {
        free(map);
        return NULL;
    }

    map->capacity = capacity;
    map->size     = 0;
    map->hash_fn  = hash_fn;
    map->equal_fn = equal_fn;

    /* 初始化 mutex */
    if (pthread_mutex_init(&map->mutex, NULL) != 0) {
        free(map->buckets);
        free(map);
        return NULL;
    }

    return map;
}