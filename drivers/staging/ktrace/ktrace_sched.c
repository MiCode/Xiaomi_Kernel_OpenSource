#define pr_fmt(fmt)  "ktrace : " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/preempt.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/ktrace.h>

#define MONITOR_PID_NR 5
#define INVALID_PID -1

#define SCHED_ITEMS_READ_MAX DEFAULT_ITEMS_PER_READ

#define BLOCK_REASON_NR 30

#define USE_THREAD_TO_UPDATE_PIDS 0

struct sched_entry {
	s64 time_stamp_ns;
	unsigned char type;
	pid_t pid;
	unsigned int time_us;
	void *pc;
};

struct sched_stats {
	atomic_t count;
	u64 time;
};

struct sched_block_reason {
	void *pc;
	int count;
	u64 time;
};

struct ktrace_sched {
	struct ktrace_queue *q;
	struct dentry *dir;

	void *buf;
	int entry_size;
	int max;

	int enable_debug;

	struct task_struct *thread;

	unsigned int filter[KTRACE_SCHED_TYPE_NR];
	struct sched_stats stats[KTRACE_SCHED_TYPE_NR];
	struct sched_block_reason block_reason[BLOCK_REASON_NR];

	pid_t pids[MONITOR_PID_NR];
	int pid_nr;

	pid_t new_pids[MONITOR_PID_NR];
	int new_pid_nr;
	bool match_all_pid;
};

static struct ktrace_sched __ks = {
	.filter = {
		[KTRACE_SCHED_TYPE_SAMPLE1] = MS_TO_US(1),
		[KTRACE_SCHED_TYPE_SAMPLE2] = MS_TO_US(1),
		[KTRACE_SCHED_TYPE_SAMPLE3] = MS_TO_US(1),
		[KTRACE_SCHED_TYPE_SAMPLE4] = MS_TO_US(1),
		[KTRACE_SCHED_TYPE_SAMPLE5] = MS_TO_US(1),
		[KTRACE_SCHED_TYPE_SAMPLE6] = MS_TO_US(1),
		[KTRACE_SCHED_TYPE_SAMPLE7] = MS_TO_US(1),
		[KTRACE_SCHED_TYPE_SAMPLE8] = MS_TO_US(1),
		[KTRACE_SCHED_TYPE_SAMPLE9] = MS_TO_US(1),
		[KTRACE_SCHED_TYPE_SAMPLE10] = MS_TO_US(1),

		[KTRACE_SCHED_TYPE_PREEMPT] = MS_TO_US(2),
		[KTRACE_SCHED_TYPE_WAIT] = MS_TO_US(2),
		[KTRACE_SCHED_TYPE_BLOCK] = MS_TO_US(2),
	}

};

static const char const *sched_event_name[KTRACE_SCHED_TYPE_NR] = {
	"sam_1",
	"sam_2",
	"sam_3",
	"sam_4",
	"sam_5",
	"sam_6",
	"sam_7",
	"sam_8",
	"sam_9",
	"sam_10",

	"p",
	"w",
	"b",
};

#define ktrace_sched_debug(enable, x...) \
	do { \
		if (enable) \
			ktrace_print(x); \
	} while (0)

static int sched_show_entry(struct seq_file *m, void *entry, bool debug)
{
	struct sched_entry *e = (struct sched_entry *)entry;
	int ret;

	if (e->pc) {
		if (debug) {
			ret = seq_printf(m, "%lld %5d <%s> %d.%03d ms %pS\n",
					e->time_stamp_ns, e->pid,
					sched_event_name[e->type],
					e->time_us / 1000, e->time_us % 1000,
					e->pc);
		} else {
			ret = seq_printf(m, "%lld %5d <%s> %d.%03d ms %ps\n",
					e->time_stamp_ns, e->pid,
					sched_event_name[e->type],
					e->time_us / 1000, e->time_us % 1000,
					e->pc);
		}
	} else {
		ret = seq_printf(m, "%lld %5d <%s> %d.%03d ms\n",
				e->time_stamp_ns, e->pid,
				sched_event_name[e->type],
				e->time_us / 1000, e->time_us % 1000);
	}

	return ret;
}

KTRACE_QUEUE_RO_SINGLE(sched);


static int sched_stats_show(struct seq_file *m, void *v)
{
	struct ktrace_sched *sched = (struct ktrace_sched *)m->private;
	int i;
	u64 total_block_time = 0;
	int total_block_cnt = 0;

	seq_puts(m, "sched stats:\n");
	for (i = 0; i < KTRACE_SCHED_TYPE_NR; i++) {
		seq_printf(m, "    %-20s    %6d     %llu ms\n", sched_event_name[i],
				atomic_read(&sched->stats[i].count),
				sched->stats[i].time / 1000);
	}

	seq_puts(m, "\n");

	seq_puts(m, "Block reason:\n");
	for (i = 0; i < BLOCK_REASON_NR; i++) {
		struct sched_block_reason *reason = &sched->block_reason[i];

		if (!reason->pc)
			break;

		total_block_time += reason->time;
		total_block_cnt += reason->count;
		seq_printf(m, "    %2d.   %-40pS  =>  %d, total %lld ms, avg %lld.%lld ms\n",
				i + 1, reason->pc, reason->count,
				reason->time / 1000, (reason->time / reason->count) / 1000,
				(reason->time / reason->count) % 1000);
	}
	seq_printf(m, "  total: %d, %lld ms\n", total_block_cnt, total_block_time / 1000);

	return 0;
}
KTRACE_ENTRY_RO_SINGLE(sched_stats);

static inline int __match_pid(pid_t *pids, int nr, pid_t new_pid)
{
	int i;
	int match = 0;

	for (i = 0; i < nr; i++) {
		if (pids[i] == INVALID_PID)
			break;

		if (pids[i] == new_pid) {
			match = 1;
			break;
		}
	}

	return match;
}

static inline int sched_match_pid(struct ktrace_sched *sched, pid_t pid)
{

	if (unlikely(sched->match_all_pid))
		return 1;

	return __match_pid(sched->pids, sched->pid_nr, pid);
}

static void sched_init_pids(struct ktrace_sched *sched)
{
	int i;

	for (i = 0; i < MONITOR_PID_NR; i++) {
		sched->pids[i] = INVALID_PID;
		sched->new_pids[i] = INVALID_PID;
	}
	sched->pid_nr = 0;
	sched->new_pid_nr = 0;
}

static void sched_erase_pids(pid_t *pids, int *nr)
{
	int i;

	for (i = 0; i < *nr; i++) {
		pids[i] = INVALID_PID;
	}
	*nr = 0;

	return;
}

static void sched_update_pids(struct ktrace_sched *sched)
{
	int i;
	int n = 0;

	if (sched->enable_debug) {
		char buf[64];
		for (i = 0; i < sched->pid_nr; i++) {
			if (sched->pids[i] == INVALID_PID)
				break;
			n += snprintf(buf + n, sizeof(buf) - n - 1, "%d  ", sched -> pids[i]);
		}

		n += snprintf(buf + n, sizeof(buf) - n - 1, "=>  ");

		for (i = 0; i < sched->new_pid_nr; i++) {
			if (sched->new_pids[i] == INVALID_PID)
				break;
			n += snprintf(buf + n, sizeof(buf) - n - 1, "%d  ", sched -> new_pids[i]);
		}

		n += snprintf(buf + n, sizeof(buf) - n - 1, "\n");

		ktrace_sched_debug(sched->enable_debug, buf);
		KTRACE_BEGIN(buf);
		KTRACE_END();
	}

	sched_erase_pids(sched->pids, &sched->pid_nr);

	for (i = 0; i < sched->new_pid_nr; i++) {
		sched->pids[i] = sched->new_pids[i];
		sched->pid_nr++;
	}

	sched_erase_pids(sched->new_pids, &sched->new_pid_nr);

}

static int sched_switch_pid(struct ktrace_sched *sched)
{
	sched_update_pids(sched);

	return 0;
}

static int sched_pid_show(struct seq_file *m, void *v)
{
	struct ktrace_sched *sched = m->private;
	int i;

	if (sched->match_all_pid) {
		seq_puts(m, "match all pids\n");
		return 0;
	}

	seq_printf(m, "%d pids registered\n", sched->pid_nr);

	for (i = 0; i < sched->pid_nr; i++) {
		if (sched->pids[i] == INVALID_PID)
			break;

		seq_printf(m, "%d  ", sched->pids[i]);
	}
	seq_putc(m, '\n');

	return 0;
}

static int sched_pid_open(struct inode *inode, struct file *file)
{
	int ret;

	ret = single_open(file, sched_pid_show, inode->i_private);
	if (!ret) {
	}
	return ret;
}

static ssize_t sched_pid_write(struct file *file, const char __user *user_buf,
		size_t size, loff_t *ppos)
{
	struct ktrace_sched *sched = ((struct seq_file *)file->private_data)->private;
	char buf[64], *tmp;

	char *pid_str;
	pid_t pid;
	int ret = 0;

	if (size >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, user_buf, size))
		return -EFAULT;

	buf[size] = 0;
	tmp = buf;

	if (strncmp(buf, "all", strlen("all")) == 0) {
		sched->match_all_pid = 1;
		return size;
	} else if (strncmp(buf, "notall", strlen("notall")) == 0) {
		sched->match_all_pid = 0;
		return size;
	}

	while ((pid_str = strsep(&tmp, ","))) {
		ret = kstrtoint(pid_str, 10, &pid);
		if (ret < 0)
			break;

		if (sched->new_pid_nr >= MONITOR_PID_NR - 1) {
			ret = -EINVAL;
			break;
		}

		sched->new_pids[sched->new_pid_nr++] = pid;
	}

	if (!ret) {
		if (USE_THREAD_TO_UPDATE_PIDS)
			wake_up_process(sched->thread);
		else
			sched_update_pids(sched);
	} else {
		sched_erase_pids(sched->new_pids, &sched->new_pid_nr);
	}
	return ret ? ret : size;
}

static const struct file_operations sched_pid_fops = {
	.owner		= THIS_MODULE,
	.open		= sched_pid_open,
	.read		= seq_read,
	.write		= sched_pid_write,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int sched_reset_write(void *data, u64 val)
{
	struct ktrace_queue *q = data;
	struct ktrace_sched *sched = q->priv;
	int clean = val;
	int i;

	if (clean) {
		ktrace_reset_queue(q);

		for (i = 0; i < KTRACE_SCHED_TYPE_NR; i++) {
			atomic_set(&sched->stats[i].count, 0);
			sched->stats[i].time = 0;
		}

		for (i = 0; i < BLOCK_REASON_NR; i++) {
			struct sched_block_reason *reason = &sched->block_reason[i];

			reason->count = 0;
			reason->pc = NULL;
			reason->time = 0;
		}
	}

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(sched_reset, NULL, sched_reset_write, "%llu\n");

static inline void sched_add_block_reason(struct ktrace_sched *sched, void *pc,
		unsigned int time)
{
	int i;

	for (i = 0; i < BLOCK_REASON_NR; i++) {
		struct sched_block_reason *reason = &sched->block_reason[i];
		if (reason->pc == NULL || reason->pc == pc) {
			if (reason->pc == NULL)
				reason->pc = pc;
			reason->count++;
			reason->time += time;
			break;
		}
	}
}

void ktrace_add_sched_event(unsigned char type, pid_t pid, u64 time_stamp, u64 delta, void *pc)
{
	struct ktrace_sched *sched = &__ks;
	struct ktrace_queue *q = sched->q;
	struct sched_entry e;
	u64 delta_us = delta >> 10;

	if (unlikely(!sched->buf))
		return;

	if (pid == 0)
		return;

	if (likely(delta_us < sched->filter[type]))
		return;

	if (!sched_match_pid(sched, pid))
		return;

	e.time_stamp_ns = time_stamp;
	e.type = type;
	e.pid = pid;
	e.time_us = (unsigned int)delta_us;
	e.pc = pc;

	atomic_inc(&sched->stats[type].count);
	sched->stats[type].time += delta_us;

	if (type == KTRACE_SCHED_TYPE_BLOCK)
		sched_add_block_reason(sched, pc, delta_us);

	ktrace_tryadd_queue(q, &e);
}

int ktrace_sched_match_pid(pid_t pid)
{
	struct ktrace_sched *sched = &__ks;

	return sched_match_pid(sched, pid);
}

static int ktrace_sched_thread(void *d)
{
	struct ktrace_sched *sched = (struct ktrace_sched *)d;
	struct sched_param param = {
		.sched_priority = 1
	};

	sched_setscheduler(current, SCHED_FIFO, &param);

	while (!kthread_should_stop()) {
		int ret;

		set_current_state(TASK_INTERRUPTIBLE);

		if (sched->new_pid_nr == 0)
			schedule();

		set_current_state(TASK_RUNNING);

		ret = sched_switch_pid(sched);
	}

	return 0;
}

int __init ktrace_sched_init(struct dentry *dir, struct ktrace_queue *q)
{
	struct ktrace_sched *sched = &__ks;
	void *buf;
	int i;

	BUILD_BUG_ON(sizeof(sched_event_name) / sizeof(sched_event_name[0])
			!= KTRACE_SCHED_TYPE_NR);

	memset(sched, sizeof(struct ktrace_sched), 0);

	if (USE_THREAD_TO_UPDATE_PIDS) {
		sched->thread = kthread_run(ktrace_sched_thread, sched, "ktrace-sched");
		if (IS_ERR(sched->thread)) {
			return PTR_ERR(sched->thread);
		}
	}

	buf = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, 1);
	if (!buf)
		return -ENOMEM;
	sched->q = q;

	sched->entry_size = sizeof(struct sched_entry);
	sched->max = (PAGE_SIZE << 1) / sched->entry_size;

	sched_init_pids(sched);

	sched->dir = debugfs_create_dir("sched", dir);

	ktrace_init_queue(q, sched->dir, sched, buf, sched->entry_size, sched->max,
			SCHED_ITEMS_READ_MAX,
			sched_show_entry);

	if (sched->dir) {
		debugfs_create_file("trace",
				S_IFREG | S_IRUGO,
				sched->dir,
				q,
				&sched_fops);

		debugfs_create_file("stats",
				S_IFREG | S_IRUGO,
				sched->dir,
				sched,
				&sched_stats_fops);

		for (i = 0; i < KTRACE_SCHED_TYPE_NR; i++) {
			char name[32];
			snprintf(name, sizeof(name) - 1, "filter_%s", sched_event_name[i]);

			debugfs_create_u32(name,
					S_IFREG | S_IRUGO | S_IWUSR,
					sched->dir,
					&sched->filter[i]);
		}

		debugfs_create_file("pids", S_IRUGO | S_IWUSR, sched->dir, sched,
				&sched_pid_fops);

		debugfs_create_bool("sched_debug",
				S_IFREG | S_IRUGO | S_IWUSR,
				sched->dir,
				&sched->enable_debug);

		debugfs_create_file("reset", S_IWUSR, sched->dir, q,
				&sched_reset);
	}

	smp_mb();
	sched->buf = buf;

	pr_info("sched ktrace init OK\n");

	return 0;
}
