
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
#include <linux/vmstat.h>
#include <linux/swap.h>
#include <linux/compaction.h>
#include <linux/ktrace.h>

#define MM_ITEMS_READ_MAX DEFAULT_ITEMS_PER_READ

struct mm_entry {
	long long time_stamp_ns;
	unsigned char type;
	pid_t pid;
	unsigned int time_us;
};

struct mm_stats {
	atomic_t count;
	u64 time;
};

struct ktrace_mm {
	struct ktrace_queue *q;
	struct dentry *dir;

	void *buf;
	int entry_size;
	int max;

	unsigned int match_all_pid;
	unsigned int filter[KTRACE_MM_TYPE_NR];
	struct mm_stats stats[KTRACE_MM_TYPE_NR];
};

static struct ktrace_mm __mm = {
	.filter = {
		[KTRACE_MM_TYPE_SAMPLE1] = MS_TO_US(1),
		[KTRACE_MM_TYPE_SAMPLE2] = MS_TO_US(1),
		[KTRACE_MM_TYPE_SAMPLE3] = MS_TO_US(1),
		[KTRACE_MM_TYPE_SAMPLE4] = MS_TO_US(1),
		[KTRACE_MM_TYPE_SAMPLE5] = MS_TO_US(1),
		[KTRACE_MM_TYPE_SAMPLE6] = MS_TO_US(1),
		[KTRACE_MM_TYPE_SAMPLE7] = MS_TO_US(1),
		[KTRACE_MM_TYPE_SAMPLE8] = MS_TO_US(1),
		[KTRACE_MM_TYPE_SAMPLE9] = MS_TO_US(1),
		[KTRACE_MM_TYPE_SAMPLE10] = MS_TO_US(1),

		[KTRACE_MM_TYPE_SLOW_PATH] = MS_TO_US(1),
		[KTRACE_MM_TYPE_COMPACT] = MS_TO_US(1),
		[KTRACE_MM_TYPE_DIRECT_RECLAIM] = MS_TO_US(1),
	}
};

static const char *const mm_event_name[KTRACE_MM_TYPE_NR] = {
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

	"slow_path",
	"compact",
	"direct_reclaim",
};

void ktrace_add_mm_event(unsigned char type, u64 time_stamp, u64 delta)
{
	struct ktrace_mm *mm = &__mm;
	struct ktrace_queue *q = mm->q;
	struct mm_entry e;
	u64 delta_us = delta >> 10;
	pid_t pid = current->pid;

	if (unlikely(!mm->buf || type >= KTRACE_MM_TYPE_NR))
		return;

	if (!mm->match_all_pid && !ktrace_sched_match_pid(pid))
		return;

	if (likely(delta_us < mm->filter[type]))
		return;

	e.time_stamp_ns = time_stamp;
	e.type = type;
	e.pid = pid;
	e.time_us = (unsigned int)delta_us;

	atomic_inc(&mm->stats[type].count);
	mm->stats[type].time += delta_us;

	ktrace_tryadd_queue(q, &e);
}

static int mm_show_entry(struct seq_file *m, void *entry, bool debug)
{
	struct mm_entry *e = (struct mm_entry *)entry;
	int ret;

	ret = seq_printf(m, "%lld %5d <%s> %d.%03d ms\n",
			e->time_stamp_ns, e->pid,
			mm_event_name[e->type],
			e->time_us / 1000, e->time_us % 1000);

	return ret;

	return 0;
}

KTRACE_QUEUE_RO_SINGLE(mm);

static int mm_fragment_show(struct seq_file *m, void *v)
{
	int nid;
	unsigned int order;

	lru_add_drain_all();

	seq_printf(m, "%-15s        ", "fragmentation ");
	for (order = 0; order < MAX_ORDER; ++order)
		seq_printf(m, "%6d ", order);
	seq_putc(m, '\n');

	for_each_online_node(nid) {
		pg_data_t *pgdat = NODE_DATA(nid);
		int zoneid;

		if (!node_state(pgdat->node_id, N_MEMORY))
			continue;

		for (zoneid = 0; zoneid < MAX_NR_ZONES; zoneid++) {
			struct zone *zone = &pgdat->node_zones[zoneid];
			int fragindex;
			unsigned long flags;

			if (!populated_zone(zone))
				continue;

			seq_printf(m, "Node %d, zone %8s  ",
				pgdat->node_id, zone->name);

			if (!spin_trylock_irqsave(&zone->lock, flags))
				return  0;

			for (order = 0; order < MAX_ORDER; order++) {
				fragindex = fragmentation_index(zone, order);
				if (fragindex == -1000) {
					seq_printf(m, "%6s ", "ok");
				} else {
					seq_printf(m, "%6d ", fragindex);
				}
			}
			seq_putc(m, '\n');
			spin_unlock_irqrestore(&zone->lock, flags);
		}
	}

	return 0;
}

KTRACE_ENTRY_RO_SINGLE(mm_fragment);

static int mm_stats_show(struct seq_file *m, void *v)
{
	struct ktrace_mm *mm = (struct ktrace_mm *)m->private;
	int i;

	seq_puts(m, "statatistics:\n");
	for (i = 0; i < KTRACE_MM_TYPE_NR; i++) {
		seq_printf(m, "    %-32s    %6d     %llu ms\n", mm_event_name[i],
				atomic_read(&mm->stats[i].count),
				mm->stats[i].time >> 10);
	}

	return 0;
}
KTRACE_ENTRY_RO_SINGLE(mm_stats);

static int mm_reset_write(void *data, u64 val)
{
	struct ktrace_queue *q = data;
	struct ktrace_mm *mm = q->priv;
	int clean = val;
	int i;

	if (clean) {
		ktrace_reset_queue(q);

		for (i = 0; i < KTRACE_MM_TYPE_NR; i++) {
			atomic_set(&mm->stats[i].count, 0);
			mm->stats[i].time = 0;
		}
	}

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(mm_reset, NULL, mm_reset_write, "%llu\n");

int __init ktrace_mm_init(struct dentry *dir, struct ktrace_queue *q)
{
	struct ktrace_mm *mm = &__mm;
	void *buf;
	int i;

	BUILD_BUG_ON(sizeof(mm_event_name) / sizeof(mm_event_name[0])
			!= KTRACE_MM_TYPE_NR);

	memset(mm, sizeof(struct ktrace_mm), 0);

	buf = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, 1);
	if (!buf)
		return -ENOMEM;
	mm->q = q;

	mm->entry_size = sizeof(struct mm_entry);
	mm->max = (PAGE_SIZE << 1) / mm->entry_size;

	mm->dir = debugfs_create_dir("mm", dir);

	ktrace_init_queue(q, mm->dir, mm, buf, mm->entry_size, mm->max,
			MM_ITEMS_READ_MAX,
			mm_show_entry);

	if (mm->dir) {
		debugfs_create_file("trace",
				S_IFREG | S_IRUGO,
				mm->dir,
				q,
				&mm_fops);

		debugfs_create_bool("match_all_pids",
				S_IFREG | S_IRUGO | S_IWUSR ,
				mm->dir,
				&mm->match_all_pid);

		debugfs_create_file("fragmentation",
				S_IFREG | S_IRUGO,
				mm->dir,
				q,
				&mm_fragment_fops);

		debugfs_create_file("stats",
				S_IFREG | S_IRUGO,
				mm->dir,
				mm,
				&mm_stats_fops);

		for (i = 0; i < KTRACE_MM_TYPE_NR; i++) {
			char name[32];
			snprintf(name, sizeof(name) - 1, "filter_%s", mm_event_name[i]);

			debugfs_create_u32(name,
					S_IFREG | S_IRUGO | S_IWUSR,
					mm->dir,
					&mm->filter[i]);
		}

		debugfs_create_file("reset", S_IWUSR, mm->dir, q,
				&mm_reset);
	}

	smp_mb();
	mm->buf = buf;
	pr_info("mm ktrace init OK\n");

	return 0;
}
