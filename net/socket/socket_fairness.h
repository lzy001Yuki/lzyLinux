#ifndef SOCKET_FAIRNESS_H
#define SOCKET_FAIRNESS_H

#include <pthread.h>

/**
 * 为特定线程配置Socket级别的公平管理策略
 * 
 * @param thread_id 需要配置的线程ID
 * @param max_socket_allowed 该线程允许打开的最大Socket数
 * @param priority_level 该线程的优先级（影响Socket分配策略）
 * @return 成功时返回0，失败返回非0错误码
 */
int configure_socket_fairness(pthread_t thread_id,
                             int max_socket_allowed,
                             int priority_level);

#endif /* SOCKET_FAIRNESS_H */