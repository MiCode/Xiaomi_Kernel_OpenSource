#include <linux/module.h>
#include <trace/hooks/sched.h>

static const char *camera_name = ".android.camera";

static void extend_mmput_handler(void *data, struct mm_struct *mm)
{
    if ((current->group_leader->flags & PF_EXITING) && task_nice(current) > 0) {
        if (strncmp(current->group_leader->comm, camera_name, strlen(camera_name)) == 0) {
            set_user_nice(current, -20);
        } else {
            set_user_nice(current, 0);
        }
    }
}

int __init process_exit_init(void)
{
    register_trace_android_vh_mmput(extend_mmput_handler, NULL);
    pr_info("process_exit: module init!");
    return 0;
}

void __exit process_exit_exit(void)
{
    unregister_trace_android_vh_mmput(extend_mmput_handler, NULL);
    pr_info("process_exit: module exit!");
}

module_init(process_exit_init);
module_exit(process_exit_exit);
MODULE_LICENSE("GPL");
