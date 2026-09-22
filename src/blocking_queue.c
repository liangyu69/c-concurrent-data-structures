#include"../include/queue.h"
#include<stdlib.h>
#include<stdio.h>
#include<pthread.h>

struct ConcurrentQueue{
    void**buffer;
    int capacity;
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
};


ConcurrentQueue*queue_create(int capacity){
    if(capacity<=0)return NULL;

    ConcurrentQueue*queue=malloc(sizeof(*queue));
    if(!queue)return NULL;

    queue->buffer=malloc(sizeof(void*)*capacity);
    if(!queue->buffer){
        free(queue);
        return NULL;
    }

    queue->capacity=capacity;
    queue->head=0;
    queue->tail=0;
    queue->count=0;

    if(pthread_mutex_init(&queue->mutex,NULL)!=0){
        free(queue->buffer);
        free(queue);
        return NULL;
    }

    if (pthread_cond_init(&queue->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&queue->mutex);
        free(queue->buffer);
        free(queue);
        return NULL;
    }
    if (pthread_cond_init(&queue->not_full, NULL) != 0) {
        pthread_cond_destroy(&queue->not_empty);
        pthread_mutex_destroy(&queue->mutex);
        free(queue->buffer);
        free(queue);
        return NULL;
    }

    return queue;
}

bool queue_push(ConcurrentQueue* queue, void* data){
    if (!queue) return false;

    pthread_mutex_lock(&queue->mutex);

    while(queue->count==queue->capacity){
        pthread_cond_wait(&queue->not_full,&queue->mutex);
    }

    queue->buffer[queue->tail]=data;
    queue->tail=(queue->tail+1)%(queue->capacity);
    queue->count++;

    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);

    return true;
}

void* queue_pop(ConcurrentQueue* queue){
    if (!queue) return NULL;
    pthread_mutex_lock(&queue->mutex);

    while (queue->count == 0) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }
    void*data=queue->buffer[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;

    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);

    return data;
}

void queue_destroy(ConcurrentQueue* queue){
    if (!queue) return;
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    free(queue->buffer);
    free(queue);
}
