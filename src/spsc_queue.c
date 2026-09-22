#include "../include/spsc_queue.h"
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>

/*
 * 缓存行大小。x86/ARM 通常 64 字节。
 * 用 padding 把 head 和 tail 隔到不同 cache line，
 * 避免 false sharing。
 */
#define CACHE_LINE 64

struct SPSCQueue {
    void**  buffer;
    size_t  capacity;

    /*
     * head: 消费者读位置（只被消费者写）
     * tail: 生产者写位置（只被生产者写）
     *
     * 用 alignas(CACHE_LINE) 让每个字段起始于新的 cache line。
     */
    alignas(CACHE_LINE) _Atomic size_t head;
    alignas(CACHE_LINE) _Atomic size_t tail;
};

SPSCQueue* spsc_create(size_t capacity) {
    if (capacity == 0) return NULL;

    SPSCQueue* q = malloc(sizeof(*q));
    if (!q) return NULL;

    q->buffer = malloc(capacity * sizeof(void*));
    if (!q->buffer) {
        free(q);
        return NULL;
    }

    q->capacity = capacity;
    atomic_init(&q->head, 0);
    atomic_init(&q->tail, 0);
    return q;
}

bool spsc_push(SPSCQueue* q, void* data) {
    if (!q) return false;

    /*
     * ① relaxed 读 tail：tail 是生产者自己写的，自己知道最新值，
     *    不需要 acquire 同步。
     */
    size_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);

    /*
     * ② acquire 读 head：要看到消费者最新的消费进度，
     *    否则可能误判队列已满。
     */
    size_t head = atomic_load_explicit(&q->head, memory_order_acquire);

    /* ③ 满判断：tail - head == capacity */
    if (tail - head == q->capacity) {
        return false;   /* 满，非阻塞返回 */
    }

    /* ④ 写数据到槽位 */
    q->buffer[tail % q->capacity] = data;

    /*
     * ⑤ release 写 tail：
     *    release 保证 ④ 的写不会被重排到 ⑤ 之后。
     *    即：数据先写入 buffer，tail 才更新。
     *    消费者看到新 tail 时，一定能看到 ④ 写的数据。
     */
    atomic_store_explicit(&q->tail, tail + 1, memory_order_release);
    return true;
}

void* spsc_pop(SPSCQueue* q) {
    if (!q) return NULL;

    /*
     * ① relaxed 读 head：消费者自己写的。
     */
    size_t head = atomic_load_explicit(&q->head, memory_order_relaxed);

    /*
     * ② acquire 读 tail：要看到生产者最新的发布，
     *    否则可能误判队列为空，或读到未完成的数据。
     */
    size_t tail = atomic_load_explicit(&q->tail, memory_order_acquire);

    /* ③ 空判断 */
    if (head == tail) {
        return NULL;    /* 空，非阻塞返回 */
    }

    /* ④ 读数据 */
    void* data = q->buffer[head % q->capacity];

    /*
     * ⑤ release 写 head：
     *    告知生产者"我消费了，腾出位置"。
     *    release 保证 ④ 的读完成之后，head 才更新。
     */
    atomic_store_explicit(&q->head, head + 1, memory_order_release);
    return data;
}

void spsc_destroy(SPSCQueue* q) {
    if (!q) return;
    free(q->buffer);
    free(q);
}
