#define pr_fmt(fmt) "MI_LOG: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/rwsem.h>
#include <linux/delay.h>
#include <linux/kernel_stat.h>
#include <linux/sched/stat.h>
#include <linux/proc_fs.h>
#include <linux/delayacct.h>
#include "mi_log.h"

struct proc_dir_entry *mi_log_proc;
struct proc_dir_entry *delay_entry;

struct delay_struct {
	u64 version;
	u64 blkio_delay;      /* wait for sync block io completion */
	u64 swapin_delay;     /* wait for swapin block io completion */
	u64 freepages_delay;  /* wait for memory reclaim */
	u64 cpu_runtime;
	u64 cpu_run_delay;
};

static ssize_t delay_read(struct file *file, char __user *buf,
                        size_t count, loff_t *ppos)
{
	struct task_struct *task = current;
	unsigned long flags;
	struct delay_struct d = {};
	u64 blkio_delay, swapin_delay, freepages_delay;
	loff_t dummy_pos = 0;
	if (!task)
		return -ESRCH;

	get_task_struct(task);
	raw_spin_lock_irqsave(&task->delays->lock, flags);
	blkio_delay = task->delays->blkio_delay;
	swapin_delay = task->delays->swapin_delay;
	freepages_delay = task->delays->freepages_delay;
	raw_spin_unlock_irqrestore(&task->delays->lock, flags);

	d.version = 1;
	d.blkio_delay = blkio_delay;
	d.swapin_delay = swapin_delay;
	d.freepages_delay = freepages_delay;
	if (likely(sched_info_on())) {
		d.cpu_runtime = task->se.sum_exec_runtime;
		d.cpu_run_delay = task->sched_info.run_delay;
	}

	put_task_struct(task);

	return simple_read_from_buffer(buf, count, &dummy_pos, &d, sizeof(struct delay_struct));
}

static const struct proc_ops proc_delay_file_operations = {
	.proc_read = delay_read,
};

static int __init mi_log_init(void)
{
	mi_log_proc = proc_mkdir("mi_log", NULL);
	if (!mi_log_proc) {
		pr_err("failed to create mi log node.\n");
		goto failed_to_create_sysfs;
	}

	delay_entry = proc_create("delay", S_IRUGO,
			mi_log_proc, &proc_delay_file_operations);

	pr_info("mi_log init ok\n");
	return RET_OK;

failed_to_create_sysfs:
	return RET_FAIL;
}

static void __exit mi_log_exit(void)
{
	if (mi_log_proc) {
		if (delay_entry)
			proc_remove(delay_entry);

		proc_remove(mi_log_proc);
	}
}

module_init(mi_log_init);
module_exit(mi_log_exit);

MODULE_AUTHOR("wangzhaoliang<wangzhaoliang@xaiomi.com>");
MODULE_DESCRIPTION("mi log");
MODULE_LICENSE("GPL");