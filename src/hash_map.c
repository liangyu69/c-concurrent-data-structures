#include "../include/hash_map.h"
#include <stdlib.h>

/* 负载因子阈值：元素数 / 桶数 > 0.75 时扩容 */
#define LOAD_FACTOR_THRESHOLD 0.75

/* 链表节点：存一个键值对 */
typedef struct HashNode {
    void*            key;
    void*            value;
    struct HashNode* next;
} HashNode;

/* 哈希表 */
struct HashMap {
    HashNode** buckets;    // 桶数组，每个元素是链表头
    size_t     capacity;   // 桶数量
    size_t     size;       // 当前元素个数
    hash_fn_t  hash_fn;    // 哈希函数
    equal_fn_t equal_fn;   // 比较函数
};



static void hashmap_resize(HashMap* map,size_t new_capacity){
    HashNode**new_buckets=calloc(new_capacity,sizeof(*new_buckets));
    if(!new_buckets)return;

    for(size_t i=0;i<map->capacity;i++){
        HashNode*cur=map->buckets[i];

        while(cur){
            HashNode*next=cur->next;

            size_t new_index=map->hash_fn(cur->key)%new_capacity;

            cur->next=new_buckets[new_index];
            new_buckets[new_index]=cur;

            cur=next;
        }
    }

    free(map->buckets);
    map->buckets=new_buckets;
    map->capacity=new_capacity;
}

HashMap*hashmap_create(size_t capacity,
                        hash_fn_t hash_fn,
                        equal_fn_t equal_fn){
    if(capacity==0||!hash_fn||!equal_fn)return NULL;

    HashMap*map=malloc(sizeof(*map));
    if(!map)return NULL;

    map->buckets=calloc(capacity,sizeof(*map->buckets));
    if (!map->buckets) {
        free(map);
        return NULL;
    }

    map->capacity = capacity;
    map->size     = 0;
    map->hash_fn  = hash_fn;
    map->equal_fn = equal_fn;
    return map;
}

bool hashmap_put(HashMap* map,void*key,void*value){
    if(!map||!key)return false;

    size_t index=map->hash_fn(key)%map->capacity;

    HashNode*cur=map->buckets[index];
    while(cur){
        if(map->equal_fn(cur->key,key)){
            cur->value=value;
            return true;
        }
        cur=cur->next;
    }

    HashNode*node=malloc(sizeof(*node));
    if(!node)return false;
    node->key=key;
    node->value = value;
    node->next  = map->buckets[index];   // 头插
    map->buckets[index] = node;
    map->size++;

    if ((double)map->size / map->capacity > LOAD_FACTOR_THRESHOLD) {
        hashmap_resize(map, map->capacity * 2);
    }

    return true;
}



void* hashmap_get(HashMap* map, const void* key) {
    if (!map || !key) return NULL;

    size_t index = map->hash_fn(key) % map->capacity;

    HashNode* cur = map->buckets[index];
    while (cur) {
        if (map->equal_fn(cur->key, key)) {
            return cur->value;
        }
        cur = cur->next;
    }
    return NULL;
}

bool hashmap_contains(HashMap* map,const void* key){
    if(!map||!key)return false;

    size_t index=map->hash_fn(key)%map->capacity;

    HashNode*cur=map->buckets[index];
    while(cur){
        if(map->equal_fn(cur->key,key)){
            return true;
        }
        cur=cur->next;
    }
    return false;
}

bool hashmap_remove(HashMap* map, const void* key){
    if (!map || !key) return false;

    size_t index = map->hash_fn(key) % map->capacity;

    HashNode*cur=map->buckets[index];
    HashNode*prev=NULL;

    while(cur){
        if(map->equal_fn(cur->key,key)){
            if(prev){
                prev->next=cur->next;
            }else{
                map->buckets[index]=cur->next;
            }
            free(cur);
            map->size--;
            return true;
        }
        prev=cur;
        cur=cur->next;
    }
    return false;
}

size_t hashmap_size(HashMap* map) {
    if (!map) return 0;
    return map->size;
}


void hashmap_destroy(HashMap* map){
    if(!map)return;

    for(size_t i=0;i<map->capacity;i++){
        HashNode*cur=map->buckets[i];

        while(cur){
            HashNode*next=cur->next;
            free(cur);
            cur=next;
        }
    }

    free(map->buckets);
    free(map);
}


