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


static void stripe_resize(Stripe* s, hash_fn_t hash_fn, size_t new_capacity){
    HashNode** new_buckets = calloc (new_capacity,sizeof(*new_buckets));
    if(!new_buckets)return;

    for(size_t i=0;i<s->capacity;i++){
        HashNode*cur=s->buckets[i];
        while(cur){
            HashNode* next=cur->next;
            size_t new_index=hash_fn(cur->key)%new_capacity;
            cur->next=new_buckets[new_index];
            new_buckets[new_index]=cur;
            cur=next;
        }
    }

    free(s->buckets);
    s->buckets=new_buckets;
    s->capacity=new_capacity;
}



StripedHashMap* striped_hashmap_create(size_t num_stripes,
                                        size_t capacity_per_stripe,
                                        hash_fn_t hash_fn,
                                        equal_fn_t equal_fn){
    if(0==num_stripes || 0==capacity_per_stripe || !hash_fn || !equal_fn){
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
        s->buckets=calloc(capacity_per_stripe,sizeof(*s->buckets));
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


bool striped_hashmap_put(StripedHashMap* map, void* key, void* value){
    if(!map||!key)return false;

    size_t h = map->hash_fn(key);
    size_t stripe_idx=h % map->num_stripes;
    Stripe* s=&map->stripes[stripe_idx];

    pthread_mutex_lock(&s->mutex);

    size_t index=h % s->capacity;
    HashNode* cur = s->buckets[index];
    while (cur) {
        if (map->equal_fn(cur->key, key)) {
            cur->value = value;                       /* 更新 */
            pthread_mutex_unlock(&s->mutex);
            return true;
        }
        cur = cur->next;
    }

    HashNode* node=malloc(sizeof(*node));
    if(!node){
        pthread_mutex_unlock(&s->mutex);
        return false;
    }

    node->key=key;
    node->value=value;
    node->next=s->buckets[index];
    s->buckets[index]=node;
    s->size++;

    if ((double)s->size / s->capacity > LOAD_FACTOR_THRESHOLD) {
        stripe_resize(s, map->hash_fn, s->capacity * 2);
    }

    pthread_mutex_unlock(&s->mutex);
    return true;
}


void* striped_hashmap_get(StripedHashMap* map, const void* key){
    if(!map||!key)return NULL;

    size_t h = map->hash_fn(key);
    size_t stripe_idx = h % map->num_stripes;
    Stripe* s = &map->stripes[stripe_idx];

    pthread_mutex_lock(&s->mutex);
    size_t index=h%s->capacity;
    HashNode*cur =s->buckets[index];
    void*result=NULL;

    while(cur){
        if(map->equal_fn(cur->key,key)){
            result=cur->value;
            break;
        }
        cur=cur->next;
    }

    pthread_mutex_unlock(&s->mutex);
    return result;
}


bool striped_hashmap_contains(StripedHashMap* map, const void* key) {
    if (!map || !key) return false;

    size_t h = map->hash_fn(key);
    size_t stripe_idx = h % map->num_stripes;
    Stripe* s = &map->stripes[stripe_idx];

    pthread_mutex_lock(&s->mutex);

    size_t index = h % s->capacity;
    HashNode* cur = s->buckets[index];
    bool found = false;

    while (cur) {
        if (map->equal_fn(cur->key, key)) {
            found = true;
            break;
        }
        cur = cur->next;
    }

    pthread_mutex_unlock(&s->mutex);
    return found;
}


bool striped_hashmap_remove(StripedHashMap* map, const void* key) {
    if (!map || !key) return false;

    size_t h = map->hash_fn(key);
    size_t stripe_idx = h % map->num_stripes;
    Stripe* s = &map->stripes[stripe_idx];

    pthread_mutex_lock(&s->mutex);

    size_t index = h % s->capacity;
    HashNode* cur  = s->buckets[index];
    HashNode* pre=NULL;
    bool removed=false;

    while(cur){
        if(map->equal_fn(cur->key,key)){
            if(pre){
                pre->next=cur->next;
            }
            else{
                s->buckets[index]=cur->next;
            }
            free(cur);
            s->size--;
            removed=true;
            break;
        }
        pre=cur;
        cur=cur->next;
    }

    pthread_mutex_unlock(&s->mutex);
    return removed;
}


size_t striped_hashmap_size(StripedHashMap* map) {
    if (!map) return 0;

    size_t total = 0;
    for (size_t i = 0; i < map->num_stripes; i++) {
        Stripe* s = &map->stripes[i];
        pthread_mutex_lock(&s->mutex);
        total += s->size;
        pthread_mutex_unlock(&s->mutex);
    }
    return total;
}


void striped_hashmap_destroy(StripedHashMap* map) {
    if (!map) return;

    for (size_t i = 0; i < map->num_stripes; i++) {
        Stripe* s = &map->stripes[i];

        for (size_t j = 0; j < s->capacity; j++) {
            HashNode*cur = s->buckets[j];
            while (cur) {
                HashNode* next = cur->next;
                free(cur);
                cur = next;
            }
        }
        free(s->buckets);
        pthread_mutex_destroy(&s->mutex);
    }

    free(map->stripes);
    free(map);
}