// SPDX-License-Identifier: GPL-2.0+
/*
 * mi_locking will show kernel lock wait/hold time.
 *
 * Copyright (C) 2024 Xiaomi Ltd.
 */

#define MI_LOCK_LOG_TAG       "waitlock_sort"

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/vmalloc.h>
#include <linux/sort.h>
#include <asm/div64.h>
#include <linux/kprobes.h>
#include <linux/hash.h>

#include "waitlock.h"
#include "holdlock.h"


/* save Top_N */
#define TOP_N 20
#define CLASSHASH_BITS		MAX_LOCKDEP_KEYS_BITS
#define CLASSHASH_SIZE		(1UL << CLASSHASH_BITS)
#define __classhashfn(key)	hash_long((unsigned long)key, CLASSHASH_BITS)
#define classhashentry(key)	(classhash_table + __classhashfn((key)))
static struct hlist_head classhash_table[CLASSHASH_SIZE];
/* nr_lock_classes is the number of elements of lock_classes[] that is in use */
unsigned long nr_lock_classes;
unsigned long max_lock_class_idx;
struct mi_lock_class lock_classes[MAX_LOCKDEP_KEYS];
DECLARE_BITMAP(lock_classes_in_use, MAX_LOCKDEP_KEYS);
struct mi_lock_class_stats cpu_lock_stats_mutex[MAX_CPU_KEYS][MAX_LOCKDEP_KEYS]; //8x8192
struct mi_lock_class_stats cpu_lock_stats_rwsemw[MAX_CPU_KEYS][MAX_LOCKDEP_KEYS]; //8x8192
unsigned long lock_classes_in_use[];

static spinlock_t hash_lock_mutex;
static spinlock_t hash_lock_rwsem;
/*
 * We keep a global list of all lock classes. The list is only accessed with
 * the lockdep spinlock lock held. free_lock_classes is a list with free
 * elements. These elements are linked together by the lock_entry member in
 * struct mi_lock_class.
 */
static LIST_HEAD(all_lock_classes);
static LIST_HEAD(free_lock_classes);

struct mi_lock_stat_data {
	struct mi_lock_class *class;
	struct mi_lock_class_stats stats;
};

struct mi_lock_stat_seq {
	struct proc_context *context;
	struct mi_lock_stat_data *iter_start;
	struct mi_lock_stat_data *iter_end;
	struct mi_lock_stat_data stats[MAX_LOCKDEP_KEYS];
};

#define iterate_lock_classes(idx, class)				\
	for (idx = 0, class = lock_classes; idx <= max_lock_class_idx;	\
	     idx++, class++)

/* sort on absolute number of total time */
int lock_stat_cmp_total(const void *l, const void *r)
{
	const struct mi_lock_stat_data *dl = l, *dr = r;
	s64 nl, nr;
	nl = dl->stats.mi_waittime.total;
	nr = dr->stats.mi_waittime.total;

	return (nr > nl) - (nr < nl);
}

/* sort on absolute number of avg time */
static int lock_stat_cmp_avg(const void *l, const void *r)
{
	const struct mi_lock_stat_data *dl = l, *dr = r;
	s64 nl, nr;
	nl = dl->stats.mi_waittime.avg;
	nr = dr->stats.mi_waittime.avg;

	return (nr > nl) - (nr < nl);
}

/* sort on absolute number of max time */
static int lock_stat_cmp_max(const void *l, const void *r)
{
	const struct mi_lock_stat_data *dl = l, *dr = r;
	s64 nl, nr;
	nl = dl->stats.mi_waittime.max;
	nr = dr->stats.mi_waittime.max;

	return (nr > nl) - (nr < nl);
}

/* sort on absolute number of wait_cnt */
static int lock_stat_cmp_wait_cnt(const void *l, const void *r)
{
	const struct mi_lock_stat_data *dl = l, *dr = r;
	unsigned long nl, nr;
	nl = dl->stats.mi_waittime.wait_cnt;
	nr = dr->stats.mi_waittime.wait_cnt;

	return (nr > nl) - (nr < nl);
}

static void seq_line(struct seq_file *m, char c, int offset, int length)
{
	int i;

	for (i = 0; i < offset; i++)
		seq_puts(m, " ");
	for (i = 0; i < length; i++)
		seq_printf(m, "%c", c);
	seq_puts(m, "\n");
}

static void snprint_time(char *buf, size_t bufsiz, s64 nr)
{
	s64 div;
	s32 rem;

	nr += 5; /* for display rounding */
	div = div_s64_rem(nr, 1000, &rem);
	snprintf(buf, bufsiz, "%lld.%02d", (long long)div, (int)rem/10);
}

static void seq_time(struct seq_file *m, s64 time)
{
	char num[15];

	snprint_time(num, sizeof(num), time);
	seq_printf(m, " %14s", num);
}

static void seq_lock_time(struct seq_file *m, struct mi_lock_time *lt)
{
	seq_printf(m, "%14lu", lt->wait_cnt);
	seq_time(m, lt->max);
	seq_time(m, lt->total);
	seq_time(m, lt->avg);
	seq_printf(m, "%14lu", lt->rank);
}

static void seq_stats(struct seq_file *m, struct mi_lock_stat_data *data)
{
	unsigned long ckey;
	struct mi_lock_class_stats *stats;
	struct mi_lock_class *class;
	const char *cname;
	int namelen;
	char name[30];

	class = data->class;
	stats = &data->stats;

	namelen = 30;

	rcu_read_lock_sched();
	cname = rcu_dereference_sched(class->name);

	ckey = class->key;

	if (!cname && !ckey) {
		rcu_read_unlock_sched();
		return;

	} else if (!cname) {
		const char *key_name = "null_test_name";
		snprintf(name, namelen, "%s", key_name);
	} else {
		snprintf(name, namelen, "%s", cname);
	}
	rcu_read_unlock_sched();

	seq_printf(m, "%30s:", name);
	seq_lock_time(m, &stats->mi_waittime);
	seq_puts(m, "\n");
	seq_line(m, '.', 0, 30 + 1 + 5 * (14 + 1));
}

static void seq_header(struct seq_file *m)
{
	struct mi_lock_stat_seq *data = m->private;
	struct proc_context *ctx = data->context;

	seq_printf(m, "\n");
	seq_printf(m, "Mi locking 1.0 , lock waittime table display by 's', sort by: %s \n", ctx->name);
	//f (unlikely(!debug_locks))
    //	seq_printf(m, "*WARNING* lock debugging disabled!! - possibly due to a lockdep warning\n");
	seq_line(m, '-', 0, 30 + 1 + 5 * (14 + 1));
	seq_printf(m, "%30s %14s %14s %14s %14s %14s\n",
			"class name",
			"wait-cnt",
			"waittime-max",
			"waittime-total",
			"waittime-avg",
			"Top");
	seq_line(m, '-', 0, 30 + 1 + 5 * (14 + 1));
}

static void *ls_start(struct seq_file *m, loff_t *pos)
{
	if (!g_opt_sort){
		ml_err("please enable locking_sort before cat this! \n");
		return 0;
	}

	struct mi_lock_stat_seq *data = m->private;
	struct mi_lock_stat_data *iter;

	if (*pos == 0)
		return SEQ_START_TOKEN;

	iter = data->stats + (*pos - 1);
	if (iter >= data->iter_end)
		iter = NULL;

	return iter;
}

static void *ls_next(struct seq_file *m, void *v, loff_t *pos)
{
	(*pos)++;
	return ls_start(m, pos);
}

static void ls_stop(struct seq_file *m, void *v)
{
}

static int ls_show(struct seq_file *m, void *v)
{
	if (v == SEQ_START_TOKEN)
		seq_header(m);
	else
		seq_stats(m, v);

	return 0;
}

static const struct seq_operations lockstat_ops = {
	.start	= ls_start,
	.next	= ls_next,
	.stop	= ls_stop,
	.show	= ls_show,
};

/*----------------------------------------------------------------------------------*/
static void lock_time_inc(struct mi_lock_time *lt, u64 time)
{
	if (time > lt->max)
		lt->max = time;
	lt->total += time;
	lt->wait_cnt++;
}

static inline void lock_time_add(struct mi_lock_time *src, struct mi_lock_time *dst)
{
	if (src->max > dst->max)
		dst->max = src->max;
	dst->total += src->total;
	dst->wait_cnt += src->wait_cnt;
}

/* Accumulate the mi_waittime on each CPU and store it in stats, then return stats. */
static struct mi_lock_class_stats mi_lock_stats(struct mi_lock_class *class, int type)
{
	struct mi_lock_class_stats stats;
	int cpu;

	memset(&stats, 0, sizeof(struct mi_lock_class_stats));
	for_each_possible_cpu(cpu) {
		struct mi_lock_class_stats *pcs;
		switch (type) {
			case MUTEX:
				pcs = &cpu_lock_stats_mutex[cpu][class - lock_classes];
				break;
			case RWSEM_WRITE:
				pcs = &cpu_lock_stats_rwsemw[cpu][class - lock_classes];
				break;
			default:
				continue;
		}
		lock_time_add(&pcs->mi_waittime, &stats.mi_waittime);
	}
	stats.mi_waittime.avg = stats.mi_waittime.total / stats.mi_waittime.wait_cnt;
	return stats;
}

static void mi_clear_lock_stats(struct mi_lock_class *class)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		struct mi_lock_class_stats *cpu_stats_mutex = &cpu_lock_stats_mutex[cpu][class - lock_classes];
		struct mi_lock_class_stats *cpu_stats_rwsemw = &cpu_lock_stats_rwsemw[cpu][class - lock_classes];
		memset(cpu_stats_mutex, 0, sizeof(struct mi_lock_class_stats));
		memset(cpu_stats_rwsemw, 0, sizeof(struct mi_lock_class_stats));
	}
}

static int lock_stat_open(struct inode *inode, struct file *file)
{
	int res;
	unsigned long actual_count,num_to_keep;
	struct mi_lock_class *class;
	struct proc_context *ctx = pde_data(inode);
	struct mi_lock_stat_seq *data = vmalloc(sizeof(struct mi_lock_stat_seq));

	if (!data)
		return -ENOMEM;

	memset(data, 0, sizeof(struct mi_lock_stat_seq));

	res = seq_open(file, &lockstat_ops);
	if (!res) {
		struct mi_lock_stat_data *iter = data->stats;
		struct seq_file *m = file->private_data;

		unsigned long idx;

		iterate_lock_classes(idx, class) {
			if (!test_bit(idx, lock_classes_in_use))
				continue;
			iter->class = class;
			iter->stats = mi_lock_stats(class, ctx->type);
			iter++;
		}

		data->iter_end = iter;
 
		sort(data->stats, data->iter_end - data->stats,
				sizeof(struct mi_lock_stat_data),
				ctx->cmp_fn, NULL);

		/* Calculate the number of elements to retain,
		 * which cannot exceed the actual count or the upper limit of 20.
 		 * Adjust the iter_end pointer to point to the position after the 20th element.
		 */
		actual_count = iter - data->stats;
		num_to_keep = min(actual_count, TOP_N);
		data->iter_end = data->stats + num_to_keep;

		for (int i = 0; i < TOP_N; ++i) {
			data->stats[i].stats.mi_waittime.rank = i + 1; // rank值从1开始
		}

		data->context = ctx;
		m->private = data;
	} else {
		vfree(data);
	}

	return res;
}

/*
 * echo 0 > /proc/lock_stat_overview to clear all the data
 */
static ssize_t lock_stat_write(struct file *file, const char __user *buf,
			       size_t count, loff_t *ppos)
{
	struct mi_lock_class *class;
	unsigned long idx;
	char c;

	if (count) {
		if (get_user(c, buf))
			return -EFAULT;

		if (c != '0')
			return count;

		iterate_lock_classes(idx, class) {
			if (!test_bit(idx, lock_classes_in_use))
				continue;
			mi_clear_lock_stats(class);
		}
	}
	return count;
}

static int lock_stat_release(struct inode *inode, struct file *file)
{
	struct seq_file *seq = file->private_data;

	vfree(seq->private);
	return seq_release(inode, file);
}

const struct proc_ops lock_stat_overview_fops = {
	.proc_open	= lock_stat_open,
	.proc_write	= lock_stat_write,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release = lock_stat_release,
};

DEFINE_PROC_CONTEXT(mutex_max_ctx, "mutex_max", MUTEX, lock_stat_cmp_max);
DEFINE_PROC_CONTEXT(mutex_avg_ctx, "mutex_avg", MUTEX, lock_stat_cmp_avg);
DEFINE_PROC_CONTEXT(mutex_total_ctx, "mutex_total", MUTEX, lock_stat_cmp_total);
DEFINE_PROC_CONTEXT(mutex_cnt_ctx, "mutex_cnt", MUTEX, lock_stat_cmp_wait_cnt);

DEFINE_PROC_CONTEXT(rwsemw_max_ctx, "rwsemw_max", RWSEM_WRITE, lock_stat_cmp_max);
DEFINE_PROC_CONTEXT(rwsemw_avg_ctx, "rwsemw_avg", RWSEM_WRITE, lock_stat_cmp_avg);
DEFINE_PROC_CONTEXT(rwsemw_total_ctx, "rwsemw_total", RWSEM_WRITE, lock_stat_cmp_total);
DEFINE_PROC_CONTEXT(rwsemw_cnt_ctx, "rwsemw_cnt", RWSEM_WRITE, lock_stat_cmp_wait_cnt);

/*
 * Initialize the lock_classes[] array elements, the free_lock_classes list
 * and also the delayed_free structure.
 */
static void init_data_structures_once(void)
{
	static bool __read_mostly ds_initialized, rcu_head_initialized;
	int i;

	if (likely(rcu_head_initialized))
		return;

	if (system_state >= SYSTEM_SCHEDULING) {
		rcu_head_initialized = true;
	}

	if (ds_initialized)
		return;

	ds_initialized = true;

	spin_lock_init(&hash_lock_mutex);
	spin_lock_init(&hash_lock_rwsem);

	nr_lock_classes = 0;
	max_lock_class_idx = 0;
	bitmap_zero(lock_classes_in_use, MAX_LOCKDEP_KEYS);
	memset(cpu_lock_stats_mutex, 0, sizeof(cpu_lock_stats_mutex));
	memset(cpu_lock_stats_rwsemw, 0, sizeof(cpu_lock_stats_rwsemw));
	for (i = 0; i < ARRAY_SIZE(lock_classes); i++) {
		/* Iterate through each element in the lock_classes array
		 * and add it to the tail of the free_lock_classes list via its lock_entry member.
		 */
		list_add_tail(&lock_classes[i].lock_entry, &free_lock_classes);
		INIT_LIST_HEAD(&lock_classes[i].locks_after);
		INIT_LIST_HEAD(&lock_classes[i].locks_before);
	}

}

static struct mi_lock_class *register_lock_class(struct mi_lockdep_map *lock, int force, int type)
{
	struct hlist_head *hash_head;
	struct mi_lock_class *class;
	unsigned long key = 0;
	unsigned long flags;
	int idx;
	spinlock_t *hash_lock;

	if (lock->key < 0xffffff0000000000) {
		return NULL;
	}

	if (!irqs_disabled()){
		ml_err("BUG: register_lock_class, irqs is not disable\n");
		return NULL;
	}

    /* choice spin_lock */
    if (type == MUTEX) {
        hash_lock = &hash_lock_mutex;
    } else if (type == RWSEM_WRITE) {
        hash_lock = &hash_lock_rwsem;
    } else {
        return NULL;
    }

	key = lock->key;
	hash_head = classhashentry(key);

	spin_lock_irqsave(hash_lock, flags);

	hlist_for_each_entry_rcu(class, hash_head, hash_entry) {
        if (class->key == key) {
			//ml_err("already register, lock->name = %s\n", lock->name);
            goto out_already_register;
        }
    }
	/* Allocate a new lock class and add it to the hash. */
	/* register a key in hash*/
	class = list_first_entry_or_null(&free_lock_classes, typeof(*class), lock_entry);

	if (!class) {
		ml_err("BUG:MAX_LOCKDEP_KEYS too low\n");
		spin_unlock_irqrestore(hash_lock, flags);
		return NULL;
	}

	nr_lock_classes++;
	__set_bit(class - lock_classes, lock_classes_in_use);
	class->key = key;
	class->name = lock->name;
	WARN_ON_ONCE(!list_empty(&class->locks_before));
	WARN_ON_ONCE(!list_empty(&class->locks_after));
	/*
	 * We use RCU's safe list-add method to make parallel walking of the hash-list safe:
	 */
	hlist_add_head_rcu(&class->hash_entry, hash_head);
	/*
	 * Remove the class from the free list and add it to the global list of classes.
	 */
	list_move_tail(&class->lock_entry, &all_lock_classes);
	idx = class - lock_classes;
	if (idx > max_lock_class_idx)
		max_lock_class_idx = idx;

	if (class) {
		spin_unlock_irqrestore(hash_lock, flags);
		cond_trace_printk(locking_opt_debug(LK_DEBUG_PRINTK),
			"[%s]: new class key:%lu, name:%s, nr_classes:%lu",
			__func__, class->key, class->name, nr_lock_classes);
		return class;
	}

out_already_register:
	spin_unlock_irqrestore(hash_lock, flags);
 	return class;
}

static struct mi_lock_class_stats *get_lock_stats(struct mi_lock_class *class, int type)
{
	int cpu;

    if (class == NULL || class >= lock_classes + MAX_LOCKDEP_KEYS) {
        return NULL;
    }

	cpu = raw_smp_processor_id();

	if(type == MUTEX)
		return &cpu_lock_stats_mutex[cpu][class - lock_classes];
	else
		return &cpu_lock_stats_rwsemw[cpu][class - lock_classes];
}

void handle_wait_delta(const char *name, u64 time, int type)
{
	struct mi_lockdep_map lock;
	struct mi_lock_class *class;
	struct mi_lock_class_stats *stats;
	unsigned long flags;

	init_data_structures_once();

	lock.name = name;
	lock.key = (unsigned long)name;

	/* Convert ns to ms */
	time = time / NSEC_PER_MSEC;

	raw_local_irq_save(flags);
	class = register_lock_class(&lock, 0, type);
	if (class) {
		stats = get_lock_stats(class, type);
		lock_time_inc(&stats->mi_waittime, time);
		cond_trace_printk(locking_opt_debug(LK_DEBUG_PRINTK),
			"[%s]: finish add lock time, time = %llu\n", __func__, time);
	} 
	raw_local_irq_restore(flags);
}
