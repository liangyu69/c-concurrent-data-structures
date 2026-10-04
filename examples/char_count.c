#include "../include/striped_hash_map.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <ctype.h>

#define NUM_CHARS 26
#define NTHREADS  4

/* int key 的 hash 和 equal */
static size_t int_hash(const void* key) {
    return (size_t)(unsigned int)(*(const int*)key);
}
static bool int_equal(const void* a, const void* b) {
    return *(const int*)a == *(const int*)b;
}

/* 预分配字符 key：0..25 代表 'a'..'z' */
static int g_chars[NUM_CHARS];

typedef struct {
    StripedHashMap* map;
    const char*     text;
    size_t          start;
    size_t          end;
    int             id;
} WorkerArg;

static void* worker(void* arg) {
    WorkerArg* wa = arg;
    printf("thread %d: start\n", wa->id);
    for (size_t i = wa->start; i < wa->end; i++) {
        unsigned char c = (unsigned char)wa->text[i];
        if (isalpha(c)) {
            int idx = tolower(c) - 'a';
            striped_hashmap_add(wa->map, &g_chars[idx], 1);
        }
    }
    printf("thread %d: done\n", wa->id);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <file>\n", argv[0]);
        return 1;
    }

    /* 1. 读文件到内存 */
    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* text = malloc(size + 1);
    if (!text) { fclose(f); return 1; }
    fread(text, 1, size, f);
    text[size] = '\0';
    fclose(f);

    printf("file size: %ld bytes\n", size);

    /* 2. 初始化字符 key */
    for (int i = 0; i < NUM_CHARS; i++) g_chars[i] = i;

    /* 3. 创建哈希表 */
    StripedHashMap* map = striped_hashmap_create(4, 16, int_hash, int_equal);
    assert(map);

    /* 4. 把文本切成 N 段，每线程一段 */
    pthread_t threads[NTHREADS];
    WorkerArg args[NTHREADS];

    for (int i = 0; i < NTHREADS; i++) {
        size_t start = (size_t)size * i / NTHREADS;
        size_t end   = (size_t)size * (i + 1) / NTHREADS;

        args[i] = (WorkerArg){
            .map   = map,
            .text  = text,
            .start = start,
            .end   = end,
            .id    = i
        };
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }
    for (int i = 0; i < NTHREADS; i++) pthread_join(threads[i], NULL);

    /* 5. 输出统计 */
    long total = 0;
    for (int i = 0; i < NUM_CHARS; i++) {
        void* v = striped_hashmap_get(map, &g_chars[i]);
        int count = v ? *(int*)v : 0;
        printf("'%c': %d\n", 'a' + i, count);
        total += count;
    }
    printf("total letters: %ld\n", total);

    /* 6. 释放 value */
    for (int i = 0; i < NUM_CHARS; i++) {
        void* v = striped_hashmap_get(map, &g_chars[i]);
        if (v) free(v);
    }

    striped_hashmap_destroy(map);
    free(text);

    printf("char_count passed.\n");
    return 0;
}