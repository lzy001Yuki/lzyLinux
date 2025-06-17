#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/pid.h>
SYSCALL_DEFINE3(configure_socket_fairness, pid_t, tid, 
                int, max_socket_allowed, int, priority_level)
{
    struct task_struct *task;
    int ret = -ESRCH;  // 默认错误：未找到任务

    if (max_socket_allowed < 0 || priority_level < 0)
        return -EINVAL;

    rcu_read_lock();
    task = find_task_by_vpid(tid);
    if (task) {
        get_task_struct(task);
        rcu_read_unlock();

        task->max_socket_allowed = max_socket_allowed;
        task->socket_priority_level = priority_level;
        
        put_task_struct(task);
        ret = 0;
    } else {
        rcu_read_unlock();
    }

    return ret;
}
