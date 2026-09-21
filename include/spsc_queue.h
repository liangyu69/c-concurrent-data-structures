#ifndef SPSC_QUEUE_H
#define SPSC_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>

/*
 * ============================================================================
 *  SPSCQueue —— 单生产者单消费者无锁环形队列
 * ============================================================================
 *
 *  约束：
 *    - 恰好 1 个生产者线程调用 spsc_push
 *    - 恰好 1 个消费者线程调用 spsc_pop
 *    - 违反此约束会导致未定义行为
 *
 *  特性：
 *    - 无锁：不使用 mutex，靠原子操作 + 内存序
 *    - 非阻塞：满/空时立刻返回 false/NULL，不等待
 *    - 有界：容量固定，创建时指定
 *    - 不管理数据生命周期：只存 void*，不负责释放
 *
 *  实现要点：
 *    - 每个变量只有一个写者（head 只被消费者写，tail 只被生产者写）
 *    - 生产者写 tail 用 release，消费者读 tail 用 acquire（配对）
 *    - head 与 tail 分处不同 cache line，避免 false sharing
 *
 *  典型场景：日志、网络收包、音频管道等 1P1C 高吞吐场景。
 * ============================================================================
 */

typedef struct SPSCQueue SPSCQueue;

/**
 * @brief 创建 SPSC 队列。
 * @param capacity 容量，必须 > 0。建议取 2 的幂以加速取模。
 * @return 成功返回队列指针；失败返回 NULL。
 */
SPSCQueue* spsc_create(size_t capacity);

/**
 * @brief 生产者接口：尝试入队（非阻塞）。
 * @param q    队列指针；为 NULL 返回 false。
 * @param data 要存入的数据。
 * @return 成功 true；队列满或参数非法 false。
 * @warning 只能由同一个生产者线程调用。
 */
bool spsc_push(SPSCQueue* q, void* data);

/**
 * @brief 消费者接口：尝试出队（非阻塞）。
 * @param q 队列指针；为 NULL 返回 NULL。
 * @return 成功返回数据指针；队列空或参数非法返回 NULL。
 * @warning 只能由同一个消费者线程调用。
 */
void* spsc_pop(SPSCQueue* q);

/**
 * @brief 销毁队列。
 * @param q 队列指针；为 NULL 直接返回。
 * @warning 调用前必须保证生产者和消费者都已停止使用队列。
 *          本函数不释放 data 指向的内存。
 */
void spsc_destroy(SPSCQueue* q);
 

#endif /* SPSC_QUEUE_H */