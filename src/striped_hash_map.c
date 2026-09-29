#include "../include/striped_hash_map.h"
#include <stdlib.h>
#include <pthread.h>

#define LOAD_FACTOR_THRESHOLD 0.75

typedef struct HashNode{
    void* key;
    void* value;
    struct HashNode* next;
}HashNode;


/* 一个段 = 一个独立的小哈希表 + 一把锁 */
typedef struct {
    HashNode**       buckets;
    size_t           capacity;
    size_t           size;
    pthread_mutex_t  mutex;
} Stripe;


/* 分段哈希表 */
typedef struct StripedHashMap {
    Stripe*    stripes;        // 段数组
    size_t     num_stripes;    // 段数
    hash_fn_t  hash_fn;
    equal_fn_t equal_fn;
}StripedHashMap;


StripedHashMap* striped_hashmap_create(size_t num_stripes,
                                        size_t capacity_per_stripe,
                                        hash_fn_t hash_fn,
                                        equal_fn_t equal_fn){
    if(0==num_stripes || 0==capacity_per_stripe || !hash_fn ||! equal_fn){
        return NULL;
    }

    StripedHashMap* map=malloc(sizeof(*map));
    if(!map)return NULL;

    map->stripes=calloc(num_stripes,sizeof(*map->stripes));
    if(!map->stripes){
        free(map);
        return NULL;
    }

    map->num_stripes = num_stripes;
    map->hash_fn     = hash_fn;
    map->equal_fn    = equal_fn;

    for(size_t i=0;i<num_stripes;i++){
        Stripe* s=&map->stripes[i];
        s->buckets=calloc(capacity_per_stripe,sizeod(*s->buckets));
        if(!s->buckets){
            for(size_t j=0;j<i;j++){
                free(map->stripes[j].buckets);
                pthread_mutex_destroy(&map->stripes[j].mutex);
            }
            free(map->stripes);
            free(map);
            return NULL;
        }

        s->capacity = capacity_per_stripe;
        s->size     = 0;

        if (pthread_mutex_init(&s->mutex, NULL) != 0) {
            /* 回滚 */
            free(s->buckets);  //stripes[i]->buckets
            for (size_t j = 0; j < i; j++) {
                free(map->stripes[j].buckets);
                pthread_mutex_destroy(&map->stripes[j].mutex);
            }
            free(map->stripes);
            free(map);
            return NULL;
        }
    }

    return map;
}



