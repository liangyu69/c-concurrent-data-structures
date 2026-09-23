#ifndef LOCKED_HASH_MAP_H
#define LOCKED_HASH_MAP_H

#include <stdbool.h>
#include <stddef.h>

/*
 * ============================================================================
 *  LockedHashMap —— 全局锁哈希表
 * ============================================================================
 *
 *  定位：用一把全局 mutex 保护整个哈希表的并发版本。
 *        作为并发哈希表的"基线"——最简单，但并发度最低。
 *
 *  线程安全： 所有接口都加锁，多线程可并发调用。
 *
 *  并发度：同一时刻只有一个线程能操作 map（全局锁串行化）。
 *
 *  特性：
 *    - 链地址法
 *    - 动态扩容（负载因子 > 0.75 时翻倍）
 *    - key/value 均为 void*，不拷贝，不负责生命周期
 *    - 调用者提供 hash 函数和 equal 函数
 *
 *  所有权：
 *    - map 只存 key/value 指针，不拷贝内容；
 *    - 删除节点或销毁 map 时，不 free key/value；
 *    - key/value 的生命周期由调用者管理。
 * ============================================================================
 */

typedef struct LockedHashMap LockedHashMap;

/* 哈希函数：把 key 映射成整数 */
typedef size_t (*hash_fn_t)(const void* key);

/* 比较函数：判断两个 key 是否相等 */
typedef bool (*equal_fn_t)(const void* a, const void* b);

/**
 * @brief 创建全局锁哈希表。
 * @param capacity 初始桶数量，必须 > 0。
 * @param hash_fn  哈希函数，不能为 NULL。
 * @param equal_fn 比较函数，不能为 NULL。
 * @return 成功返回 map；失败返回 NULL。
 */
LockedHashMap* locked_hashmap_create(size_t capacity,
                                     hash_fn_t hash_fn,
                                     equal_fn_t equal_fn);

/**
 * @brief 插入或更新键值对（线程安全）。
 * @param map   哈希表。
 * @param key   键（非 NULL）。
 * @param value 值。
 * @return 成功 true；失败 false。
 */
bool locked_hashmap_put(LockedHashMap* map, void* key, void* value);

/**
 * @brief 查找 key 对应的 value（线程安全）。
 * @return 找到返回 value；找不到或参数非法返回 NULL。
 */
void* locked_hashmap_get(LockedHashMap* map, const void* key);

/**
 * @brief 判断 key 是否存在（线程安全）。
 */
bool locked_hashmap_contains(LockedHashMap* map, const void* key);

/**
 * @brief 删除 key 对应的节点（线程安全）。
 * @return 删除成功 true；key 不存在或参数非法 false。
 */
bool locked_hashmap_remove(LockedHashMap* map, const void* key);

/**
 * @brief 当前元素个数（线程安全）。
 */
size_t locked_hashmap_size(LockedHashMap* map);

/**
 * @brief 销毁哈希表（线程安全，但调用前必须无其他线程在使用）。
 * @warning 只释放节点和内部数组，不释放 key/value。
 *          调用前必须保证没有其他线程正在调用本 map 的接口。
 */
void locked_hashmap_destroy(LockedHashMap* map);

#endif /* LOCKED_HASH_MAP_H */