#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/rtc.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/printk.h>
#include <linux/swap.h>
#include <linux/gfp.h>

#define MAX_RECORD_NUM		2048
#define MAX_ONE_RECORD_SIZE	128

#ifdef CONFIG_BOOTUP_RECLAIM
#define BUFF_SIZE           64
static DEFINE_SPINLOCK(mr_lock);
static DEFINE_SPINLOCK(ml_lock);
static char reclaim_buff[BUFF_SIZE];
#endif

static DEFINE_SPINLOCK(plr_lock);
struct perflock_records_buff {
	char msg[MAX_ONE_RECORD_SIZE];
	struct timespec key_time;
};

struct perflock_records_buff plr_buff[MAX_RECORD_NUM];
static u32 plr_num;
static u32 index_head;
static u32 index_tail;

static void perflock_record(const char *perflock_msg)
{
	static int m;

	if (!perflock_msg)
		return;

	if (!spin_trylock(&plr_lock))
		return;

	index_tail = m;
	getnstimeofday(&plr_buff[m].key_time);
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
			rtc_time_to_tm(plr_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					plr_buff[i].msg);
		}
	} else {
		for (i = index_head; i < MAX_RECORD_NUM; i++) {
			rtc_time_to_tm(plr_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					plr_buff[i].msg);
		}

		if (index_head > index_tail) {
			for (i = 0; i <= index_tail; i++) {
				rtc_time_to_tm(plr_buff[i].key_time.tv_sec, &tm);
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

#ifdef CONFIG_BOOTUP_RECLAIM
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
#endif

static const struct file_operations perflock_records_ops = {
	.open           = perflock_records_open,
	.read           = seq_read,
	.write		= perflock_records_write,
	.llseek         = seq_lseek,
	.release        = single_release,
};

#ifdef CONFIG_BOOTUP_RECLAIM
static const struct file_operations global_reclaim_ops = {
	.open           = global_reclaim_open,
	.read           = seq_read,
	.write          = global_reclaim_write,
	.llseek         = seq_lseek,
	.release        = single_release,
};
#endif

static int __init perf_helper_init(void)
{
	struct proc_dir_entry *entry;
#ifdef CONFIG_BOOTUP_RECLAIM
        struct proc_dir_entry *global_reclaim_entry;
#endif

	entry = proc_create("perflock_records", 0664, NULL, &perflock_records_ops);
	if (!entry)
		printk(KERN_ERR "%s: create perflock_records node failed\n");

#ifdef CONFIG_BOOTUP_RECLAIM
        global_reclaim_entry = proc_create("global_reclaim", 0664, NULL, &global_reclaim_ops);
        if (!global_reclaim_entry)
                printk(KERN_ERR "%s: create global_reclaim node failed\n");
#endif

	return 0;
}

module_init(perf_helper_init);
