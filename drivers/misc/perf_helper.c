#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/cpumask.h>

#define BUFF_SIZE       64

struct task_struct *pKswapd = NULL;
struct proc_dir_entry *kswapd_affinity_entry = NULL;
static DEFINE_MUTEX(kswapd_lock);

static void kswapd_task_show()
{
	struct task_struct *p = NULL;
	read_lock(&tasklist_lock);
	for_each_process(p)
	{
		if (!p->mm && !strncmp(p->comm, "kswapd0", strlen("kswapd0"))) {
			pKswapd = p;
			break;
		}
	}
	read_unlock(&tasklist_lock);
}

static int kswapd_affinity_init(struct seq_file *seq, void *v)
{
	return 0;
}

static int kswapd_affinity_open(struct inode *inode, struct file *file)
{
	return single_open(file, kswapd_affinity_init, NULL);
}

static ssize_t kswapd_affinity_write(struct file *file, const char __user *userbuf,
			size_t count, loff_t *data)
{
	char buf[BUFF_SIZE] = {0};
	cpumask_t mask;
	u32 cpu_mask;
	u32 cmd;

	if (count > BUFF_SIZE)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EINVAL;

	if (sscanf(buf, "%d", &cpu_mask) != 1) {
		pr_err("input param error, can not prase param\n");
		return -EINVAL;
	}

	cmd = cpu_mask >> 8 & 0xff;

	pr_info("set kswapd affinity = %d\n", cmd);

	strncpy((char *)&mask, (char *)&cmd, sizeof(char));

	mutex_lock(&kswapd_lock);
	if (pKswapd)
	{
		if (set_cpus_allowed_ptr(pKswapd, &mask))
			printk(KERN_ERR "bind kswapd fail\n");
	}
	mutex_unlock(&kswapd_lock);

	return count;
}

static const struct proc_ops kswapd_affinity_ops = {
	.proc_open           = kswapd_affinity_open,
	.proc_write          = kswapd_affinity_write,
	.proc_read           = seq_read,
	.proc_lseek          = seq_lseek,
	.proc_release        = single_release,
};

/* 驱动入口函数 */
static int __init perf_helper_init(void)
{
	kswapd_task_show();
	kswapd_affinity_entry = proc_create("kswapd_affinity", 0664, NULL, &kswapd_affinity_ops);
	if (!kswapd_affinity_entry)
		printk(KERN_ERR "%s: create kswapd_affinity node failed\n");

	return 0;
}

/* 驱动出口函数 */
static void __exit perf_helper_exit(void)
{
	remove_proc_entry("kswapd_affinity", NULL);
}

MODULE_LICENSE("GPL");

module_init(perf_helper_init);
module_exit(perf_helper_exit);

