#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <errno.h>
#include "socket_fairness.h"


#ifndef SYS_configure_socket_fairness
#define SYS_configure_socket_fairness 451  // 与内核中定义一致
/**
 * 为特定线程配置Socket级别的公平管理策略
 */
int configure_socket_fairness(pthread_t thread_id,
                             int max_socket_allowed,
                             int priority_level) {
    pid_t tid;
    
    /* 如果是当前线程，直接获取TID */
    if (pthread_equal(thread_id, pthread_self())) {
        tid = syscall(SYS_gettid);
    } else {
        /* 这个函数不是标准的，需要针对不同系统实现 */
        /* 简化版：只支持配置当前线程 */
        return -EINVAL;  // 不支持配置其他线程
    }
    
    /* 调用系统调用 */
    long ret = syscall(SYS_configure_socket_fairness, tid, max_socket_allowed, priority_level);
    
    if (ret < 0)
        return -errno;
    
    return 0;
}