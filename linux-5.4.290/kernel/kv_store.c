#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/sched.h>     
#include <linux/slab.h>      
#include <linux/uaccess.h>    
#include <linux/errno.h> 
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/sched/task.h>
#include <linux/sched/signal.h>


SYSCALL_DEFINE2(write_kv, int, k, int, v) {
    struct task_struct *cur;
    int idx;
    struct hlist_head *head;
    struct kv *tmp;
    unsigned long flags;
    struct kv* new_kv;
    cur = current;
    idx = k %1024;
    if (idx < 0) idx = -idx; // ?
    head =  &cur->kv_struct->kv_store[idx];
    spin_lock_irqsave(&cur->kv_struct->kv_locks[idx], flags);

    // check if k exist
    hlist_for_each_entry(tmp, head, node) {
        if (tmp->key == k) {
            tmp->value = v;
            spin_unlock_irqrestore(&cur->kv_struct->kv_locks[idx], flags);
            return sizeof(v);
        }
    }
    // if not

    new_kv = kmalloc(sizeof(struct kv), GFP_KERNEL);
    if (!new_kv) {
        spin_unlock_irqrestore(&cur->kv_struct->kv_locks[idx],flags);
        return -1;
    }
    new_kv->key = k;
    new_kv->value = v;
    INIT_HLIST_NODE(&new_kv->node);
    hlist_add_head(&new_kv->node, head);
    spin_unlock_irqrestore(&cur->kv_struct->kv_locks[idx], flags);
    return sizeof(v);
}

SYSCALL_DEFINE1(read_kv, int, k) {
    struct task_struct* cur;
    int idx;
    struct hlist_head *head;
    unsigned long flags;
    struct kv *tmp;
    cur = current;
    idx = k % 1024;
    if (idx < 0) idx = -idx;
    head = &cur->kv_struct->kv_store[idx];

    spin_lock_irqsave(&cur->kv_struct->kv_locks[idx], flags);

    hlist_for_each_entry(tmp, head, node) {
        if (tmp->key == k) {
            spin_unlock_irqrestore(&cur->kv_struct->kv_locks[idx], flags);
            return tmp->value;
        }
    }
    spin_unlock_irqrestore(&cur->kv_struct->kv_locks[idx], flags);
    return -1;
}

