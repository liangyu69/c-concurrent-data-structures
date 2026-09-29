#ifndef STRIPED_HASH_MAP_H
#define STRIPED_HASH_MAP_H

#include <stdbool.h>
#include <stddef.h>

/*
 * ============================================================================
 *  StripedHashMap —— 分段锁哈希表
 * ============================================================================
 *
 *  定位：把哈希表分成 N 段，每段一个独立的小哈希表 + 一把锁。
 *        操作时只锁目标段，不同段的操作可以并发。
 *        并发度 = 段数（远高于全局锁的 1）。
 *
 *  线程安全：✅ 所有接口都加锁，多线程可并发调用。
 *
 *  并发度：理想情况下 = num_stripes（key 均匀分布时）。
 *
 *  特性：
 *    - 每段独立：独立桶数组、独立锁、独立扩容
 *    - 链地址法（段内是单线程哈希表）
 *    - key/value 均为 void*，不拷贝，不负责生命周期
 *    - 调用者提供 hash 函数和 equal 函数
 *
 *  所有权：
 *    - map 只存 key/value 指针，不拷贝内容；
 *    - 删除节点或销毁 map 时，不 free key/value；
 *    - key/value 的生命周期由调用者管理。
 *
 *  局限：
 *    - size() 不精确（遍历各段求和时可能被并发修改）
 *    - 段内仍是串行（同一段的操作抢同一把锁）
 *    - 哈希不均时会退化（key 都到同一段）
 * ============================================================================
 */

typedef struct StripedHashMap StripedHashMap;

/* 哈希函数：把 key 映射成整数 */
typedef size_t (*hash_fn_t)(const void* key);

/* 比较函数：判断两个 key 是否相等 */
typedef bool (*equal_fn_t)(const void* a, const void* b);

/**
 * @brief 创建分段锁哈希表。
 * @param num_stripes 段数，必须 > 0。建议取 2 的幂（如 16、32）。
 * @param capacity_per_stripe 每段初始桶数量，必须 > 0。
 * @param hash_fn  哈希函数，不能为 NULL。
 * @param equal_fn 比较函数，不能为 NULL。
 * @return 成功返回 map；失败返回 NULL。
 */
StripedHashMap* striped_hashmap_create(size_t num_stripes,
                                       size_t capacity_per_stripe,
                                       hash_fn_t hash_fn,
                                       equal_fn_t equal_fn);

/**
 * @brief 插入或更新键值对（线程安全）。
 */
bool striped_hashmap_put(StripedHashMap* map, void* key, void* value);

/**
 * @brief 查找 key 对应的 value（线程安全）。
 */
void* striped_hashmap_get(StripedHashMap* map, const void* key);

/**
 * @brief 判断 key 是否存在（线程安全）。
 */
bool striped_hashmap_contains(StripedHashMap* map, const void* key);

/**
 * @brief 删除 key 对应的节点（线程安全）。
 */
bool striped_hashmap_remove(StripedHashMap* map, const void* key);

/**
 * @brief 当前元素个数（线程安全，但**近似值**）。
 * @note 遍历各段求和，各段可能被并发修改，所以不精确。
 */
size_t striped_hashmap_size(StripedHashMap* map);

/**
 * @brief 销毁哈希表。
 * @warning 调用前必须保证没有任何其他线程正在使用本 map。
 *          本函数不加锁（前提已保证无并发）。
 */
void striped_hashmap_destroy(StripedHashMap* map);

#endif /* STRIPED_HASH_MAP_H */