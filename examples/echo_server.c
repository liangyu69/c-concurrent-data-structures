#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../include/queue.h"

#define NTHREADS 4      // 工作线程数量
#define SBUFSIZE 16     // 缓冲区大小
#define MAXLINE 1024

static ConcurrentQueue* g_queue; 

static int byte_cnt=0;
static sem_t mutex;

static void init_echo_cnt(void){
    sem_init(&mutex,0,1);
    byte_cnt=0;
}

void echo_cnt(int connfd){
    int n;
    char buf[MAXLINE];
    static pthread_once_t once=PTHREAD_ONCE_INIT;

    pthread_once(&once,init_echo_cnt);

    while ((n = read(connfd, buf, MAXLINE - 1)) > 0) {
        buf[n] = '\0';

        // 加锁保护共享计数器
        sem_wait(&mutex);
        byte_cnt += n;
        printf("server received %d (%d total) bytes on fd %d\n", n, byte_cnt, connfd);
        sem_post(&mutex);

        write(connfd, buf, n);      // 回显
    }
}

static void* thread(void*vargp){
    pthread_detach(pthread_self());
    while(1){
        void*data=queue_pop(g_queue);
        int connfd=(int)(intptr_t)data;
        echo_cnt(connfd);
        close(connfd);
    }
    return NULL;
}

int main(int argc,char*argv[]){

    int listenfd,connfd;
    socklen_t clientlen;
    struct sockaddr_in clientaddr,addr;
    pthread_t tid;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return 1;
    }

    listenfd=socket(AF_INET,SOCK_STREAM,0);
    if (listenfd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(atoi(argv[1]));

    if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    if (listen(listenfd, 5) < 0) {
        perror("listen"); return 1;
    }

    g_queue = queue_create(SBUFSIZE);
    if (!g_queue) {
        fprintf(stderr, "queue_create failed\n");
        return 1;
    }

    // 3. 创建 NTHREADS 个工作线程
    
    for (int i = 0; i < NTHREADS; i++) {
        pthread_create(&tid, NULL, thread, NULL);
    }

    while(1){
        clientlen = sizeof(clientaddr);
        connfd = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
        if (connfd < 0) { perror("accept"); continue; }

        queue_push(g_queue, (void*)(intptr_t)connfd);
    }

    close(listenfd);
    queue_destroy(g_queue);

    return 0;
}