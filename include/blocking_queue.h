#ifndef CONCURRENT_QUEUE_H
#define CONCURRENT_QUEUE_H

#include <stdbool.h>

/*
 * ============================================================================
 *  ConcurrentQueue —— 有界线程安全队列（生产者-消费者）
 * ============================================================================
 *
 *  实现：环形数组 + pthread mutex + 两个条件变量
 *
 *  语义：
 *    - 有界：容量固定，满时 push 阻塞，空时 pop 阻塞。
 *    - 线程安全：多生产者、多消费者可并发调用。
 *    - 不管理数据生命周期：队列只存 void*，不负责释放。
 *
 *  局限性（重要）：
 *    1. 【无关闭语义】消费者在空队列上会永久阻塞，没有"关闭队列并
 *       唤醒所有等待者"的接口。调用者必须自行保证在 destroy 前，
 *       所有线程都已退出 push/pop。
 *    2. 【destroy 不安全】若 destroy 时有线程阻塞在 push/pop，行为未
 *       定义。destroy 前必须保证无并发使用者。
 *    3. 【不释放数据】队列只释放自身结构，不释放 data 指向的内存。
 *       调用者负责数据的分配与回收。
 *    4. 【阻塞式接口】push/pop 都可能永久阻塞，没有 try_push/
 *       try_pop/超时版本。
 *
 *  典型用法（服务器场景）：
 *    主线程作为生产者：accept 后 queue_push(fd)
 *    工作线程作为消费者：queue_pop() 取 fd 处理
 *    进程靠信号终止，不调用 queue_destroy。
 *
 *  若需要关闭语义，请在此基础上扩展 closed 标志 + broadcast，
 *  或改用信号量实现（参见 CSAPP sbuf）。
 * ============================================================================
 */

typedef struct ConcurrentQueue ConcurrentQueue;

/**
 * @brief 创建并发队列。
 *
 * @param capacity 队列最大容量，必须 > 0。
 * @return 成功返回队列指针；capacity <= 0 或内存分配失败返回 NULL。
 *
 * @note 返回的队列初始为空。需与 queue_destroy 配对使用。
 */
ConcurrentQueue* queue_create(int capacity);

/**
 * @brief 向队列尾部推入一个元素（生产者）。
 *
 * @param queue 队列指针；为 NULL 时返回 false。
 * @param data  要存入的数据（void*，可存任意类型）。
 * @return 成功返回 true；queue 为 NULL 返回 false。
 *
 * @warning 当队列已满（count == capacity）时，本函数会【阻塞】，
 *          直到有消费者 pop 出一个元素腾出空位。没有非阻塞版本。
 *
 * @note 队列不接管 data 的所有权，调用者需自行管理其生命周期。
 */
bool queue_push(ConcurrentQueue* queue, void* data);

/**
 * @brief 从队列头部弹出一个元素（消费者）。
 *
 * @param queue 队列指针；为 NULL 时返回 NULL。
 * @return 成功返回弹出的数据指针。
 *
 * @warning 当队列为空（count == 0）时，本函数会【阻塞】，直到有
 *          生产者 push 进新元素。若生产者已全部结束且不会再 push，
 *          本函数将【永久阻塞】。本实现没有关闭机制来唤醒它。
 *
 * @note 返回 NULL 表示参数非法（queue == NULL），不代表"队列空"——
 *       队列空时会阻塞而非返回 NULL。
 */
void* queue_pop(ConcurrentQueue* queue);

/**
 * @brief 销毁队列，释放内部缓冲区与同步资源。
 *
 * @param queue 队列指针；为 NULL 时直接返回。
 *
 * @warning 调用前必须保证：
 *   1. 没有任何线程正在或将要调用 queue_push / queue_pop；
 *      若有线程阻塞在 queue_pop 上，destroy 会导致未定义行为。
 *   2. 本函数只释放队列自身的资源，不会释放队列中残留的
 *      data 指针所指向的内存；这些数据的所有权归调用者。
 *
 * @note 建议先 pop 干净队列，或使用 queue_destroy_with() 传入
 *       free_fn 来处理残留数据。
 */
void queue_destroy(ConcurrentQueue* queue);

#endif /* CONCURRENT_QUEUE_H */