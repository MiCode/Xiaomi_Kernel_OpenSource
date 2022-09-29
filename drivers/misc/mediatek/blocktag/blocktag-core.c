// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 * Authors:
 *	Perry Hsu <perry.hsu@mediatek.com>
 *	Stanley Chu <stanley.chu@mediatek.com>
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
#include <trace/events/block.h>
#include <trace/events/writeback.h>

#define BLOCKIO_MIN_VER	"3.09"

#if IS_ENABLED(CONFIG_MTK_USE_RESERVED_EXT_MEM)
#include <linux/exm_driver.h>
#endif

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
#include <mrdump.h>
#endif

#include "mtk_blocktag.h"
#if IS_ENABLED(CONFIG_MTK_FSCMD_TRACER)
#include "fscmd-trace.h"
#endif
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_PM_DEBUG)
#include "blocktag-pm-trace.h"
#endif

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

#define mtk_btag_pidlog_index(p) \
	((unsigned long)(__page_to_pfn(p)) - \
	(dram_start_addr >> PAGE_SHIFT))

#define mtk_btag_pidlog_max_entry() \
	(mtk_btag_system_dram_size >> PAGE_SHIFT)

#define mtk_btag_pidlog_entry(idx) \
	(((struct page_pidlogger *)mtk_btag_pagelogger) + idx)

/* max dump size is 300KB which can be adjusted */
#define BLOCKIO_AEE_BUFFER_SIZE (300 * 1024)
char *blockio_aee_buffer;

/* procfs dentries */
struct proc_dir_entry *btag_proc_root;

static int mtk_btag_init_procfs(void);

/* blocktag */
LIST_HEAD(mtk_btag_list);
spinlock_t list_lock;

/* memory block for PIDLogger */
phys_addr_t dram_start_addr;
phys_addr_t dram_end_addr;

static struct mtk_blocktag *mtk_btag_find_by_name(const char *name)
{
	struct mtk_blocktag *btag, *n;
	unsigned long flags;

	spin_lock_irqsave(&list_lock, flags);
	list_for_each_entry_safe(btag, n, &mtk_btag_list, list) {
		if (!strncmp(btag->name, name, BLOCKTAG_NAME_LEN-1)) {
			spin_unlock_irqrestore(&list_lock, flags);
			return btag;
		}
	}
	spin_unlock_irqrestore(&list_lock, flags);

	return NULL;
}

struct mtk_blocktag *mtk_btag_find_by_type(
					enum mtk_btag_storage_type storage_type)
{
	struct mtk_blocktag *btag, *n;
	unsigned long flags;

	spin_lock_irqsave(&list_lock, flags);
	list_for_each_entry_safe(btag, n, &mtk_btag_list, list) {
		if (btag->storage_type == storage_type) {
			spin_unlock_irqrestore(&list_lock, flags);
			return btag;
		}
	}
	spin_unlock_irqrestore(&list_lock, flags);

	return NULL;
}

/* pid logger: page loger*/
unsigned long long mtk_btag_system_dram_size;
struct page_pidlogger *mtk_btag_pagelogger;

static size_t mtk_btag_seq_pidlog_usedmem(char **buff, unsigned long *size,
	struct seq_file *seq)
{
	size_t size_l = 0;

	if (!IS_ERR_OR_NULL(mtk_btag_pagelogger)) {
		size_l = (sizeof(struct page_pidlogger)
			* (mtk_btag_system_dram_size >> PAGE_SHIFT));
		SPREAD_PRINTF(buff, size, seq,
		"page pid logger buffer: %llu entries * %zu = %zu bytes\n",
			(mtk_btag_system_dram_size >> PAGE_SHIFT),
			sizeof(struct page_pidlogger),
			size_l);
	}
	return size_l;
}

void mtk_btag_pidlog_insert(struct mtk_btag_proc_pidlogger *pidlog, bool write,
				struct tmp_proc_pidlogger *tmplog)
{
	struct mtk_btag_proc_pidlogger_entry *pe;
	struct mtk_btag_proc_pidlogger_entry_rw *prw;
	struct tmp_proc_pidlogger_entry *te;
	int pe_idx, te_idx;
	int last_te_idx = BLOCKTAG_PIDLOG_ENTRIES - 1;

	for (; last_te_idx >= 0; last_te_idx--)
		if (tmplog->info[last_te_idx].pid)
			break;

	for (te_idx = 0; te_idx <= last_te_idx; te_idx++) {
		te = &tmplog->info[te_idx];
		for (pe_idx = 0; pe_idx < BLOCKTAG_PIDLOG_ENTRIES; pe_idx++) {
			pe = &pidlog->info[pe_idx];
			if (te->pid == pe->pid || pe->pid == 0) {
				pe->pid = te->pid;
				prw = (write) ? &pe->w : &pe->r;
				prw->count++;
				prw->length += te->length;
				break;
			}
		}
	}
}

static int mtk_btag_get_storage_type(struct bio *bio)
{
	int major = bio->bi_bdev->bd_disk ? MAJOR(bio_dev(bio)) : 0;

	if (major == SCSI_DISK0_MAJOR || major == BLOCK_EXT_MAJOR)
		return BTAG_STORAGE_UFS;
	else if (major == MMC_BLOCK_MAJOR || major == BLOCK_EXT_MAJOR)
		return BTAG_STORAGE_MMC;
	else
		return BTAG_STORAGE_UNKNOWN;
}

/*
 * pidlog: hook function for __blk_bios_map_sg()
 * rw: 0=read, 1=write
 */
static void mtk_btag_pidlog_commit_bio(struct bio_vec *bvec, __u32 *total_len,
					__u32 *top_len,
					struct tmp_proc_pidlogger *tmplog)
{
	struct page_pidlogger *ppl;
	__u32 idx;
	__s16 pid;
	int i;
	struct tmp_proc_pidlogger_entry *te;

	idx = mtk_btag_pidlog_index(bvec->bv_page);
	ppl = mtk_btag_pidlog_entry(idx);
	pid = ppl->pid;
	ppl->pid = 0;

	*total_len += bvec->bv_len;
	if (pid < 0)
		*top_len += bvec->bv_len;

	pid = abs(pid);
	for (i = 0; i < BLOCKTAG_PIDLOG_ENTRIES; i++) {
		te = &tmplog->info[i];
		if ((te->pid == pid) || (te->pid == 0)) {
			te->pid = pid;
			te->length += bvec->bv_len;
			break;
		}
	}
}

void mtk_btag_commit_req(__u16 task_id, struct request *rq, bool is_sd)
{
	struct bio *bio = rq->bio;
	struct bio_vec bvec;
	struct req_iterator rq_iter;
	int type;
	bool write;
	struct tmp_proc_pidlogger tmplog;
	__u32 total_len = 0;
	__u32 top_len = 0;

	if (unlikely(!mtk_btag_pagelogger) || !bio)
		return;

	if (bio_op(bio) != REQ_OP_READ && bio_op(bio) != REQ_OP_WRITE)
		return;

	type = mtk_btag_get_storage_type(bio);
	if (type == BTAG_STORAGE_UNKNOWN)
		return;

	memset(&tmplog, 0, sizeof(struct tmp_proc_pidlogger));
	rq_for_each_segment(bvec, rq, rq_iter) {
		if (bvec.bv_page)
			mtk_btag_pidlog_commit_bio(&bvec, &total_len,
						&top_len, &tmplog);
	}

	write  = (bio_data_dir(bio) == WRITE) ? true : false;
	if (type == BTAG_STORAGE_UFS)
		mtk_btag_pidlog_add_ufs(task_id, write, total_len, top_len,
					&tmplog);
	else if (type == BTAG_STORAGE_MMC)
		mtk_btag_pidlog_add_mmc(is_sd, write, total_len, top_len,
					&tmplog);
}

/* evaluate vmstat trace from global_node_page_state() */
void mtk_btag_vmstat_eval(struct mtk_btag_vmstat *vm)
{
	int cpu;
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

/* evaluate pidlog trace from context */
void mtk_btag_pidlog_eval(struct mtk_btag_proc_pidlogger *pl,
	struct mtk_btag_proc_pidlogger *ctx_pl)
{
	int i;

	for (i = 0; i < BLOCKTAG_PIDLOG_ENTRIES; i++) {
		if (ctx_pl->info[i].pid == 0)
			break;
	}

	if (i != 0) {
		int size = i * sizeof(struct mtk_btag_proc_pidlogger_entry);

		memcpy(&pl->info[0], &ctx_pl->info[0], size);
		memset(&ctx_pl->info[0], 0, size);
	}
}

static __u64 mtk_btag_cpu_idle_time(int cpu)
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

static __u64 mtk_btag_cpu_iowait_time(int cpu)
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
void mtk_btag_cpu_eval(struct mtk_btag_cpu *cpu)
{
	int i;
	__u64 user, nice, system, idle, iowait, irq, softirq;

	user = nice = system = idle = iowait = irq = softirq = 0;

	for_each_possible_cpu(i) {
		user += kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice += kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system += kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle += mtk_btag_cpu_idle_time(i);
		iowait += mtk_btag_cpu_iowait_time(i);
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

static void mtk_btag_throughput_rw_eval(struct mtk_btag_throughput_rw *rw)
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
void mtk_btag_throughput_eval(struct mtk_btag_throughput *tp)
{
	mtk_btag_throughput_rw_eval(&tp->r);
	mtk_btag_throughput_rw_eval(&tp->w);
}

/* get current trace in debugfs ring buffer */
struct mtk_btag_trace *mtk_btag_curr_trace(struct mtk_btag_ringtrace *rt)
{
	if (rt)
		return &rt->trace[rt->index];
	else
		return NULL;
}

/* step to next trace in debugfs ring buffer */
struct mtk_btag_trace *mtk_btag_next_trace(struct mtk_btag_ringtrace *rt)
{
	rt->index++;
	if (rt->index >= rt->max)
		rt->index = 0;

	return mtk_btag_curr_trace(rt);
}

/* clear debugfs ring buffer */
static void mtk_btag_clear_trace(struct mtk_btag_ringtrace *rt)
{
	unsigned long flags;

	spin_lock_irqsave(&rt->lock, flags);
	memset(rt->trace, 0, (sizeof(struct mtk_btag_trace) * rt->max));
	rt->index = 0;
	spin_unlock_irqrestore(&rt->lock, flags);
}

static void mtk_btag_seq_time(char **buff, unsigned long *size,
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
#define pidlog_fmt "{%05d:%05d:%08d:%05d:%08d}"

static void mtk_btag_seq_trace(char **buff, unsigned long *size,
	struct seq_file *seq, const char *name, struct mtk_btag_trace *tr)
{
	int i;

	if (tr->time <= 0)
		return;

	mtk_btag_seq_time(buff, size, seq, tr->time);
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

	for (i = 0; i < BLOCKTAG_PIDLOG_ENTRIES; i++) {
		struct mtk_btag_proc_pidlogger_entry *pe;

		pe = &tr->pidlog.info[i];

		if (pe->pid == 0)
			break;

		SPREAD_PRINTF(buff, size, seq, pidlog_fmt,
			pe->pid,
			pe->w.count,
			pe->w.length,
			pe->r.count,
			pe->r.length);
	}
	SPREAD_PRINTF(buff, size, seq, ".\n");
}

static void mtk_btag_seq_debug_show_ringtrace(char **buff, unsigned long *size,
	struct seq_file *seq, struct mtk_blocktag *btag)
{
	struct mtk_btag_ringtrace *rt = BTAG_RT(btag);
	unsigned long flags;
	int i, end;

	if (!rt)
		return;

	if (rt->index >= rt->max || rt->index < 0)
		rt->index = 0;

	SPREAD_PRINTF(buff, size, seq, "<%s: blocktag trace %s>\n",
		btag->name, BLOCKIO_MIN_VER);

	spin_lock_irqsave(&rt->lock, flags);
	end = (rt->index > 0) ? rt->index-1 : rt->max-1;
	for (i = rt->index;;) {
		mtk_btag_seq_trace(buff, size, seq, btag->name, &rt->trace[i]);
		if (i == end)
			break;
		i = (i >= rt->max-1) ? 0 : i+1;
	};
	spin_unlock_irqrestore(&rt->lock, flags);
}

static size_t mtk_btag_seq_sub_show_usedmem(char **buff, unsigned long *size,
	struct seq_file *seq, struct mtk_blocktag *btag)
{
	size_t used_mem = 0;
	size_t size_l;

	SPREAD_PRINTF(buff, size, seq, "<%s: memory usage>\n", btag->name);
	SPREAD_PRINTF(buff, size, seq, "%s blocktag: %zu bytes\n", btag->name,
		sizeof(struct mtk_blocktag));
	used_mem += sizeof(struct mtk_blocktag);

	if (BTAG_RT(btag)) {
		size_l = (sizeof(struct mtk_btag_trace) * BTAG_RT(btag)->max);
		SPREAD_PRINTF(buff, size, seq,
		"%s debug ring buffer: %d traces * %zu = %zu bytes\n",
			btag->name,
			BTAG_RT(btag)->max,
			sizeof(struct mtk_btag_trace),
			size_l);
		used_mem += size_l;
	}

	if (BTAG_CTX(btag)) {
		size_l = btag->ctx.size * btag->ctx.count;
		SPREAD_PRINTF(buff, size, seq,
			"%s queue context: %d contexts * %d = %zu bytes\n",
			btag->name,
			btag->ctx.count,
			btag->ctx.size,
			size_l);
		used_mem += size_l;
	}

	size_l = btag->ctx.mictx.nr_list * (sizeof(struct mtk_btag_mictx) +
			sizeof(struct mtk_btag_mictx_data) * btag->ctx.count);
	SPREAD_PRINTF(buff, size, seq,
		"%s mictx list: %d mictx * (%zu + %d * %zu) = %zu bytes\n",
			btag->name,
			btag->ctx.mictx.nr_list,
			sizeof(struct mtk_btag_mictx),
			btag->ctx.count,
			sizeof(struct mtk_btag_mictx_data),
			size_l);
	used_mem += size_l;

	SPREAD_PRINTF(buff, size, seq,
		"%s sub-total: %zu KB\n", btag->name, used_mem >> 10);
	return used_mem;
}

/* seq file operations */
void *mtk_btag_seq_debug_start(struct seq_file *seq, loff_t *pos)
{
	unsigned int idx;

	if (*pos < 0 || *pos >= 1)
		return NULL;

	idx = *pos + 1;
	return (void *) ((unsigned long) idx);
}

void *mtk_btag_seq_debug_next(struct seq_file *seq, void *v, loff_t *pos)
{
	unsigned int idx;

	++*pos;
	if (*pos < 0 || *pos >= 1)
		return NULL;

	idx = *pos + 1;
	return (void *) ((unsigned long) idx);
}

void mtk_btag_seq_debug_stop(struct seq_file *seq, void *v)
{
}

static int mtk_btag_seq_sub_show(struct seq_file *seq, void *v)
{
	struct mtk_blocktag *btag = seq->private;

	if (btag) {
		mtk_btag_seq_debug_show_ringtrace(NULL, NULL, seq, btag);
		if (btag->vops->seq_show) {
			seq_printf(seq, "<%s: context info>\n", btag->name);
			btag->vops->seq_show(NULL, NULL, seq);
		}
		mtk_btag_seq_sub_show_usedmem(NULL, NULL, seq, btag);
	}
	return 0;
}

static const struct seq_operations mtk_btag_seq_sub_ops = {
	.start  = mtk_btag_seq_debug_start,
	.next   = mtk_btag_seq_debug_next,
	.stop   = mtk_btag_seq_debug_stop,
	.show   = mtk_btag_seq_sub_show,
};

static int mtk_btag_sub_open(struct inode *inode, struct file *file)
{
	int rc;

	rc = seq_open(file, &mtk_btag_seq_sub_ops);

	if (!rc) {
		struct seq_file *m = file->private_data;
		struct dentry *entry = container_of(inode->i_dentry.first,
			struct dentry, d_u.d_alias);

		if (entry && entry->d_parent) {
			pr_notice("[BLOCK_TAG] %s: %s/%s\n", __func__,
				entry->d_parent->d_name.name,
				entry->d_name.name);
			m->private = mtk_btag_find_by_name(
						entry->d_parent->d_name.name);
		}
	}
	return rc;
}

/* clear ringtrace */
static ssize_t mtk_btag_sub_write(struct file *file, const char __user *ubuf,
	size_t count, loff_t *ppos)
{
	struct seq_file *seq = file->private_data;
	struct mtk_blocktag *btag;

	if (seq && seq->private) {
		btag = seq->private;
		mtk_btag_clear_trace(&btag->rt);
	}
	return count;
}

static const struct proc_ops mtk_btag_sub_fops = {
	.proc_open		= mtk_btag_sub_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= seq_release,
	.proc_write		= mtk_btag_sub_write,
};

static void mtk_btag_seq_main_info(char **buff, unsigned long *size,
	struct seq_file *seq)
{
	size_t used_mem = 0;
	struct mtk_blocktag *btag, *n;
	unsigned long flags;

	SPREAD_PRINTF(buff, size, seq, "[Trace]\n");
	spin_lock_irqsave(&list_lock, flags);
	list_for_each_entry_safe(btag, n, &mtk_btag_list, list)
		mtk_btag_seq_debug_show_ringtrace(buff, size, seq, btag);
	spin_unlock_irqrestore(&list_lock, flags);

	SPREAD_PRINTF(buff, size, seq, "[Info]\n");
	spin_lock_irqsave(&list_lock, flags);
	list_for_each_entry_safe(btag, n, &mtk_btag_list, list)
		if (btag->vops->seq_show) {
			SPREAD_PRINTF(buff, size, seq, "<%s: context info>\n",
					btag->name);
			btag->vops->seq_show(buff, size, seq);
		}
	spin_unlock_irqrestore(&list_lock, flags);

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_PM_DEBUG)
	SPREAD_PRINTF(buff, size, seq, "[BLK_PM]\n");
	mtk_btag_blk_pm_show(buff, size, seq);
#endif
#if IS_ENABLED(CONFIG_MTK_FSCMD_TRACER)
	if (!buff) {
		SPREAD_PRINTF(buff, size, seq, "[FS_CMD]\n");
		mtk_fscmd_show(buff, size, seq);
	}
#endif
	SPREAD_PRINTF(buff, size, seq, "[Memory Usage]\n");
	list_for_each_entry_safe(btag, n, &mtk_btag_list, list)
		used_mem += mtk_btag_seq_sub_show_usedmem(buff, size,
				seq, btag);

	SPREAD_PRINTF(buff, size, seq, "<blocktag core>\n");
	used_mem += mtk_btag_seq_pidlog_usedmem(buff, size, seq);

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_PM_DEBUG)
	SPREAD_PRINTF(buff, size, seq, "blk pm log: %d bytes\n",
			sizeof(struct blk_pm_logs_s));
	used_mem += sizeof(struct blk_pm_logs_s);
#endif

#if IS_ENABLED(CONFIG_MTK_FSCMD_TRACER)
	SPREAD_PRINTF(buff, size, seq, "fscmd history: %d bytes\n",
			mtk_fscmd_used_mem());
	used_mem += mtk_fscmd_used_mem();
#endif

	SPREAD_PRINTF(buff, size, seq, "aee buffer: %d bytes\n",
			BLOCKIO_AEE_BUFFER_SIZE);
	used_mem += BLOCKIO_AEE_BUFFER_SIZE;

	SPREAD_PRINTF(buff, size, seq, "earaio control unit: %d bytes\n",
			sizeof(struct mtk_btag_earaio_control));
	used_mem += sizeof(struct mtk_btag_earaio_control);

	SPREAD_PRINTF(buff, size, seq, "--------------------------------\n");
	SPREAD_PRINTF(buff, size, seq, "Total: %zu KB\n", used_mem >> 10);
}

static int mtk_btag_seq_main_show(struct seq_file *seq, void *v)
{
	mtk_btag_seq_main_info(NULL, NULL, seq);
	return 0;
}

static const struct seq_operations mtk_btag_seq_main_ops = {
	.start  = mtk_btag_seq_debug_start,
	.next   = mtk_btag_seq_debug_next,
	.stop   = mtk_btag_seq_debug_stop,
	.show   = mtk_btag_seq_main_show,
};

static int mtk_btag_main_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &mtk_btag_seq_main_ops);
}

/* clear all ringtraces */
static ssize_t mtk_btag_main_write(struct file *file, const char __user *ubuf,
	size_t count, loff_t *ppos)
{
	struct mtk_blocktag *btag, *n;
	unsigned long flags;

	spin_lock_irqsave(&list_lock, flags);
	list_for_each_entry_safe(btag, n, &mtk_btag_list, list)
		mtk_btag_clear_trace(&btag->rt);
	spin_unlock_irqrestore(&list_lock, flags);

	return count;
}

static const struct proc_ops mtk_btag_main_fops = {
	.proc_open		= mtk_btag_main_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= seq_release,
	.proc_write		= mtk_btag_main_write,
};

struct mtk_blocktag *mtk_btag_alloc(const char *name,
	enum mtk_btag_storage_type storage_type,
	__u32 ringtrace_count, size_t ctx_size, __u32 ctx_count,
	struct mtk_btag_vops *vops)
{
	struct mtk_blocktag *btag;
	unsigned long flags;

	if (!name || !ringtrace_count || !ctx_size || !ctx_count)
		return NULL;

	btag = mtk_btag_find_by_type(storage_type);
	if (btag) {
		pr_notice("[BLOCK_TAG] %s: blocktag %s already exists.\n",
			__func__, name);
		return NULL;
	}

	btag = kmalloc(sizeof(struct mtk_blocktag), GFP_NOFS);
	if (!btag)
		return NULL;

	memset(btag, 0, sizeof(struct mtk_blocktag));

	/* ringtrace */
	btag->rt.index = 0;
	btag->rt.max = ringtrace_count;
	spin_lock_init(&btag->rt.lock);
	btag->rt.trace = kcalloc(ringtrace_count,
		sizeof(struct mtk_btag_trace), GFP_NOFS);
	if (!btag->rt.trace) {
		kfree(btag);
		return NULL;
	}
	strncpy(btag->name, name, BLOCKTAG_NAME_LEN-1);
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
	mtk_btag_init_procfs();
	btag->dentry.droot = proc_mkdir(name, btag_proc_root);

	if (IS_ERR(btag->dentry.droot))
		goto out;

	btag->dentry.dlog = proc_create("blockio", S_IFREG | 0444,
		btag->dentry.droot, &mtk_btag_sub_fops);

	if (IS_ERR(btag->dentry.dlog))
		goto out;

	mtk_btag_mictx_init(btag);
out:
	spin_lock_irqsave(&list_lock, flags);
	list_add(&btag->list, &mtk_btag_list);
	spin_unlock_irqrestore(&list_lock, flags);
	mtk_btag_earaio_init_mictx(vops, storage_type, btag_proc_root);

	return btag;
}

void mtk_btag_free(struct mtk_blocktag *btag)
{
	unsigned long flags;

	if (!btag)
		return;

	spin_lock_irqsave(&list_lock, flags);
	list_del(&btag->list);
	mtk_btag_mictx_free_all(btag);
	spin_unlock_irqrestore(&list_lock, flags);

	kfree(btag->ctx.priv);
	kfree(btag->rt.trace);
	proc_remove(btag->dentry.droot);
	kfree(btag);
}

#if IS_ENABLED(CONFIG_CGROUP_SCHED)
static bool mtk_btag_is_top_in_cgrp(struct task_struct *t)
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

static bool mtk_btag_is_top_task(struct task_struct *task, int mode)
{
	struct task_struct *t_tgid = NULL;

	if (mtk_btag_is_top_in_cgrp(task))
		return true;

	if (task->tgid && task->tgid != task->pid) {
		rcu_read_lock();
		t_tgid = find_task_by_vpid(task->tgid);
		rcu_read_unlock();
		if (t_tgid) {
			if (mtk_btag_is_top_in_cgrp(t_tgid))
				return true;
		}
	}

	return false;
}

#else
static inline bool mtk_btag_is_top_task(struct task_struct *task,
					int mode)
{
	return false;
}
#endif

static void mtk_btag_pidlog_set_pid(struct page *p, int mode, bool write)
{
	struct page_pidlogger *ppl;
	short pid = current->pid;
	unsigned long idx;
	bool top;

	idx = mtk_btag_pidlog_index(p);
	if (idx >= mtk_btag_pidlog_max_entry())
		return;
	ppl = mtk_btag_pidlog_entry(idx);

	/* Using negative pid for taks with "TOP_APP" schedtune cgroup */
	top = mtk_btag_is_top_task(current, mode);
	pid = (top) ? -pid : pid;

	/* we do lockless operation here to favor performance */

	if (mode == PIDLOG_MODE_MM_MARK_DIRTY) {
		/* keep real requester anyway */
		ppl->pid = pid;
	} else {
		/* do not overwrite the real owner set before */
		if (ppl->pid == 0)
			ppl->pid = pid;
	}
}

static void btag_trace_block_bio_queue(void *data, struct bio *bio)
{
	struct bvec_iter iter;
	struct bio_vec bvec;
	int type;

	if (unlikely(!mtk_btag_pagelogger) || !bio)
		return;

	if (bio_op(bio) != REQ_OP_READ && bio_op(bio) != REQ_OP_WRITE)
		return;

	type = mtk_btag_get_storage_type(bio);
	if (type == BTAG_STORAGE_UNKNOWN)
		return;

	for_each_bio(bio) {
		bio_for_each_segment(bvec, bio, iter) {
			if (bvec.bv_page) {
				mtk_btag_pidlog_set_pid(bvec.bv_page,
					PIDLOG_MODE_BLK_BIO_QUEUE,
					(bio_op(bio) == REQ_OP_WRITE) ?
					true : false);
			}
		}
	}
}

static void btag_trace_writeback_dirty_page(void *data,
					struct page *page,
					struct address_space *mapping)
{
	/*
	 * Dirty pages may be written by writeback thread later.
	 * To get real requester of this page, we shall keep it
	 * before writeback takes over.
	 */
	if (unlikely(!mtk_btag_pagelogger) || unlikely(!page))
		return;

	mtk_btag_pidlog_set_pid(page, PIDLOG_MODE_MM_MARK_DIRTY, true);

}

struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool init;
};

static struct tracepoints_table interests[] = {
	{
		.name = "block_bio_queue",
		.func = btag_trace_block_bio_queue
	},
	{
		.name = "writeback_dirty_page",
		.func = btag_trace_writeback_dirty_page
	},
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_PM_DEBUG)
	{
		.name = "blk_pre_runtime_suspend_start",
		.func = btag_blk_pre_runtime_suspend_start
	},
	{
		.name = "blk_pre_runtime_suspend_end",
		.func = btag_blk_pre_runtime_suspend_end
	},
	{
		.name = "blk_post_runtime_suspend_start",
		.func = btag_blk_post_runtime_suspend_start
	},
	{
		.name = "blk_post_runtime_suspend_end",
		.func = btag_blk_post_runtime_suspend_end
	},
	{
		.name = "blk_pre_runtime_resume_start",
		.func = btag_blk_pre_runtime_resume_start
	},
	{
		.name = "blk_pre_runtime_resume_end",
		.func = btag_blk_pre_runtime_resume_end
	},
	{
		.name = "blk_post_runtime_resume_start",
		.func = btag_blk_post_runtime_resume_start
	},
	{
		.name = "blk_post_runtime_resume_end",
		.func = btag_blk_post_runtime_resume_end
	},
	{
		.name = "blk_set_runtime_active_start",
		.func = btag_blk_set_runtime_active_start
	},
	{
		.name = "blk_set_runtime_active_end",
		.func = btag_blk_set_runtime_active_end
	},
	{
		.name = "blk_queue_enter_sleep",
		.func = btag_blk_queue_enter_sleep
	},
	{
		.name = "blk_queue_enter_wakeup",
		.func = btag_blk_queue_enter_wakeup
	},
	{
		.name = "bio_queue_enter_sleep",
		.func = btag_bio_queue_enter_sleep
	},
	{
		.name = "bio_queue_enter_wakeup",
		.func = btag_bio_queue_enter_wakeup
	},
#endif
#if IS_ENABLED(CONFIG_MTK_FSCMD_TRACER)
	{
		.name = "sys_enter",
		.func = fscmd_trace_sys_enter
	},
	{
		.name = "sys_exit",
		.func = fscmd_trace_sys_exit
	},
	{
		.name = "f2fs_write_checkpoint",
		.func = fscmd_trace_f2fs_write_checkpoint
	},
	{
		.name = "f2fs_gc_begin",
		.func = fscmd_trace_f2fs_gc_begin
	},
	{
		.name = "f2fs_gc_end",
		.func = fscmd_trace_f2fs_gc_end
	},
	{
		.name = "f2fs_ck_rwsem",
		.func = fscmd_trace_f2fs_ck_rwsem
	},
#endif
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

static void mtk_btag_uninstall_tracepoints(void)
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

static int mtk_btag_install_tracepoints(void)
{
	int i;

	/* Install the tracepoints */
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (interests[i].tp == NULL) {
			pr_info("Error: %s not found\n",
				interests[i].name);
			/* Unload previously loaded */
			mtk_btag_uninstall_tracepoints();
			return -EINVAL;
		}

		tracepoint_probe_register(interests[i].tp,
					  interests[i].func,
					  NULL);
		interests[i].init = true;
	}

	return 0;
}

static void mtk_btag_init_memory(void)
{
	phys_addr_t start, end;

	dram_start_addr = start = 0;
	dram_end_addr = end = memblock_end_of_DRAM();
	mtk_btag_system_dram_size = (unsigned long long)(end - start);
	pr_debug("[BLOCK_TAG] DRAM: %pa - %pa, size: 0x%llx\n", &start,
		&end, (unsigned long long)(end - start));
	blockio_aee_buffer = kzalloc(BLOCKIO_AEE_BUFFER_SIZE,
			     GFP_KERNEL);
}

static void mtk_btag_init_pidlogger(void)
{
	unsigned long count = mtk_btag_system_dram_size >> PAGE_SHIFT;
	unsigned long size = count * sizeof(struct page_pidlogger);

	if (mtk_btag_pagelogger)
		goto init;

#if IS_ENABLED(CONFIG_MTK_USE_RESERVED_EXT_MEM)
	mtk_btag_pagelogger = extmem_malloc_page_align(size);
#else
	mtk_btag_pagelogger = vmalloc(size);
#endif

init:
	if (mtk_btag_pagelogger)
		memset(mtk_btag_pagelogger, 0, size);
	else
		pr_info(
		"[BLOCK_TAG] blockio: fail to allocate mtk_btag_pagelogger\n");
}

static int mtk_btag_init_procfs(void)
{
	struct proc_dir_entry *proc_entry;
	kuid_t uid;
	kgid_t gid;

	if (btag_proc_root)
		return 0;

	uid = make_kuid(&init_user_ns, 0);
	gid = make_kgid(&init_user_ns, 1001);

	btag_proc_root = proc_mkdir("blocktag", NULL);

	proc_entry = proc_create("blockio", S_IFREG | 0444, btag_proc_root,
			      &mtk_btag_main_fops);

	if (proc_entry)
		proc_set_user(proc_entry, uid, gid);
	else
		pr_info("[BLOCK_TAG} %s: failed to initialize procfs", __func__);

	return 0;
}

void mtk_btag_get_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
	unsigned long free_size = BLOCKIO_AEE_BUFFER_SIZE;
	char *buff;

	buff = blockio_aee_buffer;
	mtk_btag_seq_main_info(&buff, &free_size, NULL);
	/* return start location */
	*vaddr = (unsigned long)blockio_aee_buffer;
	*size = BLOCKIO_AEE_BUFFER_SIZE - free_size;
}
EXPORT_SYMBOL(mtk_btag_get_aee_buffer);

static int __init mtk_btag_init(void)
{
	spin_lock_init(&list_lock);

	mtk_btag_init_memory();
	mtk_btag_init_pidlogger();
	mtk_btag_init_procfs();
	mtk_btag_earaio_init();
#if IS_ENABLED(CONFIG_MTK_FSCMD_TRACER)
	mtk_fscmd_init();
#endif

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_PM_DEBUG)
	mtk_btag_blk_pm_init();
#endif
	mtk_btag_install_tracepoints();
	mrdump_set_extra_dump(AEE_EXTRA_FILE_BLOCKIO, mtk_btag_get_aee_buffer);

	return 0;
}

static void __exit mtk_btag_exit(void)
{
	mrdump_set_extra_dump(AEE_EXTRA_FILE_BLOCKIO, NULL);
	proc_remove(btag_proc_root);
	mtk_btag_uninstall_tracepoints();
	kfree(blockio_aee_buffer);
	vfree(mtk_btag_pagelogger);
}

fs_initcall(mtk_btag_init);
module_exit(mtk_btag_exit);

MODULE_AUTHOR("Perry Hsu <perry.hsu@mediatek.com>");
MODULE_AUTHOR("Stanley Chu <stanley chu@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Storage Block IO Tracer");

