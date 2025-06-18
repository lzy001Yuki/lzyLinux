#ifndef _LINUX_KV_STORE_H
#define _LINUX_KV_STORE_H

#include "sched.h"
#include <linux/types.h>
#include <linux/list.h>

struct kv_pair {
    int key;
    int value;
    struct hlist_node node;
};

static inline int kv_hash(int key) {
    return key % 1024;
}
struct task_struct;

extern int kv_store_init(struct task_struct *task);
extern void kv_store_exit(struct task_struct *task);

#endif /* _LINUX_KV_STORE_H */