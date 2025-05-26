#ifndef FECB8ECE_7A02_4E0C_9D62_35D1FCEA79E9
#define FECB8ECE_7A02_4E0C_9D62_35D1FCEA79E9

#include <linux/types.h>

struct task_info {
    pid_t pid;                   /* 进程 ID */
    void *task_struct_ptr;       /* task_struct 指针 */
};

int get_task_struct_info(struct task_info *info);

#endif /* FECB8ECE_7A02_4E0C_9D62_35D1FCEA79E9 */
