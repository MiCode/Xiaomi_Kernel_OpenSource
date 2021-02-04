/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define DEBUG 1

#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/hashtable.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>

struct pool_workqueue;
#include <trace/events/workqueue.h>

#define WQ_DUMP_QUEUE_WORK   0x1
#define WQ_DUMP_ACTIVE_WORK  0x2
#define WQ_DUMP_EXECUTE_WORK 0x4

#include "internal.h"

static unsigned int		wq_tracing;
static bool			wq_debug;
static struct kmem_cache	*work_info_cache;

#define WORK_EXEC_MAX (1000000000)
#define TAG "WQ warning! "

enum {
	WORK_QUEUED	= 0,
	WORK_EXECUTING	= 1,
};

struct work_info {
	unsigned int		cpu;
	/* cpu to execute this work, NR_CPUS if UNBOUND */
	unsigned int		state;
	/* either queued or executing */
	unsigned long		work;
	/* address of work structure */
	unsigned long		func;
	/* function addrss of the work */
	unsigned long		pwq;
	/* linked pool_workqueue address */
	unsigned long long	ts;
	/* timestamp while work queued/exec */
	char			sfunc[32];
	/* store name of the work */
	struct hlist_node	hash;
};

#define ACTIVE_WORK_BITS (7)
static DEFINE_HASHTABLE(active_works, ACTIVE_WORK_BITS);
static DEFINE_RAW_SPINLOCK(works_lock);

static u32 work_hash(struct work_info *work)
{
	return (u32)(work->work);
}

static bool work_info_equal(struct work_info *a, struct work_info *b)
{
	return (a->work == b->work);
}

static struct work_info *find_active_work(struct work_struct *work)
{
	struct work_info *wi = NULL;
	struct hlist_node *tmp;
	struct work_info target = {
		.work	= (unsigned long)work,
//		.func	= (unsigned long)work->func,
	};

	hash_for_each_possible_safe(active_works, wi, tmp,
				    hash, work_hash(&target)) {
		if (wi) {
			if (work_info_equal(wi, &target))
				return wi;
		}
	}
	return NULL;
}

static void probe_execute_work(void *ignore, struct work_struct *work)
{
	pr_debug("execute start work=%p func=%pf\n",
		 (void *)work, (void *)work->func);
}

static void probe_execute_end(void *ignore, struct work_struct *work)
{
	struct work_info *work_info = NULL;

	work_info = find_active_work(work);
	if (work_info) {
		pr_debug("execute end work=%p func=%pf\n",
			 (void *)work, (void *)work->func);
	}
}

static void probe_activate_work(void *ignore, struct work_struct *work)
{
	pr_debug("activate work=%p func=%pf\n",
		 (void *)work, (void *)work->func);
}

static void
probe_queue_work(void *ignore, unsigned int req_cpu, struct pool_workqueue *pwq,
		 struct work_struct *work)
{
	pr_debug("queue work=%p func=%pf cpu=%u pwq=%ps\n",
		 (void *)work, (void *)work->func, req_cpu, pwq);
}

static void
_work_queued(void *ignore, unsigned int req_cpu, struct pool_workqueue *pwq,
		 struct work_struct *work)
{
	gfp_t gfp = GFP_ATOMIC | __GFP_NORETRY | __GFP_NOWARN;
	struct work_info *work_info = NULL;
	raw_spin_lock(&works_lock);
	work_info = find_active_work(work);
	if (!work_info) {
		work_info = kmem_cache_zalloc(work_info_cache, gfp);
		if (!work_info)
			goto out;
		work_info->cpu = req_cpu;
		work_info->work = (unsigned long)work;
		work_info->func = (unsigned long)work->func;
		work_info->pwq = (unsigned long)pwq;
		work_info->state = WORK_QUEUED;
		snprintf(work_info->sfunc, sizeof(work_info->sfunc), "%pf",
			(void *)work_info->func);
		hash_add(active_works, &work_info->hash,
			 work_hash(work_info));
	}
	work_info->ts = sched_clock();
out:
	raw_spin_unlock(&works_lock);
}

static void _work_exec_start(void *ignore, struct work_struct *work)
{
	struct work_info *wi = NULL;
	unsigned long flags;

	raw_spin_lock_irqsave(&works_lock, flags);
	wi = find_active_work(work);
	if (!wi)
		goto not_found;
	wi->state = WORK_EXECUTING;
	wi->ts = sched_clock();
not_found:
	raw_spin_unlock_irqrestore(&works_lock, flags);
}

static void _work_exec_end(void *ignore, struct work_struct *work)
{
	struct work_info *work_info = NULL;
	unsigned long long ts;
	unsigned long flags, rem_nsec;
	struct work_info w;

	raw_spin_lock_irqsave(&works_lock, flags);
	work_info = find_active_work(work);
	if (!work_info) {
		raw_spin_unlock_irqrestore(&works_lock, flags);
		return;
	}
	ts = sched_clock() - work_info->ts;
	strncpy(w.sfunc, work_info->sfunc, sizeof(work_info->sfunc));
	w.work = work_info->work;
	hash_del(&work_info->hash);
	kmem_cache_free(work_info_cache, work_info);
	work_info = NULL;
	raw_spin_unlock_irqrestore(&works_lock, flags);

	if (ts > WORK_EXEC_MAX) {
		rem_nsec = do_div(ts, NSEC_PER_SEC);
		pr_debug(TAG "work(%s,%lx) exec %ld.%06lds, more than 1s\n",
			 w.sfunc,
			 (unsigned long)w.work,
			 (unsigned long)ts, rem_nsec / NSEC_PER_USEC);
	}

}

static void work_debug_enable(unsigned int on)
{
	if (on && !wq_debug) {
		register_trace_workqueue_queue_work(_work_queued,
			NULL);
		register_trace_workqueue_execute_start(_work_exec_start,
			NULL);
		register_trace_workqueue_execute_end(_work_exec_end,
			NULL);
	} else if (!on && wq_debug) {
		unregister_trace_workqueue_queue_work(_work_queued,
			NULL);
		unregister_trace_workqueue_execute_start(_work_exec_start,
			NULL);
		unregister_trace_workqueue_execute_end(_work_exec_end,
			NULL);
	}
	wq_debug = !!on;
}

static void print_help(struct seq_file *m)
{
	if (m != NULL) {
		SEQ_printf(m, "\n*** Usage ***\n");
		SEQ_printf(m, "commands to enable logs\n");
		SEQ_printf(m,
		"  echo [queue] [activate] [execute] > wq_enable_logs\n");
		SEQ_printf(m, "  ex. 1 1 1 to enable all logs\n");
		SEQ_printf(m, "  ex. 1 0 0 to enable queue logs\n");
	} else {
		pr_debug("\n*** Usage ***\n");
		pr_debug("commands to enable logs\n");
		pr_debug("  echo [queue work] [activate work]");
		pr_debug(" [execute work] > wq_enable_logs\n");
		pr_debug("  ex. 1 1 1 to enable all logs\n");
		pr_debug("  ex. 1 0 0 to enable queue logs\n");
	}
}

MT_DEBUG_ENTRY(wq_log);
static int mt_wq_log_show(struct seq_file *m, void *v)
{
	if (wq_tracing & WQ_DUMP_QUEUE_WORK)
		SEQ_printf(m, "wq: queue work log enabled\n");
	if (wq_tracing & WQ_DUMP_ACTIVE_WORK)
		SEQ_printf(m, "wq: active work log enabled\n");
	if (wq_tracing & WQ_DUMP_EXECUTE_WORK)
		SEQ_printf(m, "wq: execute work log enabled\n");
	if (wq_tracing == 0)
		SEQ_printf(m, "wq: no log enabled\n");

	print_help(m);
	return 0;
}

static void mt_wq_log_config(unsigned int queue, unsigned int activate,
			     unsigned int execute)
{
	unsigned int trace_queue = (0 != (wq_tracing & WQ_DUMP_QUEUE_WORK));
	unsigned int trace_activate = (0 != (wq_tracing & WQ_DUMP_ACTIVE_WORK));
	unsigned int trace_execute = (0 != (wq_tracing & WQ_DUMP_EXECUTE_WORK));
	unsigned int toggle;

	toggle = (!!queue ^ trace_queue);
	if (toggle && !trace_queue) {
		register_trace_workqueue_queue_work(probe_queue_work,
			NULL);
		wq_tracing |= WQ_DUMP_QUEUE_WORK;
	} else if (toggle && trace_queue) {
		unregister_trace_workqueue_queue_work(probe_queue_work,
			NULL);
		wq_tracing &= ~WQ_DUMP_QUEUE_WORK;
	}

	toggle = (!!activate ^ trace_activate);
	if (toggle && !trace_activate) {
		register_trace_workqueue_activate_work(probe_activate_work,
			NULL);
		wq_tracing |= WQ_DUMP_ACTIVE_WORK;
	} else if (toggle && trace_activate) {
		unregister_trace_workqueue_activate_work(probe_activate_work,
			NULL);
		wq_tracing &= ~WQ_DUMP_ACTIVE_WORK;
	}

	toggle = (!!execute ^ trace_execute);
	if (toggle && !trace_execute) {
		register_trace_workqueue_execute_start(probe_execute_work,
			NULL);
		register_trace_workqueue_execute_end(probe_execute_end,
			NULL);
		wq_tracing |= WQ_DUMP_EXECUTE_WORK;
	} else if (toggle && trace_execute) {
		unregister_trace_workqueue_execute_start(probe_execute_work,
			NULL);
		unregister_trace_workqueue_execute_end(probe_execute_end,
			NULL);
		wq_tracing &= ~WQ_DUMP_EXECUTE_WORK;
	}
}

static ssize_t
mt_wq_log_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	int ret;
	int input[3];
	char buf[64];

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = '\0';
	memset(input, 0, sizeof(input));
	ret = sscanf(buf, "%d %d %d", &input[0], &input[1], &input[2]);

	if (ret != 3) {
		print_help(NULL);
		return cnt;
	}
	mt_wq_log_config(input[0], input[1], input[2]);

	return cnt;
}

void wq_debug_dump(void)
{
	struct work_info *wi = NULL;
	struct hlist_node *tmp = NULL;
	unsigned long long ts;
	unsigned long rem_nsec;
	int i;

	pr_debug("wq_debug: %d\n", wq_debug);
	hash_for_each_safe(active_works, i, tmp, wi, hash) {
		if (wi) {
			ts = wi->ts;
			rem_nsec = do_div(ts, NSEC_PER_SEC);
			pr_debug("wq:%lx work:%lx func:%pf cpu:%u state:%s ts:%lu.%06lu\n",
				(unsigned long)wi->pwq,
				(unsigned long)wi->work,
				(void *)wi->func,
				(unsigned int)wi->cpu,
				wi->state?"exec":"queue",
				(unsigned long)ts, rem_nsec / NSEC_PER_USEC);
		}
	}
}

MT_DEBUG_ENTRY(wq_debug);
static int mt_wq_debug_show(struct seq_file *m, void *v)
{
	struct work_info *wi = NULL;
	struct hlist_node *tmp = NULL;
	unsigned long long now, ts;
	unsigned long rem_nsec;
	unsigned long flags;
	int i;

	ts = now = sched_clock();
	rem_nsec = do_div(ts, NSEC_PER_SEC);
	SEQ_printf(m, "wq_debug: %d, now: %ld.%06lu\n",
		   wq_debug, (unsigned long)ts, rem_nsec / NSEC_PER_USEC);
	raw_spin_lock_irqsave(&works_lock, flags);
	hash_for_each_safe(active_works, i, tmp, wi, hash) {
		if (wi) {
			ts = wi->ts;
			rem_nsec = do_div(ts, NSEC_PER_SEC);
			SEQ_printf(m, "wq:%lx work:%lx func:%pf",
				(unsigned long)wi->pwq,
				(unsigned long)wi->work,
				(void *)wi->func);
			SEQ_printf(m, " state:%s ts:%ld.%06lu\n",
				wi->state?"exec":"queued",
				(unsigned long)ts, rem_nsec / NSEC_PER_USEC);
		}
	}
	raw_spin_unlock_irqrestore(&works_lock, flags);
	return 0;
}

static ssize_t
mt_wq_debug_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	int ret;
	unsigned long val;

	ret = kstrtoul_from_user(ubuf, cnt, 10, &val);
	if (ret)
		return ret;
	work_debug_enable(val);

	*data += cnt;
	return cnt;
}

#ifdef CONFIG_WQ_DEBUG_SELFTEST
static struct work_struct static_test_work;
static struct work_struct *test_work;

static void test_work_fn(struct work_struct *work)
{
	pr_debug("test_work_fn exec\n");
}

static int mt_wq_selftest_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "test facility to validate workqueue exceptions\n");
	SEQ_printf(m, "1: free an activated work\n");
	SEQ_printf(m, "2: re-initialize a queued work\n");
	return 0;
}

static ssize_t
mt_wq_selftest_write(struct file *filp, const char *ubuf, size_t cnt,
		     loff_t *data)
{
	unsigned long val;
	int ret;

	ret = kstrtoul_from_user(ubuf, cnt, 10, &val);
	if (ret)
		return ret;

	if (val == 1) {
		test_work = kzalloc(sizeof(struct work_struct),
						   GFP_ATOMIC);
		if (!test_work)
			return cnt;
		INIT_WORK(test_work, test_work_fn);
		schedule_work(test_work);
		kfree(test_work);
		pr_debug("test_work freed!\n");
	} else if (val == 2) {
		INIT_WORK(&static_test_work, test_work_fn);
		schedule_work(&static_test_work);
		INIT_WORK(&static_test_work, test_work_fn);
	}
	return cnt;
}

static int mt_wq_selftest_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_wq_selftest_show, inode->i_private);
}

static const struct file_operations mt_wq_selftest_fops = {
	.open = mt_wq_selftest_open,
	.write = mt_wq_selftest_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

static int __init init_wq_debug(void)
{
	struct proc_dir_entry *pe;

	work_info_cache = kmem_cache_create("work_info_cache",
					    sizeof(struct work_info), 0,
					    0, NULL);

	if (work_info_cache) {
		work_debug_enable(1);
		pe = proc_create("mtprof/wq_debug", 0664,
			NULL, &mt_wq_debug_fops);
		if (!pe)
			return -ENOMEM;
	}
	pe = proc_create("mtprof/wq_enable_logs", 0664,
			NULL, &mt_wq_log_fops);
	if (!pe)
		return -ENOMEM;

#ifdef CONFIG_WQ_DEBUG_SELFTEST
	pe = proc_create("mtprof/wq_selftest", 0664,
		NULL, &mt_wq_selftest_fops);
	if (!pe)
		return -ENOMEM;
#endif
	return 0;
}
device_initcall(init_wq_debug);
