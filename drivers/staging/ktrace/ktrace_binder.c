#define pr_fmt(fmt)  "ktrace : " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/preempt.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/ktrace.h>

#define BINDER_ITEMS_READ_MAX DEFAULT_ITEMS_PER_READ

struct binder_entry {
	long long time_stamp_ns;
	unsigned char type;
	pid_t pid;
	unsigned int time_us;
};

struct binder_stats {
	atomic_t count;
	u64 time;
};

struct ktrace_binder {
	struct ktrace_queue *q;
	struct dentry *dir;

	void *buf;
	int entry_size;
	int max;

	unsigned int match_all_pid;
	unsigned int filter[KTRACE_BINDER_TYPE_NR];
	struct binder_stats stats[KTRACE_BINDER_TYPE_NR];
};

static struct ktrace_binder __binder = {
	.filter = {
		[KTRACE_BINDER_TYPE_SAMPLE1] = MS_TO_US(1),
		[KTRACE_BINDER_TYPE_SAMPLE2] = MS_TO_US(1),
		[KTRACE_BINDER_TYPE_SAMPLE3] = MS_TO_US(1),
		[KTRACE_BINDER_TYPE_SAMPLE4] = MS_TO_US(1),
		[KTRACE_BINDER_TYPE_SAMPLE5] = MS_TO_US(1),
		[KTRACE_BINDER_TYPE_SAMPLE6] = MS_TO_US(1),
		[KTRACE_BINDER_TYPE_SAMPLE7] = MS_TO_US(1),
		[KTRACE_BINDER_TYPE_SAMPLE8] = MS_TO_US(1),
		[KTRACE_BINDER_TYPE_SAMPLE9] = MS_TO_US(1),
		[KTRACE_BINDER_TYPE_SAMPLE10] = MS_TO_US(1),

		[KTRACE_BINDER_TYPE_TRANSACTION] = MS_TO_US(1),
	}
};

static const char *const binder_event_name[KTRACE_BINDER_TYPE_NR] = {
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

	"transaction",
};

void ktrace_add_binder_event(unsigned char type, u64 time_stamp, u64 delta)
{
	struct ktrace_binder *binder = &__binder;
	struct ktrace_queue *q = binder->q;
	struct binder_entry e;
	u64 delta_us = delta >> 10;
	pid_t pid = current->pid;

	if (unlikely(!binder->buf || type >= KTRACE_BINDER_TYPE_NR))
		return;

	if (likely(delta_us < binder->filter[type]))
		return;

	if (!binder->match_all_pid && !ktrace_sched_match_pid(pid))
		return;

	e.time_stamp_ns = time_stamp;
	e.type = type;
	e.pid = pid;
	e.time_us = (unsigned int)delta_us;

	atomic_inc(&binder->stats[type].count);
	binder->stats[type].time += delta_us;

	ktrace_tryadd_queue(q, &e);
}

static int binder_show_entry(struct seq_file *m, void *entry, bool debug)
{
	struct binder_entry *e = (struct binder_entry *)entry;
	int ret;

	ret = seq_printf(m, "%lld %5d <%s> %d.%03d ms\n",
			e->time_stamp_ns, e->pid,
			binder_event_name[e->type],
			e->time_us / 1000, e->time_us % 1000);

	return ret;
}

KTRACE_QUEUE_RO_SINGLE(binder);

static int binder_stats_show(struct seq_file *m, void *v)
{
	struct ktrace_binder *binder = (struct ktrace_binder *)m->private;
	int i;

	seq_puts(m, "statatistics:\n");
	for (i = 0; i < KTRACE_BINDER_TYPE_NR; i++) {
		seq_printf(m, "    %-32s    %6d     %llu ms\n", binder_event_name[i],
				atomic_read(&binder->stats[i].count),
				binder->stats[i].time >> 10);
	}

	return 0;
}
KTRACE_ENTRY_RO_SINGLE(binder_stats);

static int binder_reset_write(void *data, u64 val)
{
	struct ktrace_queue *q = data;
	struct ktrace_binder *binder = q->priv;
	int clean = val;
	int i;

	if (clean) {
		ktrace_reset_queue(q);

		for (i = 0; i < KTRACE_BINDER_TYPE_NR; i++) {
			atomic_set(&binder->stats[i].count, 0);
			binder->stats[i].time = 0;
		}
	}

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(binder_reset, NULL, binder_reset_write, "%llu\n");

int __init ktrace_binder_init(struct dentry *dir, struct ktrace_queue *q)
{
	struct ktrace_binder *binder = &__binder;
	void *buf;
	int i;

	BUILD_BUG_ON(sizeof(binder_event_name) / sizeof(binder_event_name[0])
			!= KTRACE_BINDER_TYPE_NR);

	memset(binder, sizeof(struct ktrace_binder), 0);

	buf = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, 1);
	if (!buf)
		return -ENOMEM;
	binder->q = q;

	binder->entry_size = sizeof(struct binder_entry);
	binder->max = (PAGE_SIZE << 1) / binder->entry_size;

	binder->dir = debugfs_create_dir("binder", dir);

	ktrace_init_queue(q, binder->dir, binder, buf, binder->entry_size, binder->max,
			BINDER_ITEMS_READ_MAX,
			binder_show_entry);

	if (binder->dir) {
		debugfs_create_file("trace",
				S_IFREG | S_IRUGO,
				binder->dir,
				q,
				&binder_fops);

		debugfs_create_bool("match_all_pids",
				S_IFREG | S_IRUGO | S_IWUSR ,
				binder->dir,
				&binder->match_all_pid);

		debugfs_create_file("stats",
				S_IFREG | S_IRUGO,
				binder->dir,
				binder,
				&binder_stats_fops);

		for (i = 0; i < KTRACE_BINDER_TYPE_NR; i++) {
			char name[32];
			snprintf(name, sizeof(name) - 1, "filter_%s", binder_event_name[i]);

			debugfs_create_u32(name,
					S_IFREG | S_IRUGO | S_IWUSR,
					binder->dir,
					&binder->filter[i]);
		}

		debugfs_create_file("reset", S_IWUSR, binder->dir, q,
				&binder_reset);
	}

	smp_mb();
	binder->buf = buf;
	pr_info("binder ktrace init OK\n");

	return 0;
}
