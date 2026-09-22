// naive_queue.c
#include "../include/naive_queue.h"
#include <stdlib.h>

struct NaiveQueue {
    void**  buffer;
    size_t  capacity;
    size_t  head;   // 普通变量，非原子
    size_t  tail;   // 普通变量，非原子
};

NaiveQueue* naive_create(size_t capacity) {
    if (capacity == 0) return NULL;
    NaiveQueue* q = malloc(sizeof(*q));
    if (!q) return NULL;
    q->buffer = malloc(capacity * sizeof(void*));
    if (!q->buffer) { free(q); return NULL; }
    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
    return q;
}

bool naive_push(NaiveQueue* q, void* data) {
    if (!q) return false;
    if (q->tail - q->head == q->capacity) return false;  // 满
    q->buffer[q->tail % q->capacity] = data;             // 写数据
    q->tail = q->tail + 1;                               // 更新 tail
    return true;
}

void* naive_pop(NaiveQueue* q) {
    if (!q) return NULL;
    if (q->head == q->tail) return NULL;                 // 空
    void* data = q->buffer[q->head % q->capacity];       // 读数据
    q->head = q->head + 1;                               // 更新 head
    return data;
}

void naive_destroy(NaiveQueue* q) {
    if (!q) return;
    free(q->buffer);
    free(q);
}