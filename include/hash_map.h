#ifndef HASH_MAP_H
#define HASH_MAP_H

#include <stdbool.h>
#include <stddef.h>

/*
 * ============================================================================
 *  HashMap —— 单线程哈希表（链地址法）
 * ============================================================================
 *
 *  定位：单线程哈希表，作为后续并发版本（全局锁/分段锁）的功能与性能基线。
 *
 *  特性：
 *    - 链地址法：桶数组 + 链表
 *    - 支持动态扩容（负载因子 > 0.75 时翻倍）
 *    - key/value 均为 void*，不拷贝，不负责生命周期
 *    - 调用者提供 hash 函数和 equal 函数
 *
 *  非线程安全：多线程同时访问需要外部同步。
 *
 *  所有权：
 *    - map 只存 key/value 指针，不拷贝内容；
 *    - 删除节点或销毁 map 时，不 free key/value；
 *    - key/value 的生命周期由调用者管理。
 * ============================================================================
 */

 typedef struct HashMap HashMap;

 /* 哈希函数：把 key 映射成整数 */
typedef size_t (*hash_fn_t)(const void* key);

/* 比较函数：判断两个 key 是否相等 */
typedef bool (*equal_fn_t)(const void* a, const void* b);

/**
 * @brief 创建哈希表。
 * @param capacity 初始桶数量，必须 > 0。
 * @param hash_fn  哈希函数，不能为 NULL。
 * @param equal_fn 比较函数，不能为 NULL。
 * @return 成功返回 map；失败返回 NULL。
 */
HashMap* hashmap_create(size_t capacity,
                        hash_fn_t hash_fn,
                        equal_fn_t equal_fn);


/**
 * @brief 插入或更新键值对。
 * @param map   哈希表。
 * @param key   键（非 NULL）。
 * @param value 值。
 * @return 成功 true；失败（参数非法/内存不足）false。
 *
 * @note key 已存在时更新 value，不新建节点。
 * @note 不拷贝 key/value，只存指针。
 */
bool hashmap_put(HashMap* map, void* key, void* value);


/**
 * @brief 查找 key 对应的 value。
 * @param map 哈希表。
 * @param key 键。
 * @return 找到返回 value；找不到或参数非法返回 NULL。
 *
 * @warning value 本身可能是 NULL，所以"返回 NULL"不代表一定"找不到"。
 *          若需要区分，请用 hashmap_contains。
 */
void* hashmap_get(HashMap* map, const void* key);

/**
 * @brief 判断 key 是否存在。
 */
bool hashmap_contains(HashMap* map, const void* key);


/**
 * @brief 删除 key 对应的节点。
 * @return 删除成功 true；key 不存在或参数非法 false。
 *
 * @note 只释放节点本身，不释放 key/value。
 */
bool hashmap_remove(HashMap* map, const void* key);


/**
 * @brief 当前元素个数。
 */
size_t hashmap_size(HashMap* map);


/**
 * @brief 销毁哈希表。
 * @param map 哈希表；为 NULL 直接返回。
 *
 * @warning 只释放节点和内部数组，不释放 key/value。
 */
void hashmap_destroy(HashMap* map);

#endif /* HASH_MAP_H */