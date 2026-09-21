#include "../include/spsc_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <stdint.h>

/* ============ 测试 1：单线程基本功能 ============ */
static void test_basic(void) {
    printf("=== test_basic ===\n");
    SPSCQueue* q = spsc_create(4);
    assert(q != NULL);

    int a = 1, b = 2, c = 3;

    /* 空队列 pop 返回 NULL */
    assert(spsc_pop(q) == NULL);

    assert(spsc_push(q, &a));
    assert(spsc_push(q, &b));
    assert(spsc_push(q, &c));

    assert(spsc_pop(q) == &a);
    assert(spsc_pop(q) == &b);
    assert(spsc_pop(q) == &c);
    assert(spsc_pop(q) == NULL);   /* 又空了 */

    spsc_destroy(q);
    printf("test_basic passed\n\n");
}

/* ============ 测试 2：满时 push 返回 false ============ */
static void test_full(void) {
    printf("=== test_full ===\n");
    SPSCQueue* q = spsc_create(3);

    int v[4] = {10, 20, 30, 40};
    assert(spsc_push(q, &v[0]));
    assert(spsc_push(q, &v[1]));
    assert(spsc_push(q, &v[2]));

    /* 满了，第 4 个应该失败 */
    assert(!spsc_push(q, &v[3]));

    /* 消费一个腾出位置 */
    assert(spsc_pop(q) == &v[0]);
    assert(spsc_push(q, &v[3]));

    spsc_destroy(q);
    printf("test_full passed\n\n");
}

/* ============ 测试 3：环形回绕 ============ */
static void test_wraparound(void) {
    printf("=== test_wraparound ===\n");
    SPSCQueue* q = spsc_create(3);

    int vals[3];
    for (int round = 0; round < 1000; round++) {
        for (int i = 0; i < 3; i++) {
            vals[i] = round * 10 + i;
            assert(spsc_push(q, &vals[i]));
        }
        for (int i = 0; i < 3; i++) {
            void* out = spsc_pop(q);
            assert(*(int*)out == round * 10 + i);
        }
    }
    spsc_destroy(q);
    printf("test_wraparound passed\n\n");
}

/* ============ 测试 4：并发生产者/消费者 ============ */
#define TOTAL 1000000
#define CAP   1024

typedef struct {
    SPSCQueue* q;
    long       sum;
} ConsumerArg;

static void* producer_fn(void* arg) {
    SPSCQueue* q = (SPSCQueue*)arg;
    for (long i = 0; i < TOTAL; i++) {
        int* v = malloc(sizeof(int));
        *v = (int)i;
        /* 满了就自旋重试（非阻塞接口的典型用法） */
        while (!spsc_push(q, v)) {
            /* spin */
        }
    }
    return NULL;
}

static void* consumer_fn(void* arg) {
    ConsumerArg* ca = (ConsumerArg*)arg;
    SPSCQueue* q = ca->q;
    long received = 0;
    while (received < TOTAL) {
        void* out = spsc_pop(q);
        if (out) {
            ca->sum += *(int*)out;
            free(out);
            received++;
        }
        /* 空就继续 spin */
    }
    return NULL;
}

static void test_concurrent(void) {
    printf("=== test_concurrent ===\n");
    SPSCQueue* q = spsc_create(CAP);

    ConsumerArg ca = { .q = q, .sum = 0 };
    pthread_t prod, cons;

    pthread_create(&prod, NULL, producer_fn, q);
    pthread_create(&cons, NULL, consumer_fn, &ca);

    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    long expected = (long)TOTAL * (TOTAL - 1) / 2;
    printf("sum = %ld (expected %ld)\n", ca.sum, expected);
    assert(ca.sum == expected);

    spsc_destroy(q);
    printf("test_concurrent passed\n\n");
}

int main(void) {
    test_basic();
    test_full();
    test_wraparound();
    test_concurrent();
    printf("All tests passed.\n");
    return 0;
}