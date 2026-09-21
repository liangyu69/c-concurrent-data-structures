#ifndef NAIVE_QUEUE_H
#define NAIVE_QUEUE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct NaiveQueue NaiveQueue;

NaiveQueue* naive_create(size_t capacity);
bool        naive_push(NaiveQueue* q, void* data);
void*       naive_pop(NaiveQueue* q);
void        naive_destroy(NaiveQueue* q);

#endif