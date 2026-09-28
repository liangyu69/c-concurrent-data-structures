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

/* 内部扩容，不加锁（调用者已持锁） */
static void locked_hashmap_resize(LockedHashMap* map, size_t new_capacity) {
    HashNode** new_buckets = calloc(new_capacity, sizeof(*new_buckets));
    if (!new_buckets) return;

    for (size_t i = 0; i < map->capacity; i++) {
        HashNode* cur = map->buckets[i];
        while (cur) {
            HashNode* next = cur->next;
            size_t new_index = map->hash_fn(cur->key) % new_capacity;
            cur->next = new_buckets[new_index];
            new_buckets[new_index] = cur;
            cur = next;
        }
    }

    free(map->buckets);
    map->buckets  = new_buckets;
    map->capacity = new_capacity;
}


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



bool locked_hashmap_put(LockedHashMap* map, void* key, void* value) {
    if (!map || !key) return false;

    pthread_mutex_lock(&map->mutex);

    /* ---- 临界区开始 ---- */

    size_t index = map->hash_fn(key) % map->capacity;

    HashNode* cur = map->buckets[index];
    while (cur) {
        if (map->equal_fn(cur->key, key)) {
            cur->value = value;            /* 已存在 → 更新 */
            pthread_mutex_unlock(&map->mutex);
            return true;
        }
        cur = cur->next;
    }

    HashNode* node = malloc(sizeof(*node));
    if (!node) {
        pthread_mutex_unlock(&map->mutex);
        return false;
    }
    node->key   = key;
    node->value = value;
    node->next  = map->buckets[index];
    map->buckets[index] = node;
    map->size++;

    if ((double)map->size / map->capacity > LOAD_FACTOR_THRESHOLD) {
        locked_hashmap_resize(map, map->capacity * 2);
    }

    /* ---- 临界区结束 ---- */

    pthread_mutex_unlock(&map->mutex);
    return true;
}


void* locked_hashmap_get(LockedHashMap* map, const void* key) {
    if (!map || !key) return NULL;

    pthread_mutex_lock(&map->mutex);
    size_t index = map->hash_fn(key) % map->capacity;

    HashNode* cur = map->buckets[index];
    while (cur) {
        if (map->equal_fn(cur->key, key)) {
            void* v = cur->value;
            pthread_mutex_unlock(&map->mutex);
            return v;
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&map->mutex);
    return NULL;
}

bool locked_hashmap_contains(LockedHashMap* map, const void* key) {
    if (!map || !key) return false;

    pthread_mutex_lock(&map->mutex);
    size_t index = map->hash_fn(key) % map->capacity;

    HashNode* cur = map->buckets[index];
    while (cur) {
        if (map->equal_fn(cur->key, key)) {
            pthread_mutex_unlock(&map->mutex);
            return true;
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&map->mutex);
    return false;
}


bool locked_hashmap_remove(LockedHashMap* map, const void* key) {
    if (!map || !key) return false;

    pthread_mutex_lock(&map->mutex);
    size_t index = map->hash_fn(key) % map->capacity;

    HashNode* cur  = map->buckets[index];
    HashNode* prev = NULL;

    while (cur) {
        if (map->equal_fn(cur->key, key)) {
            if (prev) prev->next = cur->next;
            else      map->buckets[index] = cur->next;

            free(cur);
            map->size--;
            pthread_mutex_unlock(&map->mutex);
            return true;
        }
        prev = cur;
        cur  = cur->next;
    }
    pthread_mutex_unlock(&map->mutex);
    return false;
}


size_t locked_hashmap_size(LockedHashMap* map) {
    if (!map) return 0;

    pthread_mutex_lock(&map->mutex);
    size_t s = map->size;
    pthread_mutex_unlock(&map->mutex);
    return s;
}


void locked_hashmap_destroy(LockedHashMap* map) {
    if (!map) return;

    for (size_t i = 0; i < map->capacity; i++) {
        HashNode* cur = map->buckets[i];
        while (cur) {
            HashNode* next = cur->next;
            free(cur);
            cur = next;
        }
    }
    free(map->buckets);
    pthread_mutex_destroy(&map->mutex);
    free(map);
}