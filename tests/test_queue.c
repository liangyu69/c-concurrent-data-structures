#include "../include/queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>

/* ============ 测试 1：基本 FIFO ============ */
static void test_basic(void) {
    printf("=== test_basic ===\n");
    ConcurrentQueue* q = queue_create(4);
    assert(q != NULL);

    int a = 1, b = 2, c = 3;
    queue_push(q, &a);
    queue_push(q, &b);
    queue_push(q, &c);

    assert(queue_pop(q) == &a);
    assert(queue_pop(q) == &b);
    assert(queue_pop(q) == &c);

    queue_destroy(q);
    printf("test_basic passed\n\n");
}

/* ============ 测试 2：满队列时 push 阻塞，pop 后唤醒 ============ */
typedef struct {
    ConcurrentQueue* q;
    void*             value;
    int              done;
} PushArg;

static void* blocking_push_fn(void* arg) {
    PushArg* pa = (PushArg*)arg;
    queue_push(pa->q, pa->value);   /* 队列满时会阻塞 */
    pa->done = 1;
    return NULL;
}

static void test_full_blocks_push(void) {
    printf("=== test_full_blocks_push ===\n");
    ConcurrentQueue* q = queue_create(2);
    assert(q != NULL);

    int x = 10, y = 20, z = 30;
    queue_push(q, &x);
    queue_push(q, &y);          /* 满了 */

    PushArg pa = { .q = q, .value = &z, .done = 0 };   /* 存 &z */
    pthread_t t;
    pthread_create(&t, NULL, blocking_push_fn, &pa);

    usleep(200 * 1000);
    assert(pa.done == 0);

    assert(queue_pop(q) == &x);
    pthread_join(t, NULL);
    assert(pa.done == 1);

    assert(queue_pop(q) == &y);
    assert(queue_pop(q) == &z);   /* 现在对了 */

    queue_destroy(q);
    printf("test_full_blocks_push passed\n\n");
}

/* ============ 测试 3：空队列时 pop 阻塞，push 后唤醒 ============ */
typedef struct {
    ConcurrentQueue* q;
    void*            out;
} PopArg;

static void* blocking_pop_fn(void* arg) {
    PopArg* pa = (PopArg*)arg;
    pa->out = queue_pop(pa->q);      /* 队列空时会阻塞 */
    return NULL;
}

static void test_empty_blocks_pop(void) {
    printf("=== test_empty_blocks_pop ===\n");
    ConcurrentQueue* q = queue_create(2);
    assert(q != NULL);

    PopArg pa = { .q = q, .out = NULL };
    pthread_t t;
    pthread_create(&t, NULL, blocking_pop_fn, &pa);

    usleep(200 * 1000);
    assert(pa.out == NULL);          /* 应该还阻塞着 */

    int v = 99;
    queue_push(q, &v);
    pthread_join(t, NULL);
    assert(pa.out == &v);            /* 被唤醒了 */

    queue_destroy(q);
    printf("test_empty_blocks_pop passed\n\n");
}

/* ============ 测试 4：并发生产者/消费者 ============ */
#define NUM_PRODUCERS 4
#define NUM_CONSUMERS 4
#define ITEMS_PER_PRODUCER 100000
#define TOTAL_ITEMS (NUM_PRODUCERS * ITEMS_PER_PRODUCER)
#define QUEUE_CAPACITY 16               /* 故意小于总量，测阻塞/唤醒 */

typedef struct {
    ConcurrentQueue* q;
    int              id;
    long             sum;
    long             count;
} WorkerArg;

static void* producer_fn(void* arg) {
    WorkerArg* wa = (WorkerArg*)arg;
    for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
        int* v = malloc(sizeof(int));
        *v = wa->id * ITEMS_PER_PRODUCER + i;
        queue_push(wa->q, v);
    }
    return NULL;
}

static void* consumer_fn(void* arg) {
    WorkerArg* wa = (WorkerArg*)arg;
    long target = TOTAL_ITEMS / NUM_CONSUMERS;
    for (long i = 0; i < target; i++) {
        void* out = queue_pop(wa->q);
        wa->sum += *(int*)out;
        wa->count++;
        free(out);
    }
    return NULL;
}

static void test_concurrent(void) {
    printf("=== test_concurrent ===\n");
    ConcurrentQueue* q = queue_create(QUEUE_CAPACITY);
    assert(q != NULL);

    pthread_t prod[NUM_PRODUCERS], cons[NUM_CONSUMERS];
    WorkerArg parg[NUM_PRODUCERS], carg[NUM_CONSUMERS];

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        parg[i] = (WorkerArg){ .q = q, .id = i };
        pthread_create(&prod[i], NULL, producer_fn, &parg[i]);
    }
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        carg[i] = (WorkerArg){ .q = q, .id = i };
        pthread_create(&cons[i], NULL, consumer_fn, &carg[i]);
    }

    for (int i = 0; i < NUM_PRODUCERS; i++) pthread_join(prod[i], NULL);
    for (int i = 0; i < NUM_CONSUMERS; i++) pthread_join(cons[i], NULL);

    long total_count = 0, total_sum = 0;
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        total_count += carg[i].count;
        total_sum   += carg[i].sum;
    }
    long expected_sum = (long)TOTAL_ITEMS * (TOTAL_ITEMS - 1) / 2;

    printf("total_count = %ld (expected %d)\n", total_count, TOTAL_ITEMS);
    printf("total_sum   = %ld (expected %ld)\n", total_sum, expected_sum);
    assert(total_count == TOTAL_ITEMS);
    assert(total_sum == expected_sum);

    queue_destroy(q);
    printf("test_concurrent passed\n\n");
}

/* ============ 测试 5：环形回绕 ============ */
static void test_wraparound(void) {
    printf("=== test_wraparound ===\n");
    ConcurrentQueue* q = queue_create(3);

    int vals[10];
    /* 反复 push/pop，让 head/tail 绕过好几圈 */
    for (int round = 0; round < 100; round++) {
        for (int i = 0; i < 3; i++) {
            vals[i] = round * 10 + i;
            queue_push(q, &vals[i]);
        }
        for (int i = 0; i < 3; i++) {
            void* out = queue_pop(q);
            assert(*(int*)out == round * 10 + i);
        }
    }
    queue_destroy(q);
    printf("test_wraparound passed\n\n");
}

/* ============ main ============ */
int main(void) {
    test_basic();
    test_full_blocks_push();
    test_empty_blocks_pop();
    test_wraparound();
    test_concurrent();
    printf("All tests passed.\n");
    return 0;
}