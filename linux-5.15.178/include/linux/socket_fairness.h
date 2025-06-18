// File: include/linux/socket_fairness.h

#ifndef _LINUX_SOCKET_FAIRNESS_H
#define _LINUX_SOCKET_FAIRNESS_H

#include <linux/hashtable.h>
#include <linux/types.h>

// 存储每个线程的策略
struct thread_policy {
    pid_t tid;
    int max_sockets;
    int priority;
    struct hlist_node node; // 用于哈希表
};

extern int socket_fairness_check(void);
extern int __init socket_fairness_init(void);
extern void __exit socket_fairness_exit(void);

#endif /* _LINUX_SOCKET_FAIRNESS_H */