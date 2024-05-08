// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (C) 2023 Xiaomi Inc.
* Authors:
*	Tianyang cao <caotianyang@xiaomi.com>
*/

#define DEBUG 1

#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/tick.h>
#include <linux/tracepoint.h>
#include <linux/vmalloc.h>
#include <linux/vmstat.h>
#include <linux/major.h>
#include <trace/events/block.h>
#include <trace/events/writeback.h>


#define BLOCKIO_MIN_VER	"3.09"



#include "mpbe.h"

/*
 * snprintf may return a value of size or "more" to indicate
 * that the output was truncated, thus be careful of "more"
 * case.
 */
#define SPREAD_PRINTF(buff, size, evt, fmt, args...) \
do { \
	if (buff && size && *(size)) { \
		unsigned long var = snprintf(*(buff), *(size), fmt, ##args); \
		if (var > 0) { \
			if (var > *(size)) \
				var = *(size); \
			*(size) -= var; \
			*(buff) += var; \
		} \
	} \
	if (evt) \
		seq_printf(evt, fmt, ##args); \
	if (!buff && !evt) { \
		pr_info(fmt, ##args); \
	} \
} while (0)



/* procfs dentries */
struct proc_dir_entry *btag_proc_root;

static int mpbe_init_procfs(void);
static bool mpbe_is_top_task(struct task_struct *task);

/* mpbe */
LIST_HEAD(mpbe_list);
spinlock_t list_lock;

/* memory block for PIDLogger */
phys_addr_t dram_start_addr;
phys_addr_t dram_end_addr;

static struct mpbe *mpbe_find_by_name(const char *name)
{
	struct mpbe *btag;

	rcu_read_lock();
	list_for_each_entry_rcu(btag, &mpbe_list, list) {
		if (!strncmp(btag->name, name, MPBE_NAME_LEN-1)) {
			rcu_read_unlock();
			return btag;
		}
	}
	rcu_read_unlock();

	return NULL;
}

struct mpbe *mpbe_find_by_type(
					enum mpbe_storage_type storage_type)
{
	struct mpbe *btag;

	rcu_read_lock();
	list_for_each_entry_rcu(btag, &mpbe_list, list) {
		if (btag->storage_type == storage_type) {
			rcu_read_unlock();
			return btag;
		}
	}
	rcu_read_unlock();

	return NULL;
}


static int mpbe_get_storage_type(struct bio *bio)
{
	int major = bio->bi_bdev->bd_disk ? MAJOR(bio_dev(bio)) : 0;

	if (major == SCSI_DISK0_MAJOR || major == BLOCK_EXT_MAJOR)
		return MPBE_STORAGE_UFS;
	else {
		pr_err("MPBE: major=%d\n",major);
		return MPBE_STORAGE_UNKNOWN;
	}
}

/*
 * pidlog: hook function for __blk_bios_map_sg()
 * rw: 0=read, 1=write
 */
static void mpbe_commit_bio(struct bio_vec *bvec, __u32 *total_len,
					__u32 *top_len)
{
	bool top;
	__s16 pid;

	pid = current->pid;
	/* Using negative pid for taks with "TOP_APP" schedtune cgroup */
	top = mpbe_is_top_task(current);
	pid = (top) ? -pid : pid;

	*total_len += bvec->bv_len;
	if (pid < 0)
		*top_len += bvec->bv_len;

	//pid = abs(pid);
	MPBE_DBG("pid=%d, total_len=%d, top_len=%d\n",pid,total_len,top_len);
}

void mpbe_commit_req(__u16 task_id, struct request *rq, bool is_sd)
{
	struct bio *bio = rq->bio;
	struct bio_vec bvec;
	struct req_iterator rq_iter;
	int type;
	bool write;
	__u32 total_len = 0;
	__u32 top_len = 0;

	MPBE_DBG("enter bio_op=%d\n",bio_op(bio));


	if (!bio)
		return;

	if (bio_op(bio) != REQ_OP_READ && bio_op(bio) != REQ_OP_WRITE)
		return;

	type = mpbe_get_storage_type(bio);
	if (type == MPBE_STORAGE_UNKNOWN)
		return;

	rq_for_each_segment(bvec, rq, rq_iter) {
		if (bvec.bv_page)
			mpbe_commit_bio(&bvec, &total_len,
						&top_len);
	}

	write  = (bio_data_dir(bio) == WRITE) ? true : false;
	mpbe_trace_add_ufs(task_id, write, total_len, top_len);

}

/* evaluate vmstat trace from global_node_page_state() */
void mpbe_vmstat_eval(struct mpbe_vmstat *vm)
{
	int cpu = 0;
	struct vm_event_state *this;

	vm->file_pages = ((global_node_page_state(NR_FILE_PAGES))
		<< (PAGE_SHIFT - 10));
	vm->file_dirty = ((global_node_page_state(NR_FILE_DIRTY))
		<< (PAGE_SHIFT - 10));
	vm->dirtied = ((global_node_page_state(NR_DIRTIED))
		<< (PAGE_SHIFT - 10));
	vm->writeback = ((global_node_page_state(NR_WRITEBACK))
		<< (PAGE_SHIFT - 10));
	vm->written = ((global_node_page_state(NR_WRITTEN))
		<< (PAGE_SHIFT - 10));

	/* file map fault */
	vm->fmflt = 0;

	for_each_online_cpu(cpu) {
		this = &per_cpu(vm_event_states, cpu);
		vm->fmflt += this->event[PGMAJFAULT];
	}
}


static __u64 mpbe_cpu_idle_time(int cpu)
{
	__u64 idle, idle_usecs = -1ULL;

	if (cpu_online(cpu))
		idle_usecs = get_cpu_idle_time_us(cpu, NULL);

	if (idle_usecs == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.idle */
		idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	else
		idle = idle_usecs * NSEC_PER_USEC;

	return idle;
}

static __u64 mpbe_cpu_iowait_time(int cpu)
{
	__u64 iowait, iowait_usecs = -1ULL;

	if (cpu_online(cpu))
		iowait_usecs = get_cpu_iowait_time_us(cpu, NULL);

	if (iowait_usecs == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.iowait */
		iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	else
		iowait = iowait_usecs * NSEC_PER_USEC;

	return iowait;
}

/* evaluate cpu trace from kcpustat_cpu() */
void mpbe_cpu_eval(struct mpbe_cpu *cpu)
{
	int i = 0;
	__u64 user, nice, system, idle, iowait, irq, softirq;

	user = nice = system = idle = iowait = irq = softirq = 0;

	for_each_possible_cpu(i) {
		user += kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice += kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system += kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle += mpbe_cpu_idle_time(i);
		iowait += mpbe_cpu_iowait_time(i);
		irq += kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq += kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
	}

	/*
	 * Use nsec instead nsec_to_clock_t temporarily because
	 * nsec_to_clock_t is not exported for modules.
	 */
	cpu->user = user;
	cpu->nice = nice;
	cpu->system = system;
	cpu->idle = idle;
	cpu->iowait = iowait;
	cpu->irq = irq;
	cpu->softirq = softirq;
}

static void mpbe_throughput_rw_eval(struct mpbe_throughput_rw *rw)
{
	__u64 usage;

	usage = rw->usage;

	do_div(usage, 1000000); /* convert ns to ms */

	if (usage && rw->size) {
		rw->speed = (rw->size) / (__u32)usage;  /* bytes/ms */
		rw->speed = (rw->speed*1000) >> 10; /* KB/s */
		rw->usage = usage;
	} else {
		rw->speed = 0;
		rw->size = 0;
		rw->usage = 0;
	}
}
/* calculate throughput */
void mpbe_throughput_eval(struct mpbe_throughput *tp)
{
	mpbe_throughput_rw_eval(&tp->r);
	mpbe_throughput_rw_eval(&tp->w);
}

/* get current trace in debugfs ring buffer */
struct mpbe_trace *mpbe_curr_trace(struct mpbe_ringtrace *rt)
{
	if (rt)
		return &rt->trace[rt->index];
	else
		return NULL;
}

/* step to next trace in debugfs ring buffer */
struct mpbe_trace *mpbe_next_trace(struct mpbe_ringtrace *rt)
{
	rt->index++;
	if (rt->index >= rt->max)
		rt->index = 0;

	return mpbe_curr_trace(rt);
}

/* clear debugfs ring buffer */
static void mpbe_clear_trace(struct mpbe_ringtrace *rt)
{
	unsigned long flags;

	spin_lock_irqsave(&rt->lock, flags);
	memset(rt->trace, 0, (sizeof(struct mpbe_trace) * rt->max));
	rt->index = 0;
	spin_unlock_irqrestore(&rt->lock, flags);
}

static void mpbe_seq_time(char **buff, unsigned long *size,
	struct seq_file *seq, __u64 time)
{
	__u32 nsec;

	nsec = do_div(time, 1000000000);
	SPREAD_PRINTF(buff, size, seq, "[%5lu.%06lu]", (unsigned long)time,
		(unsigned long)nsec/1000);
}

#define biolog_fmt "wl:%d%%,%lld,%lld,%d.vm:%lld,%lld,%lld,%lld,%lld,%lld." \
	"cpu:%llu,%llu,%llu,%llu,%llu,%llu,%llu.pid:%d,"
#define biolog_fmt_wt "wt:%d,%d,%lld."
#define biolog_fmt_rt "rt:%d,%d,%lld."


static void mpbe_seq_trace(char **buff, unsigned long *size,
	struct seq_file *seq, const char *name, struct mpbe_trace *tr)
{

	if (!(tr->flags & MPBE_TR_READY))
		return;

	if (tr->time <= 0)
		return;

	mpbe_seq_time(buff, size, seq, tr->time);
	SPREAD_PRINTF(buff, size, seq, "%s.q:%d.", name, tr->qid);

	if (tr->throughput.r.usage)
		SPREAD_PRINTF(buff, size, seq, biolog_fmt_rt,
			tr->throughput.r.speed,
			tr->throughput.r.size,
			tr->throughput.r.usage);
	if (tr->throughput.w.usage)
		SPREAD_PRINTF(buff, size, seq, biolog_fmt_wt,
			tr->throughput.w.speed,
			tr->throughput.w.size,
			tr->throughput.w.usage);

	SPREAD_PRINTF(buff, size, seq, biolog_fmt,
		tr->workload.percent,
		tr->workload.usage,
		tr->workload.period,
		tr->workload.count,
		tr->vmstat.file_pages,
		tr->vmstat.file_dirty,
		tr->vmstat.dirtied,
		tr->vmstat.writeback,
		tr->vmstat.written,
		tr->vmstat.fmflt,
		tr->cpu.user,
		tr->cpu.nice,
		tr->cpu.system,
		tr->cpu.idle,
		tr->cpu.iowait,
		tr->cpu.irq,
		tr->cpu.softirq,
		tr->pid);

	SPREAD_PRINTF(buff, size, seq, ".\n");
}

static void mpbe_seq_debug_show_ringtrace(char **buff, unsigned long *size,
	struct seq_file *seq, struct mpbe *btag)
{
	struct mpbe_ringtrace *rt = MPBE_RT(btag);
	unsigned long flags;
	int i, end;

	if (!rt)
		return;

	if (rt->index >= rt->max || rt->index < 0)
		rt->index = 0;

	SPREAD_PRINTF(buff, size, seq, "<%s: mpbe trace %s>\n",
		btag->name, BLOCKIO_MIN_VER);

	spin_lock_irqsave(&rt->lock, flags);
	end = (rt->index > 0) ? rt->index-1 : rt->max-1;
	for (i = rt->index;;) {
		mpbe_seq_trace(buff, size, seq, btag->name, &rt->trace[i]);
		if (i == end)
			break;
		i = (i >= rt->max-1) ? 0 : i+1;
	};
	spin_unlock_irqrestore(&rt->lock, flags);
}

static size_t mpbe_seq_sub_show_usedmem(char **buff, unsigned long *size,
	struct seq_file *seq, struct mpbe *btag)
{
	size_t used_mem = 0;
	size_t size_l;

	SPREAD_PRINTF(buff, size, seq, "<%s: memory usage>\n", btag->name);
	SPREAD_PRINTF(buff, size, seq, "%s mpbe: %zu bytes\n", btag->name,
		sizeof(struct mpbe));
	used_mem += sizeof(struct mpbe);

	if (MPBE_RT(btag)) {
		size_l = (sizeof(struct mpbe_trace) * MPBE_RT(btag)->max);
		SPREAD_PRINTF(buff, size, seq,
		"%s debug ring buffer: %d traces * %zu = %zu bytes\n",
			btag->name,
			MPBE_RT(btag)->max,
			sizeof(struct mpbe_trace),
			size_l);
		used_mem += size_l;
	}

	if (MPBE_CTX(btag)) {
		size_l = btag->ctx.size * btag->ctx.count;
		SPREAD_PRINTF(buff, size, seq,
			"%s queue context: %d contexts * %d = %zu bytes\n",
			btag->name,
			btag->ctx.count,
			btag->ctx.size,
			size_l);
		used_mem += size_l;
	}

	size_l = btag->ctx.mictx.nr_list * (sizeof(struct mpbe_mictx) +
			sizeof(struct mpbe_mictx_data) * btag->ctx.count);
	SPREAD_PRINTF(buff, size, seq,
		"%s mictx list: %d mictx * (%zu + %d * %zu) = %zu bytes\n",
			btag->name,
			btag->ctx.mictx.nr_list,
			sizeof(struct mpbe_mictx),
			btag->ctx.count,
			sizeof(struct mpbe_mictx_data),
			size_l);
	used_mem += size_l;

	SPREAD_PRINTF(buff, size, seq,
		"%s sub-total: %zu KB\n", btag->name, used_mem >> 10);
	return used_mem;
}

/* seq file operations */
void *mpbe_seq_debug_start(struct seq_file *seq, loff_t *pos)
{
	unsigned int idx;

	if (*pos < 0 || *pos >= 1)
		return NULL;

	idx = *pos + 1;
	return (void *) ((unsigned long) idx);
}

void *mpbe_seq_debug_next(struct seq_file *seq, void *v, loff_t *pos)
{
	unsigned int idx;

	++*pos;
	if (*pos < 0 || *pos >= 1)
		return NULL;

	idx = *pos + 1;
	return (void *) ((unsigned long) idx);
}

void mpbe_seq_debug_stop(struct seq_file *seq, void *v)
{
}

static int mpbe_seq_sub_show(struct seq_file *seq, void *v)
{
	struct mpbe *btag = seq->private;

	if (btag) {
		mpbe_seq_debug_show_ringtrace(NULL, NULL, seq, btag);
		if (btag->vops->seq_show) {
			seq_printf(seq, "<%s: context info>\n", btag->name);
			btag->vops->seq_show(NULL, NULL, seq);
		}
		mpbe_seq_sub_show_usedmem(NULL, NULL, seq, btag);
	}
	return 0;
}

static const struct seq_operations mpbe_seq_sub_ops = {
	.start  = mpbe_seq_debug_start,
	.next   = mpbe_seq_debug_next,
	.stop   = mpbe_seq_debug_stop,
	.show   = mpbe_seq_sub_show,
};

static int mpbe_sub_open(struct inode *inode, struct file *file)
{
	int rc;

	rc = seq_open(file, &mpbe_seq_sub_ops);

	if (!rc) {
		struct seq_file *m = file->private_data;
		struct dentry *entry = container_of(inode->i_dentry.first,
			struct dentry, d_u.d_alias);

		if (entry && entry->d_parent) {
			pr_notice("[MPBE] %s: %s/%s\n", __func__,
				entry->d_parent->d_name.name,
				entry->d_name.name);
			m->private = mpbe_find_by_name(
						entry->d_parent->d_name.name);
		}
	}
	return rc;
}

/* clear ringtrace */
static ssize_t mpbe_sub_write(struct file *file, const char __user *ubuf,
	size_t count, loff_t *ppos)
{
	struct seq_file *seq = file->private_data;
	struct mpbe *btag;

	if (seq && seq->private) {
		btag = seq->private;
		mpbe_clear_trace(&btag->rt);
	}
	return count;
}

static const struct proc_ops mpbe_sub_fops = {
	.proc_open		= mpbe_sub_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= seq_release,
	.proc_write		= mpbe_sub_write,
};

static void mpbe_seq_main_info(char **buff, unsigned long *size,
	struct seq_file *seq)
{
	size_t used_mem = 0;
	struct mpbe *btag;

	SPREAD_PRINTF(buff, size, seq, "[Trace]\n");
	rcu_read_lock();
	list_for_each_entry_rcu(btag, &mpbe_list, list)
		mpbe_seq_debug_show_ringtrace(buff, size, seq, btag);
	rcu_read_unlock();

	SPREAD_PRINTF(buff, size, seq, "[Info]\n");
	rcu_read_lock();
	list_for_each_entry_rcu(btag, &mpbe_list, list)
		if (btag->vops->seq_show) {
			SPREAD_PRINTF(buff, size, seq, "<%s: context info>\n",
					btag->name);
			btag->vops->seq_show(buff, size, seq);
		}
	rcu_read_unlock();


	SPREAD_PRINTF(buff, size, seq, "[Memory Usage]\n");
	rcu_read_lock();
	list_for_each_entry_rcu(btag, &mpbe_list, list)
		used_mem += mpbe_seq_sub_show_usedmem(buff, size,
				seq, btag);
	rcu_read_unlock();

	SPREAD_PRINTF(buff, size, seq, "earaio control unit: %lu bytes\n",
			sizeof(struct mpbe_earaio_control));
	used_mem += sizeof(struct mpbe_earaio_control);

	SPREAD_PRINTF(buff, size, seq, "--------------------------------\n");
	SPREAD_PRINTF(buff, size, seq, "Total: %zu KB\n", used_mem >> 10);
}

static int mpbe_seq_main_show(struct seq_file *seq, void *v)
{
	mpbe_seq_main_info(NULL, NULL, seq);
	return 0;
}

static const struct seq_operations mpbe_seq_main_ops = {
	.start  = mpbe_seq_debug_start,
	.next   = mpbe_seq_debug_next,
	.stop   = mpbe_seq_debug_stop,
	.show   = mpbe_seq_main_show,
};

static int mpbe_main_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &mpbe_seq_main_ops);
}

/* clear all ringtraces */
static ssize_t mpbe_main_write(struct file *file, const char __user *ubuf,
	size_t count, loff_t *ppos)
{
	struct mpbe *btag;

	rcu_read_lock();
	list_for_each_entry_rcu(btag, &mpbe_list, list)
		mpbe_clear_trace(&btag->rt);
	rcu_read_unlock();

	return count;
}

static const struct proc_ops mpbe_main_fops = {
	.proc_open		= mpbe_main_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= seq_release,
	.proc_write		= mpbe_main_write,
};

struct mpbe *mpbe_alloc(const char *name,
	enum mpbe_storage_type storage_type,
	__u32 ringtrace_count, size_t ctx_size, __u32 ctx_count,
	struct mpbe_vops *vops)
{
	struct mpbe *btag;
	unsigned long flags;

	if (!name || !ringtrace_count || !ctx_size || !ctx_count)
		return NULL;

	btag = mpbe_find_by_type(storage_type);
	if (btag) {
		pr_notice("[MPBE] %s: mpbe %s already exists.\n",
			__func__, name);
		return NULL;
	}

	btag = kmalloc(sizeof(struct mpbe), GFP_NOFS);
	if (!btag)
		return NULL;

	memset(btag, 0, sizeof(struct mpbe));

	/* ringtrace */
	btag->rt.index = 0;
	btag->rt.max = ringtrace_count;
	spin_lock_init(&btag->rt.lock);
	btag->rt.trace = kcalloc(ringtrace_count,
		sizeof(struct mpbe_trace), GFP_NOFS);
	if (!btag->rt.trace) {
		kfree(btag);
		return NULL;
	}
	strncpy(btag->name, name, MPBE_NAME_LEN-1);
	btag->storage_type = storage_type;

	/* context */
	btag->ctx.count = ctx_count;
	btag->ctx.size = ctx_size;
	btag->ctx.priv = kcalloc(ctx_count, ctx_size, GFP_NOFS);
	if (!btag->ctx.priv) {
		kfree(btag->rt.trace);
		kfree(btag);
		return NULL;
	}

	/* vops */
	btag->vops = vops;

	/* procfs dentries */
	mpbe_init_procfs();
	btag->dentry.droot = proc_mkdir(name, btag_proc_root);

	if (IS_ERR(btag->dentry.droot))
		goto out;

	btag->dentry.dlog = proc_create("blockio", S_IFREG | 0444,
		btag->dentry.droot, &mpbe_sub_fops);

	if (IS_ERR(btag->dentry.dlog))
		goto out;

	mpbe_mictx_init(btag);
out:
	spin_lock_irqsave(&list_lock, flags);
	list_add_rcu(&btag->list, &mpbe_list);
	spin_unlock_irqrestore(&list_lock, flags);
	mpbe_earaio_init_mictx(vops, storage_type, btag_proc_root);

	return btag;
}

void mpbe_free(struct mpbe *btag)
{
	unsigned long flags;

	if (!btag)
		return;

	spin_lock_irqsave(&list_lock, flags);
	list_del_rcu(&btag->list);
	spin_unlock_irqrestore(&list_lock, flags);

	synchronize_rcu();
	mpbe_mictx_free_all(btag);
	kfree(btag->ctx.priv);
	kfree(btag->rt.trace);
	proc_remove(btag->dentry.droot);
	kfree(btag);
}

#if IS_ENABLED(CONFIG_CGROUP_SCHED)
bool mpbe_is_top_in_cgrp(struct task_struct *t)
{
	struct cgroup *grp;

	rcu_read_lock();
	grp = task_cgroup(t, cpuset_cgrp_id);
	rcu_read_unlock();

	if (grp->kn->name && !strcmp("top-app", grp->kn->name))
		return true;
	else
		return false;
}

static bool mpbe_is_top_task(struct task_struct *task)
{
	struct task_struct *t_tgid = NULL;

	if (mpbe_is_top_in_cgrp(task))
		return true;

	if (task->tgid && task->tgid != task->pid) {
		rcu_read_lock();
		t_tgid = find_task_by_vpid(task->tgid);
		rcu_read_unlock();
		if (t_tgid) {
			if (mpbe_is_top_in_cgrp(t_tgid))
				return true;
		}
	}

	return false;
}

#else
static inline bool mpbe_is_top_task(struct task_struct *task)
{
	return false;
}
#endif
struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool init;
};

static struct tracepoints_table interests[] = {
	/*{
		.name = "android_vh_ufs_transfer_req_send",
		.func = mpbe_ufs_send_command
	},*/
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(interests) / sizeof(struct tracepoints_table); \
	i++)

static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(interests[i].name, tp->name) == 0)
			interests[i].tp = tp;
	}
}

static void mpbe_uninstall_tracepoints(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (interests[i].init) {
			tracepoint_probe_unregister(interests[i].tp,
						    interests[i].func,
						    NULL);
		}
	}
}

static int mpbe_install_tracepoints(void)
{
	int i;

	/* Install the tracepoints */
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (interests[i].tp == NULL) {
			pr_info("Error: %s not found\n",
				interests[i].name);
			/* Unload previously loaded */
			mpbe_uninstall_tracepoints();
			return -EINVAL;
		}

		tracepoint_probe_register(interests[i].tp,
					  interests[i].func,
					  NULL);
		interests[i].init = true;
	}

	return 0;
}


static int mpbe_init_procfs(void)
{
	struct proc_dir_entry *proc_entry;
	kuid_t uid;
	kgid_t gid;

	if (btag_proc_root)
		return 0;

	uid = make_kuid(&init_user_ns, 0);
	gid = make_kgid(&init_user_ns, 1001);

	btag_proc_root = proc_mkdir("mpbe", NULL);

	proc_entry = proc_create("blockio", S_IFREG | 0444, btag_proc_root,
			      &mpbe_main_fops);

	if (proc_entry)
		proc_set_user(proc_entry, uid, gid);
	else
		pr_info("[MPBE} %s: failed to initialize procfs", __func__);

	return 0;
}


static int __init mpbe_init(void)
{
	spin_lock_init(&list_lock);

	mpbe_init_procfs();
	mpbe_ufs_init();
	mpbe_earaio_init();
	mpbe_install_tracepoints();

	return 0;
}

static void __exit mpbe_exit(void)
{

	proc_remove(btag_proc_root);
	mpbe_uninstall_tracepoints();
}

module_init(mpbe_init);
module_exit(mpbe_exit);

MODULE_AUTHOR("Tianyang Cao <caotianyang@xiaomi.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Storage Block IO Tracer");

