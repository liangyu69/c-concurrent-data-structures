// tests/test_naive_tsan.c
#include "../include/naive_queue.h"
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <sched.h>

#define TOTAL 100000
#define CAP   64

static void* prod(void* arg) {
    NaiveQueue* q = arg;
    for (long i = 0; i < TOTAL; i++) {
        naive_push(q, (void*)(intptr_t)i);   // 直接存整数，不 malloc
        sched_yield();
    }
    return NULL;
}

static void* cons(void* arg) {
    NaiveQueue* q = arg;
    long n = 0, attempts = 0;
    while (n < TOTAL && attempts < TOTAL * 10) {
        void* out = naive_pop(q);
        if (out != NULL) n++;                 // 不 free
        attempts++;
        sched_yield();
    }
    return NULL;
}

int main(void) {
    NaiveQueue* q = naive_create(CAP);
    pthread_t a, b;
    pthread_create(&a, NULL, prod, q);
    pthread_create(&b, NULL, cons, q);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    naive_destroy(q);
    printf("Naive done\n");
    return 0;
}