#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/rtc.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/printk.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>
#include <linux/swap.h>
#include <linux/gfp.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>

#define MAX_RECORD_NUM		2048
#define MAX_ONE_RECORD_SIZE	128
#define BUFF_SIZE           64

static DEFINE_SPINLOCK(plr_lock);
static DEFINE_SPINLOCK(mr_lock);

static char reclaim_buff[BUFF_SIZE];

struct perflock_records_buff {
	char msg[MAX_ONE_RECORD_SIZE];
	struct timespec64 key_time;
};

struct perflock_records_buff plr_buff[MAX_RECORD_NUM];
static u32 plr_num;
static u32 index_head;
static u32 index_tail;

static DEFINE_SPINLOCK(ml_lock);
struct mimdlog_buff {
	char msg[MAX_ONE_RECORD_SIZE];
	struct timespec64 key_time;
};

struct mimdlog_buff ml_buff[MAX_RECORD_NUM];
static u32 ml_num;
static u32 ml_index_head;
static u32 ml_index_tail;

static void perflock_record(const char *perflock_msg)
{
	static int m;

	if (!perflock_msg)
		return;

	if (!spin_trylock(&plr_lock))
		return;

	index_tail = m;
	ktime_get_real_ts64(&plr_buff[m].key_time);
	snprintf(plr_buff[m++].msg, MAX_ONE_RECORD_SIZE, "%s", perflock_msg);

	if (m >= MAX_RECORD_NUM)
		m = 0;

	plr_num++;
	if (plr_num >= MAX_RECORD_NUM) {
		plr_num = MAX_RECORD_NUM;
		index_head = index_tail + 1;
		if (index_head >= MAX_RECORD_NUM)
			index_head = 0;
	}

	spin_unlock(&plr_lock);
}

static int perflock_records_show(struct seq_file *seq, void *v)
{
	struct rtc_time tm;
	int i = 0;

	spin_lock(&plr_lock);
	if (plr_num < MAX_RECORD_NUM) {
		for (i = 0; i < plr_num; i++) {
			rtc_time64_to_tm(plr_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					plr_buff[i].msg);
		}
	} else {
		for (i = index_head; i < MAX_RECORD_NUM; i++) {
			rtc_time64_to_tm(plr_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					plr_buff[i].msg);
		}

		if (index_head > index_tail) {
			for (i = 0; i <= index_tail; i++) {
				rtc_time64_to_tm(plr_buff[i].key_time.tv_sec, &tm);
				seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s }\n",
						tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
						plr_buff[i].msg);
			}
		}
	}
	spin_unlock(&plr_lock);

	return 0;
}

static int perflock_records_open(struct inode *inode, struct file *file)
{
	return single_open(file, perflock_records_show, NULL);
}

static ssize_t perflock_records_write(struct file *file, const char __user *userbuf,
		size_t count, loff_t *data)
{
	char buf[MAX_ONE_RECORD_SIZE] = {0};

	if (count > MAX_ONE_RECORD_SIZE)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	perflock_record(buf);

	return count;
}

static const struct proc_ops perflock_records_ops = {
	.proc_open           = perflock_records_open,
	.proc_read           = seq_read,
	.proc_write		= perflock_records_write,
	.proc_lseek         = seq_lseek,
	.proc_release        = single_release,
};

static void mimd_record(const char *mimdlog_msg)
{
	static int m;

	if (!mimdlog_msg)
		return;

	if (!spin_trylock(&ml_lock))
		return;

	ml_index_tail = m;
	ktime_get_real_ts64(&ml_buff[m].key_time);
	snprintf(ml_buff[m++].msg, MAX_ONE_RECORD_SIZE, "%s", mimdlog_msg);

	if (m >= MAX_RECORD_NUM)
		m = 0;

	ml_num++;
	if (ml_num >= MAX_RECORD_NUM) {
		ml_num = MAX_RECORD_NUM;
		ml_index_head = ml_index_tail + 1;
		if (ml_index_head >= MAX_RECORD_NUM)
			ml_index_head = 0;
	}

	spin_unlock(&ml_lock);
}

static int mimd_show(struct seq_file *seq, void *v)
{
	struct rtc_time tm;
	int i = 0;

	spin_lock(&ml_lock);
	if (ml_num < MAX_RECORD_NUM) {
		for (i = 0; i < ml_num; i++) {
			rtc_time64_to_tm(ml_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					ml_buff[i].msg);
		}
	} else {
		for (i = index_head; i < MAX_RECORD_NUM; i++) {
			rtc_time64_to_tm(ml_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					ml_buff[i].msg);
		}

		if (index_head > index_tail) {
			for (i = 0; i <= index_tail; i++) {
				rtc_time64_to_tm(ml_buff[i].key_time.tv_sec, &tm);
				seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s }\n",
						tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
						ml_buff[i].msg);
			}
		}
	}
	spin_unlock(&ml_lock);

	return 0;
}

static int mimdlog_open(struct inode *inode, struct file *file)
{
	return single_open(file, mimd_show, NULL);
}

static ssize_t mimdlog_write(struct file *file, const char __user *userbuf,
		size_t count, loff_t *data)
{
	char buf[MAX_ONE_RECORD_SIZE] = {0};

	if (count > MAX_ONE_RECORD_SIZE)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	mimd_record(buf);

	return count;
}

static int global_reclaim_show(struct seq_file *seq, void *v)
{
	spin_lock(&ml_lock);

	seq_printf(seq, "%s", reclaim_buff);

	spin_unlock(&ml_lock);

	return 0;
}

static int global_reclaim_open(struct inode *inode, struct file *file)
{
	return single_open(file, global_reclaim_show, NULL);
}

static void global_reclaim_record(unsigned long nr_reclaim)
{
	if (!spin_trylock(&mr_lock))
		return;

	snprintf(reclaim_buff, BUFF_SIZE, "reclaim %lu pages", nr_reclaim);

	spin_unlock(&mr_lock);
}

static ssize_t global_reclaim_write(struct file *file, const char __user *userbuf,
		size_t count, loff_t *data)
{
	char buf[BUFF_SIZE] = {0};
	unsigned long reclaim_size = 0;
	unsigned long nr_reclaim = 0;
	int err = 0;

	if (count > BUFF_SIZE)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	err = kstrtoul(buf, 10, &reclaim_size);
	if (err != 0)
		return err;

	if(reclaim_size <= 0)
		return 0;

	nr_reclaim = try_to_free_mem_cgroup_pages(NULL, reclaim_size, GFP_KERNEL, true);
	global_reclaim_record(nr_reclaim);

	return count;
}

static const struct proc_ops mimdlog_ops = {
	.proc_open           = mimdlog_open,
	.proc_read           = seq_read,
	.proc_write          = mimdlog_write,
	.proc_lseek          = seq_lseek,
	.proc_release        = single_release,
};

static const struct proc_ops global_reclaim_ops = {
	.proc_open           = global_reclaim_open,
	.proc_read           = seq_read,
	.proc_write          = global_reclaim_write,
	.proc_lseek          = seq_lseek,
	.proc_release        = single_release,
};

static int kswapd_pid_show(struct seq_file *seq, void *v)
{
	struct task_struct *p = NULL;
	pid_t pid = 0;

	read_lock(&tasklist_lock);
	for_each_process(p) {
		if (!p->mm && !strncmp(p->comm, "kswapd0", strlen("kswapd0"))) {
			pid = p->pid;
			break;
		}
	}
	read_unlock(&tasklist_lock);

	seq_printf(seq, "%d", pid);

	return 0;
}

static int kswapd_pid_open(struct inode *inode, struct file *file)
{
	return single_open(file, kswapd_pid_show, NULL);
}

static const struct proc_ops kswapd_pid_ops = {
	.proc_open           = kswapd_pid_open,
	.proc_read           = seq_read,
	.proc_lseek          = seq_lseek,
	.proc_release        = single_release,
};

static int __init perf_helper_init(void)
{
	struct proc_dir_entry *entry;
	struct proc_dir_entry *mimd_entry;
	struct proc_dir_entry *global_reclaim_entry;
	struct proc_dir_entry *kswapd_pid_entry;

	entry = proc_create("perflock_records", 0664, NULL, &perflock_records_ops);
	if (!entry)
		printk(KERN_ERR "%s: create perflock_records node failed\n");

	mimd_entry = proc_create("mimdlog", 0664, NULL, &mimdlog_ops);
	if (!mimd_entry)
		printk(KERN_ERR "%s: create mimdlog node failed\n");

	global_reclaim_entry = proc_create("global_reclaim", 0664, NULL, &global_reclaim_ops);
	if (!global_reclaim_entry)
		printk(KERN_ERR "%s: create global_reclaim node failed\n");

	kswapd_pid_entry = proc_create("kswapd_pid", 0664, NULL, &kswapd_pid_ops);
	if (!kswapd_pid_entry)
		printk(KERN_ERR "%s: create kswapd_pid node failed\n");

	return 0;
}

static void __exit perf_helper_exit(void)
{
	remove_proc_entry("perflock_records", NULL);
	remove_proc_entry("mimdlog", NULL);
	remove_proc_entry("global_reclaim", NULL);
	remove_proc_entry("kswapd_pid", NULL);
}

MODULE_LICENSE("GPL");

module_init(perf_helper_init);
module_exit(perf_helper_exit);
