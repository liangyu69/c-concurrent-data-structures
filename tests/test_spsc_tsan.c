// tests/test_spsc_tsan.c
#include "../include/spsc_queue.h"
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <sched.h>

#define TOTAL 100000
#define CAP   64

static void* prod(void* arg) {
    SPSCQueue* q = arg;
    for (long i = 1; i <= TOTAL; i++) {          /* ← 从 1 开始，避免存 NULL */
        while (!spsc_push(q, (void*)(intptr_t)i)) { }
        sched_yield();
    }
    return NULL;
}

static void* cons(void* arg) {
    SPSCQueue* q = arg;
    long n = 0;
    while (n < TOTAL) {
        void* out = spsc_pop(q);
        if (out != NULL) n++;                    /* 现在 NULL 只表示"空" */
        sched_yield();
    }
    return NULL;
}

int main(void) {
    SPSCQueue* q = spsc_create(CAP);

    pthread_t a, b;
    pthread_create(&a, NULL, prod, q);
    pthread_create(&b, NULL, cons, q);
    pthread_join(a, NULL);
    pthread_join(b, NULL);

    spsc_destroy(q);
    printf("SPSC done\n");
    return 0;
}