#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include "socket_fairness.h"

#define MAX_SOCKETS 20

void *thread_func(void *arg) {
    int limit = *((int*)arg);
    int sockets[MAX_SOCKETS];
    int i, count = 0;
    
    printf("Thread %ld: Setting socket limit to %d\n", 
           (long)pthread_self(), limit);
    
    // 配置当前线程的socket限制
    if (configure_socket_fairness(pthread_self(), limit, 0) != 0) {
        printf("Failed to configure socket fairness\n");
        return NULL;
    }
    
    // 尝试创建超过限制的socket
    printf("Thread %ld: Attempting to create sockets...\n", 
           (long)pthread_self());
    
    for (i = 0; i < MAX_SOCKETS; i++) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            printf("Thread %ld: Socket creation failed at #%d: %s\n", 
                   (long)pthread_self(), i, strerror(errno));
            break;
        }
        
        sockets[count++] = sock;
        printf("Thread %ld: Created socket #%d (fd=%d)\n", 
               (long)pthread_self(), i, sock);
    }
    
    printf("Thread %ld: Created %d sockets out of %d attempts\n", 
           (long)pthread_self(), count, MAX_SOCKETS);
    
    // 等待一会儿
    sleep(2);
    
    // 关闭所有socket
    for (i = 0; i < count; i++) {
        close(sockets[i]);
    }
    
    printf("Thread %ld: Closed all sockets\n", (long)pthread_self());
    return NULL;
}

int main() {
    pthread_t thread1, thread2;
    int limit1 = 5;   // 线程1的限制
    int limit2 = 10;  // 线程2的限制
    
    // 创建两个线程，设置不同的限制
    pthread_create(&thread1, NULL, thread_func, &limit1);
    pthread_create(&thread2, NULL, thread_func, &limit2);
    
    // 等待线程完成
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    
    printf("Test completed successfully\n");
    return 0;
}